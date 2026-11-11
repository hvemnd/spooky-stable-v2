/* SPDX-License-Identifier: MPL-2.0
 * Spooky Stable v2: motion and impact (fx.h). */

/* ------------------------------------------------------------------ easing --- */
static float ease_out_quad(float t)
{
    t = ss_clamp01(t);
    return 1.0f - (1.0f - t) * (1.0f - t);
}
static float ease_out_cubic(float t)
{
    float u = 1.0f - ss_clamp01(t);
    return 1.0f - u * u * u;
}
static float ease_out_back(float t)
{
    const float c1 = 1.70158f, c3 = c1 + 1.0f;
    float u = ss_clamp01(t) - 1.0f;
    return 1.0f + c3 * u * u * u + c1 * u * u;
}
static float ease_out_elastic(float t)                         /* amplitude 0.4 past the end, period 0.3 */
{
    t = ss_clamp01(t);
    if (t <= 0.0f || t >= 1.0f)
        return t;
    return 1.0f + 0.4f * fm_exp2f(-10.0f * t) * fm_sinf((t - 0.075f) * TAU / 0.3f) * 2.5f;
}
static float ease_in_quad(float t)
{
    t = ss_clamp01(t);
    return t * t;
}
static float ease_in_out_sine(float t) { return 0.5f - 0.5f * fm_cosf(3.14159265f * ss_clamp01(t)); }
static float ease_out_bounce(float t)
{
    const float n1 = 7.5625f, d1 = 2.75f;
    t = ss_clamp01(t);
    if (t < 1.0f / d1)
        return n1 * t * t;
    if (t < 2.0f / d1) {
        t -= 1.5f / d1;
        return n1 * t * t + 0.75f;
    }
    if (t < 2.5f / d1) {
        t -= 2.25f / d1;
        return n1 * t * t + 0.9375f;
    }
    t -= 2.625f / d1;
    return n1 * t * t + 0.984375f;
}

/* ------------------------------------------------------------------- state --- */
#define FX_FLASH_GAP_MS 400u                   /* never two full-screen flashes closer: under 3 Hz however fast */
#define FX_IMPACT_GAP_MS 340u
#define FX_HITSTOP_GAP_MS 220u                 /* mashing must never freeze the toy: a rest between two hitstops */

typedef struct {
    float x, y, r, vr, sq, life, life0, th;
    uint16_t c;
} ring_t;
#define NRING 16

static struct {
    float trauma, trauma_v;
    uint32_t frame;
    int hitstop;
    uint32_t hitstop_end_ms;
    uint32_t hitstops;                         /* since boot (the tests) */
    int impact;
    uint32_t impact_ok_at;
    uint32_t flash_ok_at, flashes, flash_last, flash_gap;   /* gap: the shortest seen between two (the tests) */
    uint8_t flash_frames, invert_frames, tear_frames;
    uint16_t flash_c;
    float tear_amt;
    /* transition */
    int tr_kind;
    uint32_t tr_t0;
    float tr_cx, tr_cy;
    uint16_t tr_c;
    uint8_t tr_on, tr_inverted;
    ring_t rings[NRING];
    uint32_t ring_cur;
} FX;

static void fx_trauma(float amount)
{
    FX.trauma = fm_minf(1.0f, FX.trauma + amount);
}
static void fx_trauma_v(float amount) { FX.trauma_v = fm_minf(1.0f, FX.trauma_v + amount); }

static void fx_hitstop(int frames)
{
    uint32_t now = plat_ms();
    if (FX.hitstop > 0 || (int32_t)(now - FX.hitstop_end_ms) < (int32_t)FX_HITSTOP_GAP_MS)
        return;                                /* (a frame already frozen, or one just ended: mashing stays live) */
    FX.hitstop = frames;
    FX.hitstops++;
}
static int fx_frozen(void) { return FX.hitstop > 0; }

static void fx_impact(int frames)
{
    uint32_t now = plat_ms();
    if ((int32_t)(now - FX.impact_ok_at) < 0)
        return;
    FX.impact_ok_at = now + FX_IMPACT_GAP_MS;
    FX.impact = frames;
}
static int fx_silhouette(void) { return FX.impact > 0; }

static int fx_flash_ok(void)
{
    uint32_t now = plat_ms();
    if ((int32_t)(now - FX.flash_ok_at) < 0)
        return 0;
    FX.flash_ok_at = now + FX_FLASH_GAP_MS;
    if (FX.flashes && now - FX.flash_last < FX.flash_gap)
        FX.flash_gap = now - FX.flash_last;
    FX.flash_last = now;
    FX.flashes++;
    return 1;
}
static void fx_flash(uint16_t c)
{
    if (fx_flash_ok()) {
        FX.flash_c = c;
        FX.flash_frames = 1;                   /* never more than one frame (design 11) */
    }
}
static void fx_invert(void)
{
    if (fx_flash_ok())
        FX.invert_frames = 1;
}
static void fx_tear(int frames, float amount)
{
    if (frames > FX.tear_frames)
        FX.tear_frames = (uint8_t)frames;
    FX.tear_amt = fm_maxf(FX.tear_amt, amount);
}

