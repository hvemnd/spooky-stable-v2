/* SPDX-License-Identifier: MPL-2.0
 * Spooky Stable v2: the moods (mood.h). */

/* ------------------------------------------------------------------ scales --- */
static const int8_t SC_MAJPENT[5] = {0, 2, 4, 7, 9};
static const int8_t SC_MINPENT[5] = {0, 3, 5, 7, 10};
static const int8_t SC_BLUES[6] = {0, 3, 5, 6, 7, 10};
static const int8_t SC_WHOLE[6] = {0, 2, 4, 6, 8, 10};
static const int8_t SC_LYDIAN[7] = {0, 2, 4, 6, 7, 9, 11};
static const int8_t SC_PHRYG[7] = {0, 1, 3, 5, 7, 8, 10};
static const int8_t SC_HIRA[5] = {0, 2, 3, 7, 8};
static const int8_t SC_CHROM[12] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11};

/* the table: design 2.5 exactly for name, scale, gradient, accent, character colour, particles, accessory; the root
 * (pitch class) is chosen per mood so the worlds also sit in different keys */
static const mood_t MOODS[NMOODS] = {
    {"SUNNY MEADOW", SC_MAJPENT, 5, 0, 0x87CEEB, 0xC8F0A0, HEX(0xFF6B9D), HEX(0xFFFFFF), HEX(0x6CC24A), HEX(0x4E9F35),
     HEX(0xFFFFFF), HEX(0xFFE066), 0 /* PT_STAR */, 0 /* ACC_FLOWERS */},
    {"MOONLIGHT", SC_MINPENT, 5, 9, 0x1E1B4B, 0x3B2F6B, HEX(0x7DD3FC), HEX(0xE0E7FF), HEX(0x2A2558), HEX(0x1E1A44),
     HEX(0xFDE68A), HEX(0xC7D2FE), 13 /* PT_CRESCENT */, 1 /* ACC_NIGHTCAP */},
    {"SWAMP STINK", SC_BLUES, 6, 4, 0x4A5D23, 0x2B3A12, HEX(0xA3E635), HEX(0xD9F99D), HEX(0x3A2E14), HEX(0x2A220E),
     HEX(0x8B5A2B), HEX(0x6B8E23), 4 /* PT_FLY */, 2 /* ACC_CAP */},
    {"DREAMY FLOAT", SC_WHOLE, 6, 2, 0xE9D5FF, 0xFBCFE8, HEX(0xC084FC), HEX(0xFFFFFF), HEX(0xF5C2E7), HEX(0xE9A8D8),
     HEX(0xFFFFFF), HEX(0xDDD6FE), 14 /* PT_ORB */, 3 /* ACC_HALO */},
    {"SPARKLE CASTLE", SC_LYDIAN, 7, 5, 0xFBCFE8, 0xFDE68A, HEX(0xF59E0B), HEX(0xFFFFFF), HEX(0xF4C76A), HEX(0xE0A84A),
     HEX(0xF9A8D4), HEX(0xC4B5FD), 5 /* PT_HEART */, 4 /* ACC_TIARA */},
    {"FRIENDLY SPOOKY", SC_PHRYG, 7, 4, 0xF97316, 0x4C1D95, HEX(0xA855F7), HEX(0xFDE68A), HEX(0x2E1065), HEX(0x1E0B45),
     HEX(0x3B1A5C), HEX(0xFB923C), 6 /* PT_BAT */, 5 /* ACC_WITCH */},
    {"SAKURA", SC_HIRA, 5, 9, 0xFCE7F3, 0xFBCFE8, HEX(0xF472B6), HEX(0xFFFFFF), HEX(0xE9A3C4), HEX(0xD987B0),
     HEX(0x7C4A3A), HEX(0xF9A8D4), 7 /* PT_PETAL */, 6 /* ACC_BOW */},
    {"GLITCH", SC_CHROM, 12, 0, 0x000000, 0x111827, HEX(0x22D3EE), HEX(0xFFFFFF), HEX(0x0B1220), HEX(0x070B14),
     HEX(0x22D3EE), HEX(0xF0ABFC), 9 /* PT_PIXEL */, 7 /* ACC_GOGGLES */},
};

/* ------------------------------------------------------------------ pitch --- */
static int floordiv(int a, int b) { return a >= 0 ? a / b : -((-a + b - 1) / b); }

