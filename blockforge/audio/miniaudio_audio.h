#pragma once

#include "core/platform/iplatform.h"
#include <miniaudio.h>
#include <string>

// Audio system using miniaudio for playback
class AudioSystem {
public:
    AudioSystem() : sndInstance(nullptr) {
        // Initialize miniaudio
        ma_result result = ma_init(nullptr, &sndInstance);
        if (result != MA_SUCCESS) {
            std::cerr << "Failed to initialize miniaudio" << std::endl;
        }
    }

    ~AudioSystem() {
        if (sndInstance) {
            ma_uninit(&sndInstance);
        }
    }

    // Initialize audio devices
    bool Initialize() {
        if (!sndInstance) return false;
        return true;
    }

    // Procedural sound generation
    // Generate a short noise burst for block break
    void GenerateBreakSound(BlockType type, float* samples, int numSamples) {
        // Generate different sounds based on block type
        for (int i = 0; i < numSamples; ++i) {
            // Random noise
            float noise = static_cast<float>(rand()) / RAND_MAX * 2.0f - 1.0f;
            
            // Vary pitch based on block type
            float pitch = 1.0f;
            switch (type) {
                case BlockType::Stone: pitch = 0.9f; break;
                case BlockType::Wood: pitch = 1.2f; break;
                case BlockType::Glass: // No glass type yet, but would be higher
                case BlockType::Dirt: pitch = 0.8f; break;
                default: pitch = 1.0f;
            }
            
            samples[i] = noise * pitch;
        }
    }

    // Generate click sound for block place
    void GeneratePlaceSound(float* samples, int numSamples) {
        // Simple click: short burst with envelope
        for (int i = 0; i < numSamples; ++i) {
            // Exponential decay envelope
            float t = static_cast<float>(i) / static_cast<float>(numSamples);
            float envelope = 1.0f - t; // Simple linear decay
            
            // Short click pulse
            float pulse = (i < numSamples / 4) ? 1.0f : 0.0f;
            
            samples[i] = pulse * envelope;
        }
    }

    // Generate jump sound (soft whoosh)
    void GenerateJumpSound(float* samples, int numSamples) {
        // Whoosh sound: rising noise
        for (int i = 0; i < numSamples; ++i) {
            // Rising pitch
            float progress = static_cast<float>(i) / static_cast<float>(numSamples);
            float freq = 200.0f + progress * 500.0f; // 200Hz to 700Hz
            
            // Noise modulated by sine wave
            float noise = static_cast<float>(rand()) / RAND_MAX * 2.0f - 1.0f;
            float sine = std::sin(2.0f * M_PI * freq * progress);
            
            samples[i] = noise * sine * 0.3f;
        }
    }

    // Generate hit thud sound
    void GenerateHitSound(float* samples, int numSamples) {
        // Thud: low-frequency noise burst
        for (int i = 0; i < numSamples; ++i) {
            // Decaying noise
            float t = static_cast<float>(i) / static_cast<float>(numSamples);
            float envelope = std::max(0.0f, 1.0f - t * 3.0f); // Quick decay
            float noise = static_cast<float>(rand()) / RAND_MAX * 2.0f - 1.0f;
            
            samples[i] = noise * envelope;
        }
    }

    // Generate UI click blip
    void GenerateClickSound(float* samples, int numSamples) {
        // Short high-pitch beep
        for (int i = 0; i < numSamples; ++i) {
            float t = static_cast<float>(i) / static_cast<float>(numSamples);
            // High frequency sine wave
            float freq = 800.0f;
            float sine = std::sin(2.0f * M_PI * freq * t);
            // Quick envelope
            float envelope = 1.0f - t * 5.0f;
            envelope = envelope > 0 ? envelope : 0;
            
            samples[i] = sine * envelope * 0.5f;
        }
    }

    // 3D positional audio - distance attenuation (1/r²)
    void ApplyDistanceAttenuation(float* samples, int numSamples, 
                                  float sourceX, float sourceY, float sourceZ,
                                  float listenerX, float listenerY, float listenerZ,
                                  float minDistance = 1.0f, float maxDistance = 100.0f) {
        for (int i = 0; i < numSamples; ++i) {
            // Calculate distance from source to listener
            float dx = sourceX - listenerX;
            float dy = sourceY - listenerY;
            float dz = sourceZ - listenerZ;
            float distance = std::sqrt(dx * dx + dy * dy + dz * dz);
            
            // Clamp distance
            float distanceClamped = std::max(minDistance, std::min(maxDistance, distance));
            
            // 1/r² attenuation
            float volume = 1.0f / (distanceClamped * distanceClamped);
            
            // Apply to sample
            // samples[i] would be multiplied by volume in the actual playback
            // For procedural generation, just scale the generated sample
            // This is a placeholder - actual implementation would modify existing samples
        }
    }

    // Set master volume
    void SetMasterVolume(float volume) {
        // Clamp to 0-1
        if (sndInstance) {
            ma_context_set_volume(sndInstance, 
                                  std::max(0.0f, std::min(1.0f, volume)));
        }
    }

private:
    ma_context sndInstance; // miniaudio context
};