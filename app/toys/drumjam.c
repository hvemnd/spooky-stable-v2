/* SPDX-License-Identifier: MPL-2.0
 * Spooky Stable v2: 6. Drum Jam (GLO). The unicorn plays a drum kit. The left third of the keys are drums (kick,
 * snare, toms, clap), the middle third cymbals and shakers, the right third silly keys (boing, honk, slide whistle,
 * "ow!") that sing in the mood's scale. Every hit lands on a 16th-note grid at K2's tempo, so she can't play out of
 * time. REC records four bars of her hits and loops them (the keys they play light up in the strip below, a ghost
 * hand on the newest); she plays over her own loop. Hold REC: the loop is gone. PLAY: an auto-drummer at K6's
 * density; she adds fills. A ball bounces across the top on the beat.
 *   K2 tempo 60 - 180   K3 swing 0 - 60 %   K4 the kit's tuning   K6 the auto-drummer's density   K7 crunch and chaos */

#define DJ_X 192.0f
#define DJ_Y 198.0f
enum { DV_KICK, DV_SNARE, DV_TOM1, DV_TOM2, DV_FTOM, DV_HAT, DV_CRASH, DV_COWBELL, DV_CLAP, DV_SHAKER, DV_SILLY, NDV };

static struct {
    float hit[NDV];
    float arm_l, arm_r;
    uint8_t arm_next;
    float key_lit[NKEYS];                      /* when the loop (or she) last played each key */
    uint8_t key_src[NKEYS];
    int8_t ghost_key;
    float ghost_t;
    uint8_t silly_kind;
    float silly_x;
    uint8_t fill_n;
    int32_t last_step;
} dj;

static const preset_t DRUM_PRESETS[] = {{0, IC_DRUM}, {1, IC_BIGDRUM}, {2, IC_TINYDRUM}, {3, IC_POTS}, {4, IC_WAVE}};

static int dv_of(uint32_t k)
{
    switch (DRUM_PATCH[k]) {
    case P_KICK: return DV_KICK;
    case P_SNARE: return DV_SNARE;
    case P_HTOM: return DV_TOM1;
    case P_MTOM: return DV_TOM2;
    case P_LTOM: return DV_FTOM;
    case P_CHAT:
    case P_OHAT: return DV_HAT;
    case P_CRASH: return DV_CRASH;
    case P_COWBELL: return DV_COWBELL;
    case P_CLAP: return DV_CLAP;
    case P_SHAKER: return DV_SHAKER;
    default: return DV_SILLY;
    }
}

static void dj_config(void)
{
    seq_config(60.0f + 6.0f * (float)kv(K2), 0.6f * kf(K3), (float)(kv(K4) - 6), kv(K6), g_autoplay, set.preset[TOY_DRUM],
               0.6f * kf(K7));
}

static void drum_init(void)
{
    int i;
    for (i = 0; i < NDV; i++)
        dj.hit[i] = -10000.0f;
    for (i = 0; i < NKEYS; i++)
        dj.key_lit[i] = -10000.0f;
    dj.arm_l = dj.arm_r = 0.45f;
    dj.ghost_key = -1;
    dj.fill_n = 0;
    dj.last_step = -1;
    dj_config();
}
static void drum_leave(void) { seq_enable(0); }

static void drum_audition(void)
{
    dj_config();
    seq_hit(0, 0.9f);
    seq_hit(9, 0.7f);
}

static void drum_key(uint32_t k, float vel)
{
    seq_hit(k, vel_f(vel));
    if (dv_of(k) == DV_SILLY || k % 2u)        /* a stick goes up at once: the hit lands on the grid */
        dj.arm_r = fm_maxf(dj.arm_r, 0.7f);
    else
        dj.arm_l = fm_maxf(dj.arm_l, 0.7f);
}
static void drum_release(uint32_t k, uint32_t held) { (void)k; (void)held; }
static void drum_mega(uint32_t k)              /* a big fill: toms all the way down, then the crash */
{
    (void)k;
    dj.fill_n = 8;
}

