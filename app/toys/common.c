/* SPDX-License-Identifier: MPL-2.0
 * Spooky Stable v2: what every toy shares: the unicorn's pose plumbing (the wardrobe's hat and mane, the idle
 * affordances, the charge's glow, the impact silhouette), one impact call, and small helpers. */

static rig_t uni_rig;
static float toy_crush;                        /* the toy's own bitcrush (its chaos knob), 0..1 */

static float key_big(uint32_t k) { return 1.0f - (float)(k > 26u ? 26u : k) / 26.0f; }   /* low keys: big */
static float vel_f(float v) { return 0.55f + 0.45f * ss_clamp01(v); }   /* a floor: soft hits still heard */

/* the wardrobe: the hats in the order they unlock (0: the mood's own), and the manes */
static const uint8_t HATS[] = {ACC_NONE, ACC_PARTY, ACC_CROWN, ACC_PROPELLER, ACC_BUNNY, ACC_FLOWERS, ACC_TIARA,
                               ACC_WITCH, ACC_BOW, ACC_HALO, ACC_NIGHTCAP, ACC_CAP, ACC_GOGGLES};
#define NHATS ((int)(sizeof HATS / sizeof HATS[0]))
#define UNLOCK_EVERY 30u                       /* key presses for each new item */
#define NUNLOCK (NHATS - 1 + NMANES - 1)
static int hats_unlocked(void) { return 1 + (set.unlocked + 1) / 2; }   /* items alternate: hat, mane, hat ... */
static int manes_unlocked(void) { return 1 + set.unlocked / 2; }
static int cur_hat(void) { return set.hat == 0 || set.hat >= NHATS ? mood->accessory : HATS[set.hat]; }

/* the unicorn as every toy starts her: the outfit, the pupils on K1, the body previewing K2 */
static pose_t uni_pose(float x, float y, float scale)
{
    pose_t p = pose_rest(x, y, scale);
    float k2 = kf(K2);
    p.accessory_id = (uint8_t)cur_hat();
    p.mane = set.mane;
    p.look_x = ((float)set.vol / 10.0f) * 1.6f - 0.8f;
    p.scale_x = p.scale_y = 0.97f + 0.06f * k2;
    p.mane_phase = GT.T;
    return p;
}

/* and as she is drawn: the idle behaviours, the charge, the secondary motion, the silhouette */
static void uni_finish(pose_t *p)
{
    float c = ui_charge();
    ui_idle_pose(p);
    if (c > 0.0f) {
        p->horn_glow = fm_maxf(p->horn_glow, c);
        p->vibrate += c >= 1.0f ? 2.0f : c;
        p->cheeks = fm_maxf(p->cheeks, c * 0.6f);
        if (p->eye_state == EY_OPEN)
            p->eye_state = c >= 1.0f ? EY_WIDE : EY_SQUINT;
    }
    rig_update(&uni_rig, p, GT.T, GT.fdt);
    if (fx_silhouette())
        p->tint = mix565(mood->accent, U_WHITE, 70u);   /* (the accent: a white silhouette vanishes on a white unicorn) */
}
static void uni_draw(pose_t *p)
{
    uni_finish(p);
    draw_unicorn(p, &uni_rig);
    p->tint = 0;
    ui_draw_idle(p);
}

/* one impact: the shake (proportional to how big the sound is), the freeze, the white silhouette */
static void impact(float trauma, int frames)
{
    fx_trauma(trauma);
    if (frames > 0) {
        fx_hitstop(frames);
        fx_impact(2);
    }
}

/* the toy's current preset's patch */
static int toy_patch(void) { return TOY->presets[set.preset[cur_toy] % TOY->preset_count].patch; }

/* a chord stab in the mood's scale: root, third, fifth degrees (the mood sting, flourishes) */
static void chord(int patch, int octave, int root_deg, float vel)
{
    snd_play(patch, (float)mood_degree_midi(octave, root_deg), vel);
    snd_play(patch, (float)mood_degree_midi(octave, root_deg + 2), vel * 0.8f);
    snd_play(patch, (float)mood_degree_midi(octave, root_deg + 4), vel * 0.8f);
}

