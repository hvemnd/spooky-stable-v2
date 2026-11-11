/* SPDX-License-Identifier: MPL-2.0
 * Spooky Stable v2: round every toy (ui.h). */

static struct {
    /* lights */
    uint32_t key_t0[NKEYS];
    uint32_t all_until, all_mid_until, alt_until, chase_t0, pat_until;
    uint8_t pat_every;
    /* chaos */
    uint32_t last_press;
    uint8_t over50, over90;
    /* charge */
    int8_t ch_key;
    uint32_t ch_t0;
    uint16_t ch_voice;
    float ch_level;
    /* mash */
    uint32_t mash[8];
    uint8_t mash_i;
    uint32_t mash_ok_at;
    /* idle */
    uint32_t last_input, startle_t0, snore_at;
    uint8_t snore_n;
    /* overlays */
    uint32_t preset_t0, quiet_t0, unlock_t0;
    uint8_t preset_icon, preset_i, preset_n, unlock_hat;
    /* replay */
    struct { uint32_t t; uint8_t k, vel, down, pad; } ev[256], rp[256];
    uint32_t ev_n, rp_n, rp_i, rp_t0, rp_base;
    uint8_t replaying;
    uint32_t held_rp;
    /* dancers */
    uint32_t dance_until;
} UI;

/* ----------------------------------------------------------------- lights --- */
static void ui_led_key(uint32_t k)
{
    if (k < NKEYS)
        UI.key_t0[k] = plat_ms() | 1u;
}
static void ui_led_kick(void)
{
    UI.all_until = plat_ms() + 60u;
    UI.all_mid_until = plat_ms() + 200u;
}
static void ui_led_snare(void) { UI.alt_until = plat_ms() + 140u; }
static void ui_led_hat(void) { UI.chase_t0 = plat_ms() | 1u; }
static void ui_led_flash_all(uint32_t ms)
{
    UI.all_until = plat_ms() + ms;
    UI.all_mid_until = plat_ms() + ms + 250u;
}
static void ui_led_pattern(int every, uint32_t ms)
{
    UI.pat_every = (uint8_t)(every < 2 ? 2 : every);
    UI.pat_until = plat_ms() + ms;
}

static uint8_t g_autoplay, g_rec_lit;          /* app.c sets these */

static void ui_leds(void)
{
    uint32_t now = plat_ms(), lit = 0, mid = 0, k, lit_b, dim_b, mid_b = 0, ns;
    for (k = 0; k < NKEYS; k++) {              /* each press: lit, then the middle level, then off; its */
        uint32_t t0 = UI.key_t0[k], a;          /* neighbours a ripple at the middle level */
        if (!t0)
            continue;
        a = now - t0;
        if (a < 110u) {
            lit |= 1u << k;
            if (k > 0)
                mid |= 1u << (k - 1);
            if (k + 1 < NKEYS)
                mid |= 1u << (k + 1);
        } else if (a < 320u) {
            mid |= 1u << k;
        } else {
            UI.key_t0[k] = 0;
        }
    }
    if ((int32_t)(UI.all_until - now) > 0)
        lit = 0x7FFFFFFu;
    else if ((int32_t)(UI.all_mid_until - now) > 0)
        mid = 0x7FFFFFFu;
    if ((int32_t)(UI.alt_until - now) > 0)
        lit |= 0x5555555u;
    if (UI.chase_t0) {                         /* a hat: one light runs along the keybed */
        uint32_t a = now - UI.chase_t0;
        if (a < 260u)
            lit |= 1u << (a * NKEYS / 260u);
        else
            UI.chase_t0 = 0;
    }
    if ((int32_t)(UI.pat_until - now) > 0)
        for (k = 0; k < NKEYS; k += UI.pat_every)
            lit |= 1u << k;
    if (UI.ch_level > 0.0f) {                  /* the charge fills the keybed left to right */
        uint32_t n = (uint32_t)(UI.ch_level * NKEYS);
        for (k = 0; k < n && k < NKEYS; k++)
            mid |= 1u << k;
        if (UI.ch_level >= 1.0f && (now / 80u) & 1u)
            lit |= 0x7FFFFFFu;
    }
    if (g_autoplay) {                          /* auto-play: the keys chase */
        uint32_t p = (now / 70u) % NKEYS;
        lit |= 1u << p | 1u << ((p + NKEYS - 1u) % NKEYS) | 1u << ((p + NKEYS - 2u) % NKEYS);
    }
    lit |= touch_keys();
    lit_b = 1u << TOY->button_id;
    if (g_autoplay)
        lit_b |= LED_PLAY_GREEN;
    if (g_rec_lit == 2u || (g_rec_lit == 1u && (now / 250u) & 1u))
        lit_b |= 1u << B_REC;
    else if (g_rec_lit)
        mid_b |= 1u << B_REC;
    if (set.quiet)
        mid_b |= 1u << B_SAVE;
    if (now - UI.preset_t0 < 400u)
        lit_b |= 1u << B_OCTDN | 1u << B_OCTUP;
    dim_b = 1u << B_HOME | 1u << B_EDIT | 1u << B_LFO | 1u << B_FX | 1u << B_ENV | 1u << B_GLO | 1u << B_ARP |
            1u << B_SEQ | 1u << B_PLAY | 1u << B_REC | 1u << B_OCTDN | 1u << B_OCTUP | 1u << B_SEL | 1u << B_SAVE;
    plat_lights(lit_b, lit, dim_b, 0x7FFFFFFu, 2);
    plat_lights_mid(mid_b, mid & ~lit);
    /* the dim layer breathes slowly (quieter in quiet mode) */
    ns = (uint32_t)((set.quiet ? 700.0f : 1500.0f) + (set.quiet ? 300.0f : 1100.0f) * (0.5f + 0.5f * fm_sinf(TAU * (float)now / 4000.0f)));
    plat_dim_ns(ns);
}

