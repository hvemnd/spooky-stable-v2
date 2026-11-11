/* SPDX-License-Identifier: MPL-2.0
 * Spooky Stable v2: 1. Unicorn Fart (HOME). The star. Any key: she braces (the freeze), then a rainbow cloud bursts
 * out of her bottom with stars and notes, and she stretches up in relief and wobbles back. Low keys: big, deep, long;
 * high keys small with a squeak on top. 1 in 10 clouds hides a winking skull (more with chaos).
 *   K2 duration 0.3 - 1.5 s   K3 cloud size   K4 pitch-bend depth   K6 cloud style (rainbow / skull / glitter /
 *   slime)   K7 chaos: shake, particles, skulls.   Hold a key: charge; let go full: the MEGA fart (she takes off). */

#define FART_X 150.0f
#define FART_Y 194.0f
enum { CS_RAINBOW, CS_SKULL, CS_GLITTER, CS_SLIME };

typedef struct {
    float x, y, t0, R;
    uint8_t on, skull, style;
} cloud_t;
#define NCLOUD 8

static struct {
    float act_t0, s, mega_t0;
    cloud_t c[NCLOUD];
    uint32_t cur;
} fart;

static const preset_t FART_PRESETS[] = {
    {P_FART_TUBA, IC_HORN}, {P_FART_KAZOO, IC_KAZOO}, {P_FART_DIDGE, IC_PIPE}, {P_FART_SLIDE, IC_SLIDE},
    {P_FART_BASSOON, IC_WAVE}};

static float fart_rear_x(float sc) { return FART_X - 50.0f * sc; }
static float fart_rear_y(float sc) { return FART_Y - 52.0f * sc; }

static void fart_init(void)
{
    int i;
    fart.act_t0 = fart.mega_t0 = -10000.0f;
    for (i = 0; i < NCLOUD; i++)
        fart.c[i].on = 0;
}

static void cloud_spawn(float x, float y, float R, int style, int skull)
{
    cloud_t *c = &fart.c[fart.cur++ % NCLOUD];
    c->x = x;
    c->y = y;
    c->R = R;
    c->t0 = GT.T;
    c->on = 1;
    c->style = (uint8_t)style;
    c->skull = (uint8_t)skull;
}

static void fart_burst(float x, float y, float s, float chaos_k, int style)
{
    static const uint16_t SC[4] = {CC_YELLOW, CC_PINK, U_WHITE, CC_BLUE};
    int n = (int)((8.0f + 10.0f * s) * (1.0f + chaos_k));
    part_burst(PT_STAR, x, y, n, 2.0f, 5.5f, 22, SC[ss_rnd() & 3u], 4);
    part_mood_burst(x, y, n / 2, 1.5f, 4.0f);
    part_burst(PT_NOTE, x, y, 3 + (int)(3.0f * s), 1.0f, 2.5f, 26, RAINBOW[ss_rnd() % 6u], 3);
    part_fan(PT_CLOUD, x, y, 4 + (int)(4.0f * s), 3.14159f, 0.5f, 1.0f, 3.0f, 26, style == CS_SLIME ? CC_SNOT : lighten(CC_PINK, 100u), (int)(5.0f + 6.0f * s));
    if (style == CS_GLITTER)
        part_burst(PT_SPARKLE, x, y, n, 1.0f, 4.0f, 30, CC_GOLD_L, 2);
    if (style == CS_SLIME) {
        int i;
        for (i = 0; i < 6 + (int)(6.0f * s); i++) {
            particle_t *p = part_spawn(PT_SLIME, x + ss_rnds() * 10.0f, y, -1.0f - 2.0f * ss_rndf(), -1.5f - 1.5f * ss_rndf(), 60, 0, 3);
            p->floor = (uint8_t)(FART_Y + 4.0f);
        }
    }
    ring_spawn(x, y, 6.0f + 10.0f * s, 3.5f, 1.0f, 450.0f, 5.0f, CC_PINK);
    ring_spawn(x, y, 4.0f + 6.0f * s, 2.6f, 1.0f, 550.0f, 4.0f, mood->accent);
}

