/* SPDX-License-Identifier: MPL-2.0
 * Spooky Stable v2: 5. Unicorn Sneeze (ENV). Hold any key: she breathes in ("ah... ah..."), leaning back, cheeks
 * swelling, horn brightening, a glowing bar rising at the left edge, buzzing. Let go: a silent held breath (three
 * frames), then CHOO: a white cone out of her nose, everything in it blown off the screen and bouncing off the edges,
 * a burst of whatever K6 says, a big shake, and she recoils. The longer the breath, the bigger the blast. A quick tap
 * is a tiny "tchoo".
 *   K2 charge time (to full, 0.5 - 2 s)   K3 blast force   K4 pitch   K6 what flies out (snot / rainbow / confetti /
 *   ducks)   K7 chaos: the crunch, and a torn screen */

#define SNZ_X 96.0f
#define SNZ_Y 194.0f
enum { SN_IDLE, SN_CHARGE, SN_PAUSE, SN_BLAST };
enum { SO_SNOT, SO_RAINBOW, SO_CONFETTI, SO_DUCKS };

static struct {
    uint8_t phase;
    uint32_t held, t0_ms;
    float charge, blast_t0, force;
    uint16_t inhale;
    int8_t pause_frames;
    uint32_t key;
} snz;

static const preset_t SNEEZE_PRESETS[] = {
    {P_SNEEZE_TRUMPET, IC_HORN}, {P_SNEEZE_HONK, IC_MOUTH}, {P_SNEEZE_TINY, IC_TINY}, {P_SNEEZE_GIANT, IC_GIANT}};

static float snz_full_ms(void) { return 500.0f + 1500.0f * kf(K2); }
static float snz_nose_x(void) { return SNZ_X + 66.0f; }
static float snz_nose_y(void) { return SNZ_Y - 78.0f; }

static void snz_stop_inhale(void)
{
    if (snz.inhale)
        snd_off(snz.inhale);
    snz.inhale = 0;
}
static void sneeze_init(void)
{
    snz_stop_inhale();
    snz.phase = SN_IDLE;
    snz.held = 0;
    snz.blast_t0 = -10000.0f;
}
static void sneeze_leave(void) { snz_stop_inhale(); }

static void sneeze_key(uint32_t k, float vel)
{
    (void)vel;
    snz.held |= 1u << k;
    snz.key = k;
    if (snz.phase == SN_CHARGE)
        return;
    if (snz.phase != SN_IDLE && snz.phase != SN_BLAST)
        return;
    {                                          /* the breath in: at once */
        note_t n = snd_defaults(P_INHALE, (float)mood_degree_midi(4, (kv(K4) - 10) / 3), 0.9f);
        n.flags = NF_HOLD;
        n.tscale = snz_full_ms() / 1500.0f;
        snz.inhale = snd_note(&n);
    }
    snz.phase = SN_CHARGE;
    snz.t0_ms = plat_ms();
    snz.charge = 0.0f;
}

static void sneeze_release(uint32_t k, uint32_t held)
{
    (void)held;
    snz.held &= ~(1u << k);
    if (snz.held || snz.phase != SN_CHARGE)
        return;
    snz.charge = fm_maxf(snz.charge, 0.15f);   /* a tap: a tiny one */
    snz_stop_inhale();
    snz.phase = SN_PAUSE;                      /* the held breath: silence, frozen */
    snz.pause_frames = 3;
    fx_hitstop(3);
}

static void sneeze_blast(void)
{
    float c = snz.charge, force = (5.0f + 11.0f * c) * (0.5f + kf(K3)), ch = kf(K7), nx = snz_nose_x(), ny = snz_nose_y();
    int n = 10 + (int)(34.0f * c), what = kv(K6), i;
    note_t nt = snd_defaults(toy_patch(), (float)mood_degree_midi(4, 2 + (kv(K4) - 10) / 2), 0.6f + 0.4f * c);
    nt.tscale = 0.7f + 0.7f * c;
    nt.crush = ch * 0.6f;
    snd_note(&nt);
    snz.phase = SN_BLAST;
    snz.blast_t0 = GT.T;
    snz.force = force;
    impact(0.3f + 0.55f * c, 5);
    if (c > 0.5f)
        fx_flash(U_WHITE);
    if (ch > 0.5f)
        fx_tear(3 + (int)(ch * 4.0f), ch);
    mood_props_blast(nx, ny, 1.0f, -0.15f, 0.8f, force);   /* everything in the cone flies */
    for (i = 0; i < n; i++) {
        float a = -0.15f + 0.55f * ss_rnds(), v = (2.0f + 6.0f * ss_rndf()) * (0.6f + 0.6f * c);
        int type = what == SO_SNOT ? PT_SNOT : what == SO_RAINBOW ? PT_STAR : what == SO_CONFETTI ? PT_CONFETTI : PT_DUCK;
        uint16_t col = what == SO_RAINBOW ? RAINBOW[i % 6] : 0;
        particle_t *p = part_spawn(type, nx, ny, v * fm_cosf(a), v * fm_sinf(a), what == SO_DUCKS ? 90 : 40, col,
                                   what == SO_DUCKS ? 4 + ss_rndi(3) : what == SO_SNOT ? 2 + ss_rndi(3) : 4);
        if (what == SO_SNOT)
            p->floor = (uint8_t)(SNZ_Y + 6.0f);
    }
    part_mood_burst(nx, ny, n / 3, 2.0f, 6.0f);
    ring_spawn(nx, ny, 6.0f, 6.0f, 0.7f, 380.0f, 5.0f, U_WHITE);
    ui_led_flash_all(120u);
}

