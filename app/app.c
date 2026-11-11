/* SPDX-License-Identifier: MPL-2.0
 * Spooky Stable v2: eight silly toys for a 3-year-old on the M-VAVE FM-1. No menus, no text: a button picks a toy,
 * any key makes it act, every knob does something, and nothing she plays can sound wrong (every note snaps to the
 * mood's scale).
 *
 *   HOME  Unicorn Fart    EDIT  Unicorn Poop    LFO  Unicorn Pee     FX   Unicorn Burp
 *   ENV   Unicorn Sneeze  GLO   Drum Jam        ARP  Toilet Flush    SEQ  Booger Flick
 *   PLAY  auto-play       REC   Drum Jam's loop recorder; elsewhere the last 10 s played back
 *   OCT- / OCT+  the toy's sound presets (hold OCT+: back to the first)
 *   SEL   the next unlocked hat (hold: the next mane)      SAVE  a surprise (hold 1 s: quiet mode, 20 %)
 *   K1 volume (70 % cap)   K5 (SELECT) the mood: scale and colours together   K2 K3 K4 K6 (ALGO) K7 (PRESET) the toy's
 *   Hold the toy's own button: sweep the screen clean (in the Toilet: flush everything).
 *
 * Part of fm1-base's unity build (device/unity.c, host/plat_host.c), after the platform: gfx.c (cv_px, cv_begin,
 * cv_blit, RGB, swap16), announce.c and lib/touch.h are in scope. This file is the app's own unity build: the modules
 * below are included in order. Everything is drawn procedurally into one 240x240 canvas and sent once a frame. */
#include "util.h"
#include "draw.h"
#include "fx.h"
#include "mood.h"
#include "particles.h"
#include "patch.h"
#include "voice.h"
#include "unicorn.h"
#include "toys.h"
#include "knobs.h"
#include "ui.h"

#include "draw.c"
#include "fx.c"
#include "mood.c"
#include "particles.c"
#include "patch.c"
#include "voice.c"
#include "unicorn.c"
#include "knobs.c"
#include "ui.c"

#include "toys/common.c"
#include "toys/fart.c"
#include "toys/poop.c"
#include "toys/pee.c"
#include "toys/burp.c"
#include "toys/sneeze.c"
#include "toys/drumjam.c"
#include "toys/toilet.c"
#include "toys/booger.c"

/* the registry: one line a toy */
static const toy_def_t *TOYREG[NTOYS] = {&TOY_FART_DEF, &TOY_POOP_DEF, &TOY_PEE_DEF, &TOY_BURP_DEF, &TOY_SNEEZE_DEF,
                                         &TOY_DRUM_DEF, &TOY_TOILET_DEF, &TOY_BOOGER_DEF};

/* ============================================================ app state === */
#define LONG_MS 700u                           /* a long press */
static struct {
    uint32_t last_ms;
    uint32_t keys, btn;
    uint32_t key_t0[NKEYS];
    uint32_t btn_t0[NB];
    uint32_t btn_long;                         /* buttons whose long press has fired */
    uint8_t trans_n;
    uint32_t tick_at;                          /* the transition's tick at its end */
    /* auto-play */
    uint8_t autoplay;
    uint32_t auto_next, auto_release;
    int8_t auto_key;
    int8_t auto_deg;
    uint8_t spoke;
    uint32_t megas, mood_changes, cleans;      /* (the tests) */
} G;

static void snd_apply_volume(void)
{
    snd_volume((float)set.vol / 10.0f);
    snd_quiet(set.quiet);
}

/* ======================================================= mood: the magic trick === */
static void seq_pitches(void)
{
    uint32_t k;
    for (k = 0; k < NKEYS; k++)
        seq_set_pitch(k, (float)mood_key_midi(k > 17u ? k - 18u : k, 5));
}

static void on_mood_landed(int m)
{
    set.mood = (uint8_t)m;
    mood_set(m, 1);
    seq_pitches();
    part_mood_burst(120.0f, 110.0f, 30, 2.0f, 6.0f);
    part_burst(PT_SPARKLE, 120.0f, 110.0f, 16, 1.0f, 4.0f, 24, mood->accent, 2);
    ring_spawn(120.0f, 110.0f, 10.0f, 7.0f, 1.0f, 420.0f, 6.0f, mood->accent);
    chord(P_CHIME, 5, 0, 0.7f);                /* a three-note sting in the new scale */
    ui_led_flash_all(150u);
    fx_trauma(0.15f);
    G.mood_changes++;
    knobs_save_soon();
}

