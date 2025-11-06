#pragma once
#include <cstdint>
#include <cmath>
#include <cstring>


static constexpr int32_t BUF_MAX = 1600;
static constexpr int32_t ORD_MAX = 50;
static constexpr float TWO_PI = 6.28318530717958647692f;


struct TalkBoxParams {
    float wet     = 1.0f;       // [0..1]
    float dry     = 0.0f;       // [0..1]
    float quality = 1.0f;       // [0..1]
};


class TalkBoxProcessor {
    public:
        TalkBoxProcessor();        // Constructor
        ~TalkBoxProcessor();       // Destructor

        // Update parameters in runtime
        void updateParams(const TalkBoxParams& params);

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
        float* buf0_;
        float* buf1_;
        float* car0_;
        float* car1_;
        float* window_;

        // Processing state
        int32_t   N_ = 0;            // current window size
        int32_t   order_ = 0;        // LPC order
        int32_t   pos_ = 0;          // write index
        int32_t   K_ = 0;            // half-rate toggle
        float fs_ = 48000.0f;        // Store the sample rate
        float wet_gain_ = 0.5f;
        float dry_gain_ = 0.0f;
        float emphasis_ = 0.0f;
        float FX_ = 0.0f;

        // Pre-emphasis and de-emphasis filter states
        float d0_ = 0, d1_ = 0, d2_ = 0, d3_ = 0, d4_ = 0;
        float u0_ = 0, u1_ = 0, u2_ = 0, u3_ = 0, u4_ = 0;
};