static void sneeze_mega(uint32_t k)
{
    (void)k;
    snz_stop_inhale();
    snz.charge = 1.0f;
    snz.phase = SN_PAUSE;
    snz.pause_frames = 3;
    fx_hitstop(3);
}

static void sneeze_update(float dt)
{
    (void)dt;
    if (snz.phase == SN_CHARGE) {
        snz.charge = fm_minf(1.0f, (float)(plat_ms() - snz.t0_ms) / snz_full_ms());
        if (!snz.held && !touch_keys())        /* (let go without a release seen: an auto-play key) */
            sneeze_release(snz.key, 0);
    } else if (snz.phase == SN_PAUSE) {        /* the freeze is over (update runs again): CHOO */
        sneeze_blast();
    } else if (snz.phase == SN_BLAST && GT.T - snz.blast_t0 > 900.0f) {
        snz.phase = SN_IDLE;
        snz.charge = 0.0f;
    }
}

static void sneeze_draw(float T)
{
    float c = snz.charge, a = T - snz.blast_t0;
    pose_t p;
    draw_hills(186);
    p = uni_pose(SNZ_X, SNZ_Y, 1.0f);
    if (snz.phase == SN_CHARGE || snz.phase == SN_PAUSE) {   /* breathing in */
        float e = ease_out_cubic(c);
        p.rot_deg = 15.0f * e;
        p.head_rot = 6.0f * e;
        p.cheeks = 1.2f * e > 1.0f ? 1.0f : 1.2f * e;
        p.horn_glow = e;
        p.vibrate = 2.0f * e;
        p.eye_state = c < 0.6f ? EY_SQUINT : EY_WIDE;
        p.mouth_state = MO_O;
        p.scale_y *= 1.0f + 0.05f * e;
        p.scale_x *= 1.0f - 0.03f * e;
        p.tail_phase = 0.6f * e;
    } else if (snz.phase == SN_BLAST && a < 900.0f) {         /* CHOO: snapped forward, then recoiling back */
        float e = ease_out_elastic(a / 700.0f);
        p.x -= 24.0f * c * (1.0f - ease_out_cubic(a / 500.0f)) * (a < 60.0f ? a / 60.0f : 1.0f);
        p.rot_deg = -14.0f * (1.0f - e);
        p.head_rot = -10.0f * (1.0f - e);
        p.eye_state = a < 400.0f ? EY_SQUINT : EY_SPIRAL;
        if (a > 650.0f)
            p.eye_state = EY_HAPPY;
        p.mouth_state = a < 300.0f ? MO_OPEN : MO_GRIN;
        p.scale_x *= 1.0f + 0.12f * (1.0f - e);
        p.scale_y *= 1.0f - 0.1f * (1.0f - e);
        p.blush = 1.0f;
    }
    uni_draw(&p);
    /* the cone: a white fan sweeping out of the nose */
    if (snz.phase == SN_BLAST && a < 260.0f) {
        float nx = snz_nose_x(), ny = snz_nose_y(), len = 40.0f + 240.0f * ease_out_cubic(a / 160.0f), w = len * 0.42f;
        uint32_t al = (uint32_t)(230.0f * (1.0f - a / 260.0f));
        tri_a(nx, ny, nx + len, ny - w - len * 0.15f, nx + len, ny + w - len * 0.15f, U_WHITE, al);
        tri_a(nx, ny, nx + len * 0.8f, ny - w * 0.4f - len * 0.12f, nx + len * 0.8f, ny + w * 0.4f - len * 0.12f, lighten(mood->accent, 180u), al);
        glow(nx + 10.0f, ny, 26.0f, U_WHITE, al);
    }
    /* the breath meter at the left edge: a rising, glowing bar */
    if (snz.phase == SN_CHARGE || snz.phase == SN_PAUSE) {
        float h = 200.0f * c, y = 220.0f - h;
        dr_ox = dr_oy = 0;
        box(3.0f, 20.0f, 10.0f, 200.0f, mix565(mood->accent, CC_INK, 200u));
        box(4.0f, y, 8.0f, h, c >= 1.0f && ((uint32_t)(T / 80.0f) & 1u) ? U_WHITE : mood->accent);
        box(5.0f, y, 2.0f, h, lighten(mood->accent, 140u));
        glow(8.0f, y, 14.0f + 10.0f * c, scale565(mood->accent, 200u), 230u);
    }
}

static void sneeze_clean(void) { sneeze_init(); }

static const toy_def_t TOY_SNEEZE_DEF = {
    "UNICORN SNEEZE", B_ENV, TF_HOLD, sneeze_init, sneeze_leave, sneeze_key, sneeze_release, sneeze_mega, 0, sneeze_update,
    sneeze_draw, sneeze_clean, SNEEZE_PRESETS, 4,
    {{KI_CHARGE, 21, 0, 6}, {KI_FORCE, 21, 0, 10}, {KI_PITCH, 21, 0, 10}, {KI_STYLE, 4, 1, 0}, {KI_CHAOS, 21, 0, 4}},
};