/* ------------------------------------------------------------------ chaos --- */
static void ui_chaos_press(void)
{
    UI.last_press = plat_ms();
    chaos = fm_minf(1.0f, chaos + 0.055f);
    if (chaos >= 0.5f && !UI.over50) {
        UI.over50 = 1;
        app_flourish(0);
    }
    if (chaos >= 0.9f && !UI.over90) {
        UI.over90 = 1;
        app_flourish(1);
        ui_dancers(4000u);
    }
}

static void ui_draw_chaos(void)                /* a balloon in the top corner, blowing up with the chaos */
{
    float T = (float)plat_ms(), c = chaos, r = 5.0f + 12.0f * c, sway = 2.0f * fm_sinf(T / 500.0f);
    float shake = c > 0.9f ? 1.5f * fm_sinf(T / 25.0f) : 0.0f, x = 16.0f + sway + shake, y = 8.0f + r * 1.1f;
    int i;
    dr_ox = dr_oy = 0;
    for (i = 0; i < 6; i++)                    /* its string */
        cap(x + 1.5f * fm_sinf(T / 300.0f + (float)i), y + r + (float)i * 4.0f, x + 1.5f * fm_sinf(T / 300.0f + (float)i + 1.0f),
            y + r + (float)(i + 1) * 4.0f, 0.6f, U_LINE);
    if (c > 0.5f)
        glow(x, y, r * 2.0f, scale565(mood->accent, 140u), (uint32_t)(255.0f * (c - 0.4f)));
    ell(x, y, r * 0.9f + 1.0f, r + 1.0f, U_LINE);
    ell(x, y, r * 0.9f, r, c > 0.9f ? lighten(mood->accent, (uint32_t)(80.0f + 60.0f * fm_sinf(T / 60.0f))) : mood->accent);
    tri(x - 2.5f, y + r + 3.0f, x + 2.5f, y + r + 3.0f, x, y + r - 1.0f, darken(mood->accent, 60u));
    ell(x - r * 0.35f, y - r * 0.4f, r * 0.2f, r * 0.3f, lighten(mood->accent, 170u));
}

/* ----------------------------------------------------------------- charge --- */
#define CHARGE_DELAY_MS 300u
#define CHARGE_MS 1200u                        /* fixed 1.2 s, except Sneeze (K2) */
static void ui_charge_press(uint32_t k)
{
    if (UI.ch_key >= 0)
        return;
    UI.ch_key = (int8_t)k;
    UI.ch_t0 = plat_ms();
    UI.ch_level = 0.0f;
}
static void ui_charge_cancel(void)
{
    if (UI.ch_voice)
        snd_off(UI.ch_voice);
    UI.ch_voice = 0;
    UI.ch_key = -1;
    UI.ch_level = 0.0f;
}
static int ui_charge_release(uint32_t k)
{
    int full;
    if (UI.ch_key != (int8_t)k)
        return 0;
    full = UI.ch_level >= 1.0f;
    ui_charge_cancel();
    return full;
}
static float ui_charge(void) { return UI.ch_level; }