/* ================================================================ toys === */
static void switch_toy(int t)
{
    static const float CORNER[NTOYS][2] = {{0, 0}, {240, 0}, {0, 240}, {240, 240}, {120, 0}, {120, 240}, {0, 120}, {240, 120}};
    int kind;
    ui_charge_cancel();
    if (TOY && TOY->leave)
        TOY->leave();
    if (ui_replaying())
        ui_replay_toggle();
    part_clear();
    fx_clear();
    minis_clear();
    mood_props_home();
    mood_bg_tint(0, 0.0f);
    toy_crush = 0.0f;
    part_on_floor = 0;
    cur_toy = (uint8_t)(t % NTOYS);
    knobs_enter_toy();
    uni_rig.init = 0;
    seq_enable(cur_toy == TOY_DRUM);
    TOY->init();
    kind = mood_i == MOOD_GLITCH ? TR_GLITCH : (int)(G.trans_n++ % (NTRANS - 1));
    fx_transition(kind, CORNER[cur_toy][0], CORNER[cur_toy][1], mix565(mood->accent, CC_INK, 140u));
    snd_play(P_WHOOSH, 0.0f, 0.8f);            /* a quick swell, then a tick in time at its end */
    G.tick_at = plat_ms() + 210u;
    G.spoke = 0;
}

/* ================================================================ keys === */
static void unlock_check(void)
{
    if (set.actions % UNLOCK_EVERY != 0u || set.unlocked >= NUNLOCK)
        return;
    set.unlocked++;
    if (set.unlocked & 1u) {                   /* a hat: on at once, so she sees it */
        set.hat = (uint8_t)(hats_unlocked() - 1);
        ui_unlock_flash(HATS[set.hat]);
    } else {
        set.mane = (uint8_t)(manes_unlocked() - 1);
        ui_unlock_flash(cur_hat());
    }
    chord(P_CHIME, 6, 0, 0.6f);
    part_burst(PT_CONFETTI, 120.0f, 96.0f, 24, 2.0f, 6.0f, 36, 0, 4);
    knobs_save_soon();
}

/* everything at once: the mash (5 presses in 200 ms) */
static void app_mega_all(uint32_t k)
{
    G.megas++;
    fx_invert();
    chord(P_STAB, 4, 0, 0.8f);
    snd_play(P_FART_TUBA, (float)mood_degree_midi(2, 0), 0.9f);
    snd_play(P_PLOP_MARIMBA, (float)mood_degree_midi(5, 2), 0.8f);
    part_mood_burst(120.0f, 110.0f, 40, 2.0f, 8.0f);
    part_burst(PT_CONFETTI, 120.0f, 100.0f, 40, 2.0f, 7.0f, 40, 0, 4);
    part_fan(PT_CLOUD, 90.0f, 140.0f, 8, 3.14159f, 0.6f, 1.0f, 3.0f, 30, lighten(CC_PINK, 100u), 9);
    part_fan(PT_BUBBLE, 160.0f, 110.0f, 10, -1.57f, 0.8f, 1.0f, 3.0f, 50, mood->accent, 4);
    part_fan(PT_DROPLET, 100.0f, 150.0f, 14, -2.4f, 0.4f, 3.0f, 6.0f, 50, 0, 2);
    part_fan(PT_POOPLET, 90.0f, 150.0f, 4, -2.2f, 0.4f, 2.0f, 4.0f, 50, 0, 4);
    ring_spawn(120.0f, 120.0f, 10.0f, 8.0f, 1.0f, 500.0f, 7.0f, U_WHITE);
    impact(0.6f, 4);
    ui_led_flash_all(250u);
    if (TOY->on_mega)
        TOY->on_mega(k);
}

static void app_flourish(int big)
{
    chord(P_STAB, big ? 5 : 4, big ? 2 : 0, big ? 0.8f : 0.6f);
    part_burst(PT_CONFETTI, 120.0f, 80.0f, big ? 40 : 20, 2.0f, big ? 8.0f : 5.0f, 40, 0, 4);
    part_mood_burst(120.0f, 80.0f, big ? 30 : 14, 2.0f, 6.0f);
    fx_trauma(big ? 0.5f : 0.25f);
    if (big)
        ui_led_flash_all(400u);
}

static void app_key_down(uint32_t k, float vel, int synthetic)
{
    if (k >= NKEYS)
        return;
    G.key_t0[k] = plat_ms();
    if (!synthetic) {
        ui_rec_event(k, vel, 1);
        ui_input();
        set.actions++;
        unlock_check();
    }
    ui_led_key(k);
    ui_chaos_press();
    TOY->on_key(k, vel);
    if (!(TOY->flags & TF_HOLD))
        ui_charge_press(k);
    if (chaos > 0.7f && cur_toy != TOY_DRUM)   /* high chaos: an extra sparkle voice on top, in the scale */
        snd_play(P_CHIME, (float)mood_key_midi(k, 6), 0.3f);
    if (ui_mash_press())
        app_mega_all(k);
}