static void fart_key(uint32_t k, float vel)
{
    float s = key_big(k), v = vel_f(vel), dur = (0.3f + 1.2f * kf(K2)) * (0.6f + 0.4f * s), ch = kf(K7);
    int style = kv(K6), skull = style == CS_SKULL ? ss_rndf() < 0.8f : ss_rndf() < 0.1f + 0.4f * ch;
    note_t n = snd_defaults(toy_patch(), (float)mood_key_midi(k, 3), v);
    n.tscale = dur / 0.3f;
    n.bend = 0.3f + 1.7f * kf(K4);
    n.crush = ch * 0.15f;
    snd_note(&n);
    if (k >= 18u)                              /* high keys: a squeak on top */
        snd_play(P_SQUEAK, (float)mood_key_midi(k - 18u, 7), 0.8f);
    fart.act_t0 = GT.T;
    fart.s = s;
    cloud_spawn(fart_rear_x(1.0f) - 6.0f, fart_rear_y(1.0f), (12.0f + 22.0f * s) * (0.55f + 0.9f * kf(K3)) * v, style, skull);
    fart_burst(fart_rear_x(1.0f) - 6.0f, fart_rear_y(1.0f), s, ch, style);
    impact((0.22f + 0.33f * s) * (0.6f + 0.8f * ch) * v, 3);
    fx_flash(CC_YELLOW);
    if (s > 0.6f)
        ui_led_kick();
}

static void fart_release(uint32_t k, uint32_t held) { (void)k; (void)held; }

static void fart_mega(uint32_t k)
{
    int i;
    note_t n = snd_defaults(toy_patch(), (float)mood_degree_midi(2, 0), 1.0f);
    n.tscale = 5.0f;
    n.bend = 1.6f;
    snd_note(&n);
    chord(P_STAB, 5, 0, 0.7f);
    fart.mega_t0 = GT.T;
    fart.act_t0 = GT.T;
    fart.s = 1.0f;
    for (i = 0; i < 3; i++)
        cloud_spawn(fart_rear_x(1.0f) - 10.0f - 30.0f * (float)i, fart_rear_y(1.0f) + 10.0f * (float)i, 34.0f - 4.0f * (float)i, i == 1 ? CS_GLITTER : kv(K6), i == 2);
    fart_burst(fart_rear_x(1.0f) - 10.0f, fart_rear_y(1.0f), 1.0f, 1.0f, kv(K6));
    part_burst(PT_CONFETTI, 120.0f, 100.0f, 40, 2.0f, 7.0f, 40, 0, 4);
    mood_props_blast(fart_rear_x(1.0f), fart_rear_y(1.0f), -1.0f, -0.2f, -0.2f, 9.0f);
    impact(0.9f, 5);
    fx_flash(U_WHITE);
    ui_led_flash_all(300u);
    (void)k;
}

static void fart_update(float dt) { (void)dt; }

static void draw_skull(float x, float y, float s, int wink)
{
    disc(x, y, 10.0f * s + 1.0f, U_LINE);
    box(x - 6.0f * s - 1.0f, y + 4.0f * s, 12.0f * s + 2.0f, 8.0f * s, U_LINE);
    disc(x, y, 10.0f * s, U_WHITE);
    box(x - 6.0f * s, y + 4.0f * s, 12.0f * s, 7.0f * s, U_WHITE);
    disc(x - 4.0f * s, y, 3.0f * s, U_LINE);
    if (wink)
        cap(x + 1.5f * s, y, x + 6.5f * s, y - 1.0f * s, fm_maxf(0.9f * s, 0.8f), U_LINE);
    else
        disc(x + 4.0f * s, y, 3.0f * s, U_LINE);
    tri(x - 1.5f * s, y + 5.0f * s, x + 1.5f * s, y + 5.0f * s, x, y + 3.0f * s, U_LINE);
    disc_a(x - 7.0f * s, y + 3.0f * s, 2.0f * s, CC_PINK, 200u);
    disc_a(x + 7.0f * s, y + 3.0f * s, 2.0f * s, CC_PINK, 200u);
    arc_dots(x, y + 6.0f * s, 3.5f * s, 0.5f, 2.6f, fm_maxf(1.0f * s, 0.8f), U_LINE);   /* a smile, no teeth */
}