static void ring_spawn(float x, float y, float r, float vr, float sq, float life_ms, float th, uint16_t c)
{
    ring_t *g = &FX.rings[FX.ring_cur++ % NRING];
    g->x = x;
    g->y = y;
    g->r = r;
    g->vr = vr;
    g->sq = sq;
    g->life = g->life0 = life_ms;
    g->th = th;
    g->c = c;
}

static void fx_transition(int kind, float cx, float cy, uint16_t c)
{
    FX.tr_kind = kind % NTRANS;
    FX.tr_t0 = plat_ms();
    FX.tr_cx = cx;
    FX.tr_cy = cy;
    FX.tr_c = c;
    FX.tr_on = 1;
    FX.tr_inverted = 0;
}
#define FX_TR_MS 220u                          /* 150 - 250 ms: she never waits */
static int fx_in_transition(void) { return FX.tr_on; }

static void fx_clear(void)
{
    uint32_t i;
    for (i = 0; i < NRING; i++)
        FX.rings[i].life = 0.0f;
    FX.trauma = FX.trauma_v = 0.0f;
}

static void fx_update(float dt)
{
    uint32_t i;
    float k = dt / 33.3f;
    FX.trauma = fm_maxf(0.0f, FX.trauma - 0.04f * k);
    FX.trauma_v = fm_maxf(0.0f, FX.trauma_v - 0.03f * k);
    for (i = 0; i < NRING; i++) {
        ring_t *g = &FX.rings[i];
        if (g->life <= 0.0f)
            continue;
        g->r += g->vr * dt / 16.67f;
        g->life -= dt;
    }
}

/* every frame: hitstop's count, the impact, and the shake's offset (trauma squared: quadratic falloff) */
static void fx_frame(void)
{
    float s = FX.trauma * FX.trauma, v = FX.trauma_v * FX.trauma_v;
    FX.frame++;
    if (FX.hitstop > 0 && --FX.hitstop == 0)
        FX.hitstop_end_ms = plat_ms();
    if (FX.impact > 0)
        FX.impact--;
    dr_ox = (int32_t)(s * 8.0f * ss_noise(FX.frame));
    dr_oy = (int32_t)(s * 8.0f * ss_noise(FX.frame + 37u) + v * 7.0f * fm_sinf((float)FX.frame * 0.9f));
    if (dr_ox > 10)
        dr_ox = 10;
    if (dr_ox < -10)
        dr_ox = -10;
    if (dr_oy > 10)
        dr_oy = 10;
    if (dr_oy < -10)
        dr_oy = -10;
}

static void fx_draw_world(void)
{
    uint32_t i;
    for (i = 0; i < NRING; i++) {
        ring_t *g = &FX.rings[i];
        if (g->life <= 0.0f)
            continue;
        ring_a(g->x, g->y, g->r, g->r * g->sq, g->th, g->c, (uint32_t)(256.0f * ss_clamp01(g->life / g->life0)));
    }
}

/* ------------------------------------------------------------- post effects --- */
static void post_wash(uint16_t c, uint32_t a)        /* the one-frame flash (at most one in 400 ms) */
{
    uint32_t i;
    for (i = 0; i < 240u * 240u; i++)
        cv_px[i] = swap16(mix565(swap16(cv_px[i]), c, a));
}
static void post_invert(void)
{
    uint32_t i;
    for (i = 0; i < 240u * 240u; i++)
        cv_px[i] ^= 0xFFFFu;
}
static void post_tear(uint32_t seed, float amt)       /* bands of rows torn sideways, a few colour-swapped */
{
    static uint16_t tmp[240];
    int32_t b, nb = 3 + (int32_t)(amt * 6.0f);
    for (b = 0; b < nb; b++) {
        uint32_t h = ss_hash(seed * 31u + (uint32_t)b);
        int32_t y0 = (int32_t)(h % 230u), hgt = 3 + (int32_t)((h >> 8) % 14u);
        int32_t dx = (int32_t)(((int32_t)((h >> 16) % 81u) - 40) * amt), y, x;
        if (dx == 0)
            dx = 3;
        for (y = y0; y < y0 + hgt && y < 240; y++) {
            uint16_t *row = &cv_px[y * 240];
            for (x = 0; x < 240; x++)
                tmp[x] = row[(x - dx + 480) % 240];
            for (x = 0; x < 240; x++)
                row[x] = (h & 0x1000000u) && amt >= 0.5f ? (uint16_t)(tmp[x] ^ 0x1FF8u) : tmp[x];
        }
    }
}

