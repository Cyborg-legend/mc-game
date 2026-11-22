#include <miniaudio.h>
#include <string>
#include <vector>
#include <cmath>

static ma_context ctx;
static ma_engine engine;
static ma_device device;
static bool audioInitialized = false;
static float masterVolume = 0.5f;

bool AudioInit() {
    ma_context_config ctxConfig = ma_context_config_init();
    if (ma_context_init(&ctxConfig, &ctx) != MA_SUCCESS) return false;

    ma_engine_config engConfig = ma_engine_config_init();
    if (ma_engine_init(&ctx, &engConfig, &engine) != MA_SUCCESS) return false;

    ma_device_config devConfig = ma_device_config_init(ma_device_type_playback);
    devConfig.playback.format = ma_format_f32;
    devConfig.playback.channels = 2;
    devConfig.playback.sampleRate = 44100;
    devConfig.playback.deviceId = MA_DEVICE_ID_DEFAULT;

    if (ma_device_init(&engine, &devConfig, &device) != MA_SUCCESS) return false;
    if (ma_device_start(&device) != MA_SUCCESS) return false;

    audioInitialized = true;
    return true;
}

void AudioShutdown() {
    if (!audioInitialized) return;
    ma_device_uninit(&device);
    ma_engine_uninit(&engine);
    ma_context_uninit(&ctx);
    audioInitialized = false;
}

void AudioSetListener(float x, float y, float z) {
    if (!audioInitialized) return;
    ma_engine_set_listener(&engine, x, y, z, 0, -1, 0, 0, 0, -1);
}

void AudioSetVolume(float vol) {
    masterVolume = vol;
    if (audioInitialized)
        ma_engine_set_volume(&engine, vol);
}

void AudioPlayBreakSound() {
    if (!audioInitialized) return;
    static float samples[441];
    for (int i = 0; i < 441; ++i) {
        samples[i] = ((float)rand() / RAND_MAX * 2.0f - 1.0f) * (1.0f - (float)i / 441.0f) * 0.3f;
    }

    ma_sound_config config = ma_sound_config_init();
    config.playback.frameCount = 441;
    config.playback.channels = 1;
    config.playback.sampleRate = 44100;
    config.loop = false;

    ma_sound sound;
    if (ma_sound_init_from_data(&engine, samples, sizeof(samples), &config, &sound) == MA_SUCCESS) {
        ma_sound_start(&sound);
    }
}

void AudioPlayPlaceSound() {
    if (!audioInitialized) return;
    static float samples[220];
    for (int i = 0; i < 220; ++i) {
        samples[i] = sinf((float)i / 220.0f * 3.14159f * 4.0f) * (1.0f - (float)i / 220.0f) * 0.2f;
    }

    ma_sound_config config = ma_sound_config_init();
    config.playback.frameCount = 220;
    config.playback.channels = 1;
    config.playback.sampleRate = 44100;
    config.loop = false;

    ma_sound sound;
    if (ma_sound_init_from_data(&engine, samples, sizeof(samples), &config, &sound) == MA_SUCCESS) {
        ma_sound_start(&sound);
    }
}

void AudioPlayJumpSound() {
    if (!audioInitialized) return;
    static float samples[330];
    for (int i = 0; i < 330; ++i) {
        float t = (float)i / 330.0f;
        samples[i] = sinf(t * 3.14159f * 6.0f) * (1.0f - t) * 0.1f;
    }

    ma_sound_config config = ma_sound_config_init();
    config.playback.frameCount = 330;
    config.playback.channels = 1;
    config.playback.sampleRate = 44100;
    config.loop = false;

    ma_sound sound;
    if (ma_sound_init_from_data(&engine, samples, sizeof(samples), &config, &sound) == MA_SUCCESS) {
        ma_sound_start(&sound);
    }
}   