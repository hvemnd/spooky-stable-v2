/* SPDX-License-Identifier: MPL-2.0
 * Spooky Stable v2: motion and impact (design 2.1 - 2.3, 2.9, 11): the easing library, the trauma screen shake,
 * hitstop, the impact silhouette, the one shared full-screen flash limiter, shockwave rings and the toy transitions. */
#pragma once

/* easing: t in 0..1 */
static float ease_out_quad(float t);
static float ease_out_cubic(float t);
static float ease_out_back(float t);        /* overshoot 1.7 */
static float ease_out_elastic(float t);     /* amp 0.4, period 0.3 */
static float ease_in_quad(float t);
static float ease_in_out_sine(float t);
static float ease_out_bounce(float t);

static void fx_trauma(float amount);        /* every toy: never a fixed amplitude */
static void fx_trauma_v(float amount);      /* the slow vertical rumble (a burp: inside the belly) */
static void fx_hitstop(int frames);         /* freezes every update for that many frames; draw goes on */
static int fx_frozen(void);
static void fx_impact(int frames);          /* the character drawn as a white silhouette (rate-limited) */
static int fx_silhouette(void);
static int fx_flash_ok(void);               /* the full-screen limiter: one in 400 ms (photosensitivity) */
static void fx_flash(uint16_t c);           /* a one-frame wash */
static void fx_invert(void);                /* a one-frame invert */
static void fx_tear(int frames, float amount);   /* rows torn sideways (glitch, a sneeze's screen rip) */
static void ring_spawn(float x, float y, float r, float vr, float sq, float life_ms, float th, uint16_t c);

enum { TR_IRIS, TR_PORTAL, TR_TEAR, TR_SPIN, TR_DISSOLVE, TR_GLITCH, NTRANS };
static void fx_transition(int kind, float cx, float cy, uint16_t c);
static int fx_in_transition(void);

static void fx_update(float dt_ms);         /* game time (skipped in hitstop) */
static void fx_frame(void);                 /* every frame, real time: the shake offset */
static void fx_draw_world(void);            /* rings, in the world (shaken) */
static void fx_draw_post(void);             /* flash, invert, tear, transition: over everything */
static void fx_clear(void);