static void drum_hit_visual(uint32_t k, uint8_t src)
{
    int dv = dv_of(k);
    float ch = 1.0f + 1.5f * kf(K7);
    dj.hit[dv] = GT.T;
    dj.key_lit[k] = GT.T;
    dj.key_src[k] = src;
    if (src == HIT_LOOP) {
        dj.ghost_key = (int8_t)k;
        dj.ghost_t = GT.T;
    }
    if (dv == DV_HAT || dv == DV_SHAKER || dv == DV_CRASH || dv == DV_SNARE || dv == DV_TOM1)
        dj.arm_l = 1.0f, dj.arm_next = 1;
    else
        dj.arm_r = 1.0f, dj.arm_next = 0;
    switch (dv) {
    case DV_KICK:
        fx_trauma(0.35f * ch * (src == HIT_LIVE ? 1.0f : 0.6f));
        ui_led_kick();
        part_fan(PT_DUST, 110.0f, 214.0f, 4, -1.57f, 1.2f, 0.5f, 1.5f, 14, 0, 4);
        break;
    case DV_SNARE:
        ui_led_snare();
        part_burst(PT_SPARKLE, 52.0f, 166.0f, (int)(3.0f * ch), 1.0f, 3.0f, 10, U_WHITE, 2);
        break;
    case DV_HAT:
    case DV_SHAKER:
        ui_led_hat();
        break;
    case DV_CRASH:                             /* a fan of sparkles off the cymbal */
        ui_led_flash_all(40u);
        part_fan(PT_SPARKLE, 40.0f, 92.0f, (int)(10.0f * ch), -0.8f, 0.9f, 2.0f, 5.0f, 20, CC_GOLD_L, 2);
        fx_trauma(0.12f);
        break;
    case DV_SILLY: {
        static const uint8_t KIND[9] = {0, 1, 2, 3, 0, 1, 2, 3, 0};
        dj.silly_kind = KIND[(k - 18u) % 9u];
        dj.silly_x = 30.0f + (float)(k - 18u) * 22.0f;
        part_burst(PT_NOTE, dj.silly_x, 70.0f, 3, 1.0f, 2.5f, 24, RAINBOW[k % 6u], 3);
        break;
    }
    default:
        fx_trauma(0.08f * ch);
        break;
    }
    if (kf(K7) > 0.6f && dv == DV_KICK)
        part_mood_burst(110.0f, 180.0f, 6, 1.5f, 4.0f);
}

static void drum_update(float dt)
{
    uint8_t k, src;
    int32_t step = (int32_t)(seq_beat() * 4.0f);
    float r = fm_minf(1.0f, dt / 140.0f);
    dj_config();
    toy_crush = 0.0f;
    while (seq_pop(&k, &src))
        drum_hit_visual(k, src);
    if (dj.fill_n && step != dj.last_step) {   /* the fill: one hit a step */
        static const uint8_t FILL[8] = {5, 5, 3, 3, 1, 1, 0, 13};
        seq_hit(FILL[8 - dj.fill_n], 0.95f);
        dj.fill_n--;
    }
    dj.last_step = step;
    dj.arm_l += (0.42f - dj.arm_l) * r;        /* the sticks come back down to ready */
    dj.arm_r += (0.42f - dj.arm_r) * r;
}

/* ------------------------------------------------------------------ the kit --- */
static float hit_squash(float T, int dv) { float a = T - dj.hit[dv]; return a >= 0.0f && a < 300.0f ? 1.0f - ease_out_elastic(a / 300.0f) : 0.0f; }