/* ------------------------------------------------------------------- mash --- */
static int ui_mash_press(void)
{
    uint32_t now = plat_ms(), old = UI.mash[(UI.mash_i + 4u) % 8u];
    UI.mash[UI.mash_i % 8u] = now;
    UI.mash_i++;
    if (UI.mash_i >= 5u && now - old <= 200u && (int32_t)(now - UI.mash_ok_at) >= 0) {
        UI.mash_ok_at = now + 1500u;
        return 1;
    }
    return 0;
}

/* ------------------------------------------------------------------- idle --- */
static int ui_idle_state(void)
{
    uint32_t a = plat_ms() - UI.last_input;
    if (g_autoplay || ui_replaying())
        return IDLE_AWAKE;
    return a < 10000u ? IDLE_AWAKE : a < 30000u ? IDLE_WAVE : a < 60000u ? IDLE_SLEEP : IDLE_DREAM;
}
static int ui_input(void)
{
    int st = ui_idle_state();
    UI.last_input = plat_ms();
    if (st >= IDLE_SLEEP) {                    /* she wakes with a start */
        UI.startle_t0 = plat_ms() | 1u;
        snd_play(P_PLOP_BOING, (float)mood_degree_midi(5, 4), 0.6f);
        return 1;
    }
    return 0;
}

static void ui_idle_pose(pose_t *p)
{
    int st = ui_idle_state();
    float T = (float)plat_ms(), a = (float)(plat_ms() - UI.startle_t0);
    if (UI.startle_t0 && a < 700.0f) {         /* the startle: squashed, then stretched, eyes wide */
        float e = ease_out_elastic(a / 700.0f);
        p->scale_y *= 0.8f + 0.2f * e;
        p->scale_x *= 1.15f - 0.15f * e;
        p->eye_state = EY_WIDE;
        p->mouth_state = MO_O;
        return;
    }
    switch (st) {
    case IDLE_WAVE:                            /* she looks at you and waves */
        p->look_x = 0.0f;
        p->look_y = 0.25f;
        p->head_rot += 5.0f;
        p->arm_l = 0.75f + 0.18f * fm_sinf(T / 140.0f);
        p->mouth_state = MO_GRIN;
        break;
    case IDLE_SLEEP:
    case IDLE_DREAM:                           /* asleep, breathing */
        p->eye_state = EY_SLEEP;
        p->mouth_state = MO_CLOSED;
        p->head_rot -= 9.0f;
        p->scale_y *= 1.0f + 0.03f * fm_sinf(TAU * T / 2400.0f);
        p->scale_x *= 1.0f - 0.015f * fm_sinf(TAU * T / 2400.0f);
        p->blush = 0.6f;
        p->tail_phase = 0.0f;
        break;
    default:
        break;
    }
}

static void draw_mini_scene(float x, float y, int which, float T)
{
    pose_t p = pose_rest(x + 6.0f, y + 12.0f, 0.22f);
    p.accessory_id = mood->accessory;
    switch (which) {
    case 0:                                    /* a fart cloud */
        p.tail_phase = 0.9f;
        draw_unicorn(&p, 0);
        disc(x - 12.0f, y + 2.0f, 6.0f + 1.5f * fm_sinf(T / 200.0f), CC_PINK);
        disc(x - 12.0f, y + 2.0f, 4.0f, CC_YELLOW);
        break;
    case 1:                                    /* a poop pile */
        draw_unicorn(&p, 0);
        draw_pooplet(x - 13.0f, y + 12.0f, 0.8f);
        draw_pooplet(x - 19.0f, y + 12.0f, 0.6f);
        break;
    case 2:                                    /* drums */
        p.arm_l = 0.5f + 0.3f * fm_sinf(T / 120.0f);
        draw_unicorn(&p, 0);
        ell(x + 2.0f, y + 9.0f, 9.0f, 5.0f, mood->accent);
        break;
    default:                                   /* bubbles */
        draw_unicorn(&p, 0);
        ring_a(x - 12.0f, y - 2.0f - (float)((uint32_t)(T / 40.0f) % 12u), 3.0f, 3.0f, 1.0f, U_WHITE, 256u);
        break;
    }
}

