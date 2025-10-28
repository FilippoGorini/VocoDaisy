#pragma once
#include <cstdint>
#include <cmath>
#include <cstring>


static constexpr int32_t BUF_MAX = 1600;
static constexpr int32_t ORD_MAX = 50;
static constexpr float TWO_PI = 6.28318530717958647692f;


struct TalkBoxParams {
    float wet     = 0.5f;       // [0..1]
    float dry     = 0.0f;       // [0..1]
    float quality = 1.0f;       // [0..1]
};


class TalkBoxProcessor {
    public:
        TalkBoxProcessor();        // Constructor
        ~TalkBoxProcessor();       // Destructor

        // Initialize engine: must call before processing
        void init(float sampleRate, const TalkBoxParams& params);

        // Process a block of `frames` samples; modulator and carrier are mono
        void processBlock(const float* modIn,
                            const float* carIn,
                            float* outL,
                            float* outR,
                            int32_t frames  );  

    private:
        // LPC helper functions (credits to mda plugins)
        void lpc(float* buf, float* car, int32_t n, int32_t o);
        void lpc_durbin(float* r, int32_t p, float* k, float* g);

        // Overlap-add buffers for voice and carrier
        float* buf0;
        float* buf1;
        float* car0;
        float* car1;
        float* window;

        // Processing state
        int32_t   N = 0;           // current window size
        int32_t   O = 0;           // LPC order
        int32_t   pos = 0;         // write index
        int32_t   K = 0;           // half-rate toggle
        float wet = 0.5f;
        float dry = 0.0f;
        float emphasis = 0.0f;
        float FX = 0.0f;

        // Pre-emphasis and de-emphasis filter states
        float d0 = 0, d1 = 0, d2 = 0, d3 = 0, d4 = 0;
        float u0 = 0, u1 = 0, u2 = 0, u3 = 0, u4 = 0;
};
