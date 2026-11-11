/* SPDX-License-Identifier: MPL-2.0
 * Spooky Stable v2: the eight moods (design 2.5, 3.1). A mood is a whole world: the scale every note snaps to, the
 * sky's gradient, the accent and character colours, the particles, the unicorn's accessory and the prop layers.
 * K5 picks one; every toy reads mood->... rather than its own colours. */
#pragma once

enum { MOOD_SUNNY, MOOD_MOON, MOOD_SWAMP, MOOD_DREAMY, MOOD_CASTLE, MOOD_SPOOKY, MOOD_SAKURA, MOOD_GLITCH, NMOODS };

/* the props a mood's world is made of */
enum { PR_CLOUD, PR_DAISY, PR_STAR, PR_MOON, PR_CATTAIL, PR_MUDBUB, PR_BUBBLE, PR_TOWER, PR_BUNTING, PR_TREE,
       PR_PUMPKIN, PR_BLOSSOM, PR_PIXEL, PR_SUN, NPROPKINDS };

typedef struct {
    const char *name;
    const int8_t *scale;
    uint8_t nscale;
    uint8_t root;                     /* pitch class of degree 0 (0 = C) */
    uint32_t top, bot;                /* the sky's gradient, 0xRRGGBB */
    uint16_t accent, chr;             /* the accent; the character's (unicorn's) body colour */
    uint16_t ground, ground2;         /* the floor every toy stands on (derived from the gradient: design 2.5's table
                                       * has no ground column, so these are the bottom colour, deeper) */
    uint16_t prop1, prop2;            /* the props' two colours (likewise derived) */
    uint8_t particle;                 /* PT_*: what bursts by default */
    uint8_t accessory;                /* ACC_*: the unicorn's hat in this world */
} mood_t;

static const mood_t *mood;            /* the current mood */
static uint8_t mood_i;

static void mood_init(void);
static void mood_set(int i, int fade);              /* fade: crossfade the sky over 300 ms */
static int mood_degree_midi(int octave, int deg);   /* scale degree -> MIDI note in that octave (C of octave n = 12n) */
static int mood_key_degree(uint32_t k);             /* key 0..26 -> degree 0 .. 2 octaves */
static int mood_key_midi(uint32_t k, int octave);
static float quantize_to_scale(float midi);         /* any pitch -> the nearest note of the scale */
static uint16_t mood_mane(int i);                   /* the mane's six colours: the rainbow leaning to the accent */

/* the sky */
static void mood_bg_tint(uint16_t c, float amt);    /* a wash over the sky (the pee's blue-green), 0..1 */
static void mood_draw_sky(void);
static void mood_draw_ground(int32_t y0);           /* a floor band from y0 down, the mood's */

/* the prop layers: drift in parallax, fly when blasted, swirl into the toilet, come home again */
static void mood_props_update(float dt);
static void mood_props_draw(float T, int front);    /* front 0: far and mid layers; 1: the near layer */
static void mood_props_blast(float x, float y, float dx, float dy, float cone, float force);
static void mood_props_suck(float x, float y);
static void mood_props_home(void);
static int mood_props_away(void);                   /* how many are flying or gone */
