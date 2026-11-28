#include "audio.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

/* The mixer is deliberately tiny: short mono blips queued on the system wave
 * device. Platforms without the winmm API simply stay silent. */
#ifdef _WIN32

#include <windows.h>
#include <mmsystem.h>

#define AUDIO_RATE 22050
#define AUDIO_VOICES 16
#define AUDIO_SAMPLES (AUDIO_RATE / 3)

static HWAVEOUT g_device;
static WAVEHDR g_headers[AUDIO_VOICES];
static short *g_buffers[AUDIO_VOICES];
static int g_next_voice;
static int g_ready;

static float envelope(float t, float duration) {
    const float attack = t < 0.008f ? t / 0.008f : 1.0f;
    const float decay = 1.0f - t / duration;
    return attack * (decay > 0.0f ? decay * decay : 0.0f);
}

static void synthesise(BgSound sound, short *out, int samples) {
    const float duration = (float)samples / (float)AUDIO_RATE;
    unsigned int noise = 0x12345u + (unsigned int)sound * 7919u;
    float phase = 0.0f;
    float frequency = 220.0f;
    float noise_mix = 0.15f;
    float gain = 0.28f;

    switch (sound) {
        case BG_SOUND_PLACE: frequency = 180.0f; noise_mix = 0.35f; gain = 0.22f; break;
        case BG_SOUND_BREAK: frequency = 120.0f; noise_mix = 0.75f; gain = 0.24f; break;
        case BG_SOUND_HIT:   frequency = 260.0f; noise_mix = 0.45f; gain = 0.26f; break;
        case BG_SOUND_HURT:  frequency = 150.0f; noise_mix = 0.25f; gain = 0.32f; break;
        case BG_SOUND_CRAFT: frequency = 520.0f; noise_mix = 0.05f; gain = 0.20f; break;
        case BG_SOUND_RIFT:  frequency = 90.0f;  noise_mix = 0.30f; gain = 0.30f; break;
        case BG_SOUND_PICKUP: frequency = 700.0f; noise_mix = 0.02f; gain = 0.16f; break;
        case BG_SOUND_CLICK: frequency = 440.0f; noise_mix = 0.0f;  gain = 0.12f; break;
        case BG_SOUND_BOSS:  frequency = 62.0f;  noise_mix = 0.40f; gain = 0.34f; break;
        case BG_SOUND_WIN:   frequency = 330.0f; noise_mix = 0.0f;  gain = 0.26f; break;
        case BG_SOUND_RUMBLE: frequency = 48.0f; noise_mix = 0.55f; gain = 0.30f; break;
        default: break;
    }

    for (int i = 0; i < samples; ++i) {
        const float t = (float)i / (float)AUDIO_RATE;
        const float env = envelope(t, duration);
        float sweep = frequency;
        if (sound == BG_SOUND_RIFT || sound == BG_SOUND_BOSS ||
            sound == BG_SOUND_RUMBLE) {
            sweep = frequency * (1.0f - t * 0.8f);
        } else if (sound == BG_SOUND_PICKUP || sound == BG_SOUND_CRAFT) {
            sweep = frequency * (1.0f + t * 1.6f);
        } else if (sound == BG_SOUND_WIN) {
            sweep = frequency * (1.0f + sinf(t * 12.0f) * 0.25f);
        }
        phase += sweep / (float)AUDIO_RATE;
        if (phase > 1.0f) {
            phase -= 1.0f;
        }
        noise = noise * 1664525u + 1013904223u;
        const float hiss = ((float)((noise >> 9) & 0xFFFFu) / 32768.0f) - 1.0f;
        const float tone = (phase < 0.5f ? 1.0f : -1.0f) * 0.6f +
                           (phase * 2.0f - 1.0f) * 0.4f;
        float value = tone * (1.0f - noise_mix) + hiss * noise_mix;
        value *= env * gain;
        if (value > 1.0f) value = 1.0f;
        if (value < -1.0f) value = -1.0f;
        out[i] = (short)(value * 32767.0f);
    }
}

void audio_init(void) {
    WAVEFORMATEX format;
    format.wFormatTag = WAVE_FORMAT_PCM;
    format.nChannels = 1;
    format.nSamplesPerSec = AUDIO_RATE;
    format.wBitsPerSample = 16;
    format.nBlockAlign = (WORD)(format.nChannels * format.wBitsPerSample / 8);
    format.nAvgBytesPerSec = format.nSamplesPerSec * format.nBlockAlign;
    format.cbSize = 0;

    if (waveOutOpen(&g_device, WAVE_MAPPER, &format, 0, 0, CALLBACK_NULL) != MMSYSERR_NOERROR) {
        g_device = NULL;
        return;
    }

    for (int i = 0; i < AUDIO_VOICES; ++i) {
        g_buffers[i] = (short *)calloc(AUDIO_SAMPLES, sizeof(short));
        if (!g_buffers[i]) {
            audio_shutdown();
            return;
        }
        memset(&g_headers[i], 0, sizeof(g_headers[i]));
        g_headers[i].lpData = (LPSTR)g_buffers[i];
        g_headers[i].dwBufferLength = (DWORD)(AUDIO_SAMPLES * sizeof(short));
        if (waveOutPrepareHeader(g_device, &g_headers[i], sizeof(WAVEHDR)) !=
            MMSYSERR_NOERROR) {
            audio_shutdown();
            return;
        }
    }
    g_next_voice = 0;
    g_ready = 1;
}

void audio_shutdown(void) {
    if (g_device) {
        waveOutReset(g_device);
        for (int i = 0; i < AUDIO_VOICES; ++i) {
            if (g_headers[i].lpData) {
                waveOutUnprepareHeader(g_device, &g_headers[i], sizeof(WAVEHDR));
            }
        }
        waveOutClose(g_device);
        g_device = NULL;
    }
    for (int i = 0; i < AUDIO_VOICES; ++i) {
        free(g_buffers[i]);
        g_buffers[i] = NULL;
        memset(&g_headers[i], 0, sizeof(g_headers[i]));
    }
    g_ready = 0;
}

void audio_play(BgSound sound) {
    if (!g_ready || !g_device || sound < 0 || sound >= BG_SOUND_COUNT) {
        return;
    }
    const int voice = g_next_voice;
    g_next_voice = (g_next_voice + 1) % AUDIO_VOICES;
    if ((g_headers[voice].dwFlags & WHDR_INQUEUE) &&
        !(g_headers[voice].dwFlags & WHDR_DONE)) {
        return; /* Still sounding; drop this one rather than stutter. */
    }
    WAVEHDR *header = &g_headers[voice];
    waveOutUnprepareHeader(g_device, header, sizeof(WAVEHDR));
    synthesise(sound, g_buffers[voice], AUDIO_SAMPLES);
    header->dwBufferLength = (DWORD)(AUDIO_SAMPLES * sizeof(short));
    header->dwFlags = 0;
    if (waveOutPrepareHeader(g_device, header, sizeof(WAVEHDR)) == MMSYSERR_NOERROR) {
        waveOutWrite(g_device, header, sizeof(WAVEHDR));
    }
}

#else

void audio_init(void) {
}

void audio_shutdown(void) {
}

void audio_play(BgSound sound) {
    (void)sound;
}

#endif
