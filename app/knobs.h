/* SPDX-License-Identifier: MPL-2.0
 * Spooky Stable v2: the knobs (design 4) and what is remembered across power cycles. All seven are live in every toy:
 * K1 volume and K5 mood everywhere, the other five the toy's own (each toy remembers its own). A turn shows an icon
 * and its level for 800 ms; smooth knobs ease so nothing jitters.
 * Panel: K1..K4 are the FM-1's K1..K4; K5 its SELECT knob, K6 ALGO, K7 PRESET (the encoders have no push switch). */
#pragma once

#define SET_MAGIC 0x53535432u              /* "SST2" */
typedef struct {
    uint32_t magic;
    uint8_t vol;                           /* 0..10 (K1) */
    uint8_t quiet;
    uint8_t mood;                          /* K5 */
    uint8_t hat, mane;                     /* the wardrobe: 0 = the mood's own */
    uint8_t unlocked;                      /* wardrobe items unlocked */
    uint8_t rsv[2];
    uint8_t preset[NTOYS];
    uint8_t knob[NTOYS][NTKNOBS];
    uint32_t actions;                      /* every key press ever (the wardrobe's progression) */
} settings_t;
static settings_t set;

static void knobs_load(void);
static void knobs_save_soon(void);
static void knobs_tick(void);              /* saves when due */
static void knobs_poll(void);              /* the encoders */
static void knobs_enter_toy(void);         /* the eased values jump to the new toy's */
static int kv(int k);                      /* a knob's value (K2..K7 the current toy's) */
static float kf(int k);                    /* 0..1, eased */
static void knobs_update(float dt);
static void knobs_draw(void);
static void knobs_show(int icon, int steps, int value, uint32_t ms);
static void draw_kicon(int icon, float x, float y, float s, uint16_t c);
static int knob_slot(int k);               /* K2..K7 -> 0..4 (K5: -1) */