static void ui_draw_idle(const pose_t *p)
{
    int st = ui_idle_state();
    float T = (float)plat_ms(), hx = p->x + (float)p->facing * 42.0f * p->scale, hy = p->y - p->lift - 112.0f * p->scale;
    float a = (float)(plat_ms() - UI.startle_t0);
    if (UI.startle_t0 && a < 700.0f) {         /* "!" */
        float s = ease_out_back(a / 200.0f) * p->scale;
        dr_ox = dr_oy = 0;
        box(hx - 3.0f * s, hy - 34.0f * s, 6.0f * s, 18.0f * s, U_LINE);
        box(hx - 2.0f * s, hy - 33.0f * s, 4.0f * s, 16.0f * s, U_WHITE);
        disc(hx, hy - 9.0f * s, 3.5f * s, U_LINE);
        disc(hx, hy - 9.0f * s, 2.5f * s, U_WHITE);
        return;
    }
    if (st >= IDLE_SLEEP) {                    /* sleep bubbles drifting up (no "Zzz": no text) */
        int i;
        for (i = 0; i < 3; i++) {
            float ph = T / 1600.0f + (float)i / 3.0f, u = ph - fm_floorf(ph);
            ring_a(hx + 8.0f + 10.0f * u, hy - 30.0f * u, 2.0f + 5.0f * u, 2.0f + 5.0f * u, 1.2f, U_WHITE, (uint32_t)(250.0f * (1.0f - u)));
        }
    }
    if (st == IDLE_DREAM) {                    /* a dream: mini versions of the toys, one after another */
        float bx = 70.0f, by = 50.0f, r = 34.0f + 1.5f * fm_sinf(T / 500.0f);
        disc(hx - 6.0f, hy - 8.0f, 3.0f, U_WHITE);
        disc(hx - 16.0f, hy - 20.0f, 5.0f, U_WHITE);
        disc(bx + 22.0f, by + 30.0f, 7.0f, U_WHITE);
        disc(bx, by, r + 2.0f, U_LINE);
        disc(bx, by, r, U_WHITE);
        disc(bx - 18.0f, by - 16.0f, 16.0f, U_WHITE);
        disc(bx + 18.0f, by - 14.0f, 14.0f, U_WHITE);
        disc(bx, by, r - 4.0f, lighten(mood->accent, 190u));
        draw_mini_scene(bx, by, (int)((uint32_t)(T / 3000.0f) % 4u), T);
    }
}

/* --------------------------------------------------------------- overlays --- */
static void ui_preset_flash(int icon, int index, int count)
{
    UI.preset_t0 = plat_ms();
    UI.preset_icon = (uint8_t)icon;
    UI.preset_i = (uint8_t)index;
    UI.preset_n = (uint8_t)count;
    ui_led_pattern(2 + index, 1000u);
}
static void ui_quiet_flash(void) { UI.quiet_t0 = plat_ms() | 1u; }
static void ui_unlock_flash(int hat)
{
    UI.unlock_t0 = plat_ms() | 1u;
    UI.unlock_hat = (uint8_t)hat;
}

