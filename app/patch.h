/* SPDX-License-Identifier: MPL-2.0
 * Spooky Stable v2: the sound patches (design 3.4 - 3.7). One patch_t per sound, in the flash; a toy's presets are a
 * list of them, cycled with OCT- / OCT+. Times in ms, levels 0..100, pitch offsets in tenths of a semitone. */
#pragma once

/* the pitch envelope's shapes */
enum { SH_LIN, SH_INQ, SH_EXP, SH_ELASTIC };
/* the filters (main and the noise layer's own) */
enum { FT_NONE, FT_LP, FT_BP, FT_HP };
/* flags */
#define PA_HOLD 0x01u          /* sustains until note-off (a held flute, an inhale) */
#define PA_GLIDE 0x02u         /* portamento: snd_glide moves it to a new note over 60 ms */
#define PA_CLAP 0x04u          /* the noise retriggers at 0 / 12 / 24 ms */
#define PA_DUAL 0x08u          /* the two operators summed, not modulating (a cowbell's two tones) */
#define PA_GURGLE 0x10u        /* the filter swept 300 -> 1200 -> 200 Hz over the decay */
#define PA_KICK 0x20u          /* ducks the melodic bus (-4 dB, 120 ms) */
#define PA_DRUM 0x40u          /* on the drum bus (never ducked) */
#define PA_LINEAR 0x80u        /* linear decay (else exponential-ish) */

typedef struct {
    const char *name;
    uint8_t ratio_num, ratio_den;     /* the modulator at num/den times the carrier (design's "1:2" = 2/1) */
    uint8_t mod_level, feedback;      /* modulation index (100 = 4 rad) and the modulator's feedback */
    uint16_t a_ms, d_ms;              /* amp envelope */
    uint8_t s_level;
    uint16_t r_ms;
    int16_t pe0, pe1;                 /* pitch envelope (1/10 st): from pe0 to pe1 over pt1, then to pe2 over pt2 */
    uint16_t pt1;
    int16_t pe2;
    uint16_t pt2;                     /* 0: one stage (holds pe1) */
    uint8_t pshape;
    uint8_t vib_rate, vib_depth;      /* Hz; 1/10 st */
    uint16_t vib_delay_ms;
    uint8_t noise_amt;
    uint16_t noise_a_ms, noise_d_ms;  /* the noise's own envelope; d 0: it follows the amp envelope */
    uint8_t noise_ft;
    uint16_t noise_hz;
    uint8_t ft;                       /* the main filter */
    uint16_t cutoff_hz;
    uint8_t res;
    int8_t fenv_amt;                  /* 1/10 octave at the start, decaying over fenv_ms (negative: a rising sweep) */
    uint16_t fenv_ms;
    uint8_t delay_send, chorus_send, crush;
    uint16_t base_hz;                 /* a drum's fixed pitch (bypasses the scale); 0 = pitched */
    uint8_t level;                    /* 0 means 100 */
    uint8_t tone;                     /* the carrier's level, 0 means 100; 255 means none (noise only) */
    uint16_t tone_d_ms;               /* the tone's own decay (a snare's body), 0 none */
    uint8_t trem_rate, trem_depth;    /* amplitude flutter: what makes a fart blat; Hz, 0..100 */
    uint8_t drive;                    /* a soft waveshaper */
    uint16_t gate_ms;                 /* a one-shot with sustain: held this long, then released */
    uint8_t flags;
} patch_t;

enum {
    P_FART_TUBA, P_FART_KAZOO, P_FART_DIDGE, P_FART_SLIDE, P_FART_BASSOON, P_SQUEAK,
    P_PLOP_MARIMBA, P_PLOP_WOOD, P_PLOP_DROP, P_PLOP_PIZZ, P_PLOP_BOING,
    P_PEE_FLUTE, P_PEE_OCARINA, P_PEE_WHISTLE, P_PEE_PANPIPE, P_PEE_HUM,
    P_BURP_TROMBONE, P_BURP_FROG, P_BURP_TUBA, P_BURP_BABY, P_BURP_ROBOT, P_GULP,
    P_SNEEZE_TRUMPET, P_SNEEZE_HONK, P_SNEEZE_TINY, P_SNEEZE_GIANT, P_INHALE,
    P_KICK, P_SNARE, P_CHAT, P_OHAT, P_CLAP, P_LTOM, P_MTOM, P_HTOM, P_CRASH, P_SHAKER, P_COWBELL,
    P_HONK, P_SLIDEWH, P_OW,
    P_GURGLE, P_GURGLE_BIG, P_GURGLE_BUBBLY, P_GURGLE_WHOOSH, P_BLOOP,
    P_FLICK_TWANG, P_FLICK_ZIP, P_FLICK_SPROING, P_SLURP, P_SPLAT,
    P_WHOOSH, P_TICK, P_CHIME, P_WHINE, P_SNORE, P_POP, P_SWOOSH, P_STAB, P_GIGGLE,
    NPATCH
};
static const patch_t PATCHES[NPATCH];

/* the preset icons (ui.c draws them) */
enum { IC_HORN, IC_KAZOO, IC_PIPE, IC_SLIDE, IC_BELLS, IC_BLOCK, IC_DROP, IC_STRING, IC_SPRING, IC_FROG, IC_ROBOT,
       IC_DRUM, IC_BIGDRUM, IC_TINYDRUM, IC_POTS, IC_MOUTH, IC_WAVE, IC_BUBBLES, IC_TINY, IC_GIANT, NICONS };

typedef struct {
    uint8_t patch;                    /* the toy's main sound (Drum Jam: the kit) */
    uint8_t icon;
} preset_t;

/* Drum Jam's kits: deltas on the one drum set (design 3.6: "derive variants by delta") */
typedef struct {
    int8_t tune;                      /* semitones */
    uint8_t decay;                    /* x / 10 */
    uint8_t crush, delay, chorus;     /* 0..100, added */
} kit_t;
#define NKITS 5
static const kit_t KITS[NKITS];