/* the transitions reveal the new toy from under a cover (no second framebuffer: the cover is drawn over the new
 * frame, shrinking): iris from the button's corner, portal ring, page tear, clock spin, pixel dissolve, glitch */
static void tr_rows_outside_circle(float cx, float cy, float r, uint16_t c)
{
    int32_t y;
    for (y = 0; y < 240; y++) {
        float dy = (float)y - cy, w2 = r * r - dy * dy;
        if (w2 <= 0.0f) {
            hl(0, 239, y, c);
        } else {
            int32_t w = (int32_t)fm_sqrtf(w2), x0 = (int32_t)cx - w, x1 = (int32_t)cx + w;
            hl(0, x0 - 1, y, c);
            hl(x1 + 1, 239, y, c);
        }
    }
}
static void fx_draw_transition(void)
{
    float t = (float)(plat_ms() - FX.tr_t0) / (float)FX_TR_MS, e;
    int32_t y, x;
    uint16_t c = FX.tr_c;
    if (t >= 1.0f) {
        FX.tr_on = 0;
        return;
    }
    e = ease_out_cubic(t);
    dr_ox = dr_oy = 0;
    switch (FX.tr_kind) {
    case TR_IRIS: {
        float r = 345.0f * e;
        tr_rows_outside_circle(FX.tr_cx, FX.tr_cy, r, c);
        ring_a(FX.tr_cx, FX.tr_cy, r + 4.0f, r + 4.0f, 6.0f, lighten(c, 120u), 256u);
        break;
    }
    case TR_PORTAL: {
        float r = 175.0f * e;
        tr_rows_outside_circle(120.0f, 120.0f, r, c);
        ring_a(120.0f, 120.0f, r + 8.0f, r + 8.0f, 9.0f, lighten(c, 90u), 256u);
        ring_a(120.0f, 120.0f, r + 2.0f, r + 2.0f, 3.0f, U_WHITE, 256u);
        break;
    }
    case TR_TEAR:                                     /* a zigzag edge sweeps right to left, the old page torn away */
        for (y = 0; y < 240; y++) {
            int32_t zz = (int32_t)((y / 6) & 1 ? (y % 6) * 3 : (6 - y % 6) * 3);
            x = (int32_t)(e * 270.0f) - 15 + zz;
            hl(0, 239 - x, y, c);
            hl(239 - x - 2, 239 - x + 1, y, U_WHITE);
        }
        break;
    case TR_SPIN: {                                   /* a clock wipe: the cover is the sector not yet swept */
        float a1 = e * TAU;
        for (y = 0; y < 240; y += 2)
            for (x = 0; x < 240; x += 8) {
                /* the angle from 12 o'clock, clockwise: atan by its octant (cheap enough at 8 x 2 px cells) */
                float dx = (float)x + 4.0f - 120.0f, dy = (float)y + 1.0f - 120.0f, ax = ss_absf(dx), ay = ss_absf(dy);
                float m = ax > ay ? ay / fm_maxf(ax, 0.001f) : ax / fm_maxf(ay, 0.001f);
                float at = m * (0.7853982f + 0.273f * (1.0f - m));       /* atan(m), 0 .. pi/4 */
                float th = ax > ay ? 1.5707963f - at : at;               /* from the vertical axis */
                if (dy > 0.0f)
                    th = 3.14159265f - th;
                if (dx < 0.0f)
                    th = TAU - th;
                if (th > a1) {
                    hl(x, x + 7, y, c);
                    hl(x, x + 7, y + 1, c);
                }
            }
        break;
    }
    case TR_DISSOLVE:                                 /* 16 px blocks drop away in a scrambled order */
        for (y = 0; y < 15; y++)
            for (x = 0; x < 15; x++)
                if ((float)(ss_hash((uint32_t)(y * 15 + x) + 77u) & 255u) > 255.0f * e) {
                    int32_t r;
                    for (r = 0; r < 16; r++)
                        hl(x * 16, x * 16 + 15, y * 16 + r, (x + y) & 1 ? c : lighten(c, 40u));
                }
        break;
    default:                                          /* glitch: torn rows, and one inverted frame at the start */
        post_tear(plat_ms() / 30u, 1.0f - e);
        if (!FX.tr_inverted) {
            FX.tr_inverted = 1;
            if (fx_flash_ok())
                post_invert();
        }
        break;
    }
}

static void fx_draw_post(void)
{
    dr_ox = dr_oy = 0;
    if (FX.flash_frames) {
        FX.flash_frames--;
        post_wash(FX.flash_c, 110u);
    }
    if (FX.invert_frames) {
        FX.invert_frames--;
        post_invert();
    }
    if (FX.tear_frames) {
        FX.tear_frames--;
        post_tear(FX.frame, FX.tear_amt);
        if (!FX.tear_frames)
            FX.tear_amt = 0.0f;
    }
    if (FX.tr_on)
        fx_draw_transition();
}
