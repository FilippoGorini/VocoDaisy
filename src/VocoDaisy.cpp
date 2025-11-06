#include "daisy_seed.h"
#include "daisysp.h"
#include "TalkBoxProcessor.h"

using namespace daisy;
using namespace daisysp;

DaisySeed hw;
TalkBoxProcessor talkbox;

// Optional: parameters struct for initialization
TalkBoxParams params;

void AudioCallback(AudioHandle::InputBuffer in, AudioHandle::OutputBuffer out, size_t size)
{
    // Process the block
    talkbox.processBlock(
        in[0],      // modulator (voice)
        in[1],      // carrier
        out[0],     // left output
        out[1],     // right output
        size
    );
}

int main(void)
{
    // Initialize Daisy Seed hardware
    hw.Init();
    hw.SetAudioBlockSize(48); // block size
    hw.SetAudioSampleRate(SaiHandle::Config::SampleRate::SAI_48KHZ);
	// Get actual sample rate after audio setup
	float sample_rate = hw.AudioSampleRate();

    // Initialize TalkBoxProcessor
    params.quality = 1.0f; 				// adjust as needed
    params.wet     = 1.0f; 				// full effect
    params.dry     = 0.0f; 				// ignore dry voice if desired
    talkbox.init(sample_rate, params); 	// match hw sample rate

    // Start audio with callback
    hw.StartAudio(AudioCallback);

    while(1) {}
}