static void app_key_up(uint32_t k, int synthetic)
{
    uint32_t held;
    if (k >= NKEYS)
        return;
    held = plat_ms() - G.key_t0[k];
    if (!synthetic)
        ui_rec_event(k, 0.0f, 0);
    TOY->on_release(k, held);
    if (ui_charge_release(k) && TOY->on_mega)
        TOY->on_mega(k);
}

/* ============================================================== buttons === */
static int toy_of_button(uint32_t b)
{
    int t;
    for (t = 0; t < NTOYS; t++)
        if (TOYREG[t]->button_id == b)
            return t;
    return -1;
}

static void preset_step(int d)
{
    int n = TOY->preset_count, p = ((int)set.preset[cur_toy] + d + n) % n;
    set.preset[cur_toy] = (uint8_t)p;
    ui_preset_flash(TOY->presets[p].icon, p, n);
    if (cur_toy == TOY_DRUM)                   /* audition: a kit plays a kick and a snare; the rest a note */
        drum_audition();
    else
        snd_play(TOY->presets[p].patch, (float)mood_degree_midi(4, 0), 0.8f);
    knobs_save_soon();
}

static void app_clean(void)
{
    G.cleans++;
    if (TOY->clean)
        TOY->clean();
    if (cur_toy != TOY_TOILET) {               /* (the toilet flushes instead) */
        sweep_start();
        snd_play(P_SWOOSH, 0.0f, 0.8f);
        part_clear();
        minis_clear();
        mood_props_home();
    }
}

static void button_press(uint32_t b)
{
    int t = toy_of_button(b);
    if (t >= 0) {
        if (t != cur_toy)
            switch_toy(t);                     /* another toy: at once */
        return;                                /* (this toy's: a long press cleans, decided on hold) */
    }
    switch (b) {
    case B_PLAY:
        G.autoplay ^= 1u;
        g_autoplay = G.autoplay;
        G.auto_next = plat_ms() + 300u;
        if (!G.autoplay && G.auto_key >= 0) {
            app_key_up((uint32_t)G.auto_key, 1);
            G.auto_key = -1;
        }
        break;
    case B_OCTDN:
        preset_step(-1);
        break;
    case B_OCTUP:
        preset_step(1);
        break;
    default:
        break;
    }
}

static void button_long(uint32_t b)
{
    int t = toy_of_button(b);
    if (t >= 0 && t == cur_toy) {
        app_clean();
        return;
    }
    switch (b) {
    case B_OCTUP:                              /* back to the first preset */
        set.preset[cur_toy] = 0;
        ui_preset_flash(TOY->presets[0].icon, 0, TOY->preset_count);
        knobs_save_soon();
        break;
    case B_REC:
        if (cur_toy == TOY_DRUM)
            seq_clear();
        break;
    case B_SEL:                                /* the next mane */
        set.mane = (uint8_t)((set.mane + 1u) % (uint32_t)manes_unlocked());
        ui_unlock_flash(cur_hat());
        snd_play(P_CHIME, (float)mood_degree_midi(6, 4), 0.5f);
        knobs_save_soon();
        break;
    case B_SAVE:                               /* quiet mode: bedtime */
        set.quiet ^= 1u;
        snd_apply_volume();
        ui_quiet_flash();
        knobs_save_soon();
        break;
    default:
        break;
    }
}

static void button_tap(uint32_t b)             /* released before LONG_MS */
{
    int t = toy_of_button(b);
    if (t >= 0 && t == cur_toy) {              /* this toy's button: she waves hello */
        ui_input();
        snd_play(P_CHIME, (float)mood_degree_midi(5, 0), 0.5f);
        part_mood_burst(120.0f, 90.0f, 8, 1.0f, 3.0f);
        return;
    }
    switch (b) {
    case B_REC:
        if (cur_toy == TOY_DRUM)
            seq_rec();
        else
            ui_replay_toggle();
        break;
    case B_SEL:                                /* the next hat she has */
        set.hat = (uint8_t)((set.hat + 1u) % (uint32_t)hats_unlocked());
        snd_play(P_POP, (float)mood_degree_midi(6, 2), 0.6f);
        part_burst(PT_SPARKLE, 190.0f, 70.0f, 10, 1.0f, 3.0f, 20, mood->accent, 2);
        knobs_save_soon();
        break;
    case B_SAVE:                               /* a surprise */
        app_flourish(0);
        break;
    default:
        break;
    }
}