/* the preset icons: big friendly glyphs of the instrument, no text */
static void draw_preset_icon(int icon, float x, float y, float s)
{
    uint16_t a = mood->accent, g = CC_GOLD, d = CC_GOLD_D;
    int i;
    switch (icon) {
    case IC_HORN:                              /* a tuba */
        cap(x - 14.0f * s, y + 10.0f * s, x + 6.0f * s, y + 10.0f * s, 6.0f * s, g);
        cap(x + 6.0f * s, y + 10.0f * s, x + 6.0f * s, y - 10.0f * s, 6.0f * s, g);
        tri(x + 6.0f * s, y - 8.0f * s, x - 6.0f * s, y - 26.0f * s, x + 20.0f * s, y - 26.0f * s, g);
        ell(x + 7.0f * s, y - 26.0f * s, 13.0f * s, 3.5f * s, d);
        disc(x - 14.0f * s, y + 10.0f * s, 3.0f * s, d);
        break;
    case IC_KAZOO:
        ell(x, y, 20.0f * s, 7.0f * s, a);
        tri(x + 18.0f * s, y - 7.0f * s, x + 18.0f * s, y + 7.0f * s, x + 26.0f * s, y, darken(a, 60u));
        disc(x - 2.0f * s, y - 7.0f * s, 4.5f * s, lighten(a, 120u));
        break;
    case IC_PIPE:                              /* a didgeridoo / flute */
        cap(x - 22.0f * s, y + 6.0f * s, x + 22.0f * s, y - 6.0f * s, 5.0f * s, d);
        for (i = 0; i < 4; i++)
            disc(x - 8.0f * s + (float)i * 7.0f * s, y + 2.0f * s - (float)i * 1.9f * s, 1.8f * s, CC_INK);
        break;
    case IC_SLIDE:                             /* a slide whistle */
        cap(x - 20.0f * s, y, x + 8.0f * s, y, 5.0f * s, U_WHITE);
        cap(x + 8.0f * s, y, x + 22.0f * s, y, 2.0f * s, a);
        disc(x + 24.0f * s, y, 4.0f * s, a);
        tri(x - 12.0f * s, y - 5.0f * s, x - 6.0f * s, y - 5.0f * s, x - 9.0f * s, y - 1.0f * s, CC_INK);
        break;
    case IC_BELLS:                             /* marimba bars */
        for (i = 0; i < 5; i++)
            box(x - 22.0f * s + (float)i * 9.0f * s, y - (12.0f - (float)i * 2.0f) * s, 7.0f * s, (24.0f - (float)i * 4.0f) * s, RAINBOW[i]);
        break;
    case IC_BLOCK:                             /* a woodblock */
        box(x - 16.0f * s, y - 8.0f * s, 32.0f * s, 16.0f * s, CC_BROWN);
        box(x - 12.0f * s, y - 2.0f * s, 24.0f * s, 3.0f * s, CC_BROWN_D);
        break;
    case IC_DROP:
        disc(x, y + 6.0f * s, 12.0f * s, HEX(0x60C8FF));
        tri(x - 11.0f * s, y + 2.0f * s, x + 11.0f * s, y + 2.0f * s, x, y - 20.0f * s, HEX(0x60C8FF));
        disc(x - 4.0f * s, y + 3.0f * s, 3.0f * s, U_WHITE);
        break;
    case IC_STRING:                            /* a little violin */
        ell(x, y + 8.0f * s, 11.0f * s, 9.0f * s, CC_BROWN);
        ell(x, y - 6.0f * s, 8.0f * s, 7.0f * s, CC_BROWN);
        box(x - 1.5f * s, y - 26.0f * s, 3.0f * s, 22.0f * s, CC_BROWN_D);
        for (i = -1; i <= 1; i++)
            box(x + (float)i * 2.0f * s, y - 20.0f * s, 0.8f * s + 0.4f, 34.0f * s, U_BONE);
        break;
    case IC_SPRING:                            /* boing */
        for (i = 0; i < 5; i++)
            ell(x, y + 12.0f * s - (float)i * 6.0f * s, 10.0f * s, 3.0f * s, i & 1 ? a : lighten(a, 100u));
        break;
    case IC_FROG:
        ell(x, y + 4.0f * s, 20.0f * s, 13.0f * s, CC_GREEN);
        disc(x - 9.0f * s, y - 9.0f * s, 6.0f * s, CC_GREEN);
        disc(x + 9.0f * s, y - 9.0f * s, 6.0f * s, CC_GREEN);
        disc(x - 9.0f * s, y - 9.0f * s, 3.0f * s, U_WHITE);
        disc(x + 9.0f * s, y - 9.0f * s, 3.0f * s, U_WHITE);
        disc(x - 9.0f * s, y - 9.0f * s, 1.5f * s, CC_INK);
        disc(x + 9.0f * s, y - 9.0f * s, 1.5f * s, CC_INK);
        arc_dots(x, y + 2.0f * s, 10.0f * s, 0.4f, 2.7f, 2.0f * s, CC_INK);
        break;
    case IC_ROBOT:
        box(x - 14.0f * s, y - 12.0f * s, 28.0f * s, 24.0f * s, HEX(0xA0AEC0));
        box(x - 9.0f * s, y - 6.0f * s, 6.0f * s, 6.0f * s, a);
        box(x + 3.0f * s, y - 6.0f * s, 6.0f * s, 6.0f * s, a);
        box(x - 8.0f * s, y + 5.0f * s, 16.0f * s, 3.0f * s, CC_INK);
        cap(x, y - 12.0f * s, x, y - 20.0f * s, 1.2f * s, CC_INK);
        disc(x, y - 21.0f * s, 3.0f * s, CC_PINK);
        break;
    case IC_DRUM:
    case IC_BIGDRUM:
    case IC_TINYDRUM: {
        float k = icon == IC_BIGDRUM ? 1.3f : icon == IC_TINYDRUM ? 0.7f : 1.0f;
        ell(x, y + 10.0f * s * k, 18.0f * s * k, 6.0f * s * k, darken(a, 60u));
        box(x - 18.0f * s * k, y - 4.0f * s * k, 36.0f * s * k, 14.0f * s * k, darken(a, 60u));
        ell(x, y - 4.0f * s * k, 18.0f * s * k, 6.0f * s * k, U_WHITE);
        cap(x - 6.0f * s, y - 10.0f * s, x - 20.0f * s, y - 24.0f * s, 1.6f * s, CC_BROWN);
        cap(x + 6.0f * s, y - 10.0f * s, x + 20.0f * s, y - 24.0f * s, 1.6f * s, CC_BROWN);
        break;
    }
    case IC_POTS:                              /* a pot and a spoon */
        ell(x, y + 4.0f * s, 18.0f * s, 12.0f * s, HEX(0x94A3B8));
        box(x - 18.0f * s, y - 6.0f * s, 36.0f * s, 10.0f * s, HEX(0x94A3B8));
        ell(x, y - 6.0f * s, 18.0f * s, 4.0f * s, HEX(0xCBD5E1));
        cap(x + 4.0f * s, y - 10.0f * s, x + 22.0f * s, y - 24.0f * s, 2.0f * s, CC_BROWN);
        break;
    case IC_MOUTH:                             /* a big open mouth (burps, sneezes) */
        ell(x, y, 20.0f * s, 14.0f * s, CC_INK);
        ell(x, y + 6.0f * s, 11.0f * s, 6.0f * s, CC_PINK);
        box(x - 12.0f * s, y - 13.0f * s, 24.0f * s, 4.0f * s, U_WHITE);
        break;
    case IC_WAVE:
        for (i = 0; i < 3; i++)
            arc_dots(x - 4.0f * s, y, (8.0f + 7.0f * (float)i) * s, -0.9f, 0.9f, 3.0f * s, a);
        break;
    case IC_BUBBLES:
        ring_a(x - 8.0f * s, y + 6.0f * s, 9.0f * s, 9.0f * s, 2.0f * s, a, 256u);
        ring_a(x + 10.0f * s, y - 4.0f * s, 7.0f * s, 7.0f * s, 2.0f * s, a, 256u);
        ring_a(x - 2.0f * s, y - 14.0f * s, 5.0f * s, 5.0f * s, 1.5f * s, a, 256u);
        break;
    case IC_TINY:
        disc(x, y, 7.0f * s, a);
        disc(x - 2.0f * s, y - 2.0f * s, 2.0f * s, U_WHITE);
        break;
    default:                                   /* IC_GIANT */
        disc(x, y, 22.0f * s, a);
        disc(x - 7.0f * s, y - 7.0f * s, 6.0f * s, lighten(a, 140u));
        break;
    }
}