static int mood_degree_midi(int octave, int deg)
{
    int n = mood->nscale, o = floordiv(deg, n), d = deg - o * n;
    return 12 * (octave + o) + mood->root + mood->scale[d];
}
static int mood_key_degree(uint32_t k)
{
    int n = mood->nscale, span = n > 7 ? 25 : 2 * n + 1;          /* two octaves, both ends */
    return (int)(k > 26u ? 26u : k) * span / 27;
}
static int mood_key_midi(uint32_t k, int octave) { return mood_degree_midi(octave, mood_key_degree(k)); }

static float quantize_to_scale(float midi)
{
    int m = (int)(midi + 0.5f), best = m, bd = 99, i, o;
    for (o = -1; o <= 1; o++)
        for (i = 0; i < mood->nscale; i++) {
            int base = floordiv(m - mood->root, 12) + o, cand = 12 * base + mood->root + mood->scale[i], d = cand - m;
            if (d < 0)
                d = -d;
            if (d < bd) {
                bd = d;
                best = cand;
            }
        }
    return (float)best;
}

static uint16_t mood_mane(int i) { return mix565(RAINBOW[i % 6], mood->accent, mood_i == MOOD_SUNNY ? 0u : 90u); }

/* -------------------------------------------------------------------- sky --- */
/* The sky is one column of 240 rows, each a 4-pixel ordered-dither pattern (4x4 Bayer), built when the mood or the
 * wash changes and blitted as plain row fills every frame: smooth 240 px gradients in RGB565, no per-pixel maths. */
static const uint8_t BAYER4[4][4] = {{0, 8, 2, 10}, {12, 4, 14, 6}, {3, 11, 1, 9}, {15, 7, 13, 5}};
static uint16_t bg_pat[240][4] PLAT_POOL;      /* byte-swapped, ready for the canvas */
static struct {
    float from_t[3], from_b[3], to_t[3], to_b[3];
    uint32_t fade_t0;
    uint8_t fading;
    uint16_t tint;
    float tint_amt;
    uint32_t key;                              /* what the cached column was built from */
} sky;

static void rgb_of(uint32_t h, float *c)
{
    c[0] = (float)((h >> 16) & 255u);
    c[1] = (float)((h >> 8) & 255u);
    c[2] = (float)(h & 255u);
}

static void sky_build(const float *t, const float *b)
{
    float tr = (float)(sky.tint >> 11) * (255.0f / 31.0f), tg = (float)((sky.tint >> 5) & 63u) * (255.0f / 63.0f);
    float tb = (float)(sky.tint & 31u) * (255.0f / 31.0f), ta = sky.tint_amt;
    int32_t y, x;
    for (y = 0; y < 240; y++) {
        float u = (float)y / 239.0f, r = ss_lerp(t[0], b[0], u), g = ss_lerp(t[1], b[1], u), bl = ss_lerp(t[2], b[2], u);
        r = ss_lerp(r, tr, ta);
        g = ss_lerp(g, tg, ta);
        bl = ss_lerp(bl, tb, ta);
        for (x = 0; x < 4; x++) {
            float th = ((float)BAYER4[y & 3][x] + 0.5f) / 16.0f;
            int32_t r5 = (int32_t)(r * (31.0f / 255.0f) + th), g6 = (int32_t)(g * (63.0f / 255.0f) + th);
            int32_t b5 = (int32_t)(bl * (31.0f / 255.0f) + th);
            r5 = r5 > 31 ? 31 : r5;
            g6 = g6 > 63 ? 63 : g6;
            b5 = b5 > 31 ? 31 : b5;
            bg_pat[y][x] = swap16((uint32_t)(r5 << 11 | g6 << 5 | b5));
        }
    }
}

static void mood_bg_tint(uint16_t c, float amt)
{
    sky.tint = c;
    sky.tint_amt = ss_clamp01(amt);
}

