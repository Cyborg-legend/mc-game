#include "audio/audio.h"

#include <cmath>
#include <cstring>
#include <string>

namespace {
constexpr int kSampleRate = 44100;
constexpr int kChannels = 2;
constexpr float kPi = 3.14159265358979323846f;

void FillTone(std::vector<float>& out, float freq, float seconds, float noiseMix, float decay) {
    const int frames = static_cast<int>(kSampleRate * seconds);
    out.assign(static_cast<size_t>(frames) * kChannels, 0.0f);
    unsigned int seed = 12345u;
    for (int i = 0; i < frames; ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(kSampleRate);
        seed = seed * 1664525u + 1013904223u;
        const float noise = (static_cast<float>((seed >> 8) & 0xFFFF) / 32767.5f) - 1.0f;
        const float env = std::exp(-t * decay);
        const float v = (std::sin(2.0f * kPi * freq * t) * (1.0f - noiseMix) + noise * noiseMix) * env * 0.45f;
        out[static_cast<size_t>(i) * kChannels + 0] = v;
        out[static_cast<size_t>(i) * kChannels + 1] = v;
    }
}
} // namespace

void AudioSystem::GenerateSamples() {
    FillTone(sampleStorage_[0], 220.0f, 0.14f, 0.85f, 18.0f); // break
    FillTone(sampleStorage_[1], 520.0f, 0.08f, 0.20f, 30.0f); // place
    FillTone(sampleStorage_[2], 330.0f, 0.16f, 0.35f, 12.0f); // jump
    FillTone(sampleStorage_[3], 110.0f, 0.18f, 0.50f, 10.0f); // hit
    FillTone(sampleStorage_[4], 880.0f, 0.06f, 0.05f, 40.0f); // ui
}

int AudioSystem::IndexForName(const char* name) const {
    if (!name) return -1;
    const std::string n(name);
    if (n == "break") return 0;
    if (n == "place") return 1;
    if (n == "jump") return 2;
    if (n == "hit") return 3;
    if (n == "ui") return 4;
    if (n == "portal") return 4;
    return -1;
}

bool AudioSystem::Initialize() {
    ma_engine_config config = ma_engine_config_init();
    if (ma_engine_init(&config, &engine_) != MA_SUCCESS) {
        return false;
    }
    engineReady_ = true;

    GenerateSamples();

    for (int i = 0; i < kSoundCount; ++i) {
        const ma_uint64 frames = static_cast<ma_uint64>(sampleStorage_[i].size() / kChannels);
        ma_audio_buffer_config bufferConfig = ma_audio_buffer_config_init(
            ma_format_f32, kChannels, frames, sampleStorage_[i].data(), nullptr);
        if (ma_audio_buffer_init(&bufferConfig, &buffers_[i]) != MA_SUCCESS) {
            continue;
        }
        if (ma_sound_init_from_data_source(&engine_, &buffers_[i], 0, nullptr, &sounds_[i]) != MA_SUCCESS) {
            ma_audio_buffer_uninit(&buffers_[i]);
            continue;
        }
        ma_sound_set_spatialization_enabled(&sounds_[i], MA_FALSE);
        bufferReady_[i] = true;
    }

    return true;
}

void AudioSystem::Shutdown() {
    if (!engineReady_) {
        return;
    }
    for (int i = 0; i < kSoundCount; ++i) {
        if (bufferReady_[i]) {
            ma_sound_uninit(&sounds_[i]);
            ma_audio_buffer_uninit(&buffers_[i]);
            bufferReady_[i] = false;
        }
    }
    ma_engine_uninit(&engine_);
    engineReady_ = false;
}

void AudioSystem::PlaySound(const char* name, float x, float y, float z, float volume) {
    (void)x; (void)y; (void)z;
    if (!engineReady_) {
        return;
    }
    const int index = IndexForName(name);
    if (index < 0 || !bufferReady_[index]) {
        return;
    }
    ma_sound_set_volume(&sounds_[index], volume);
    ma_sound_seek_to_pcm_frame(&sounds_[index], 0);
    ma_sound_start(&sounds_[index]);
}

void AudioSystem::UpdateListener(float x, float y, float z) {
    if (!engineReady_) {
        return;
    }
    ma_engine_listener_set_position(&engine_, 0, x, y, z);
}
