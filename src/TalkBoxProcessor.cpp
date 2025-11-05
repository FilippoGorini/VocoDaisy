#include "TalkBoxProcessor.h"
#include <algorithm>
#include <cmath>

using namespace std;


// Class constructor
TalkBoxProcessor::TalkBoxProcessor() {       
    // Allocate memory for the four main overlap-add (OLA) buffers.
    // - buf0/buf1 hold the *modulator* (voice) signal, windowed.
    //   They are later overwritten by the synthesized (vocoded) output.
    // - car0/car1 hold the *carrier* (synth) signal.
    buf0    = new float[BUF_MAX];               // Allocate memory for the buffers
    car0    = new float[BUF_MAX];
    buf1    = new float[BUF_MAX];
    car1    = new float[BUF_MAX];

    // Allocate memory for the Hanning window lookup table.
    window  = new float[BUF_MAX];

    // Initialize state variables.
    // - N = 1: Setting N to 1 (or any value != a calculated newN)
    //   ensures that the Hanning window is *always* calculated
    //   on the first call to init().
    // - K = 0: This is the toggle for the half-rate processing.
    N = 1;
    K = 0;              // Suggested by Gemini revision
    
    // Zero out all buffers to prevent processing garbage audio on the first pass.
    memset(buf0,0,sizeof(float)*BUF_MAX);      
    memset(buf1,0,sizeof(float)*BUF_MAX);
    memset(car0,0,sizeof(float)*BUF_MAX);
    memset(car1,0,sizeof(float)*BUF_MAX);
}

// Class destructor
TalkBoxProcessor::~TalkBoxProcessor() {     
    delete[] buf0; delete[] car0;
    delete[] buf1; delete[] car1;
    delete[] window;
}


// Class initialization method
void TalkBoxProcessor::init(float sampleRate, const TalkBoxParams& params) {
    // Clamp sample rate
    float fs = std::clamp(sampleRate, 8000.0f, 96000.0f);    

    // Compute window length N (in samples). This is the "analysis frame" size.
    // The magic number 0.01633f corresponds to ~784 samples at 48kHz.
    int32_t newN = static_cast<int32_t>(0.01633f * fs);
    newN = std::min(newN, BUF_MAX);     // Ensure it doesn't exceed buffer size

    // Compute LPC order order from quality slider
    //      order = (0.0001 + 0.0004 * quality) * fs
    order = static_cast<int32_t>((0.0001f + 0.0004f * params.quality) * fs);

    // Clamp order to be less than ORD_MAX to prevent stack buffer overflows
    // in the lpc() and lpc_durbin() functions, which use stack arrays of size ORD_MAX.
    order = std::min(order, ORD_MAX - 1);

    // Recompute Hanning window only if N changed
    if (newN != N) {
        N = newN;
        float dp    = TWO_PI / static_cast<float>(N);
        float phase = 0.0f;
        for (int32_t i = 0; i < N; ++i) {
            window[i] = 0.5f - 0.5f * std::cos(phase);
            phase    += dp;
        }
    }

    // Compute wet/dry gains exactly as in the plugin
    wet_gain = 0.5f * params.wet * params.wet;
    dry_gain = 2.0f * params.dry * params.dry;

    // Reset OLA write pointers and processing state.
    pos      = 0;
    emphasis = 0.0f;
    FX       = 0.0f;

    // Zero all pre-/de-emphasis all-pass filter states
    d0 = d1 = d2 = d3 = d4 = 0.0f;
    u0 = u1 = u2 = u3 = u4 = 0.0f;
}


