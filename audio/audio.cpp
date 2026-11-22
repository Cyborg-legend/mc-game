#include "audio.h"
#include <cmath>
#include <cstring>
#include <string>

// Static sound data buffers - these are generated at init
static const float kSampleRate = 44100.0f;
static const int kNumChannels = 2; // Stereo

// Generate a noise burst sound (for block break)
static void GenerateNoiseBurst(float* samples, int numSamples) {
    for (int i = 0; i < numSamples; ++i) {
        // Random noise
        float val = static_cast<float>(rand()) / RAND_MAX - 0.5f;
        samples[i * kNumChannels + 0] = val; // Left
        samples[i * kNumChannels + 1] = val; // Right
    }
}

// Generate a click sound (for block place)
static void GenerateClick(float* samples, int numSamples) {
    // Simple sine wave click
    for (int i = 0; i < numSamples; ++i) {
        float t = static_cast<float>(i) / kSampleRate;
        // Short click envelope
        float envelope = 1.0f - (static_cast<float>(i) / static_cast<float>(numSamples));
        float val = std::sinf(2.0f * 440.0f * 3.14159f * t) * envelope * 0.5f;
        samples[i * kNumChannels + 0] = val;
        samples[i * kNumChannels + 1] = val;
    }
}

// Generate a whoosh sound (for jump)
static void GenerateWhoosh(float* samples, int numSamples) {
    // Rising noise burst
    for (int i = 0; i < numSamples; ++i) {
        float t = static_cast<float>(i) / kSampleRate;
        // Rising pitch
        float freq = 200.0f + t * 1000.0f;
        float val = std::sinf(2.0f * 3.14159f * freq * t) * 0.3f;
        // Noise component
        val += (static_cast<float>(rand()) / RAND_MAX - 0.5f) * 0.1f;
        // Envelope
        float envelope = 1.0f - (static_cast<float>(i) / static_cast<float>(numSamples)) * 0.5f;
        val *= envelope;
        samples[i * kNumChannels + 0] = val;
        samples[i * kNumChannels + 1] = val;
    }
}

// Generate a thud sound (for hit)
static void GenerateThud(float* samples, int numSamples) {
    // Low frequency thud
    for (int i = 0; i < numSamples; ++i) {
        float t = static_cast<float>(i) / kSampleRate;
        // Low frequency sine with fast decay
        float freq = 100.0f;
        float val = std::sinf(2.0f * 3.14159f * freq * t) * std::exp(-t * 5.0f);
        val *= 0.4f;
        samples[i * kNumChannels + 0] = val;
        samples[i * kNumChannels + 1] = val;
    }
}

// Generate a UI blip sound
static void GenerateBlip(float* samples, int numSamples) {
    // Short high-pitch beep
    for (int i = 0; i < numSamples; ++i) {
        float t = static_cast<float>(i) / kSampleRate;
        // High frequency sine
        float val = std::sinf(2.0f * 880.0f * 3.14159f * t);
        // Fast envelope
        float envelope = 1.0f - (static_cast<float>(i) / static_cast<float>(numSamples)) * 0.8f;
        val *= envelope;
        samples[i * kNumChannels + 0] = val * 0.5f;
        samples[i * kNumChannels + 1] = val * 0.5f;
    }
}

bool AudioSystem::Initialize() {
    // miniaudio uses ma_engine_init, not ma_engine_engine_config
    ma_engine_config config = ma_engine_init_config();
    config.playback.channels = kNumChannels;
    config.capture.channels = 0;

    ma_result result = ma_engine_init(&config, &engine);
    if (result != MA_SUCCESS) {
        return false;
    }

    // Initialize default listener
    ma_engine_set_listener(&engine, nullptr);

    return true;
}

void AudioSystem::Shutdown() {
    if (engine) {
        ma_engine_uninit(&engine);
    }
}

void AudioSystem::PlaySound(const char* name, float x, float y, float z, float volume) {
    // Generate sound data based on name
    int numSamples = 44100 / 10; // ~0.1 second clip
    float* samples = new float[numSamples * kNumChannels];

    if (std::string(name) == "break") {
        GenerateNoiseBurst(samples, numSamples);
    } else if (std::string(name) == "place") {
        GenerateClick(samples, numSamples);
    } else if (std::string(name) == "jump") {
        GenerateWhoosh(samples, numSamples);
    } else if (std::string(name) == "hit") {
        GenerateThud(samples, numSamples);
    } else if (std::string(name) == "ui") {
        GenerateBlip(samples, numSamples);
    } else {
        delete[] samples;
        return;
    }

    // Create miniaudio sound using ma_sound_init_from_data_device
    ma_sound_config soundConfig = ma_sound_config_init();
    soundConfig.playback.format = ma_format_f32;
    soundConfig.playback.channels = kNumChannels;
    soundConfig.playback.sampleRate = kSampleRate;

    ma_sound sound;
    result = ma_sound_init_from_data_device(&engine, samples, numSamples * kNumChannels * sizeof(float),
        &soundConfig, nullptr, nullptr, &sound);

    if (result == MA_SUCCESS) {
        // Set 3D position if we have position data
        // miniaudio 3D would use panning based on listener position
        // For now, just play at default position
        ma_sound_start(&sound);
    }

    delete[] samples;
}

void AudioSystem::UpdateListener(float x, float y, float z) {
    // Update the engine's listener position using correct API
    ma_engine_set_listener(&engine, &(ma_vec3){ x, y, z });
}