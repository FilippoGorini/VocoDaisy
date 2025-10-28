#include "TalkBoxProcessor.h"
#include <algorithm>
#include <cmath>

using namespace std;


TalkBoxProcessor::TalkBoxProcessor() {          // This is the constructor
    buf0    = new float[BUF_MAX];               // Allocate memory for the buffers
    car0    = new float[BUF_MAX];
    buf1    = new float[BUF_MAX];
    car1    = new float[BUF_MAX];
    window  = new float[BUF_MAX];
    K = 0;                                      // Suggested by Gemini revision
    memset(buf0,0,sizeof(float)*BUF_MAX);       // Zero out the buffers
    memset(buf1,0,sizeof(float)*BUF_MAX);
    memset(car0,0,sizeof(float)*BUF_MAX);
    memset(car1,0,sizeof(float)*BUF_MAX);
}


TalkBoxProcessor::~TalkBoxProcessor() {         // This is the destructor
    delete[] buf0; delete[] car0;
    delete[] buf1; delete[] car1;
    delete[] window;
}


void TalkBoxProcessor::init(float sampleRate, const TalkBoxParams& p) {
    // 1) Clamp sample rate
    float fs = std::clamp(sampleRate, 8000.0f, 96000.0f);       // Clamp the sampling frequency to 8kHz - 96kHz

    // 2) Compute window length N (in samples)
    int32_t newN = static_cast<int32_t>(0.01633f * fs);
    newN = std::min(newN, BUF_MAX);

    // 3) Compute LPC order O from quality slider
    //    O = (0.0001 + 0.0004 * quality) * fs
    O = static_cast<int32_t>((0.0001f + 0.0004f * p.quality) * fs);

    // 4) Recompute Hanning window only if N changed
    if (newN != N) {
        N = newN;
        float dp    = TWO_PI / static_cast<float>(N);
        float phase = 0.0f;
        for (int32_t i = 0; i < N; ++i) {
            window[i] = 0.5f - 0.5f * std::cos(phase);
            phase    += dp;
        }
    }

    // 5) Compute wet/dry gains exactly as in the plugin
    wet = 0.5f * p.wet * p.wet;
    dry = 2.0f * p.dry * p.dry;

    // 6) Reset overlap-add indices & state
    pos      = 0;
    emphasis = 0.0f;
    FX       = 0.0f;

    // 7) Zero all pre-/de-emphasis filter states
    d0 = d1 = d2 = d3 = d4 = 0.0f;
    u0 = u1 = u2 = u3 = u4 = 0.0f;
}

void TalkBoxProcessor::processBlock(const float* modIn,
                                    const float* carIn,
                                    float* outL,
                                    float* outR,
                                    int32_t frames)
{
    // local copies of state
    int32_t   p0    = pos;
    int32_t   p1    = (pos + N/2) % N;
    float e     = emphasis;
    float fx    = FX;
    const float h0 = 0.3f;
    const float h1 = 0.77f;

    for (int32_t n = 0; n < frames; ++n)
    {
        // 1) Read inputs
        float o = modIn[n];   // modulator
        float x = carIn[n];   // carrier
        float dr = o;         // dry path copy

        // 2) Pre-filter the carrier via two all-pass sections
        {
            float p = d0 + h0 * x;
            d0 = d1;  d1 = x - h0 * p;
            float q = d2 + h1 * d4;
            d2 = d3;  d3 = d4 - h1 * q;
            d4 = x;
            x = p + q;
        }

        // 3) Every other sample, push through the window & run LPC
        if (K++)
        {
            K = 0;

            // a) capture the filtered carrier into both overlap buffers
            car0[p0] = car1[p1] = x;

            // b) Pre-emphasis on modulator
            x = o - e;
            e = o;

            // c) Window & overlap-add from buf0
            float w = window[p0];
            fx = buf0[p0] * w;
            buf0[p0] = x * w;
            if (++p0 >= N)
            {
                lpc(buf0, car0, N, O);
                p0 = 0;
            }

            // d) Window & overlap-add from buf1
            float w2 = 1.0f - w;
            fx += buf1[p1] * w2;
            buf1[p1] = x * w2;
            if (++p1 >= N)
            {
                lpc(buf1, car1, N, O);
                p1 = 0;
            }
        }

        // 4) Post-filter the combined LPC output via two all-pass sections
        {
            float p = u0 + h0 * fx;
            u0 = u1;  u1 = fx - h0 * p;
            float q = u2 + h1 * u4;
            u2 = u3;  u3 = u4 - h1 * q;
            u4 = fx;
            x = p + q;
        }

        // 5) Mix wet (vocoder) + dry (voice)
        float out = wet * x + dry * dr;
        outL[n] = out;
        outR[n] = out;
    }

    // store state back to members
    pos       = p0;
    emphasis  = e;
    FX        = fx;

    // At the end of your processBlock
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
