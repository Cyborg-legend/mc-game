#pragma once

#include <miniaudio.h>
#include <vector>

// Procedural sound system on top of miniaudio.
class AudioSystem {
public:
    bool Initialize();
    void Shutdown();

    void PlaySound(const char* name, float x = 0.0f, float y = 0.0f, float z = 0.0f, float volume = 1.0f);
    void UpdateListener(float x, float y, float z);

private:
    static constexpr int kSoundCount = 5;

    ma_engine engine_{};
    bool engineReady_ = false;

    std::vector<float> sampleStorage_[kSoundCount];
    ma_audio_buffer buffers_[kSoundCount]{};
    ma_sound sounds_[kSoundCount]{};
    bool bufferReady_[kSoundCount]{};

    int IndexForName(const char* name) const;
    void GenerateSamples();
};
