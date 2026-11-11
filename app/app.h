/* SPDX-License-Identifier: MPL-2.0
 * Spooky Stable v2: a no-menu, no-text toy firmware for a 3-year-old, on fm1-base (MPL-2.0).
 * This header is the app's half of the contract with the platform (device/unity.c, host/plat_host.c). */
#pragma once
#include <stdint.h>
#include "plat.h"

#define APP_NAME "SPOOKY STABLE"
#define APP_SPLASH_RGB RGB(168, 85, 247)
#define APP_BLOCK 256u                   /* frames per audio block: 5.8 ms at 44.1 kHz */
#define APP_TOUCH                        /* velocity from the keys (lib/touch.h) */
#define APP_CANVAS_PX (240u * 240u)      /* one full-screen canvas in the RAM pool: the whole frame drawn, then sent */
#define APP_DIM_LEVELS                   /* plat_dim_ns: the key LEDs' dim layer breathes at idle (ui.c) */
#define APP_OVERLOAD                     /* app_overload: the render ran late, the synth sheds voices (voice.c) */

/* the store: the app's settings (volume, mood, presets, every toy's knobs, wardrobe) and the panel calibration */
enum { OBJ_SETTINGS, OBJ_PANEL, OBJ_NOBJ };
#define APP_STORE_MAP {0xFC000u, 0xDC000u}

/* the contract: the platform calls these */
void app_init(void);
void app_render(int32_t *lr, uint32_t n, uint32_t master_q12);
void app_panic(void);
void app_ui_init(void);
void app_frame(void);
void app_input_only(void);
void app_say(const char *a, const char *b);
void app_overload(void);

/* the host simulator's checks (app.c app_expect: toy, mood, voices, particles, poops, pee, boogers, loop, ...) */
#define APP_HOST_EXPECT
int app_expect(const char *what, int *got);