static void draw_clouds(float T)
{
    static const uint16_t RB[5] = {CC_PINK, CC_PURPLE, CC_BLUE, CC_GREEN, CC_YELLOW};
    int i, k;
    for (i = 0; i < NCLOUD; i++) {
        cloud_t *c = &fart.c[i];
        float a = T - c->t0, r, al, cx, cy;
        if (!c->on)
            continue;
        if (a > 1600.0f) {
            c->on = 0;
            continue;
        }
        r = 2.0f * c->R * ease_out_back(a / 240.0f);           /* pops out past full size, settles */
        al = a < 250.0f ? 1.0f : 1.0f - (a - 250.0f) / 1350.0f;
        cx = c->x - a * 0.012f - (a < 240.0f ? 0.0f : 0.0f);
        cy = c->y - (a > 200.0f ? (a - 200.0f) * 0.028f : 0.0f);
        if (r < 1.0f)
            continue;
        if (a < 300.0f)                                          /* the core glows */
            glow(cx, cy, r * 1.3f, scale565(c->style == CS_SLIME ? CC_SNOT : CC_YELLOW, 150u), (uint32_t)(255.0f * (1.0f - a / 300.0f)));
        for (k = 0; k < 7; k++) {                                /* puffy bumps round the edge, turning */
            float ang = TAU * (float)k / 7.0f + a / 800.0f, wob = 1.0f + 0.08f * fm_sinf(a / 90.0f + (float)k);
            uint16_t bc = c->style == CS_SLIME ? CC_SNOT : c->style == CS_GLITTER ? lighten(mood->accent, 140u) : CC_PINK;
            disc_a(cx + r * 0.74f * fm_cosf(ang), cy + r * 0.62f * fm_sinf(ang), r * 0.42f * wob, bc, (uint32_t)(225.0f * al));
        }
        for (k = 0; k < 5; k++) {
            uint16_t cc = RB[k];
            if (c->style == CS_SLIME)
                cc = mix565(CC_SNOT, CC_GREEN, (uint32_t)(k * 50));
            else if (c->style == CS_GLITTER)
                cc = k & 1 ? U_WHITE : lighten(mood->accent, (uint32_t)(60 + k * 30));
            else if (c->style == CS_SKULL)
                cc = mix565(RB[k], CC_PURPLE, 120u);
            disc_a(cx, cy, r * (1.0f - 0.17f * (float)k), cc, (uint32_t)(235.0f * al));
        }
        if (c->style == CS_GLITTER && ((uint32_t)(a / 60.0f) & 1u))
            star4(cx + r * 0.4f * fm_cosf(a / 100.0f), cy - r * 0.3f, 4.0f, U_WHITE);
        if (c->skull && a > 260.0f && a < 1100.0f) {
            float pop = ease_out_back((a - 260.0f) / 200.0f);
            draw_skull(cx, cy - 2.0f, fm_maxf(c->R / 26.0f, 0.5f) * pop, a > 640.0f && a < 880.0f);
        }
    }
}

static void fart_draw(float T)
{
    float a = T - fart.act_t0, m = T - fart.mega_t0;
    pose_t p;
    draw_hills(186);
    p = uni_pose(FART_X, FART_Y, 1.0f);
    p.tail_phase = 0.2f;
    p.mouth_state = MO_CLOSED;
    if (a >= 0.0f && a < 1600.0f) {
        if (a < 1.0f && fx_frozen()) {         /* anticipation: braced, bottom out, cheeks puffed, straining */
            p.scale_y *= 0.86f;
            p.scale_x *= 1.12f;
            p.rot_deg = -5.0f;
            p.cheeks = 1.0f;
            p.tail_phase = 1.0f;
            p.eye_state = EY_SQUINT;
            p.mouth_state = MO_STRAIN;
            p.spread = 0.5f;
        } else {                               /* the release: stretched up, overshoot, wobble to rest */
            float e = ease_out_elastic(a / 520.0f), u = 1.0f - e;
            p.scale_y *= 1.0f + 0.16f * u * (0.6f + 0.4f * fart.s);
            p.scale_x *= 1.0f - 0.11f * u * (0.6f + 0.4f * fart.s);
            p.rot_deg = 6.0f * u;
            p.head_rot = 8.0f * ss_pulse(a, 0.0f, 700.0f);
            p.tail_phase = a < 650.0f ? 1.0f : ss_clamp01(1.0f - (a - 650.0f) / 400.0f);
            p.eye_state = a < 450.0f ? EY_WIDE : a < 1200.0f ? EY_HAPPY : EY_OPEN;
            p.mouth_state = a < 450.0f ? MO_O : a < 1200.0f ? MO_GRIN : MO_CLOSED;
            p.blush = ss_pulse(a, 200.0f, 1500.0f);
            p.horn_glow = fm_maxf(0.0f, 1.0f - a / 600.0f) * fart.s;
        }
    }
    if (m >= 0.0f && m < 1500.0f)              /* the mega fart: she takes off on it, and bounces back down */
        p.lift = 60.0f * (m < 400.0f ? ease_out_cubic(m / 400.0f) : 1.0f - ease_out_bounce((m - 400.0f) / 1100.0f));
    uni_draw(&p);
    draw_clouds(T);
}

static void fart_clean(void) { fart_init(); }

static const toy_def_t TOY_FART_DEF = {
    "UNICORN FART", B_HOME, 0, fart_init, 0, fart_key, fart_release, fart_mega, 0, fart_update, fart_draw, fart_clean,
    FART_PRESETS, 5,
    {{KI_CLOCK, 21, 0, 4}, {KI_SIZE, 21, 0, 10}, {KI_BEND, 21, 0, 8}, {KI_STYLE, 4, 1, 0}, {KI_CHAOS, 21, 0, 6}},
};
