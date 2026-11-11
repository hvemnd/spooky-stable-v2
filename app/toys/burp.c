/* SPDX-License-Identifier: MPL-2.0
 * Spooky Stable v2: 4. Unicorn Burp (FX). A gulp at once (her belly starts to swell: 1.35x over 250 ms, overshooting),
 * a held breath, then BURP: her head goes up, mouth wide, bubbles float out, the screen rumbles up and down (inside
 * the belly), and the belly sinks back with a little dip below normal.
 *   K2 duration   K3 how big the belly gets   K4 pitch   K6 bubbles   K7 echo and chaos */

#define BURP_X 140.0f
#define BURP_Y 194.0f
enum { BU_IDLE, BU_INFLATE, BU_HOLD, BU_BURP };

static struct {
    uint8_t phase;
    float t0, inflate_ms, target, s, belly_from;
    uint32_t key;
    float vel;
    float burp_t0;
    uint8_t echoes;
    float echo_at;
} burp;

static const preset_t BURP_PRESETS[] = {
    {P_BURP_TROMBONE, IC_HORN}, {P_BURP_FROG, IC_FROG}, {P_BURP_TUBA, IC_GIANT}, {P_BURP_BABY, IC_TINY},
    {P_BURP_ROBOT, IC_ROBOT}};

static void burp_init(void)
{
    burp.phase = BU_IDLE;
    burp.burp_t0 = -10000.0f;
    burp.target = 1.0f;
    burp.echoes = 0;
}

static float burp_mouth_x(void) { return BURP_X + 63.0f; }
static float burp_mouth_y(void) { return BURP_Y - 70.0f; }

static void burp_key(uint32_t k, float vel)
{
    float s = key_big(k);
    burp.inflate_ms = burp.phase != BU_IDLE ? 120.0f : 250.0f;   /* mashing: a quicker gulp */
    burp.belly_from = burp.phase == BU_BURP ? 1.0f : burp.target;
    burp.phase = BU_INFLATE;
    burp.t0 = GT.T;
    burp.key = k;
    burp.vel = vel;
    burp.s = s;
    burp.target = 1.15f + 0.45f * kf(K3) * (0.7f + 0.3f * s);
    snd_play(P_GULP, (float)mood_key_midi(k, 4), 0.7f);           /* the gulp: at once */
    part_burst(PT_SPARKLE, burp_mouth_x(), burp_mouth_y(), 4, 0.5f, 1.5f, 10, U_WHITE, 1);
}

static void burp_release(uint32_t k, uint32_t held) { (void)k; (void)held; }

static void burp_blow(float big)
{
    int deg = mood_key_degree(burp.key) + (kv(K4) - 10) / 2, n, i;
    float s = burp.s, ch = kf(K7);
    note_t nt = snd_defaults(toy_patch(), (float)mood_degree_midi(big > 1.0f ? 2 : 3, deg), vel_f(burp.vel));
    nt.tscale = (0.5f + 1.5f * kf(K2)) * big;
    nt.send = 0.6f * ch;
    snd_note(&nt);
    burp.phase = BU_BURP;
    burp.burp_t0 = GT.T;
    fx_trauma_v(0.45f + 0.35f * s * big);
    fx_trauma(0.1f * ch);
    fx_hitstop(3);
    fx_impact(2);
    n = (int)((6.0f + 10.0f * kf(K6)) * big);
    part_fan(PT_BUBBLE, burp_mouth_x(), burp_mouth_y(), n, -0.9f, 0.7f, 1.0f, 3.0f, 60, 0, 3 + (int)(3.0f * s));
    for (i = 0; i < n / 3; i++)
        part_spawn(PT_BUBBLE, burp_mouth_x(), burp_mouth_y(), 0.5f + ss_rndf() * 2.0f, -0.5f - ss_rndf(), 70, lighten(mood->accent, 120u), 5 + ss_rndi(4));
    ring_spawn(burp_mouth_x() + 6.0f, burp_mouth_y(), 6.0f, 3.0f, 0.8f, 380.0f, 3.0f, U_WHITE);
    burp.echoes = (uint8_t)(ch * 3.0f);        /* the echo: smaller burps of bubbles after */
    burp.echo_at = GT.T + 220.0f;
}