static void ui_draw_overlays(void)
{
    uint32_t now = plat_ms();
    dr_ox = dr_oy = 0;
    if (now - UI.preset_t0 < 1000u) {          /* a new preset: its icon pops in, the dots say which */
        float a = (float)(now - UI.preset_t0), s = ease_out_elastic(a / 450.0f) * (a > 850.0f ? 1.0f - (a - 850.0f) / 150.0f : 1.0f);
        int i;
        if (s > 0.05f) {
            disc(120.0f, 82.0f, 40.0f * s + 2.0f, U_LINE);
            disc(120.0f, 82.0f, 40.0f * s, lighten(mood->accent, 200u));
            draw_preset_icon(UI.preset_icon, 120.0f, 82.0f, 1.25f * s);
            for (i = 0; i < UI.preset_n; i++)
                disc(120.0f - 8.0f * (float)(UI.preset_n - 1) + 16.0f * (float)i, 132.0f, i == UI.preset_i ? 5.0f * s : 3.0f * s,
                     i == UI.preset_i ? mood->accent : U_WHITE);
        }
    }
    if (UI.quiet_t0 && now - UI.quiet_t0 < 1500u) {   /* quiet mode on or off: a big moon (or sun) */
        float a = (float)(now - UI.quiet_t0), s = ease_out_back(a / 300.0f) * (a > 1200.0f ? 1.0f - (a - 1200.0f) / 300.0f : 1.0f);
        if (s > 0.05f) {
            disc(120.0f, 100.0f, 34.0f * s, set.quiet ? U_BONE : CC_YELLOW);
            if (set.quiet)
                disc(134.0f, 90.0f, 28.0f * s, HEX(0x1E1B4B));
        }
    }
    if (set.quiet) {                           /* and a little moon in the corner while it lasts */
        disc(226.0f, 14.0f, 7.0f, U_BONE);
        disc(230.0f, 11.0f, 6.0f, mix565(mood->accent, CC_INK, 200u));
    }
    if (UI.unlock_t0 && now - UI.unlock_t0 < 1800u) {   /* a new outfit! */
        float a = (float)(now - UI.unlock_t0), s = ease_out_elastic(a / 500.0f) * (a > 1500.0f ? 1.0f - (a - 1500.0f) / 300.0f : 1.0f);
        int i;
        if (s > 0.05f) {
            for (i = 0; i < 10; i++) {
                float an = (float)i * 0.628f + a / 600.0f;
                star4(120.0f + 46.0f * s * fm_cosf(an), 96.0f + 46.0f * s * fm_sinf(an), 5.0f * s, RAINBOW[i % 6]);
            }
            disc(120.0f, 96.0f, 30.0f * s, lighten(mood->accent, 180u));
            draw_accessory(UI.unlock_hat, 120.0f, 110.0f, 1.6f * s, 0.0f, (float)now);
        }
    }
    if (ui_replaying()) {                      /* a replay: a little tape spinning */
        float an = (float)now / 200.0f;
        ring_a(200.0f, 14.0f, 6.0f, 6.0f, 2.0f, U_WHITE, 256u);
        ring_a(216.0f, 14.0f, 6.0f, 6.0f, 2.0f, U_WHITE, 256u);
        cap(200.0f, 14.0f, 200.0f + 4.0f * fm_cosf(an), 14.0f + 4.0f * fm_sinf(an), 1.0f, U_WHITE);
        cap(216.0f, 14.0f, 216.0f + 4.0f * fm_cosf(an), 14.0f + 4.0f * fm_sinf(an), 1.0f, U_WHITE);
    }
}