static void mood_draw_sky(void)
{
    float t[3], b[3], f = 1.0f;
    uint32_t key, i;
    int32_t y, x;
    if (sky.fading) {
        f = ss_clamp01((float)(plat_ms() - sky.fade_t0) / 300.0f);
        if (f >= 1.0f)
            sky.fading = 0;
    }
    for (i = 0; i < 3; i++) {
        t[i] = ss_lerp(sky.from_t[i], sky.to_t[i], ease_in_out_sine(f));
        b[i] = ss_lerp(sky.from_b[i], sky.to_b[i], ease_in_out_sine(f));
    }
    key = (uint32_t)(f * 255.0f) | (uint32_t)mood_i << 8 | (uint32_t)(sky.tint_amt * 64.0f) << 12 | (uint32_t)sky.tint << 16;
    if (key != sky.key || sky.fading) {
        sky.key = key;
        sky_build(t, b);
    }
    for (y = 0; y < 240; y++) {
        uint16_t *row = &cv_px[y * 240], *p = bg_pat[y];
        for (x = 0; x < 240; x += 4) {
            row[x] = p[0];
            row[x + 1] = p[1];
            row[x + 2] = p[2];
            row[x + 3] = p[3];
        }
    }
}

static void mood_set(int i, int fade)
{
    float t[3], b[3];
    int k;
    i = ((i % NMOODS) + NMOODS) % NMOODS;
    if (!mood)
        fade = 0;
    if (fade) {                         /* from wherever the sky is now */
        float f = sky.fading ? ss_clamp01((float)(plat_ms() - sky.fade_t0) / 300.0f) : 1.0f;
        for (k = 0; k < 3; k++) {
            t[k] = ss_lerp(sky.from_t[k], sky.to_t[k], f);
            b[k] = ss_lerp(sky.from_b[k], sky.to_b[k], f);
        }
    }
    mood_i = (uint8_t)i;
    mood = &MOODS[i];
    rgb_of(mood->top, sky.to_t);
    rgb_of(mood->bot, sky.to_b);
    if (fade) {
        for (k = 0; k < 3; k++) {
            sky.from_t[k] = t[k];
            sky.from_b[k] = b[k];
        }
        sky.fade_t0 = plat_ms();
        sky.fading = 1;
    } else {
        for (k = 0; k < 3; k++) {
            sky.from_t[k] = sky.to_t[k];
            sky.from_b[k] = sky.to_b[k];
        }
        sky.fading = 0;
    }
    sky.key = 0xFFFFFFFFu;
    mood_props_home();
}

static void mood_init(void) { mood_set(0, 0); }

/* a floor band: the mood's ground, with a little texture of its own */
static void mood_draw_ground(int32_t y0)
{
    int32_t i;
    box(-12.0f, (float)y0, 264.0f, 252.0f - (float)y0, mood->ground);
    box(-12.0f, (float)y0, 264.0f, 3.0f, lighten(mood->ground, 50u));
    for (i = 0; i < 14; i++) {
        uint32_t h = ss_hash((uint32_t)i * 41u + 9u);
        float fx = (float)(h % 250u) - 5.0f, fy = (float)y0 + 8.0f + (float)((h >> 9) % (uint32_t)(240 - y0 > 12 ? 240 - y0 - 8 : 4));
        switch (mood_i) {
        case MOOD_SUNNY:                       /* grass tufts and tiny flowers */
            tri(fx - 3.0f, fy + 3.0f, fx + 3.0f, fy + 3.0f, fx, fy - 4.0f, mood->ground2);
            if (i % 3 == 0)
                disc(fx + 4.0f, fy, 1.6f, RAINBOW[i % 6]);
            break;
        case MOOD_GLITCH:
            box(fx, fy, 6.0f, 1.0f, mix565(mood->accent, mood->ground, 170u));
            break;
        case MOOD_SWAMP:
            ell(fx, fy, 5.0f, 1.5f, mood->ground2);
            break;
        default:
            ell(fx, fy, 3.0f, 1.2f, mood->ground2);
            break;
        }
    }
    if (mood_i == MOOD_GLITCH)                 /* a perspective grid */
        for (i = 0; i < 9; i++)
            cap(120.0f, (float)y0, -120.0f + (float)i * 60.0f, 250.0f, 0.5f, mix565(mood->accent, mood->ground, 140u));
}

/* ------------------------------------------------------------------ props --- */
enum { PS_HOME, PS_FLY, PS_SUCK, PS_GONE, PS_BACK };
typedef struct {
    uint8_t kind, layer, state, pad;
    float hx, hy, s;                           /* home and size */
    float x, y, vx, vy, rot, t, sx, sy;        /* where it is while away; t: ms in the state */
} prop_t;
#define NPROPS 9
static prop_t props[NPROPS];
static uint8_t nprops;
static float suck_x, suck_y;

