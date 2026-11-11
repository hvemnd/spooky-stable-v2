/* SPDX-License-Identifier: MPL-2.0
 * Spooky Stable v2: drawing primitives on the 240x240 canvas (cv_px, byte-swapped RGB565).
 * World shapes (ell, disc, tri, box, cap ...) are moved by the screen shake (dr_ox, dr_oy); hl / hl_a are raw rows.
 * Everything is scanline fills: no per-pixel maths over the whole screen. */
#pragma once

static int32_t dr_ox, dr_oy;          /* the screen shake's offset for world shapes (fx.c sets it each frame) */
static uint16_t dr_solid;             /* != 0: every shape drawn in this one colour (an impact silhouette) */
static uint8_t dr_solid_on;
static uint32_t dr_glows;             /* glow sprites drawn this frame (at most DR_GLOW_MAX) */
#define DR_GLOW_MAX 8u

static void hl(int32_t x0, int32_t x1, int32_t y, uint16_t c);
static void hl_a(int32_t x0, int32_t x1, int32_t y, uint16_t c, uint32_t a);
static void ell_a(float x, float y, float rx, float ry, uint16_t c, uint32_t a);
static void ell(float x, float y, float rx, float ry, uint16_t c);
static void disc(float x, float y, float r, uint16_t c);
static void disc_a(float x, float y, float r, uint16_t c, uint32_t a);
static void ring_a(float x, float y, float rx, float ry, float th, uint16_t c, uint32_t a);
static void tri_a(float x0, float y0, float x1, float y1, float x2, float y2, uint16_t c, uint32_t a);
static void tri(float x0, float y0, float x1, float y1, float x2, float y2, uint16_t c);
static void quad(float x0, float y0, float x1, float y1, float x2, float y2, float x3, float y3, uint16_t c);
static void box_a(float x, float y, float w, float h, uint16_t c, uint32_t a);
static void box(float x, float y, float w, float h, uint16_t c);
static void cap(float x0, float y0, float x1, float y1, float r, uint16_t c);
static void arc_dots(float x, float y, float r, float a0, float a1, float th, uint16_t c);
static void star4(float x, float y, float r, uint16_t c);
static void heart(float x, float y, float s, uint16_t c);
static void glow(float x, float y, float r, uint16_t tint, uint32_t amt);
static void draw_init(void);
static void dr_solid_begin(uint16_t c);
static void dr_solid_end(void);
