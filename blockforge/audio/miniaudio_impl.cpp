// The single translation unit that compiles miniaudio's implementation.
//
// audio.h only declares the miniaudio API, so without one file defining
// MINIAUDIO_IMPLEMENTATION the ma_* symbols never reach the linker and the
// executable fails with LNK2019 (ma_engine_init, ma_sound_start, ...).

#define MINIAUDIO_IMPLEMENTATION
#include <miniaudio.h>