static void burp_mega(uint32_t k)
{
    burp_key(k, 1.0f);
    burp.target = 1.8f;
    burp.inflate_ms = 160.0f;
    mood_props_blast(burp_mouth_x(), burp_mouth_y(), 1.0f, 0.0f, -1.0f, 8.0f);
    chord(P_STAB, 3, 0, 0.7f);
}

static void burp_update(float dt)
{
    float a = GT.T - burp.t0;
    (void)dt;
    if (burp.phase == BU_INFLATE && a >= burp.inflate_ms) {
        burp.phase = BU_HOLD;
        burp.t0 = GT.T;
    } else if (burp.phase == BU_HOLD && a >= 130.0f) {   /* four frames' held breath */
        burp_blow(burp.target > 1.7f ? 2.0f : 1.0f);
        if (burp.target > 1.7f) {
            impact(0.8f, 5);
            fx_flash(U_WHITE);
            part_mood_burst(burp_mouth_x(), burp_mouth_y(), 30, 2.0f, 7.0f);
        }
    } else if (burp.phase == BU_BURP && GT.T - burp.burp_t0 > 900.0f) {
        burp.phase = BU_IDLE;
        burp.target = 1.0f;
    }
    if (burp.echoes && GT.T >= burp.echo_at) {
        burp.echoes--;
        burp.echo_at = GT.T + 220.0f;
        part_fan(PT_BUBBLE, burp_mouth_x(), burp_mouth_y(), 4, -0.9f, 0.6f, 1.0f, 2.0f, 40, 0, 3);
        fx_trauma_v(0.2f);
    }
}

static float burp_belly(float T)
{
    float a;
    switch (burp.phase) {
    case BU_INFLATE:
        a = (T - burp.t0) / burp.inflate_ms;
        return ss_lerp(burp.belly_from, burp.target, ease_out_back(a));
    case BU_HOLD:
        return burp.target + 0.02f * fm_sinf(T / 25.0f);     /* trembling, about to go */
    case BU_BURP:
        a = (T - burp.burp_t0) / 500.0f;                     /* back down, a dip below normal, settling */
        return 1.0f + (burp.target - 1.0f) * (1.0f - ease_out_elastic(a));
    default:
        return 1.0f;
    }
}

static void burp_draw(float T)
{
    float b = burp_belly(T), a = T - burp.burp_t0;
    pose_t p;
    draw_hills(186);
    p = uni_pose(BURP_X, BURP_Y, 1.0f);
    p.belly = fm_maxf(0.85f, b);
    if (burp.phase == BU_INFLATE || burp.phase == BU_HOLD) {
        p.cheeks = ss_clamp01((b - 1.0f) * 3.0f);
        p.eye_state = EY_WIDE;
        p.mouth_state = MO_CLOSED;
        p.rot_deg = -3.0f * (b - 1.0f) * 3.0f;
        p.scale_y *= 1.0f + 0.03f * fm_sinf(T / 30.0f) * (burp.phase == BU_HOLD);
        p.blush = 0.5f;
    } else if (a >= 0.0f && a < 900.0f) {      /* BURP: head up, mouth wide */
        float e = ease_out_elastic(a / 600.0f);
        p.head_rot = 14.0f * (1.0f - ss_ramp(a, 400.0f, 900.0f));
        p.rot_deg = 6.0f * (1.0f - e);
        p.mouth_state = a < 500.0f ? MO_O : MO_GRIN;
        p.eye_state = a < 500.0f ? EY_SQUINT : EY_HAPPY;
        p.cheeks = 0.0f;
        p.blush = ss_pulse(a, 200.0f, 900.0f);
    }
    uni_draw(&p);
}

static void burp_clean(void) { burp_init(); }

static const toy_def_t TOY_BURP_DEF = {
    "UNICORN BURP", B_FX, 0, burp_init, 0, burp_key, burp_release, burp_mega, 0, burp_update, burp_draw, burp_clean,
    BURP_PRESETS, 5,
    {{KI_CLOCK, 21, 0, 6}, {KI_SIZE, 21, 0, 10}, {KI_PITCH, 21, 0, 10}, {KI_BUBBLES, 21, 0, 6}, {KI_ECHO, 21, 0, 4}},
};
