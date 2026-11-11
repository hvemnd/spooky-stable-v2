/* SPDX-License-Identifier: MPL-2.0
 * Spooky Stable v2: 3. Unicorn Pee (LFO). Hold a key: her horn glows, then a glittering stream arcs into the pool
 * for as long as it is held (half a second at least, eight at most), splashing and rippling, and a flute sings with
 * it. Another key while it runs glides the flute to that note (in the scale). Sometimes a face smiles up from the pool.
 *   K2 pulse (steady .. spurting)   K3 arc height   K4 glide range   K6 stream colour (sunny / rainbow / glitter /
 *   glow)   K7 splash chaos and how big the puddle grows */

#define PEE_X 156.0f
#define PEE_Y 196.0f
#define POOL_Y 210.0f

static struct {
    uint8_t active, tapering;
    uint32_t held;
    float t0, taper_t0, s, emit, reflect_t0, pool, last_ripple;
    uint16_t voice;
    float tint;
} pee;

static const preset_t PEE_PRESETS[] = {
    {P_PEE_FLUTE, IC_PIPE}, {P_PEE_OCARINA, IC_BELLS}, {P_PEE_WHISTLE, IC_SLIDE}, {P_PEE_PANPIPE, IC_WAVE},
    {P_PEE_HUM, IC_KAZOO}};

static uint16_t pee_water(void) { return mix565(HEX(0x4FB8F5), mood->accent, 45u); }

static void pee_stop(void)
{
    if (pee.voice)
        snd_off(pee.voice);
    pee.voice = 0;
    pee.active = 0;
}
static void pee_init(void)
{
    pee_stop();
    pee.held = 0;
    pee.reflect_t0 = -10000.0f;
    pee.pool = 0.0f;
    pee.tint = 0.0f;
}
static void pee_leave(void) { pee_stop(); }

static float pee_midi(uint32_t k) { return (float)mood_key_midi(k, 5); }

static void pee_key(uint32_t k, float vel)
{
    pee.held |= 1u << k;
    pee.s = key_big(k) * vel_f(vel);
    if (!pee.active) {
        note_t n = snd_defaults(toy_patch(), pee_midi(k), 0.85f);
        n.flags = NF_HOLD;
        n.vib = 0.6f * kf(K4);
        n.pulse = kv(K2) ? 1.0f + 7.0f * kf(K2) : 0.0f;
        pee.active = 1;
        pee.t0 = GT.T;
        pee.emit = 0.0f;
        pee.voice = snd_note(&n);
        fx_flash(HEX(0x60C8FF));
    } else {                                   /* a new key while it runs: the flute glides there */
        snd_glide(pee.voice, pee_midi(k));
    }
    pee.tapering = 0;
}
static void pee_release(uint32_t k, uint32_t held)
{
    (void)held;
    pee.held &= ~(1u << k);
}

static void pee_knob(int knob, int value)      /* the pulse and the glide range change a running stream at once */
{
    (void)knob;
    (void)value;
    if (pee.active)
        snd_mod(pee.voice, 0.6f * (float)kv(K4) / 20.0f, kv(K2) ? 1.0f + 7.0f * (float)kv(K2) / 20.0f : 0.0f);
}

static void pee_mega(uint32_t k)               /* a rainbow fountain */
{
    int i;
    (void)k;
    for (i = 0; i < 40; i++) {
        particle_t *p = part_spawn(PT_DROPLET, PEE_X - 50.0f, PEE_Y - 56.0f, -1.0f - 5.0f * ss_rndf(), -4.0f - 4.0f * ss_rndf(), 80,
                                   RAINBOW[i % 6], 3);
        p->floor = (uint8_t)POOL_Y;
    }
    chord(P_CHIME, 5, 0, 0.7f);
    impact(0.5f, 4);
    pee.reflect_t0 = GT.T;
}