/* ================================================================ input === */
static void input(void)
{
    uint32_t keys, btn, k, pressed, released, bp, br, now = plat_ms();
    touch_poll();
    keys = touch_keys();
    btn = plat_buttons();
    pressed = keys & ~G.keys;
    released = G.keys & ~keys;
    G.keys = keys;
    for (k = 0; k < NKEYS; k++) {
        if (pressed >> k & 1u)
            app_key_down(k, (float)touch_vel(k) / 127.0f, 0);
        if (released >> k & 1u)
            app_key_up(k, 0);
    }
    bp = btn & ~G.btn;
    br = G.btn & ~btn;
    G.btn = btn;
    for (k = 0; k < NB; k++) {
        if (bp >> k & 1u) {
            G.btn_t0[k] = now;
            G.btn_long &= ~(1u << k);
            ui_input();
            button_press(k);
        }
        if ((btn >> k & 1u) && !(G.btn_long >> k & 1u) && now - G.btn_t0[k] >= (k == B_SAVE ? 1000u : LONG_MS)) {
            G.btn_long |= 1u << k;
            button_long(k);
        }
        if ((br >> k & 1u) && !(G.btn_long >> k & 1u))
            button_tap(k);
    }
    knobs_poll();
}
void app_input_only(void) { input(); }

/* auto-play: the toy plays itself, a little tune walking up and down the scale */
static void autoplay_tick(void)
{
    uint32_t now = plat_ms();
    if (!G.autoplay || cur_toy == TOY_DRUM)    /* (Drum Jam's PLAY is its auto-drummer) */
        return;
    if ((int32_t)(now - G.auto_next) >= 0) {
        if (G.auto_key >= 0)
            app_key_up((uint32_t)G.auto_key, 1);
        G.auto_deg = (int8_t)clamp(G.auto_deg + (ss_rnd() & 1u ? 3 : -3) + ss_rndi(3) - 1, 0, 26);
        G.auto_key = G.auto_deg;
        app_key_down((uint32_t)G.auto_key, 0.75f, 1);
        G.auto_release = now + 250u + (uint32_t)ss_rndi(500);
        G.auto_next = now + 1400u + (uint32_t)ss_rndi(600);
    }
    if (G.auto_key >= 0 && (int32_t)(now - G.auto_release) >= 0) {
        app_key_up((uint32_t)G.auto_key, 1);
        G.auto_key = -1;
    }
}

/* ================================================================== app === */
void app_say(const char *a, const char *b)
{
    if (ann_on())
        ann_say(a, b, ANN_NOW);
}
static void speak(void)                         /* for a screen reader, if one listens: which toy */
{
    if (ann_on() && (ann_where() || !G.spoke)) {
        G.spoke = 1;
        ann_say(APP_NAME, TOY->name, ANN_NOW);
    }
    ann_tick(plat_ms());
}

void app_init(void)
{
    voice_init();
    draw_init();
    knobs_load();
    mood_set(set.mood, 0);
    snd_apply_volume();
    seq_pitches();
}

void app_ui_init(void)
{
    touch_on(1);
    lcd_fill(0, 0, 240, 240, HEX(0x1E1B4B));
    ss_seed ^= plat_ms() * 2654435761u + 1u;
    G.last_ms = plat_ms();
    G.auto_key = -1;
    G.auto_deg = 8;
    FX.flash_ok_at = plat_ms();
    FX.flash_gap = 0xFFFFFFFFu;
    ui_init();
    cur_toy = 0;
    switch_toy(TOY_FART);
}

