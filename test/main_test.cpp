#include <iostream>
#include <vector>
#include <cstdint>
#include <string>
#include <algorithm>

#define DR_WAV_IMPLEMENTATION
#include "dr_wav.h"
#include "TalkBoxProcessor.h"

// Load WAV to mono float
bool loadWavToMono(const char* path, std::vector<float>& monoData, unsigned int& sampleRate, uint64_t& totalFrames) {
    drwav wav;
    if (!drwav_init_file(&wav, path, nullptr)) {
        std::cerr << "Failed to open WAV file: " << path << std::endl;
        return false;
    }

    sampleRate = wav.sampleRate;
    totalFrames = wav.totalPCMFrameCount;
    unsigned int channels = wav.channels;

    std::vector<float> interleavedData(totalFrames * channels);
    drwav_read_pcm_frames_f32(&wav, totalFrames, interleavedData.data());
    drwav_uninit(&wav);

    monoData.resize(totalFrames);
    if (channels == 1) {
        monoData = interleavedData;
    } else {
        for (uint64_t i = 0; i < totalFrames; ++i) {
            monoData[i] = interleavedData[i * channels]; // first channel
        }
    }
    return true;
}

int main(int argc, char** argv) {

    std::string modPath = "mod.wav";
    std::string carPath = "car.wav";
    std::string outPath = "out.wav";
    int blockSize = 48;

    if (argc == 5) {
        modPath   = argv[1];
        carPath   = argv[2];
        outPath   = argv[3];
        blockSize = std::stoi(argv[4]);
    } else {
        std::cout << "Usage: " << argv[0]
                << " <modulator.wav> <carrier.wav> <output.wav> <blockSize>\n";
        std::cout << "No arguments provided - using defaults:\n";
        std::cout << "  modulator: " << modPath << "\n"
                << "  carrier:   " << carPath << "\n"
                << "  output:    " << outPath << "\n"
                << "  blockSize: " << blockSize << "\n";
    }

    // Load modulator
    std::vector<float> modMonoData;
    unsigned int modSampleRate;
    uint64_t modTotalFrames;
    if (!loadWavToMono(modPath.c_str(), modMonoData, modSampleRate, modTotalFrames)) return 1;

    // Load carrier
    std::vector<float> carMonoData;
    unsigned int carSampleRate;
    uint64_t carTotalFrames;
    if (!loadWavToMono(carPath.c_str(), carMonoData, carSampleRate, carTotalFrames)) return 1;

    if (modSampleRate != carSampleRate) {
        std::cerr << "Sample rates must match!\n";
        return 1;
    }

    uint64_t totalFrames = std::min(modTotalFrames, carTotalFrames);
    modMonoData.resize(totalFrames);
    carMonoData.resize(totalFrames);

    std::vector<float> outL(totalFrames);
    std::vector<float> outR(totalFrames);

    TalkBoxParams params{1.0f, 0.0f, 1.0f};
    TalkBoxProcessor engine;
    engine.init(static_cast<float>(modSampleRate), params);

    for (uint64_t pos = 0; pos < totalFrames; pos += blockSize) {
        int curBlock = std::min(blockSize, static_cast<int>(totalFrames - pos));
        engine.processBlock(modMonoData.data() + pos,
                            carMonoData.data() + pos,
                            outL.data() + pos,
                            outR.data() + pos,
                            curBlock);
    }

    drwav_data_format fmt = {};
    fmt.container     = drwav_container_riff;
    fmt.format        = DR_WAVE_FORMAT_IEEE_FLOAT;
    fmt.channels      = 2;
    fmt.sampleRate    = modSampleRate;
    fmt.bitsPerSample = 32;

    drwav outWav;
    if (!drwav_init_file_write(&outWav, outPath.c_str(), &fmt, nullptr)) {
        std::cerr << "Failed to open output WAV!\n";
        return 1;
    }

    // Interleave L/R
    std::vector<float> interleaved(totalFrames * 2);
    for (uint64_t i = 0; i < totalFrames; ++i) {
        interleaved[2*i]   = outL[i];
        interleaved[2*i+1] = outR[i];
    }

    drwav_write_pcm_frames(&outWav, totalFrames, interleaved.data());
    drwav_uninit(&outWav);

    std::cout << "Processing done: " << totalFrames << " frames written to " << outPath << "\n";
    return 0;
}