/* the cleanup swoosh: a wave of the mood's accent sweeping the screen */
static uint32_t sweep_t0;
static void sweep_start(void) { sweep_t0 = plat_ms() | 1u; }
static void draw_sweep(void)
{
    float a;
    int32_t y;
    if (!sweep_t0)
        return;
    a = (float)(plat_ms() - sweep_t0) / 450.0f;
    if (a >= 1.0f) {
        sweep_t0 = 0;
        return;
    }
    dr_ox = dr_oy = 0;
    for (y = 0; y < 240; y += 2) {
        float x = ease_in_out_sine(a) * 340.0f - 50.0f + 14.0f * fm_sinf((float)y * 0.07f + a * 9.0f);
        hl((int32_t)x - 26, (int32_t)x, y, mood->accent);
        hl((int32_t)x - 26, (int32_t)x, y + 1, mood->accent);
        hl((int32_t)x, (int32_t)x + 6, y, U_WHITE);
        hl((int32_t)x, (int32_t)x + 6, y + 1, U_WHITE);
    }
}

/* little shared drawings */
static void draw_hills(int32_t y0)              /* rolling hills behind the ground, in the mood's floor colours */
{
    ell(60.0f, (float)y0 + 46.0f, 140.0f, 70.0f, mood->ground2);
    ell(200.0f, (float)y0 + 52.0f, 130.0f, 66.0f, mood->ground);
    mood_draw_ground(y0 + 10);
}

#ifndef PROPS
#define PROPS 1                                /* v1's ghost, kept only as a tiny background prop (never a toy) */
#endif
#if PROPS
/* a tiny friendly ghost that giggles and floats away (v1's every-third poop) */
typedef struct {
    float x, y, vx, t0;
    uint8_t on;
} mini_t;
#define NMINI 4
static mini_t minis[NMINI];
static void mini_spawn(float x, float y)
{
    int i;
    for (i = 0; i < NMINI; i++)
        if (!minis[i].on)
            break;
    if (i == NMINI)
        i = ss_rndi(NMINI);
    minis[i].x = x;
    minis[i].y = y;
    minis[i].vx = ss_rndf() < 0.5f ? -0.7f : 0.7f;
    minis[i].t0 = GT.T;
    minis[i].on = 1;
    snd_play(P_GIGGLE, quantize_to_scale(86.0f + (float)ss_rndi(5)), 0.8f);
}
static void minis_update(float dt)
{
    int i;
    for (i = 0; i < NMINI; i++)
        if (minis[i].on) {
            minis[i].y -= 0.9f * dt / 16.67f;
            minis[i].x += minis[i].vx * dt / 16.67f;
            if (GT.T - minis[i].t0 > 2400.0f)
                minis[i].on = 0;
        }
}
static void minis_draw(void)
{
    int i;
    for (i = 0; i < NMINI; i++) {
        float a, r, x, y;
        if (!minis[i].on)
            continue;
        a = GT.T - minis[i].t0;
        r = 9.0f * ss_clamp01(a / 150.0f) * (a > 2000.0f ? 1.0f - (a - 2000.0f) / 400.0f : 1.0f);
        x = minis[i].x + fm_sinf(a / 180.0f) * 6.0f;
        y = minis[i].y;
        if (r < 1.0f)
            continue;
        disc(x, y, r + 1.0f, HEX(0x7864D2));
        box(x - r - 1.0f, y, 2.0f * r + 2.0f, r * 0.9f, HEX(0x7864D2));
        disc(x, y, r, HEX(0xFAFAFF));
        box(x - r, y, 2.0f * r, r * 0.9f, HEX(0xFAFAFF));
        disc(x - r * 0.36f, y - r * 0.05f, r * 0.16f, CC_INK);
        disc(x + r * 0.36f, y - r * 0.05f, r * 0.16f, CC_INK);
        ell(x, y + r * 0.35f, r * 0.14f, r * 0.12f, CC_INK);
        disc_a(x - r * 0.6f, y + r * 0.2f, r * 0.15f, CC_PINK, 200u);
        disc_a(x + r * 0.6f, y + r * 0.2f, r * 0.15f, CC_PINK, 200u);
    }
}
static void minis_clear(void)
{
    int i;
    for (i = 0; i < NMINI; i++)
        minis[i].on = 0;
}
#else
static void mini_spawn(float x, float y) { (void)x; (void)y; }
static void minis_update(float dt) { (void)dt; }
static void minis_draw(void) {}
static void minis_clear(void) {}
#endif
