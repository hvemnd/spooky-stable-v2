/* SPDX-License-Identifier: MPL-2.0
 * Spooky Stable v2: shared helpers (random numbers, small maths, RGB565 colour) used by every module.
 * Header-only: part of app.c's unity build, after the platform (gfx.c's RGB and swap16 are in scope). */
#pragma once
#include "fastmath.h"

#define TAU 6.2831853f
#define AFS 44100.0f

/* ------------------------------------------------------------------ random --- */
static uint32_t ss_seed = 0x2545F491u;
static uint32_t ss_rnd(void)
{
    uint32_t x = ss_seed;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    return ss_seed = x;
}
static float ss_rndf(void) { return (float)(ss_rnd() >> 8) * (1.0f / 16777216.0f); }
static float ss_rnds(void) { return ss_rndf() * 2.0f - 1.0f; }            /* -1 .. 1 */
static int32_t ss_rndi(int32_t n) { return n > 0 ? (int32_t)(ss_rnd() % (uint32_t)n) : 0; }
static uint32_t ss_hash(uint32_t x)
{
    x ^= x >> 16;
    x *= 0x7FEB352Du;
    x ^= x >> 15;
    x *= 0x846CA68Bu;
    x ^= x >> 16;
    return x;
}
static float ss_noise(uint32_t f) { return (float)(ss_hash(f) & 0xFFFFu) * (2.0f / 65535.0f) - 1.0f; }   /* -1..1 */

/* ------------------------------------------------------------------- maths --- */
static float ss_lerp(float a, float b, float t) { return a + (b - a) * t; }
static float ss_clamp01(float t) { return t < 0.0f ? 0.0f : t > 1.0f ? 1.0f : t; }
static float ss_absf(float x) { return x < 0.0f ? -x : x; }
static float ss_ramp(float a, float t0, float t1) { return ss_clamp01((a - t0) / (t1 - t0)); }   /* 0 at t0, 1 at t1 */
static float ss_pulse(float a, float t0, float t1)               /* 0 -> 1 -> 0 between t0 and t1 (a sine hump) */
{
    if (a <= t0 || a >= t1)
        return 0.0f;
    return fm_sinf(3.14159265f * (a - t0) / (t1 - t0));
}
static int32_t ss_mini(int32_t a, int32_t b) { return a < b ? a : b; }
static int32_t ss_maxi(int32_t a, int32_t b) { return a > b ? a : b; }

/* ----------------------------------------------------------------- colours --- */
/* hex 0xRRGGBB -> RGB565 at compile time (the design's colours are written as in the document) */
#define HEX(h) RGB(((h) >> 16) & 0xFFu, ((h) >> 8) & 0xFFu, (h) & 0xFFu)

/* the universal constants allowed anywhere (design 2.5) */
#define U_WHITE HEX(0xFFFFFF)
#define U_BLACK HEX(0x000000)
#define U_LINE HEX(0x3B2F2F)          /* the soft brown outline every character gets */
#define U_BONE HEX(0xFDE68A)
/* The content colours the three v1 toys are made of (their visual identity, which v2 keeps): the rainbow, the poop's
 * browns, the pee's yellow, the horn's golds. Not mood colours: they are what the thing *is*, so they stay the same in
 * every world; everything around them is the mood's. */
#define CC_PINK HEX(0xFF6B9D)
#define CC_ORANGE HEX(0xF97316)
#define CC_YELLOW HEX(0xFFE63C)
#define CC_GREEN HEX(0x22C55E)
#define CC_BLUE HEX(0x38BDF8)
#define CC_PURPLE HEX(0xA855F7)
#define CC_BROWN HEX(0x96562A)
#define CC_BROWN_D HEX(0x50281A)
#define CC_GOLD HEX(0xFFC828)
#define CC_GOLD_L HEX(0xFFE89A)
#define CC_GOLD_D HEX(0xE09A10)
#define CC_PEE HEX(0xFFE14D)
#define CC_SNOT HEX(0x9BE15D)
#define CC_BLUSH HEX(0xFF8FB8)
#define CC_INK HEX(0x16102C)
static const uint16_t RAINBOW[6] = {CC_PINK, CC_ORANGE, CC_YELLOW, CC_GREEN, CC_BLUE, CC_PURPLE};

static inline uint16_t mix565(uint32_t a, uint32_t b, uint32_t t)   /* t 0..256: a -> b */
{
    uint32_t x = (a | a << 16) & 0x07E0F81Fu, y = (b | b << 16) & 0x07E0F81Fu, a5 = t >> 3;
    if (t >= 256u)
        return (uint16_t)b;
    x = (x + (((y - x) * a5) >> 5)) & 0x07E0F81Fu;
    return (uint16_t)(x | x >> 16);
}
static inline uint16_t add565(uint32_t a, uint32_t b)               /* additive, each channel saturating */
{
    uint32_t r = (a >> 11) + (b >> 11), g = ((a >> 5) & 63u) + ((b >> 5) & 63u), bl = (a & 31u) + (b & 31u);
    if (r > 31u)
        r = 31u;
    if (g > 63u)
        g = 63u;
    if (bl > 31u)
        bl = 31u;
    return (uint16_t)(r << 11 | g << 5 | bl);
}
static inline uint16_t scale565(uint32_t c, uint32_t t)            /* c * t / 256 */
{
    return (uint16_t)((((c >> 11) * t >> 8) << 11) | ((((c >> 5) & 63u) * t >> 8) << 5) | ((c & 31u) * t >> 8));
}
static uint16_t hue565(int32_t h)                                   /* 0..1535 round the wheel, fully saturated */
{
    uint32_t s, f, r = 0, g = 0, b = 0;
    h %= 1536;
    if (h < 0)
        h += 1536;
    s = (uint32_t)h >> 8;
    f = (uint32_t)h & 255u;
    switch (s) {
    case 0: r = 255; g = f; break;
    case 1: r = 255 - f; g = 255; break;
    case 2: g = 255; b = f; break;
    case 3: g = 255 - f; b = 255; break;
    case 4: r = f; b = 255; break;
    default: r = 255; b = 255 - f; break;
    }
    return RGB(r, g, b);
}
static uint16_t lighten(uint16_t c, uint32_t t) { return mix565(c, U_WHITE, t); }
static uint16_t darken(uint16_t c, uint32_t t) { return mix565(c, U_BLACK, t); }
