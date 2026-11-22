#pragma once

#include <miniaudio.h>

struct AudioSource {
    ma_sound sound;
    bool isPlaying = false;
    float volume = 1.0f;
    float lastX = 0.0f;
    float lastY = 0.0f;
    float lastZ = 0.0f;
};

struct AudioSystem {
    ma_engine engine;
    
    // Initialize audio engine
    bool Initialize();
    
    // Shut down audio
    void Shutdown();
    
    // Play a sound at a position (3D)
    void PlaySound(const char* name, float x, float y, float z, float volume = 1.0f);
    
    // Update 3D listener position
    void UpdateListener(float x, float y, float z);
    
    // Get the engine handle
    ma_engine GetEngine() const { return engine; }
};