typedef struct {
    uint8_t kind, layer;
    int16_t x, y;
    uint8_t s;                                  /* size, 1/10 */
} prop_def_t;
static const prop_def_t PROP_DEFS[NMOODS][NPROPS] = {
    {{PR_SUN, 0, 200, 34, 10}, {PR_CLOUD, 0, 40, 30, 8}, {PR_CLOUD, 1, 150, 62, 10}, {PR_CLOUD, 0, 96, 18, 6},
     {PR_DAISY, 2, 20, 192, 10}, {PR_DAISY, 2, 222, 196, 9}, {PR_DAISY, 2, 112, 204, 7}, {PR_CLOUD, 1, 230, 100, 6}, {255, 0, 0, 0, 0}},
    {{PR_MOON, 0, 190, 44, 10}, {PR_STAR, 0, 30, 26, 10}, {PR_STAR, 1, 92, 50, 8}, {PR_STAR, 0, 140, 16, 7},
     {PR_STAR, 1, 60, 96, 6}, {PR_STAR, 0, 226, 104, 7}, {PR_STAR, 1, 14, 128, 8}, {255, 0, 0, 0, 0}},
    {{PR_CATTAIL, 2, 14, 192, 10}, {PR_CATTAIL, 2, 30, 198, 8}, {PR_CATTAIL, 2, 226, 194, 11}, {PR_MUDBUB, 1, 70, 196, 10},
     {PR_MUDBUB, 1, 180, 200, 8}, {PR_CLOUD, 0, 60, 30, 7}, {PR_CLOUD, 0, 180, 50, 6}, {255, 0, 0, 0, 0}},
    {{PR_BUBBLE, 0, 30, 40, 10}, {PR_BUBBLE, 1, 90, 90, 7}, {PR_BUBBLE, 0, 200, 30, 12}, {PR_BUBBLE, 2, 220, 140, 6},
     {PR_BUBBLE, 1, 150, 20, 5}, {PR_CLOUD, 0, 120, 60, 9}, {PR_BUBBLE, 2, 16, 150, 8}, {255, 0, 0, 0, 0}},
    {{PR_TOWER, 0, 28, 120, 10}, {PR_TOWER, 0, 212, 110, 12}, {PR_BUNTING, 1, 120, 14, 10}, {PR_CLOUD, 0, 120, 44, 7},
     {PR_STAR, 1, 70, 70, 6}, {PR_STAR, 1, 170, 80, 6}, {255, 0, 0, 0, 0}},
    {{PR_MOON, 0, 196, 36, 8}, {PR_TREE, 0, 26, 150, 10}, {PR_TREE, 1, 222, 160, 8}, {PR_PUMPKIN, 2, 30, 200, 10},
     {PR_PUMPKIN, 2, 210, 204, 8}, {PR_STAR, 0, 100, 20, 5}, {PR_STAR, 0, 140, 44, 4}, {255, 0, 0, 0, 0}},
    {{PR_BLOSSOM, 1, 0, 24, 12}, {PR_BLOSSOM, 1, 240, 40, 10}, {PR_CLOUD, 0, 110, 30, 7}, {PR_SUN, 0, 170, 24, 7},
     {PR_BLOSSOM, 2, 216, 150, 7}, {PR_DAISY, 2, 24, 198, 8}, {255, 0, 0, 0, 0}},
    {{PR_PIXEL, 0, 30, 30, 10}, {PR_PIXEL, 1, 200, 50, 8}, {PR_PIXEL, 0, 120, 20, 6}, {PR_PIXEL, 2, 220, 150, 9},
     {PR_PIXEL, 1, 20, 120, 7}, {PR_PIXEL, 0, 160, 90, 5}, {255, 0, 0, 0, 0}},
};

static void mood_props_home(void)
{
    int i;
    nprops = 0;
    for (i = 0; i < NPROPS && PROP_DEFS[mood_i][i].kind != 255; i++) {
        prop_t *p = &props[nprops++];
        const prop_def_t *d = &PROP_DEFS[mood_i][i];
        p->kind = d->kind;
        p->layer = d->layer;
        p->hx = (float)d->x;
        p->hy = (float)d->y;
        p->s = (float)d->s / 10.0f;
        p->state = PS_HOME;
        p->t = 0.0f;
        p->rot = 0.0f;
        p->sx = p->sy = 1.0f;
    }
}

