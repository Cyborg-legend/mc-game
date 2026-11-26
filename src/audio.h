#ifndef BLOCKFORGE_C_AUDIO_H
#define BLOCKFORGE_C_AUDIO_H

/* Every sound is synthesised at runtime, like the textures: no asset files. */
typedef enum {
    BG_SOUND_PLACE = 0,
    BG_SOUND_BREAK,
    BG_SOUND_HIT,
    BG_SOUND_HURT,
    BG_SOUND_CRAFT,
    BG_SOUND_RIFT,
    BG_SOUND_PICKUP,
    BG_SOUND_CLICK,
    BG_SOUND_BOSS,
    BG_SOUND_WIN,
    BG_SOUND_COUNT
} BgSound;

void audio_init(void);
void audio_shutdown(void);
void audio_play(BgSound sound);

#endif /* BLOCKFORGE_C_AUDIO_H */
