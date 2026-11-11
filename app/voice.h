/* SPDX-License-Identifier: MPL-2.0
 * Spooky Stable v2: the synth (design 3.2, 3.3). 12 voices, each 2-operator FM -> pitch envelope -> noise layer ->
 * amp envelope -> state-variable filter -> FX sends; oldest-first stealing with a 5 ms fade; a shared delay, chorus
 * and bitcrush; the kick ducks the melodic bus; a soft-knee limiter after the volume, the whole capped at 70 %.
 * The main loop asks for notes through a queue; the render (the audio interrupt) owns the voices. Drum Jam's
 * sequencer runs in the render too, so its 16th-note grid is sample-accurate. */
#pragma once

#define NVOICE 12
#define VOL_CAP 0.70f                 /* safety: never more than 70 % of full scale */
#define QUIET_CAP 0.20f               /* quiet mode */

#define NF_HOLD 1u                    /* held until snd_off (with any patch) */
#define NF_ONESHOT 2u                 /* never held, even with a PA_HOLD patch (an audition, snd_play) */
#define NF_RELEASED 4u                /* (its note-off came before it started: a stolen voice still fading) */

typedef struct {
    uint8_t patch, flags;
    uint16_t handle;
    float midi;                       /* pitched: the note; a drum: semitones of transposition */
    float vel;                        /* 0..1 */
    float tscale;                     /* every envelope time but the attack, times this */
    float bend;                       /* the pitch envelope's depth, times this */
    float crush;                      /* 0..1 more bitcrush */
    float send;                       /* 0..1 more delay */
    float vib;                        /* more vibrato depth, semitones */
    float pulse;                      /* an amplitude pulse, Hz (the pee's spurts); 0 none */
} note_t;

static uint16_t snd_play(int patch, float midi, float vel);   /* the usual: a one-shot at default times */
static uint16_t snd_note(const note_t *n);
static note_t snd_defaults(int patch, float midi, float vel);
static void snd_off(uint16_t handle);
static void snd_glide(uint16_t handle, float midi);
static void snd_mod(uint16_t handle, float vib, float pulse);
static void snd_all_off(void);
static void snd_volume(float v);       /* 0..1 of the cap (K1) */
static void snd_quiet(int on);         /* quiet mode: the cap down to 20 % */
static void snd_chaos(float crush);    /* the chaos knob's bitcrush on the melodic bus, 0..1 */
static int snd_voices(void);

/* Drum Jam's sequencer (in the render) */
enum { LOOP_NONE, LOOP_REC, LOOP_PLAY };
enum { HIT_LIVE, HIT_LOOP, HIT_AUTO };
static void seq_enable(int on);
static void seq_config(float bpm, float swing, float tune, int density, int autoplay, int kit, float crush);
static void seq_hit(uint32_t key, float vel);
static void seq_rec(void);              /* REC: record four bars (again) */
static void seq_clear(void);
static int seq_pop(uint8_t *key, uint8_t *src);   /* the hits it played, for the screen */
static float seq_beat(void);            /* quarter notes since it started (fractional) */
static int seq_loop_state(void);
static uint32_t seq_loop_mask(int step);
static int seq_loop_pos(void);           /* the step within the four-bar loop, 0..63 */
static void seq_set_pitch(uint32_t key, float midi);   /* the silly keys' notes (the mood's scale) */