/* ----------------------------------------------------------------- replay --- */
static void ui_rec_event(uint32_t k, float vel, int down)
{
    uint32_t i = UI.ev_n++ % 256u;
    UI.ev[i].t = plat_ms();
    UI.ev[i].k = (uint8_t)k;
    UI.ev[i].vel = (uint8_t)(vel * 127.0f);
    UI.ev[i].down = (uint8_t)down;
}
static int ui_replaying(void) { return UI.replaying; }
static void ui_replay_stop(void)
{
    uint32_t k;
    for (k = 0; k < NKEYS; k++)
        if (UI.held_rp >> k & 1u)
            app_key_up(k, 1);
    UI.held_rp = 0;
    UI.replaying = 0;
}
static void ui_replay_toggle(void)
{
    uint32_t now = plat_ms(), n = UI.ev_n < 256u ? UI.ev_n : 256u, i;
    if (UI.replaying) {
        ui_replay_stop();
        return;
    }
    UI.rp_n = 0;
    for (i = 0; i < n; i++) {                  /* the last 10 s, in order */
        uint32_t j = (UI.ev_n - n + i) % 256u;
        if (now - UI.ev[j].t <= 10000u)
            UI.rp[UI.rp_n++] = UI.ev[j];
    }
    if (!UI.rp_n) {                            /* nothing to play back: a little shrug of a sound */
        snd_play(P_TICK, 72.0f, 0.6f);
        return;
    }
    UI.replaying = 1;
    UI.rp_i = 0;
    UI.rp_t0 = now;
    UI.rp_base = UI.rp[0].t;
}
static void ui_replay_tick(void)
{
    uint32_t now = plat_ms();
    if (!UI.replaying)
        return;
    while (UI.rp_i < UI.rp_n && now - UI.rp_t0 >= UI.rp[UI.rp_i].t - UI.rp_base) {
        uint32_t k = UI.rp[UI.rp_i].k;
        if (UI.rp[UI.rp_i].down) {
            UI.held_rp |= 1u << k;
            app_key_down(k, (float)UI.rp[UI.rp_i].vel / 127.0f, 1);
        } else if (UI.held_rp >> k & 1u) {
            UI.held_rp &= ~(1u << k);
            app_key_up(k, 1);
        }
        UI.rp_i++;
    }
    if (UI.rp_i >= UI.rp_n)
        ui_replay_stop();
}