void app_frame(void)
{
    uint32_t now = plat_ms(), dt = now - G.last_ms;
    float fdt;
    G.last_ms = now;
    if (dt > 50u)
        dt = 50u;
    fdt = (float)dt;
    GT.fdt = fdt;
    input();
    speak();
    autoplay_tick();
    knobs_tick();
    if (G.tick_at && (int32_t)(now - G.tick_at) >= 0) {
        G.tick_at = 0;
        snd_play(P_TICK, (float)mood_degree_midi(6, 0), 0.6f);
    }
    /* update: skipped while frozen (hitstop), so the impact reads; drawing goes on */
    if (!fx_frozen()) {
        GT.T += fdt;
        TOY->update(fdt);
        particles_update(fdt);
        fx_update(fdt);
        mood_props_update(fdt);
        minis_update(fdt);
    }
    ui_update(fdt);
    knobs_update(fdt);
    snd_chaos(fm_maxf(0.0f, chaos - 0.55f) * 0.6f + toy_crush);
    fx_frame();
    /* draw: the whole frame on the canvas, then one transfer */
    cv_begin(240, 240, 0);
    dr_glows = 0;
    {
        int32_t sx = dr_ox, sy = dr_oy;
        dr_ox = dr_oy = 0;
        mood_draw_sky();                       /* (the sky never shakes, so the edges never show) */
        dr_ox = sx / 3;
        dr_oy = sy / 3;
        mood_props_draw(GT.T, 0);              /* the far layers shake less: parallax */
        dr_ox = sx;
        dr_oy = sy;
    }
    TOY->draw(GT.T);
    mood_props_draw(GT.T, 1);
    fx_draw_world();
    particles_draw();
    minis_draw();
    ui_draw_dancers(GT.T);
    draw_sweep();
    ui_draw_chaos();
    fx_draw_post();
    ui_draw_overlays();
    knobs_draw();
    cv_blit(0, 0);
    g_rec_lit = cur_toy == TOY_DRUM ? (uint8_t)(seq_loop_state() == LOOP_REC ? 1 : seq_loop_state() == LOOP_PLAY ? 3 : 0)
                                    : (uint8_t)(ui_replaying() ? 2 : 0);
    ui_leds();
}

#ifdef PLAT_HOST
static int toy_expect(const char *what, int *got)
{
    int i, n = 0;
    if (!strcmp(what, "poops"))
        *got = poop_count();
    else if (!strcmp(what, "minis")) {
        for (i = 0; i < NMINI; i++)
            n += minis[i].on;
        *got = n;
    } else if (!strcmp(what, "pee"))
        *got = pee.active;
    else if (!strcmp(what, "clouds")) {
        for (i = 0; i < NCLOUD; i++)
            n += fart.c[i].on;
        *got = n;
    } else if (!strcmp(what, "burp"))
        *got = burp.phase;
    else if (!strcmp(what, "sneeze"))
        *got = snz.phase;
    else if (!strcmp(what, "boogers"))
        *got = booger_count();
    else if (!strcmp(what, "flushing"))
        *got = toil_flushing();
    else if (!strcmp(what, "swallowed"))
        *got = (int)toil.swallowed;
    else
        return 0;
    return 1;
}

int app_expect(const char *what, int *got)
{
    if (!strcmp(what, "toy"))
        *got = cur_toy;
    else if (!strcmp(what, "mood"))
        *got = mood_i;
    else if (!strcmp(what, "mood_changes"))
        *got = (int)G.mood_changes;
    else if (!strcmp(what, "auto"))
        *got = G.autoplay;
    else if (!strcmp(what, "voices"))
        *got = snd_voices();
    else if (!strcmp(what, "particles"))
        *got = part_count();
    else if (!strcmp(what, "vol"))
        *got = set.vol;
    else if (!strcmp(what, "quiet"))
        *got = set.quiet;
    else if (!strcmp(what, "preset"))
        *got = set.preset[cur_toy];
    else if (!strcmp(what, "k2") || !strcmp(what, "k3") || !strcmp(what, "k4") || !strcmp(what, "k6") || !strcmp(what, "k7"))
        *got = kv(what[1] == '2' ? K2 : what[1] == '3' ? K3 : what[1] == '4' ? K4 : what[1] == '6' ? K6 : K7);
    else if (!strcmp(what, "flashes"))
        *got = (int)FX.flashes;
    else if (!strcmp(what, "flash_gap"))      /* the shortest time between two full-screen flashes, ms */
        *got = FX.flash_gap > 100000u ? 100000 : (int)FX.flash_gap;
    else if (!strcmp(what, "hitstops"))
        *got = (int)FX.hitstops;
    else if (!strcmp(what, "megas"))
        *got = (int)G.megas;
    else if (!strcmp(what, "cleans"))
        *got = (int)G.cleans;
    else if (!strcmp(what, "chaos"))
        *got = (int)(chaos * 100.0f);
    else if (!strcmp(what, "idle"))
        *got = ui_idle_state();
    else if (!strcmp(what, "charge"))
        *got = (int)(ui_charge() * 100.0f);
    else if (!strcmp(what, "replay"))
        *got = ui_replaying();
    else if (!strcmp(what, "loop"))
        *got = seq_loop_state();
    else if (!strcmp(what, "unlocked"))
        *got = set.unlocked;
    else if (!strcmp(what, "hat"))
        *got = set.hat;
    else if (!strcmp(what, "props_away"))
        *got = mood_props_away();
    else if (!toy_expect(what, got))
        return 0;
    return 1;
}
#endif