static void pee_splash(particle_t *d)
{
    float x = (float)d->x / 16.0f, y = POOL_Y, ch = kf(K7);
    int i, n = 1 + (int)(ch * 3.0f);
    for (i = 0; i < n; i++) {
        particle_t *p = part_spawn(PT_SLIME, x, y - 1.0f, ss_rnds() * (0.6f + ch), -1.0f - ss_rndf() * (1.5f + ch * 2.0f), 9, 0,
                                   2);
        p->color = ss_rnd() & 1u ? U_WHITE : pee_water();
    }
    if (GT.T - pee.last_ripple > 70.0f) {
        pee.last_ripple = GT.T;
        ring_spawn(x, y + 4.0f, 3.0f, 1.2f, 0.3f, 600.0f, 2.0f, lighten(pee_water(), 140u));
    }
}

static void pee_update(float dt)
{
    float el = GT.T - pee.t0, rate;
    int n, i, style = kv(K6);
    part_on_floor = pee_splash;
    if (!pee.active) {
        pee.tint = fm_maxf(0.0f, pee.tint - dt / 300.0f);
        pee.pool = fm_maxf(0.0f, pee.pool - dt / 6000.0f);
        mood_bg_tint(pee_water(), pee.tint * 0.22f);
        return;
    }
    pee.tint = fm_minf(1.0f, pee.tint + dt / 200.0f);
    mood_bg_tint(pee_water(), pee.tint * 0.22f);
    if (el < 200.0f)                           /* charging: the horn glows */
        return;
    if (el - dt < 200.0f) {                    /* the stream starts */
        impact(0.12f, 2);
        part_burst(PT_SPARKLE, PEE_X - 50.0f, PEE_Y - 56.0f, 6, 1.0f, 2.0f, 12, U_WHITE, 2);
    }
    if (!pee.tapering && ((!pee.held && el > 700.0f) || el > 8000.0f)) {
        pee.tapering = 1;
        pee.taper_t0 = GT.T;
    }
    rate = pee.tapering ? 1.0f - (GT.T - pee.taper_t0) / 300.0f : 1.0f;
    if (rate <= 0.0f) {
        pee_stop();
        if (ss_rndf() < 0.35f)                 /* sometimes a friendly face smiles up from the pool */
            pee.reflect_t0 = GT.T;
        return;
    }
    if (kv(K2)) {                              /* the pulse: spurts */
        float q = 0.5f + 0.5f * fm_sinf(TAU * (1.0f + 7.0f * kf(K2)) * el / 1000.0f);
        rate *= 0.15f + 0.85f * q * q;
    }
    pee.pool = fm_minf(1.0f, pee.pool + dt * (0.00004f + 0.00012f * kf(K7)));
    pee.emit += dt / 16.67f * 3.0f * rate;
    n = (int)pee.emit;
    pee.emit -= (float)n;
    for (i = 0; i < n; i++) {
        float s = pee.s, h = 0.5f + 1.2f * kf(K3), bob = fm_sinf(TAU * GT.T / 1000.0f) * 2.0f;
        uint16_t c = style == 1 ? RAINBOW[(uint32_t)(GT.T / 50.0f + (float)i) % 6u] : style == 2 ? ((i & 1) ? U_WHITE : HEX(0x7DD3FC))
                     : style == 3 ? lighten(mood->accent, (uint32_t)(60 + 60 * (i & 1))) : ((i & 1) ? CC_PEE : HEX(0x78E6FF));
        particle_t *p = part_spawn(PT_DROPLET, PEE_X - 48.0f, PEE_Y - 58.0f + bob,
                                   -(0.9f + 1.8f * s + ss_rndf() * 0.3f), -(1.0f + 2.0f * s * h + ss_rndf() * 0.3f) * h, 90, c,
                                   (int)(2.0f + 2.0f * s * rate));
        p->floor = (uint8_t)POOL_Y;
        if (style == 2 && (i & 1))
            part_spawn(PT_SPARKLE, (float)p->x / 16.0f, (float)p->y / 16.0f, (float)p->vx / 40.0f, (float)p->vy / 40.0f, 10, U_WHITE, 1);
    }
}