/* ---------------------------------------------------------------- dancers --- */
static void ui_dancers(uint32_t ms) { UI.dance_until = plat_ms() + ms; }
static void ui_draw_dancers(float T)
{
    uint32_t now = plat_ms();
    int i;
    if ((int32_t)(UI.dance_until - now) <= 0)
        return;
    for (i = 0; i < 2; i++) {                  /* two little unicorns hopping on the beat */
        float ph = T / 250.0f + (float)i * 3.14159f, hop = ss_absf(fm_sinf(ph));
        pose_t p = pose_rest(i ? 214.0f : 26.0f, 236.0f, 0.3f);
        p.facing = i ? -1 : 1;
        p.lift = 14.0f * hop;
        p.scale_y = 0.9f + 0.2f * hop;
        p.scale_x = 1.1f - 0.2f * hop;
        p.arm_l = 0.6f + 0.4f * fm_sinf(ph * 2.0f);
        p.mouth_state = MO_GRIN;
        p.eye_state = EY_HAPPY;
        p.accessory_id = (uint8_t)(ACC_PARTY + i);
        p.mane = (uint8_t)(1 + i);
        p.mane_phase = T;
        draw_unicorn(&p, 0);
    }
}

/* ------------------------------------------------------------------ update --- */
static void ui_update(float dt)
{
    uint32_t now = plat_ms();
    int st = ui_idle_state();
    /* the chaos drains after 600 ms without a press: from full in about 2 s */
    if (now - UI.last_press > 600u)
        chaos = fm_maxf(0.0f, chaos - dt * 0.0005f);
    if (chaos < 0.4f)
        UI.over50 = 0;
    if (chaos < 0.8f)
        UI.over90 = 0;
    part_mult = 1.0f + chaos;
    /* the charge: from 300 ms of holding, a rising whine; full after 1.2 s more */
    if (UI.ch_key >= 0) {
        uint32_t held = now - UI.ch_t0;
        if (!(touch_keys() >> UI.ch_key & 1u) && !(UI.held_rp >> UI.ch_key & 1u) && held > 100u && !g_autoplay) {
            ui_charge_cancel();                /* (let go without a release seen) */
        } else if (held >= CHARGE_DELAY_MS) {
            float lvl = fm_minf(1.0f, (float)(held - CHARGE_DELAY_MS) / (float)CHARGE_MS);
            if (!UI.ch_voice) {
                note_t n = snd_defaults(P_WHINE, (float)mood_degree_midi(5, 0), 0.7f);
                n.flags = NF_HOLD;
                UI.ch_voice = snd_note(&n);
            }
            if (lvl >= 1.0f && UI.ch_level < 1.0f) {   /* full: a ding, and it buzzes */
                snd_play(P_CHIME, (float)mood_degree_midi(6, 0), 0.6f);
                fx_trauma(0.15f);
            }
            UI.ch_level = lvl;
        }
    }
    /* snoring, on a slow beat, root and fifth */
    if (st >= IDLE_SLEEP) {
        if ((int32_t)(now - UI.snore_at) >= 0) {
            UI.snore_at = now + 2400u;
            snd_play(P_SNORE, (float)mood_degree_midi(3, (UI.snore_n++ & 1u) ? mood->nscale > 3 ? 3 : 2 : 0), 0.7f);
        }
    } else {
        UI.snore_at = now + 600u;
    }
    ui_replay_tick();
}

static void ui_clear(void)
{
    ui_charge_cancel();
    UI.dance_until = 0;
}

static void ui_init(void)
{
    UI.preset_t0 = plat_ms() - 100000u;
    UI.ch_key = -1;
    UI.last_input = plat_ms();
    UI.mash_ok_at = plat_ms();
}