static void draw_drum(float x, float y, float rx, float h, float sq, uint16_t shell)
{
    float ry = rx * 0.32f, top = y - h * (1.0f - 0.18f * sq);
    ell(x, y, rx + 1.5f, ry + 1.5f, U_LINE);
    box(x - rx - 1.5f, top, 2.0f * rx + 3.0f, y - top, U_LINE);
    ell(x, y, rx, ry, darken(shell, 60u));
    box(x - rx, top, 2.0f * rx, y - top, shell);
    box(x - rx, top + (y - top) * 0.45f, 2.0f * rx, 2.0f, lighten(shell, 90u));
    ell(x, top, rx + 1.5f, ry + 1.5f, U_LINE);
    ell(x, top, rx * (1.0f + 0.08f * sq), ry * (1.0f - 0.3f * sq), U_WHITE);
    ell_a(x - rx * 0.3f, top - ry * 0.2f, rx * 0.3f, ry * 0.3f, U_BONE, 120u);
}
static void draw_cymbal(float x, float y, float rx, float wob, uint16_t c)
{
    float tilt = wob * 0.35f;
    cap(x, y, x, 214.0f, 1.2f, U_LINE);
    tri(x - rx - 1.0f, y + tilt * rx + 1.0f, x + rx + 1.0f, y - tilt * rx + 1.0f, x, y - 5.0f, U_LINE);
    ell(x, y, rx + 1.0f, rx * 0.22f + 1.0f, U_LINE);
    ell(x, y, rx, rx * 0.22f, c);
    tri(x - rx, y + tilt * rx, x + rx, y - tilt * rx, x, y - 4.0f, c);
    disc(x, y - 2.0f, 2.0f, darken(c, 60u));
    ell_a(x - rx * 0.4f, y - 1.0f, rx * 0.3f, 1.5f, U_WHITE, 160u);
}