// Process a block of samples
void TalkBoxProcessor::processBlock(const float* modIn, 
                                    const float* carIn,
                                    float* outL,
                                    float* outR,
                                    int32_t frames)     // block size
{
    // Accessing local variables is usually faster than accessing class 
    // member variables repeatedly, so we create local copies of the state variables
    int32_t p0      = pos;
    int32_t p1      = (pos + N/2) % N;      // 50% offset pointer
    float   emph    = emphasis;
    float   fx      = FX;

    // Filter coefficients for the all-pass pre/post filters
    const float h0  = 0.3f;
    const float h1  = 0.77f;

    // Main loop which processes 1 sample at a time
    for (int32_t n = 0; n < frames; ++n)
    {
        // Read inputs (m and c are just single samples)
        float m = modIn[n];       // modulator
        float c = carIn[n];       // carrier
        float dry = m;            // dry path copy

        // Pre-filter the carrier
        // This is a fixed filter (two 1st-order all-pass sections)
        // that "smears" the phase. It's not part of LPC, but
        // it thickens the carrier sound, making the result less "buzzy".
        {
            float p = d0 + h0 * c;
            d0 = d1;  d1 = c - h0 * p;
            float q = d2 + h1 * d4;
            d2 = d3;  d3 = d4 - h1 * q;
            d4 = c;
            c = p + q;    // c now holds the filtered carrier
        }

        // Half-Rate Processing: Run LPC every OTHER sample.
        // The 'K' variable toggles 0, 1, 0, 1...
        if (K++)
        {
            K = 0;           // reset toggle

            // Capture the filtered carrier into both OLA buffers.
            // p0 and p1 are the two 50%-offset write pointers.
            car0[p0] = car1[p1] = c;

            // Pre-emphasis on modulator (o).
            // This is a simple high-pass filter: x = o(t) - o(t-1)
            // It boosts high frequencies, which helps the LPC algorithm "see" high-frequency formants more clearly.
            c = m - emph;
            emph = m;

            // Window & OLA for the *first* buffer (buf0)
            float w = window[p0];

            // Read "old" vocoded audio *out* of the buffer, fading it
            // *out* with the window. This sample was written N samples ago.
            fx = buf0[p0] * w;

            // Write the *new* pre-emphasized modulator *in*, fading it
            // *in* with the window.
            buf0[p0] = c * w;

            // Check if this buffer is full...
            if (++p0 >= N)
            {   
                // If yes, run the LPC analysis/synthesis.
                // lpc() will:
                //   1. ANALYZE 'buf0' (modulator) to find filter coeffs.
                //   2. SYNTHESIZE by filtering 'car0' (carrier)
                //   3. OVERWRITE 'buf0' with the new vocoded audio.
                lpc(buf0, car0, N, order);
                p0 = 0;         // Wrap pointer
            }

            // Window & OLA for the *second* buffer (buf1)
            // This is identical, but uses the 50%-offset pointer 'p1' and a complementary window.
            float w2 = 1.0f - w;

            // Read "old" vocoded audio and *add* it to fx.
            // This is the "overlap-add": we add the fading-out
            // signal from buf1 to the fading-out signal from buf0.
            fx += buf1[p1] * w2;

            // Write the *new* modulator in.
            buf1[p1] = c * w2;

            // Check if this buffer is full...
            if (++p1 >= N)
            {   
                // As before, if yes run LPC analysis/synthesis.
                lpc(buf1, car1, N, order);
                p1 = 0;         // Wrap pointer
            }
        }

        // Post-filter the combined LPC output (fx)
        // This applies the *exact same* all-pass filter as in step 2.
        // This is a common technique to "un-smear" the phase,
        // though in this case it just adds more color.
        {
            float p = u0 + h0 * fx;
            u0 = u1;  u1 = fx - h0 * p;
            float q = u2 + h1 * u4;
            u2 = u3;  u3 = u4 - h1 * q;
            u4 = fx;
            c = p + q;    // 'c' is now the final WET (vocoded) signal
        }

        // Mix wet (vocoded) + dry (voice)
        float out = wet_gain * c + dry_gain * dry;

        // Write to stereo output buffers
        outL[n] = out;
        outR[n] = out;
    }

    // store state back to member variables
    pos       = p0;
    emphasis  = emph;
    FX        = fx;

    // This code prevents "denormal" numbers (very, very small floats
    // near zero) from crippling the FPU. If a filter state is
    // effectively zero, just set it to 0.0f.
    float den = 1.0e-10f;
    if (std::abs(d0) < den) d0 = 0.0f;
    if (std::abs(d1) < den) d1 = 0.0f;
    if (std::abs(d2) < den) d2 = 0.0f;
    if (std::abs(d3) < den) d3 = 0.0f;
    // d4 is an input snapshot, not a decaying state in the same way,
    // but the original doesn't clear it.
    // The original only clears d0,d1,d2,d3 and u0,u1,u2,u3.
    if (std::abs(u0) < den) u0 = 0.0f;
    if (std::abs(u1) < den) u1 = 0.0f;
    if (std::abs(u2) < den) u2 = 0.0f;
    if (std::abs(u3) < den) u3 = 0.0f;
}


void TalkBoxProcessor::lpc(float* buf, float* car, int32_t n, int32_t o)
{
    float z[ORD_MAX], r[ORD_MAX], k[ORD_MAX], G, x;
    int32_t i, j, nn = n;

    r[0] = 0.0f;  // ensure it's initialized just to avoid the warning when compiling
    for (j = 0; j <= o; j++, nn--)  //buf[] is already emphasized and windowed
    {
        z[j] = r[j] = 0.0f;
        for (i = 0; i < nn; i++) r[j] += buf[i] * buf[i + j]; //autocorrelation
    }
    r[0] *= 1.001f;  //stability fix

    float min = 0.00001f;
    if (r[0] < min) { for (i = 0; i < n; i++) buf[i] = 0.0f; return; }

    lpc_durbin(r, o, k, &G);  //calc reflection coeffs

    for (i = 0; i <= o; i++)
    {
        if (k[i] > 0.995f) k[i] = 0.995f; else if (k[i] < -0.995f) k[i] = -.995f;
    }

    for (i = 0; i < n; i++)
    {
        x = G * car[i];
        for (j = o; j > 0; j--)  //lattice filter
        {
            x -= k[j] * z[j - 1];
            z[j] = z[j - 1] + k[j] * x;
        }
        buf[i] = z[0] = x;  //output buf[] will be windowed elsewhere
    }
}


void TalkBoxProcessor::lpc_durbin(float* r, int32_t p, float* k, float* g)
{
    int32_t i, j;
    float a[ORD_MAX], at[ORD_MAX], e = r[0];

    for (i = 0; i <= p; i++) a[i] = at[i] = 0.0f; //probably don't need to clear at[] or k[]

    for (i = 1; i <= p; i++)
    {
        k[i] = -r[i];

        for (j = 1; j < i; j++)
        {
            at[j] = a[j];
            k[i] -= a[j] * r[i - j];
        }
        if (fabs(e) < 1.0e-20f) { e = 0.0f;  break; }
        k[i] /= e;

        a[i] = k[i];
        for (j = 1; j < i; j++) a[j] = at[j] + k[i] * at[i - j];

        e *= 1.0f - k[i] * k[i];
    }

    if (e < 1.0e-20f) e = 0.0f;
    *g = (float)sqrt(e);
}