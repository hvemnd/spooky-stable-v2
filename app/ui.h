/* SPDX-License-Identifier: MPL-2.0
 * Spooky Stable v2: what goes round every toy (design 2.10, 3.7, 7): the key and button lights, the chaos meter,
 * the charge, mash detection, idling (wave, sleep and snore, dream), the overlays (preset icon, quiet moon, a new
 * outfit, the startle "!"), the 10-second replay and the backup dancers.
 * The FM-1's key LEDs are one colour (off, dim, a middle level, lit), so the design's colours become brightness and
 * motion: a press lights its key and fades through the middle level; a kick lights them all, a snare every other one,
 * a hat runs one light along; the idle keybed breathes. */
#pragma once

/* the lights */
static void ui_led_key(uint32_t k);
static void ui_led_kick(void);
static void ui_led_snare(void);
static void ui_led_hat(void);
static void ui_led_flash_all(uint32_t ms);
static void ui_led_pattern(int every, uint32_t ms);   /* every Nth key lit: a preset's signature */
static void ui_leds(void);

/* the chaos meter */
static float chaos;
static void ui_chaos_press(void);
static void ui_draw_chaos(void);

/* the charge: any held key in a toy without TF_HOLD */
static void ui_charge_press(uint32_t k);
static int ui_charge_release(uint32_t k);             /* 1: it was full (the toy's mega) */
static float ui_charge(void);                         /* 0..1 */
static void ui_charge_cancel(void);

/* mashing: 5 presses in 200 ms */
static int ui_mash_press(void);

/* idling */
enum { IDLE_AWAKE, IDLE_WAVE, IDLE_SLEEP, IDLE_DREAM };
static int ui_idle_state(void);
static int ui_input(void);                            /* real input: wakes her; 1 if she was asleep (a startle) */
static void ui_idle_pose(pose_t *p);
static void ui_draw_idle(const pose_t *p);           /* dream bubbles, the startle "!" */

/* the overlays */
static void ui_preset_flash(int icon, int index, int count);
static void ui_quiet_flash(void);
static void ui_unlock_flash(int hat);
static void ui_draw_overlays(void);
static void draw_preset_icon(int icon, float x, float y, float s);

/* the replay (REC outside Drum Jam): the last 10 s of her keys, played back */
static void ui_rec_event(uint32_t k, float vel, int down);
static void ui_replay_toggle(void);
static int ui_replaying(void);
static void ui_replay_tick(void);

/* the backup dancers */
static void ui_dancers(uint32_t ms);
static void ui_draw_dancers(float T);

static void ui_update(float dt);
static void ui_clear(void);

/* app.c's, called from here */
static void app_key_down(uint32_t k, float vel, int synthetic);
static void app_key_up(uint32_t k, int synthetic);
static void app_flourish(int big);