static void pee_draw(float T)
{
    float a, el = T - pee.t0, pr = 1.0f + 0.45f * pee.pool, wf;
    int i, style = kv(K6);
    pose_t p;
    uint16_t water = pee_water(), deep = darken(water, 70u), light = lighten(water, 120u);
    mood_draw_ground(196);
    /* the glitter waterfall at the left */
    wf = 36.0f;
    box(0.0f, 36.0f, wf, POOL_Y - 26.0f, deep);
    box(wf - 5.0f, 36.0f, 5.0f, POOL_Y - 26.0f, light);
    ell(18.0f, 36.0f, 22.0f, 6.0f, light);
    for (i = 0; i < 12; i++) {
        float sx = 2.0f + (float)i * 3.0f, sy = 36.0f + (float)(((uint32_t)(T * 0.15f) + ss_hash((uint32_t)i) % 170u) % 170u);
        box(sx, sy, 1.5f, 14.0f, (i & 1) ? U_WHITE : light);
        if (((uint32_t)(T / 90.0f) + (uint32_t)i) % 5u == 0u)
            star4(sx, sy + 7.0f, 2.5f, CC_YELLOW);
    }
    /* the pool: it grows with the puddle */
    ell(80.0f, POOL_Y + 8.0f, 96.0f * pr, 22.0f * pr, deep);
    ell(80.0f, POOL_Y + 5.0f, 90.0f * pr, 16.0f * pr, water);
    ell_a(60.0f, POOL_Y, 40.0f * pr, 4.0f, U_WHITE, 70u);
    a = T - pee.reflect_t0;
    if (a >= 0.0f && a < 1600.0f) {            /* the pool's friendly reflection */
        uint32_t al = (uint32_t)(200.0f * ss_clamp01(a / 250.0f) * ss_clamp01((1600.0f - a) / 300.0f));
        ell_a(64.0f, POOL_Y + 8.0f, 15.0f, 8.0f, U_WHITE, al);
        ell_a(59.0f, POOL_Y + 6.0f, 2.0f, 2.5f, CC_INK, al);
        ell_a(69.0f, POOL_Y + 6.0f, 2.0f, 2.5f, CC_INK, al);
        ell_a(64.0f, POOL_Y + 11.0f, 4.0f, 1.5f, CC_INK, al);
        ell_a(56.0f, POOL_Y + 10.0f, 2.5f, 1.2f, CC_PINK, al);
        ell_a(72.0f, POOL_Y + 10.0f, 2.5f, 1.2f, CC_PINK, al);
    }
    p = uni_pose(PEE_X, PEE_Y, 1.0f);
    p.spread = 1.0f;
    if (pee.active) {
        p.horn_glow = el < 200.0f ? el / 200.0f : 0.6f + 0.4f * fm_sinf(T / 80.0f);
        p.eye_state = el < 200.0f ? EY_WIDE : EY_HAPPY;
        p.mouth_state = el < 200.0f ? MO_O : MO_GRIN;
        p.tail_phase = 0.8f;
        p.blush = 0.8f;
        p.rot_deg = el < 200.0f ? 3.0f * el / 200.0f : 3.0f + 1.0f * fm_sinf(T / 300.0f);
        p.scale_y *= 1.0f + 0.02f * fm_sinf(T / 120.0f);
        if (style == 3)
            glow(PEE_X - 50.0f, PEE_Y - 56.0f, 18.0f, scale565(mood->accent, 140u), 200u);
    }
    uni_draw(&p);
    for (i = 0; i < 5; i++)                    /* grass tufts in front */
        tri(140.0f + (float)i * 20.0f, 240.0f, 150.0f + (float)i * 20.0f, 240.0f, 145.0f + (float)i * 20.0f, 224.0f, mood->ground2);
}

static void pee_clean(void)
{
    pee_stop();
    pee.pool = 0.0f;
}

static const toy_def_t TOY_PEE_DEF = {
    "UNICORN PEE", B_LFO, TF_HOLD, pee_init, pee_leave, pee_key, pee_release, pee_mega, pee_knob, pee_update, pee_draw, pee_clean,
    PEE_PRESETS, 5,
    {{KI_CLOCK, 21, 0, 0}, {KI_HEIGHT, 21, 0, 8}, {KI_BEND, 21, 0, 6}, {KI_STYLE, 4, 1, 0}, {KI_CHAOS, 21, 0, 6}},
};