static void drum_draw(float T)
{
    float beat = seq_beat(), f = beat - fm_floorf(beat), sq;
    int i, b = (int)beat & 3, ls = seq_loop_state();
    static const float PADX[4] = {48.0f, 96.0f, 144.0f, 192.0f};
    uint16_t shell = mood->accent;
    pose_t p;
    /* a stage */
    mood_draw_ground(204);
    box(0.0f, 200.0f, 240.0f, 4.0f, lighten(mood->ground, 70u));
    /* the bouncing ball metronome: one pad a beat, the first the big one */
    for (i = 0; i < 4; i++) {
        float lit = b == i ? 1.0f - f : 0.0f;
        ell(PADX[i], 44.0f, i == 0 ? 11.0f : 8.0f, 3.0f, mix565(mood->accent, U_WHITE, (uint32_t)(200.0f * lit)));
        if (lit > 0.6f)
            glow(PADX[i], 44.0f, 16.0f, scale565(mood->accent, 160u), (uint32_t)(255.0f * lit));
    }
    {
        int from = b, to = (b + 1) & 3;
        float x = ss_lerp(PADX[from], PADX[to], f), arc = to == 0 ? 40.0f : 24.0f;
        float y = 38.0f - arc * 4.0f * f * (1.0f - f), s = f < 0.1f ? 0.7f + 3.0f * f : f > 0.92f ? 1.0f - (f - 0.92f) * 3.0f : 1.0f;
        disc(x, y + 1.0f, 6.0f + 1.0f, U_LINE);
        ell(x, y + (1.0f - s) * 5.0f, 6.0f / fm_sqrtf(s), 6.0f * s, U_WHITE);
        disc(x - 2.0f, y - 2.0f, 1.6f, lighten(mood->accent, 200u));
    }
    /* the drummer behind her kit, sticks in her front hooves */
    p = uni_pose(DJ_X, DJ_Y, 0.9f);
    p.facing = -1;
    p.arm_l = dj.arm_l;
    p.arm_r = dj.arm_r;
    p.mouth_state = MO_GRIN;
    p.eye_state = fm_sinf(T / 1700.0f) > 0.7f ? EY_HAPPY : EY_OPEN;
    p.head_rot = 4.0f * fm_sinf(TAU * beat * 0.5f);                 /* she nods to the beat */
    p.scale_y *= 1.0f - 0.03f * ss_absf(fm_sinf(TAU * beat * 0.5f));
    p.look_x = -0.7f;
    uni_draw(&p);
    for (i = 0; i < 2; i++) {                                         /* the sticks */
        float hx = uni_hoof[i][0], hy = uni_hoof[i][1], ang = -2.6f - (i ? dj.arm_r : dj.arm_l) * 0.6f;
        cap(hx, hy, hx + 22.0f * fm_cosf(ang), hy + 22.0f * fm_sinf(ang), 2.2f, U_LINE);
        cap(hx, hy, hx + 22.0f * fm_cosf(ang), hy + 22.0f * fm_sinf(ang), 1.4f, CC_GOLD_L);
    }
    /* the kit, in front */
    draw_cymbal(40.0f, 92.0f - 6.0f * hit_squash(T, DV_CRASH), 24.0f, fm_sinf(T / 40.0f) * hit_squash(T, DV_CRASH) * 2.0f, CC_GOLD);
    draw_cymbal(20.0f, 136.0f + 3.0f * hit_squash(T, DV_HAT), 15.0f, 0.0f, CC_GOLD_L);
    draw_cymbal(20.0f, 140.0f, 15.0f, 0.0f, CC_GOLD);
    sq = hit_squash(T, DV_SNARE);
    draw_drum(52.0f, 182.0f, 20.0f, 16.0f, sq, lighten(shell, 60u));
    sq = hit_squash(T, DV_TOM1);
    draw_drum(84.0f, 146.0f, 15.0f, 14.0f, sq, shell);
    sq = hit_squash(T, DV_TOM2);
    draw_drum(122.0f, 144.0f, 16.0f, 15.0f, sq, shell);
    sq = hit_squash(T, DV_FTOM);
    draw_drum(178.0f, 214.0f, 20.0f, 26.0f, sq, darken(shell, 30u));
    sq = hit_squash(T, DV_KICK);                                       /* the kick: a big round face */
    {
        float r = 32.0f * (1.0f + 0.1f * sq), cy = 182.0f + 2.0f * sq;
        disc(104.0f, cy, r + 2.0f, U_LINE);
        disc(104.0f, cy, r, darken(shell, 40u));
        disc(104.0f, cy, r - 5.0f, lighten(shell, 170u));
        star4(104.0f, cy, 12.0f + 4.0f * sq, shell);
        disc(104.0f, cy, 3.0f, U_WHITE);
        ell(80.0f, cy + r - 2.0f, 4.0f, 3.0f, U_LINE);
        ell(128.0f, cy + r - 2.0f, 4.0f, 3.0f, U_LINE);
    }
    {                                                                  /* the cowbell on the kick */
        float sw = fm_sinf(T / 50.0f) * hit_squash(T, DV_COWBELL) * 0.5f;
        quad(98.0f + sw * 6.0f, 138.0f, 110.0f + sw * 6.0f, 138.0f, 113.0f, 150.0f, 95.0f, 150.0f, HEX(0x94A3B8));
        ell(104.0f, 150.0f, 9.0f, 2.5f, HEX(0x64748B));
    }
    if (hit_squash(T, DV_SHAKER) > 0.0f) {                            /* a shaker, shaking */
        float j = fm_sinf(T / 15.0f) * 4.0f;
        ell(150.0f + j, 112.0f, 6.0f, 9.0f, CC_ORANGE);
        ell(150.0f + j, 108.0f, 3.0f, 3.0f, CC_YELLOW);
    }
    if (hit_squash(T, DV_CLAP) > 0.0f) {                              /* two little hands clapping */
        float a = T - dj.hit[DV_CLAP], g = 10.0f * (1.0f - ease_out_back(a / 120.0f));
        disc(150.0f - 6.0f - g, 100.0f, 7.0f, U_LINE);
        disc(150.0f + 6.0f + g, 100.0f, 7.0f, U_LINE);
        disc(150.0f - 6.0f - g, 100.0f, 6.0f, mood->chr);
        disc(150.0f + 6.0f + g, 100.0f, 6.0f, mood->chr);
        star4(150.0f, 88.0f, 6.0f, CC_YELLOW);
    }
    if (T - dj.hit[DV_SILLY] < 500.0f) {                             /* a silly key: its toy pops up */
        float a = T - dj.hit[DV_SILLY], s = ease_out_back(a / 200.0f) * (a > 380.0f ? 1.0f - (a - 380.0f) / 120.0f : 1.0f);
        static const uint8_t IC[4] = {IC_SPRING, IC_HORN, IC_SLIDE, IC_MOUTH};
        if (s > 0.05f)
            draw_preset_icon(IC[dj.silly_kind & 3u], dj.silly_x, 76.0f, 0.8f * s);
    }
    /* the loop strip: 27 little keys; the loop's light up, a ghost hand on the newest; REC: a red dot and how far */
    if (ls != LOOP_NONE) {
        float pos = (float)seq_loop_pos() / 64.0f;
        dr_ox = dr_oy = 0;
        box(0.0f, 224.0f, 240.0f, 16.0f, mix565(CC_INK, mood->accent, 40u));
        for (i = 0; i < NKEYS; i++) {
            float x = 4.0f + (float)i * 8.6f, a = T - dj.key_lit[i];
            int black = i == 1 || i == 3 || i == 5 || i == 8 || i == 10 || i == 13 || i == 15 || i == 17 || i == 20 || i == 22 || i == 25;
            uint16_t c = black ? HEX(0x302848) : HEX(0xE8E4F4);
            if (a >= 0.0f && a < 160.0f)
                c = dj.key_src[i] == HIT_LOOP ? mood->accent : lighten(mood->accent, 120u);
            box(x, 227.0f, 7.0f, 11.0f, c);
        }
        if (ls == LOOP_REC) {
            disc(232.0f, 214.0f, 5.0f, ((uint32_t)(T / 250.0f) & 1u) ? HEX(0xFF3040) : HEX(0x802030));
            box(0.0f, 224.0f, 240.0f * pos, 2.0f, HEX(0xFF3040));
        } else {
            box(240.0f * pos - 1.0f, 224.0f, 3.0f, 3.0f, U_WHITE);
        }
        if (dj.ghost_key >= 0 && T - dj.ghost_t < 400.0f) {          /* the ghost hand */
            float x = 7.5f + (float)dj.ghost_key * 8.6f, y = 214.0f + 3.0f * ease_out_back((T - dj.ghost_t) / 150.0f);
            uint32_t al = (uint32_t)(170.0f * (1.0f - (T - dj.ghost_t) / 400.0f));
            ell_a(x, y, 7.0f, 6.0f, U_WHITE, al);
            for (i = 0; i < 4; i++)
                ell_a(x - 5.0f + (float)i * 3.3f, y - 7.0f + (i == 0 || i == 3 ? 2.0f : 0.0f), 1.6f, 4.0f, U_WHITE, al);
            ell_a(x + 7.0f, y + 1.0f, 3.5f, 1.6f, U_WHITE, al);
        }
    }
}

static void drum_clean(void) { seq_clear(); }

static const toy_def_t TOY_DRUM_DEF = {
    "DRUM JAM", B_GLO, 0, drum_init, drum_leave, drum_key, drum_release, drum_mega, 0, drum_update, drum_draw, drum_clean,
    DRUM_PRESETS, NKITS,
    {{KI_CLOCK, 21, 0, 8}, {KI_SWING, 21, 0, 0}, {KI_PITCH, 13, 0, 6}, {KI_DRUM, 11, 0, 5}, {KI_CHAOS, 21, 0, 0}},
};