static const float LAYER_SPEED[3] = {0.25f, 0.5f, 1.0f};
static void prop_home_pos(const prop_t *p, float T, float *x, float *y)
{
    float sp = LAYER_SPEED[p->layer];
    *x = p->hx + sp * 14.0f * fm_sinf(T / 5200.0f + p->hy * 0.05f);
    *y = p->hy + sp * 3.0f * fm_sinf(T / 1700.0f + p->hx * 0.07f);
}

static void mood_props_blast(float x, float y, float dx, float dy, float cone, float force)
{
    int i;
    float l = fm_rsqrtf(dx * dx + dy * dy + 1e-6f);
    dx *= l;
    dy *= l;
    for (i = 0; i < nprops; i++) {
        prop_t *p = &props[i];
        float px, py, vx, vy, d, c;
        if (p->state != PS_HOME && p->state != PS_BACK)
            continue;
        prop_home_pos(p, (float)plat_ms(), &px, &py);
        if (p->state == PS_BACK) {
            px = p->x;
            py = p->y;
        }
        vx = px - x;
        vy = py - y;
        d = fm_sqrtf(vx * vx + vy * vy) + 0.001f;
        c = (vx * dx + vy * dy) / d;
        if (c < cone)                              /* outside the cone */
            continue;
        p->state = PS_FLY;
        p->x = px;
        p->y = py;
        p->vx = (vx / d * 0.6f + dx * 0.6f) * force * (0.8f + 0.4f * ss_rndf());
        p->vy = (vy / d * 0.6f + dy * 0.6f) * force * (0.8f + 0.4f * ss_rndf()) - 2.0f;
        p->rot = ss_rnds() * 0.3f;
        p->t = 0.0f;
    }
}

static void mood_props_suck(float x, float y)
{
    int i;
    suck_x = x;
    suck_y = y;
    for (i = 0; i < nprops; i++) {
        prop_t *p = &props[i];
        if (p->state == PS_SUCK || p->state == PS_GONE)
            continue;
        if (p->state == PS_HOME)
            prop_home_pos(p, (float)plat_ms(), &p->x, &p->y);
        p->state = PS_SUCK;
        p->t = -(float)i * 60.0f;                  /* one after another */
        p->vx = p->x;                              /* (where the swirl starts) */
        p->vy = p->y;
    }
}

static int mood_props_away(void)
{
    int i, n = 0;
    for (i = 0; i < nprops; i++)
        n += props[i].state != PS_HOME;
    return n;
}

static void mood_props_update(float dt)
{
    int i;
    float k = dt / 16.67f;
    for (i = 0; i < nprops; i++) {
        prop_t *p = &props[i];
        p->t += dt;
        switch (p->state) {
        case PS_FLY:                               /* ballistic, bouncing off the edges, then home again */
            p->vy += 0.25f * k;
            p->x += p->vx * k;
            p->y += p->vy * k;
            p->rot += p->vx * 0.02f * k;
            if (p->x < 6.0f && p->vx < 0.0f) { p->x = 6.0f; p->vx = -p->vx * 0.7f; }
            if (p->x > 234.0f && p->vx > 0.0f) { p->x = 234.0f; p->vx = -p->vx * 0.7f; }
            if (p->y < 6.0f && p->vy < 0.0f) { p->y = 6.0f; p->vy = -p->vy * 0.7f; }
            if (p->y > 226.0f && p->vy > 0.0f) { p->y = 226.0f; p->vy = -p->vy * 0.55f; p->vx *= 0.8f; }
            if (p->t > 2600.0f) {
                p->state = PS_BACK;
                p->t = 0.0f;
                p->vx = p->x;
                p->vy = p->y;
            }
            break;
        case PS_BACK:
            if (p->t >= 900.0f) {
                p->state = PS_HOME;
                p->rot = 0.0f;
            } else {
                float hx, hy, e = ease_in_out_sine(p->t / 900.0f);
                prop_home_pos(p, (float)plat_ms(), &hx, &hy);
                p->x = ss_lerp(p->vx, hx, e);
                p->y = ss_lerp(p->vy, hy, e);
                p->rot *= 0.9f;
            }
            break;
        case PS_SUCK:                              /* a spiral in, faster and smaller (ease_in_quad), then a pop */
            if (p->t > 0.0f) {
                float u = ss_clamp01(p->t / 900.0f), e = ease_in_quad(u), ang = u * 9.0f + (float)i;
                float r = (1.0f - e) * fm_sqrtf((p->vx - suck_x) * (p->vx - suck_x) + (p->vy - suck_y) * (p->vy - suck_y));
                p->x = suck_x + r * fm_cosf(ang);
                p->y = suck_y + r * fm_sinf(ang) * 0.5f;
                p->sx = p->sy = 1.0f - e * 0.9f;
                p->rot = ang;
                if (u >= 1.0f) {
                    p->state = PS_GONE;
                    p->t = 0.0f;
                }
            }
            break;
        case PS_GONE:
            if (p->t > 1800.0f + (float)i * 120.0f) {
                p->state = PS_HOME;
                p->t = 0.0f;
                p->sx = p->sy = 0.0f;
            }
            break;
        default:                                   /* home: popping back in grows from nothing */
            if (p->sx < 1.0f) {
                p->sx = p->sy = ease_out_back(ss_clamp01(p->t / 350.0f));
            }
            break;
        }
    }
}

