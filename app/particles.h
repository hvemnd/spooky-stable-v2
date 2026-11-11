/* SPDX-License-Identifier: MPL-2.0
 * Spooky Stable v2: particles v2 (design 2.7): one fixed pool of 200, 20 bytes each, no allocation; positions in
 * 1/16 px, stepped at a fixed 30 Hz tick. A burst with colour 0 takes the mood's colours, and part_mood_burst the
 * mood's own type, so every toy re-skins itself when K5 turns. */
#pragma once

enum { PT_STAR, PT_SPARKLE, PT_SLIME, PT_BUBBLE, PT_FLY, PT_HEART, PT_BAT, PT_PETAL, PT_CONFETTI, PT_PIXEL, PT_CLOUD,
       PT_DROPLET, PT_NOTE, PT_CRESCENT, PT_ORB, PT_SNOT, PT_DUCK, PT_POOPLET, PT_DUST, NPT };

typedef struct {
    int16_t x, y, vx, vy;             /* 1/16 px; 1/16 px a tick (FLY: vx, vy hold its orbit's centre) */
    uint8_t life, max_life, type, size;
    uint16_t color;
    uint8_t floor;                    /* the floor's y (px), 0 none */
    int8_t g;                         /* gravity, 1/16 px a tick per tick */
    uint8_t aux, flags;               /* per type: an orbit's angle, a stain's frames ... */
} particle_t;                         /* 20 bytes */
#define NPARTS 200
#define PF_ADD 1u                     /* drawn with a little glow */
#define PF_STAIN 2u                   /* landed: a 2-frame stain */

static float part_mult = 1.0f;        /* the chaos's particle multiplier (ui.c) */
static void (*part_on_floor)(particle_t *p);   /* the toy's: a drop or a poop landed */

static particle_t *part_spawn(int type, float x, float y, float vx, float vy, int life, uint16_t c, int size);
static void part_burst(int type, float x, float y, int n, float v0, float v1, int life, uint16_t c, int size);
static void part_fan(int type, float x, float y, int n, float ang, float spread, float v0, float v1, int life,
                     uint16_t c, int size);
static void part_mood_burst(float x, float y, int n, float v0, float v1);
static int part_count(void);
static void part_clear(void);
static void part_suck(float x, float y);   /* every particle swirls into a point (the toilet) */
static void particles_update(float dt);
static void particles_draw(void);