/* one prop, at x, y, scale s (s already includes its own size and any squash) */
static void prop_draw(const prop_t *p, float x, float y, float s, float T)
{
    uint16_t a = mood->accent, c1 = mood->prop1, c2 = mood->prop2;
    int i;
    if (s < 0.05f)
        return;
    switch (p->kind) {
    case PR_SUN:
        glow(x, y, 30.0f * s, HEX(0x806010), 160u);
        disc(x, y, 15.0f * s, c2);
        disc(x - 4.0f * s, y - 4.0f * s, 6.0f * s, lighten(c2, 120u));
        break;
    case PR_CLOUD: {
        uint16_t sh;
        c1 = lighten(mood->chr, 90u);
        sh = mix565(c1, mix565(mood->accent, HEX(0x8090B0), 160u), 80u);
        ell(x, y + 4.0f * s, 24.0f * s, 8.0f * s, sh);
        disc(x - 12.0f * s, y + 1.0f * s, 9.0f * s, c1);
        disc(x + 1.0f * s, y - 5.0f * s, 12.0f * s, c1);
        disc(x + 13.0f * s, y + 1.0f * s, 8.0f * s, c1);
        ell(x, y + 3.0f * s, 20.0f * s, 6.0f * s, c1);
        break;
    }
    case PR_DAISY:
        cap(x, y, x, y + 14.0f * s, 1.0f * s, mood->ground2);
        for (i = 0; i < 6; i++) {
            float an = (float)i * TAU / 6.0f + T / 3000.0f;
            disc(x + 5.0f * s * fm_cosf(an), y + 5.0f * s * fm_sinf(an), 3.2f * s, U_WHITE);
        }
        disc(x, y, 3.0f * s, CC_YELLOW);
        break;
    case PR_STAR: {
        float tw = 0.7f + 0.3f * fm_sinf(T / 260.0f + p->hx);
        glow(x, y, 12.0f * s, scale565(c1, 120u), 140u);
        star4(x, y, 7.0f * s * tw, c1);
        disc(x, y, 1.5f * s, U_WHITE);
        break;
    }
    case PR_MOON: {                                /* a big friendly sleepy moon */
        c1 = U_BONE;
        glow(x, y, 38.0f * s, scale565(c1, 90u), 130u);
        disc(x, y, 22.0f * s, c1);
        disc(x - 8.0f * s, y + 7.0f * s, 4.0f * s, darken(c1, 30u));
        disc(x + 9.0f * s, y - 9.0f * s, 3.0f * s, darken(c1, 30u));
        arc_dots(x - 7.0f * s, y - 2.0f * s, 3.5f * s, 0.3f, 2.8f, 1.6f * s, darken(c1, 110u));
        arc_dots(x + 6.0f * s, y - 2.0f * s, 3.5f * s, 0.3f, 2.8f, 1.6f * s, darken(c1, 110u));
        arc_dots(x, y + 5.0f * s, 5.0f * s, 0.5f, 2.6f, 1.6f * s, darken(c1, 110u));
        disc_a(x - 12.0f * s, y + 5.0f * s, 3.0f * s, CC_BLUSH, 140u);
        disc_a(x + 11.0f * s, y + 5.0f * s, 3.0f * s, CC_BLUSH, 140u);
        break;
    }
    case PR_CATTAIL: {
        float sw = 3.0f * fm_sinf(T / 900.0f + p->hx);
        cap(x, y + 30.0f * s, x + sw, y - 10.0f * s, 1.2f * s, c2);
        cap(x + sw, y - 10.0f * s, x + sw * 1.2f, y - 26.0f * s, 3.5f * s, c1);
        cap(x - 2.0f, y + 30.0f * s, x - 9.0f * s, y + 6.0f * s, 1.5f * s, c2);
        break;
    }
    case PR_MUDBUB: {                              /* a mud bubble grows and pops, over and over */
        float ph = fm_floorf(T / 1600.0f + p->hx * 0.01f), u = T / 1600.0f + p->hx * 0.01f - ph;
        float r = 9.0f * s * ease_out_back(fm_minf(u * 1.4f, 1.0f));
        ell(x, y + 2.0f, 13.0f * s, 3.0f * s, darken(c1, 60u));
        if (u < 0.85f) {
            ell(x, y - r * 0.5f, r, r * 0.8f, c1);
            disc(x - r * 0.35f, y - r * 0.8f, fm_maxf(r * 0.2f, 0.6f), lighten(c1, 120u));
        } else {
            arc_dots(x, y - 4.0f, 10.0f * s * (u - 0.6f) * 3.0f, 3.4f, 6.0f, 1.5f, c2);
        }
        break;
    }
    case PR_BUBBLE: {
        float r = 12.0f * s * (1.0f + 0.04f * fm_sinf(T / 300.0f + p->hy));
        ring_a(x, y, r, r, fm_maxf(1.5f * s, 1.0f), a, 200u);
        disc_a(x, y, r - 1.0f, c1, 50u);
        ell(x - r * 0.4f, y - r * 0.45f, r * 0.22f, r * 0.14f, U_WHITE);
        break;
    }
    case PR_TOWER: {
        float w = 16.0f * s, h = 60.0f * s;
        box(x - w, y - h, 2.0f * w, h + 80.0f, c1);
        box(x + w * 0.5f, y - h, w * 0.5f, h + 80.0f, darken(c1, 30u));
        for (i = 0; i < 3; i++)
            box(x - w + (float)i * w * 0.8f, y - h - 5.0f * s, w * 0.45f, 6.0f * s, c1);
        tri(x - w - 3.0f * s, y - h - 4.0f * s, x + w + 3.0f * s, y - h - 4.0f * s, x, y - h - 34.0f * s, c2);
        cap(x, y - h - 34.0f * s, x, y - h - 46.0f * s, 0.8f, U_LINE);
        tri(x, y - h - 46.0f * s, x, y - h - 40.0f * s, x + 10.0f * s * (1.0f + 0.15f * fm_sinf(T / 200.0f)), y - h - 43.0f * s, a);
        ell(x, y - h * 0.55f, 4.0f * s, 6.0f * s, darken(c2, 80u));
        break;
    }
    case PR_BUNTING: {                             /* a string of flags across the top, swinging */
        float sag = 10.0f + 2.0f * fm_sinf(T / 700.0f);
        for (i = 0; i < 9; i++) {
            float u = (float)i / 8.0f, fx = x - 120.0f + 240.0f * u, fy = y + sag * 4.0f * u * (1.0f - u);
            uint16_t fc = i % 3 == 0 ? a : i % 3 == 1 ? c1 : c2;
            tri(fx - 9.0f, fy, fx + 9.0f, fy, fx, fy + 16.0f, fc);
        }
        for (i = 0; i < 16; i++) {
            float u0 = (float)i / 16.0f, u1 = (float)(i + 1) / 16.0f;
            cap(x - 120.0f + 240.0f * u0, y + sag * 4.0f * u0 * (1.0f - u0), x - 120.0f + 240.0f * u1,
                y + sag * 4.0f * u1 * (1.0f - u1), 0.6f, U_LINE);
        }
        break;
    }
    case PR_TREE: {                                /* a crooked friendly tree, with a face in its trunk */
        uint16_t tc = c1;
        cap(x, y + 60.0f, x + 4.0f * s, y - 10.0f * s, 6.0f * s, tc);
        cap(x + 4.0f * s, y - 10.0f * s, x - 6.0f * s, y - 34.0f * s, 4.5f * s, tc);
        cap(x - 6.0f * s, y - 34.0f * s, x + 6.0f * s, y - 50.0f * s, 3.0f * s, tc);
        cap(x + 2.0f * s, y - 22.0f * s, x + 22.0f * s, y - 34.0f * s, 2.5f * s, tc);
        cap(x - 2.0f * s, y - 14.0f * s, x - 20.0f * s, y - 26.0f * s, 2.5f * s, tc);
        disc(x + 22.0f * s, y - 34.0f * s, 3.0f * s, tc);
        disc(x - 1.0f * s, y + 6.0f * s, 2.0f * s, U_BONE);
        disc(x + 6.0f * s, y + 6.0f * s, 2.0f * s, U_BONE);
        arc_dots(x + 2.5f * s, y + 10.0f * s, 3.0f * s, 0.4f, 2.7f, 1.2f, U_BONE);
        break;
    }
    case PR_PUMPKIN: {                             /* friendly: round eyes, a smile, no teeth */
        float r = 12.0f * s;
        ell(x - r * 0.55f, y, r * 0.6f, r * 0.85f, darken(c2, 40u));
        ell(x + r * 0.55f, y, r * 0.6f, r * 0.85f, darken(c2, 40u));
        ell(x, y, r * 0.65f, r * 0.9f, c2);
        cap(x, y - r * 0.85f, x + 2.0f * s, y - r * 1.3f, 1.6f * s, CC_GREEN);
        disc(x - r * 0.35f, y - r * 0.15f, r * 0.17f, U_BONE);
        disc(x + r * 0.35f, y - r * 0.15f, r * 0.17f, U_BONE);
        arc_dots(x, y + r * 0.1f, r * 0.4f, 0.4f, 2.7f, 1.5f * s, U_BONE);
        glow(x, y, r * 1.8f, HEX(0x603000), 90u);
        break;
    }
    case PR_BLOSSOM: {                             /* a cherry branch reaching in, puffs of blossom */
        float d = x < 120.0f ? 1.0f : -1.0f;
        cap(x, y, x + d * 40.0f * s, y + 14.0f * s, 2.5f * s, c1);
        cap(x + d * 24.0f * s, y + 9.0f * s, x + d * 38.0f * s, y - 6.0f * s, 1.6f * s, c1);
        for (i = 0; i < 5; i++) {
            float bx = x + d * (12.0f + 8.0f * (float)i) * s, by = y + (6.0f + 3.0f * fm_sinf((float)i * 2.0f)) * s - (i & 1 ? 8.0f : 0.0f) * s;
            disc(bx, by, 6.5f * s, c2);
            disc(bx, by, 2.0f * s, lighten(a, 100u));
        }
        break;
    }
    default: {                                     /* PR_PIXEL: a floating pixel block, its colours split */
        float w = 10.0f * s, j = (float)((uint32_t)(T / 90.0f + p->hx) % 3u);
        box(x - w - 2.0f, y - w, 2.0f * w, 2.0f * w, HEX(0xFF0040));
        box(x - w + 2.0f, y - w, 2.0f * w, 2.0f * w, HEX(0x00FFC0));
        box(x - w + j - 1.0f, y - w, 2.0f * w, 2.0f * w, a);
        box(x - w * 0.5f, y - w * 0.5f, w * 0.6f, w * 0.6f, lighten(a, 160u));
        break;
    }
    }
}

static void mood_props_draw(float T, int front)
{
    int i;
    for (i = 0; i < nprops; i++) {
        prop_t *p = &props[i];
        float x, y;
        if ((p->layer == 2) != (front != 0) || p->state == PS_GONE)
            continue;
        if (p->state == PS_HOME) {
            prop_home_pos(p, T, &x, &y);
            prop_draw(p, x, y, p->s * p->sx, T);
        } else {
            prop_draw(p, p->x, p->y, p->s * (p->state == PS_SUCK ? p->sx : 1.0f), T);
        }
    }
    if (front && mood_i == MOOD_GLITCH) {          /* scanlines, and now and then a tear */
        int32_t y;
        for (y = (int32_t)(T / 40.0f) % 6; y < 240; y += 6)
            hl_a(0, 239, y, U_BLACK, 70u);
        if ((ss_hash((uint32_t)(T / 500.0f)) & 15u) == 0u && (ss_rnd() & 7u) == 0u)
            fx_tear(1, 0.25f);                 /* (gentle: no colour swap below half strength) */
    }
}
