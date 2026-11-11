/* SPDX-License-Identifier: MPL-2.0
 * Spooky Stable v2: the unicorn rig (unicorn.h). Local coordinates face right with the ground at y = 0; the parts
 * are v1's unicorn (which she loves) moved onto that anchor, so every toy's unicorn is the same one. */

static const uint16_t MANES[NMANES - 1][6] = {
    {HEX(0xFBCFE8), HEX(0xFDE68A), HEX(0xBBF7D0), HEX(0xBAE6FD), HEX(0xDDD6FE), HEX(0xFBCFE8)},   /* pastel */
    {HEX(0xFDE047), HEX(0xFB923C), HEX(0xEF4444), HEX(0xF97316), HEX(0xFACC15), HEX(0xFB7185)},   /* fire */
    {HEX(0x22D3EE), HEX(0x38BDF8), HEX(0x6366F1), HEX(0x2DD4BF), HEX(0x60A5FA), HEX(0xA5F3FC)},   /* ocean */
    {HEX(0xF472B6), HEX(0xFFFFFF), HEX(0xF472B6), HEX(0xFFFFFF), HEX(0xF472B6), HEX(0xFFFFFF)},   /* candy */
    {HEX(0xFFC828), HEX(0xFFE89A), HEX(0xE09A10), HEX(0xFFC828), HEX(0xFFE89A), HEX(0xE09A10)},   /* gold */
};
static uint16_t mane_color(int pal, int i)
{
    if (pal <= 0 || pal >= NMANES)
        return mood_mane(i);
    return MANES[pal - 1][i % 6];
}

static pose_t pose_rest(float x, float y, float scale)
{
    pose_t p;
    p.x = x;
    p.y = y;
    p.scale_x = p.scale_y = 1.0f;
    p.rot_deg = p.head_rot = 0.0f;
    p.leg_phase = 0.0f;
    p.tail_phase = 0.15f;
    p.mane_phase = 0.0f;
    p.eye_state = EY_OPEN;
    p.mouth_state = MO_CLOSED;
    p.accessory_id = ACC_NONE;
    p.facing = 1;
    p.blush = 0.0f;
    p.tint = 0;
    p.scale = scale;
    p.lift = 0.0f;
    p.belly = 1.0f;
    p.cheeks = 0.0f;
    p.horn_glow = 0.0f;
    p.look_x = p.look_y = 0.0f;
    p.spread = 0.0f;
    p.arm_l = p.arm_r = 0.0f;
    p.vibrate = 0.0f;
    p.pick = 0.0f;
    p.mane = 0;
    return p;
}

static void rig_update(rig_t *r, pose_t *p, float T, float dt)
{
    float k = fm_minf(dt, 50.0f) * 0.001f, hx, hy, vx, vy;
    if (!r->init) {
        r->init = 1;
        r->tail = p->tail_phase;
        r->hx = p->x;
        r->hy = p->y;
        r->blink_at = T + 1500.0f;
        r->lx = p->look_x;
        r->ly = p->look_y;
    }
    /* the tail follows on a spring: about three frames behind, with a little swing */
    r->tail_v += ((p->tail_phase - r->tail) * 260.0f - r->tail_v * 22.0f) * k;
    r->tail += r->tail_v * k;
    p->tail_phase = r->tail;
    /* the hat jiggles: the head's motion kicks a spring that lags and wobbles */
    hx = p->x;
    hy = p->y - p->lift - 100.0f * p->scale * p->scale_y;
    vx = k > 0.0f ? (hx - r->hx) / (k * 1000.0f) : 0.0f;
    vy = k > 0.0f ? (hy - r->hy) / (k * 1000.0f) : 0.0f;
    r->hx = hx;
    r->hy = hy;
    r->acc_vx += (-r->acc_x * 300.0f - r->acc_vx * 14.0f) * k - vx * 9.0f;
    r->acc_vy += (-r->acc_y * 300.0f - r->acc_vy * 14.0f) * k - vy * 9.0f;
    r->acc_x = fm_clampf(r->acc_x + r->acc_vx * k, -6.0f, 6.0f);
    r->acc_y = fm_clampf(r->acc_y + r->acc_vy * k, -6.0f, 6.0f);
    /* blinks every 2 - 5 s */
    if (T >= r->blink_at) {
        r->blink_until = T + 130.0f;
        r->blink_at = T + 2000.0f + 3000.0f * ss_rndf();
    }
    if (p->eye_state == EY_OPEN && T < r->blink_until)
        p->eye_state = EY_BLINK;
    /* the pupils glide to where she looks */
    r->lx += (p->look_x - r->lx) * fm_minf(1.0f, dt / 90.0f);
    r->ly += (p->look_y - r->ly) * fm_minf(1.0f, dt / 90.0f);
    p->look_x = r->lx;
    p->look_y = r->ly;
    p->mane_phase = T;
}

/* --------------------------------------------------------------- transform --- */
typedef struct {
    float x, y, sx, sy, cs, sn, hcs, hsn;
    float f;                          /* facing */
} xf_t;
static xf_t ux;
#define HPX 30.0f                     /* the head's pivot (the top of the neck) */
#define HPY (-62.0f)

static void xp(float lx, float ly, int head, float *ox, float *oy)
{
    float x, y;
    if (head) {                       /* the head tilts about the neck first */
        float dx = lx - HPX, dy = ly - HPY;
        lx = HPX + dx * ux.hcs + dy * ux.hsn;
        ly = HPY - dx * ux.hsn + dy * ux.hcs;
    }
    lx *= ux.sx;
    ly *= ux.sy;
    x = lx * ux.cs + ly * ux.sn;
    y = -lx * ux.sn + ly * ux.cs;
    *ox = ux.x + ux.f * x;
    *oy = ux.y + y;
}
static void u_ell(float lx, float ly, float rx, float ry, int head, float o, uint16_t c)
{
    float x, y;
    xp(lx, ly, head, &x, &y);
    ell(x, y, rx * ss_absf(ux.sx) + o, ry * ux.sy + o, c);
}
static void u_ell_a(float lx, float ly, float rx, float ry, int head, uint16_t c, uint32_t a)
{
    float x, y;
    xp(lx, ly, head, &x, &y);
    ell_a(x, y, rx * ss_absf(ux.sx), ry * ux.sy, c, a);
}
static void u_tri(float x0, float y0, float x1, float y1, float x2, float y2, int head, float o, uint16_t c)
{
    float ax, ay, bx, by, cx, cy, mx, my;
    xp(x0, y0, head, &ax, &ay);
    xp(x1, y1, head, &bx, &by);
    xp(x2, y2, head, &cx, &cy);
    if (o > 0.0f) {                   /* the outline: each corner pushed out from the middle */
        float l;
        mx = (ax + bx + cx) / 3.0f;
        my = (ay + by + cy) / 3.0f;
        l = fm_rsqrtf((ax - mx) * (ax - mx) + (ay - my) * (ay - my) + 0.01f) * o * 1.6f;
        ax += (ax - mx) * l;
        ay += (ay - my) * l;
        l = fm_rsqrtf((bx - mx) * (bx - mx) + (by - my) * (by - my) + 0.01f) * o * 1.6f;
        bx += (bx - mx) * l;
        by += (by - my) * l;
        l = fm_rsqrtf((cx - mx) * (cx - mx) + (cy - my) * (cy - my) + 0.01f) * o * 1.6f;
        cx += (cx - mx) * l;
        cy += (cy - my) * l;
    }
    tri(ax, ay, bx, by, cx, cy, c);
}
static void u_cap(float x0, float y0, float x1, float y1, float r, int head, float o, uint16_t c)
{
    float ax, ay, bx, by;
    xp(x0, y0, head, &ax, &ay);
    xp(x1, y1, head, &bx, &by);
    cap(ax, ay, bx, by, r * ux.sy + o, c);
}

/* ------------------------------------------------------------------- draw --- */
typedef struct {
    float hx, hy, fx, fy;             /* hip and hoof */
} leg_t;

static void legs_of(const pose_t *p, leg_t L[4])
{
    static const float HIPX[4] = {-30.0f, 22.0f, -18.0f, 32.0f};   /* near rear, near front, far rear, far front */
    int i;
    for (i = 0; i < 4; i++) {
        int front = i & 1;
        float ph = p->leg_phase + (i == 0 || i == 3 ? 0.0f : 3.14159f), arm = i == 1 ? p->arm_l : i == 3 ? p->arm_r : 0.0f;
        L[i].hx = HIPX[i];
        L[i].hy = -34.0f;
        L[i].fx = HIPX[i] + (front ? 7.0f : -7.0f) * p->spread + 5.0f * fm_sinf(ph) * (p->leg_phase != 0.0f);
        L[i].fy = -3.0f - (p->leg_phase != 0.0f ? 4.0f * fm_maxf(0.0f, fm_cosf(ph)) : 0.0f);
        if (arm > 0.0f) {             /* raised in front: an arm */
            float a = arm * 2.2f;
            L[i].fx = HIPX[i] + 31.0f * fm_sinf(a);
            L[i].fy = -34.0f + 31.0f * fm_cosf(a);
        }
        if (i == 1 && p->pick > 0.0f) {   /* up to the nose (a cartoon arm stretches) */
            L[i].fx = ss_lerp(L[i].fx, 63.0f, p->pick);
            L[i].fy = ss_lerp(L[i].fy, -73.0f, p->pick);
        }
    }
}

static void draw_unicorn(const pose_t *p, const rig_t *r)
{
    float sc = p->scale, rot = p->rot_deg * (3.14159f / 180.0f), hr = p->head_rot * (3.14159f / 180.0f), T = p->mane_phase;
    float o = fm_maxf(1.5f * sc, 1.0f), tdx, tdy, tl, b = p->belly, bx = 1.0f + 0.55f * (b - 1.0f);
    uint16_t body = mood->chr, hoof = mix565(mood->accent, U_LINE, 110u), farc = mix565(body, U_LINE, 50u);
    uint16_t blush = CC_BLUSH;
    leg_t L[4];
    int i, pass;
    float vx = p->vibrate > 0.0f ? p->vibrate * ss_noise((uint32_t)(T * 0.06f)) : 0.0f;
    if (sc < 0.05f)
        return;
    /* the ground shadow: shrinks as she jumps */
    ell_a(p->x, p->y + 1.0f, 40.0f * sc * p->scale_x / (1.0f + p->lift / 60.0f), 6.0f * sc, U_LINE, 70u);
    if (p->tint)
        dr_solid_begin(p->tint);
    ux.x = p->x + vx;
    ux.y = p->y - p->lift;
    ux.sx = sc * p->scale_x;
    ux.sy = sc * p->scale_y;
    ux.cs = fm_cosf(rot);
    ux.sn = fm_sinf(rot);
    ux.hcs = fm_cosf(hr);
    ux.hsn = fm_sinf(hr);
    ux.f = p->facing < 0 ? -1.0f : 1.0f;
    legs_of(p, L);
    xp(L[1].fx, L[1].fy, 0, &uni_hoof[0][0], &uni_hoof[0][1]);
    xp(L[3].fx, L[3].fy, 0, &uni_hoof[1][0], &uni_hoof[1][1]);
    /* the tail: hanging down and back, or lifted up and back; each puff a little behind the one before */
    tdx = ss_lerp(-0.45f, -0.8f, p->tail_phase);
    tdy = ss_lerp(0.9f, -0.6f, p->tail_phase);
    tl = fm_rsqrtf(tdx * tdx + tdy * tdy);
    tdx *= tl;
    tdy *= tl;
    for (pass = 0; pass < 2; pass++)
        for (i = 0; i < 6; i++) {
            float w = fm_sinf(T / 260.0f - (float)i * 0.9f) * 1.6f * (float)i;
            u_ell(-38.0f + 7.0f * (float)i * tdx - w * tdy, -50.0f + 7.0f * (float)i * tdy + w * tdx * 0.3f,
                  8.5f - 0.8f * (float)i, 8.5f - 0.8f * (float)i, 0, pass ? 0.0f : o, pass ? mane_color(p->mane, i) : U_LINE);
        }
    /* the far legs, a little darker */
    for (pass = 0; pass < 2; pass++)
        for (i = 2; i < 4; i++) {
            u_cap(L[i].hx, L[i].hy - 2.0f, L[i].fx, L[i].fy - 2.0f, 5.0f, 0, pass ? 0.0f : o, pass ? farc : U_LINE);
            if (pass)
                u_ell(L[i].fx, L[i].fy, 5.5f, 3.5f, 0, 0.0f, darken(hoof, 40u));
        }
    /* body, near legs, neck, head, snout, ear: one outline round them all, then the fills (a sticker) */
    for (pass = 0; pass < 2; pass++) {
        float e = pass ? 0.0f : o;
        uint16_t c = pass ? body : U_LINE;
        for (i = 0; i < 2; i++)
            if (i == 0 || p->pick <= 0.0f)
                u_cap(L[i].hx, L[i].hy, L[i].fx, L[i].fy, 5.5f, 0, e, c);
        u_ell(0.0f, -38.0f - (b - 1.0f) * 9.0f, 42.0f * bx, 26.0f * b, 0, e, c);
        u_ell(28.0f, -60.0f, 13.0f, 20.0f, 0, e, c);
        u_ell(40.0f, -80.0f, 19.0f, 19.0f, 1, e, c);
        u_ell(56.0f, -73.0f, 14.0f + 3.0f * p->cheeks, 11.0f + 2.0f * p->cheeks, 1, e, c);
        u_tri(29.0f, -93.0f, 38.0f, -95.0f, 31.0f, -110.0f, 1, e, c);
    }
    for (i = 0; i < 2; i++)                      /* hooves */
        if (i == 0 || p->pick <= 0.0f)
            u_ell(L[i].fx, L[i].fy, 6.0f, 3.8f, 0, 0.0f, hoof);
    u_ell_a(-2.0f, -30.0f - (b - 1.0f) * 6.0f, 26.0f * bx, 10.0f * b, 0, blush, 60u);   /* the belly's blush */
    if (b > 1.08f)                                                                       /* and its shine */
        u_ell_a(-14.0f, -52.0f - (b - 1.0f) * 14.0f, 9.0f * bx, 5.0f * b, 0, U_WHITE, (uint32_t)(400.0f * (b - 1.0f)));
    u_tri(31.0f, -96.0f, 36.0f, -96.0f, 32.0f, -105.0f, 1, 0.0f, blush);                  /* inner ear */
    /* the mane: six puffs down the neck, each wobbling on its own phase */
    for (pass = 0; pass < 2; pass++)
        for (i = 0; i < 6; i++) {
            float t = (float)i / 5.0f, w = fm_sinf(T / 240.0f + (float)i) * 1.4f;
            u_ell(ss_lerp(35.0f, 20.0f, t) - w, ss_lerp(-99.0f, -63.0f, t), 7.5f, 7.5f, i < 3,
                  pass ? 0.0f : o, pass ? mane_color(p->mane, i) : U_LINE);
        }
    /* the horn: a glow when charged, then three golds base to tip, and its stripes */
    if (p->horn_glow > 0.02f) {
        float gx, gy;
        xp(45.0f, -118.0f, 1, &gx, &gy);
        glow(gx, gy, (10.0f + 22.0f * p->horn_glow) * sc, mix565(CC_GOLD, mood->accent, 100u), (uint32_t)(255.0f * p->horn_glow));
        disc_a(gx, gy, (3.0f + 5.0f * p->horn_glow) * sc, U_WHITE, (uint32_t)(200.0f * p->horn_glow));
    }
    u_tri(34.0f, -95.0f, 46.0f, -97.0f, 46.5f, -126.0f, 1, o, U_LINE);
    u_tri(35.0f, -96.0f, 45.0f, -97.0f, 46.0f, -125.0f, 1, 0.0f, CC_GOLD_D);
    u_tri(37.0f, -100.0f, 45.0f, -101.0f, 46.0f, -125.0f, 1, 0.0f, CC_GOLD);
    u_tri(41.0f, -110.0f, 45.0f, -111.0f, 46.0f, -125.0f, 1, 0.0f, CC_GOLD_L);
    u_cap(37.5f, -103.0f, 44.5f, -106.0f, 0.9f, 1, 0.0f, CC_GOLD_D);
    u_cap(40.0f, -112.0f, 45.0f, -114.5f, 0.8f, 1, 0.0f, CC_GOLD_D);
    /* the forelock over the horn's base */
    u_ell(45.0f, -98.0f, 5.5f, 4.5f, 1, o, U_LINE);
    u_ell(45.0f, -98.0f, 5.5f, 4.5f, 1, 0.0f, mane_color(p->mane, 0));
    /* the face: cheek, nostril, mouth, the big eye */
    u_ell_a(55.0f, -70.0f, 4.0f + 5.0f * p->cheeks, 3.5f + 4.0f * p->cheeks, 1, blush, 150u + (uint32_t)(100.0f * p->blush));
    u_ell(66.0f, -77.0f, 1.6f, 1.6f, 1, 0.0f, U_LINE);
    {
        float mx, my, s = sc;
        xp(61.0f, -66.0f, 1, &mx, &my);
        switch (p->mouth_state) {
        case MO_OPEN:
            ell(mx, my, 3.8f * s, 3.0f * s, CC_INK);
            ell(mx, my + 1.2f * s, 2.4f * s, 1.3f * s, CC_PINK);
            break;
        case MO_O:
            disc(mx, my, 3.4f * s, CC_INK);
            disc(mx, my + 0.8f * s, 2.0f * s, mix565(CC_PINK, CC_INK, 90u));
            break;
        case MO_GRIN:
            ell(mx - 1.5f * ux.f * s, my - 0.5f * s, 6.5f * s, 4.2f * s, CC_INK);
            ell(mx - 1.5f * ux.f * s, my - 2.0f * s, 5.4f * s, 1.8f * s, U_WHITE);
            ell(mx - 1.5f * ux.f * s, my + 2.0f * s, 3.0f * s, 1.2f * s, CC_PINK);
            break;
        case MO_STRAIN: {                         /* a wobbling wavy line */
            int k;
            for (k = 0; k < 4; k++) {
                float x0 = mx - 5.0f * ux.f * s + (float)k * 2.6f * ux.f * s, x1 = x0 + 2.6f * ux.f * s;
                float y0 = my + fm_sinf(T / 30.0f + (float)k * 2.2f) * 1.4f * s, y1 = my + fm_sinf(T / 30.0f + (float)(k + 1) * 2.2f) * 1.4f * s;
                cap(x0, y0, x1, y1, 0.9f * s, CC_INK);
            }
            break;
        }
        default:
            arc_dots(mx - 1.0f * ux.f * s, my - 3.5f * s, 4.0f * s, 0.6f, 2.5f, 1.8f * s, CC_INK);
            break;
        }
    }
    if (p->accessory_id != ACC_GOGGLES) {
        float ex, ey, s = sc * (p->eye_state == EY_WIDE ? 1.35f : 1.0f);
        xp(44.0f, -84.0f, 1, &ex, &ey);
        switch (p->eye_state) {
        case EY_BLINK:
        case EY_SLEEP:
            arc_dots(ex, ey - 2.0f * sc, 5.5f * sc, 0.4f, 2.75f, 1.8f * sc, U_LINE);
            break;
        case EY_HAPPY:
            arc_dots(ex, ey + 2.5f * sc, 5.5f * sc, 3.6f, 5.85f, 1.9f * sc, U_LINE);
            break;
        case EY_SQUINT:                           /* > < : screwed up tight */
            cap(ex - 4.0f * sc * ux.f, ey - 3.5f * sc, ex + 3.0f * sc * ux.f, ey, 1.0f * sc, U_LINE);
            cap(ex + 3.0f * sc * ux.f, ey, ex - 4.0f * sc * ux.f, ey + 3.5f * sc, 1.0f * sc, U_LINE);
            break;
        case EY_SPIRAL: {
            float th;
            ell(ex, ey, 8.5f * s, 9.0f * s, U_LINE);
            ell(ex, ey, 7.5f * s, 8.0f * s, U_WHITE);
            for (th = 0.0f; th < 13.0f; th += 0.45f) {
                float rr = 7.0f * s * th / 13.0f, a = th - T / 70.0f;
                disc(ex + rr * fm_cosf(a), ey + rr * fm_sinf(a), fm_maxf(0.8f * s, 0.6f), mood->accent);
            }
            break;
        }
        default:
            ell(ex, ey, 8.5f * s, 9.5f * s, U_LINE);
            ell(ex, ey, 7.5f * s, 8.5f * s, U_WHITE);
            {
                float px = ex + p->look_x * 2.8f * s * ux.f, py = ey + p->look_y * 2.6f * s + 0.5f * s;
                disc(px, py, 4.4f * s, CC_INK);
                disc(px, py, 2.0f * s, mix565(CC_INK, mood->accent, 70u));
                disc(px - 1.4f * s, py - 2.0f * s, 1.6f * s, U_WHITE);
            }
            break;
        }
    }
    if (p->pick > 0.0f) {                         /* the arm up at the nose, in front of the face */
        u_cap(L[1].hx, L[1].hy, L[1].fx, L[1].fy, 5.0f, 0, o, U_LINE);
        u_cap(L[1].hx, L[1].hy, L[1].fx, L[1].fy, 5.0f, 0, 0.0f, body);
        u_ell(L[1].fx, L[1].fy, 5.0f, 5.0f, 0, o, U_LINE);
        u_ell(L[1].fx, L[1].fy, 5.0f, 5.0f, 0, 0.0f, hoof);
    }
    /* the accessory, last, on top of the head; it lags the head a little and jiggles */
    if (p->accessory_id != ACC_NONE) {
        float ax, ay;
        xp(40.0f, -100.0f, 1, &ax, &ay);
        if (r) {
            ax += r->acc_x;
            ay += r->acc_y;
        }
        draw_accessory(p->accessory_id, ax, ay, sc * ux.f, -(rot + hr) * ux.f + (r ? r->acc_x * 0.03f : 0.0f), T);
    }
    if (p->tint)
        dr_solid_end();
}

/* -------------------------------------------------------------- accessories --- */
/* at (x, y) the top of the head; s the scale (negative: facing left), rot its tilt in radians */
static void acc_pt(float x, float y, float s, float cs, float sn, float lx, float ly, float *ox, float *oy)
{
    lx *= s;
    ly *= ss_absf(s);
    *ox = x + lx * cs - ly * sn;
    *oy = y + lx * sn + ly * cs;
}
#define AP(lx, ly, X, Y) acc_pt(x, y, s, cs, sn, (lx), (ly), &(X), &(Y))

static void draw_accessory(int id, float x, float y, float s, float rot, float T)
{
    float cs = fm_cosf(rot), sn = fm_sinf(rot), a = ss_absf(s), ax, ay, bx, by, cx, cy, dx, dy;
    uint16_t ac = mood->accent;
    int i;
    switch (id) {
    case ACC_FLOWERS:                             /* a crown of daisies */
        for (i = 0; i < 5; i++) {
            float lx = -14.0f + 7.0f * (float)i, ly = -2.0f - 3.0f * fm_sinf(3.14159f * (float)i / 4.0f);
            int k;
            AP(lx, ly, ax, ay);
            for (k = 0; k < 5; k++)
                disc(ax + 2.6f * a * fm_cosf((float)k * 1.2566f), ay + 2.6f * a * fm_sinf((float)k * 1.2566f), 2.0f * a,
                     i & 1 ? U_WHITE : lighten(ac, 120u));
            disc(ax, ay, 1.6f * a, CC_YELLOW);
        }
        break;
    case ACC_NIGHTCAP:                            /* a floppy sleeping cap with a pompom */
        AP(-13.0f, 0.0f, ax, ay);
        AP(13.0f, -2.0f, bx, by);
        AP(-4.0f + 3.0f * fm_sinf(T / 500.0f), -26.0f, cx, cy);
        AP(-20.0f + 2.0f * fm_sinf(T / 400.0f), -16.0f, dx, dy);
        tri(ax, ay, bx, by, cx, cy, ac);
        tri(ax, ay, cx, cy, dx, dy, ac);
        for (i = 0; i < 4; i++) {
            float u = (float)i / 3.0f, ux0, uy0;
            AP(ss_lerp(-12.0f, 12.0f, u), ss_lerp(-1.0f, -2.5f, u), ux0, uy0);
            disc(ux0, uy0, 3.0f * a, U_WHITE);
        }
        disc(dx, dy, 4.0f * a, U_WHITE);
        disc(ax + (cx - ax) * 0.5f, ay + (cy - ay) * 0.5f, 1.5f * a, CC_YELLOW);
        break;
    case ACC_CAP:                                 /* a cap on backwards */
        AP(0.0f, -6.0f, ax, ay);
        ell(ax, ay, 14.0f * a, 9.0f * a, ac);
        AP(-14.0f, -1.0f, bx, by);
        AP(-26.0f, 1.0f, cx, cy);
        cap(bx, by, cx, cy, 2.5f * a, darken(ac, 60u));
        AP(2.0f, -10.0f, dx, dy);
        disc(dx, dy, 3.0f * a, lighten(ac, 120u));
        AP(0.0f, -15.0f, dx, dy);
        disc(dx, dy, 1.6f * a, darken(ac, 80u));
        break;
    case ACC_HALO:                                /* a glowing ring floating above */
        AP(0.0f, -14.0f + 2.0f * fm_sinf(T / 400.0f), ax, ay);
        glow(ax, ay, 20.0f * a, scale565(ac, 110u), 180u);
        ring_a(ax, ay, 14.0f * a, 4.0f * a, 2.5f * a, CC_GOLD_L, 256u);
        break;
    case ACC_TIARA:
        for (i = 0; i < 5; i++) {
            float lx = -12.0f + 6.0f * (float)i, h = i == 2 ? 13.0f : i & 1 ? 7.0f : 9.0f;
            AP(lx - 3.0f, 0.0f, ax, ay);
            AP(lx + 3.0f, 0.0f, bx, by);
            AP(lx, -h, cx, cy);
            tri(ax, ay, bx, by, cx, cy, CC_GOLD);
        }
        AP(-14.0f, 0.0f, ax, ay);
        AP(14.0f, 0.0f, bx, by);
        cap(ax, ay, bx, by, 1.6f * a, CC_GOLD_D);
        AP(0.0f, -6.0f, ax, ay);
        disc(ax, ay, 2.6f * a, ac);
        disc(ax - 0.8f * a, ay - 0.8f * a, 0.9f * a, U_WHITE);
        break;
    case ACC_WITCH: {                             /* friendly: a floppy tip, a buckle, stars */
        float flop = 3.0f * fm_sinf(T / 450.0f);
        AP(-20.0f, 0.0f, ax, ay);
        AP(20.0f, 0.0f, bx, by);
        ell((ax + bx) * 0.5f, (ay + by) * 0.5f, 20.0f * a, 4.0f * a, darken(ac, 90u));
        AP(-11.0f, -1.0f, ax, ay);
        AP(11.0f, -1.0f, bx, by);
        AP(2.0f, -22.0f, cx, cy);
        AP(12.0f + flop, -30.0f, dx, dy);
        tri(ax, ay, bx, by, cx, cy, darken(ac, 60u));
        cap(cx, cy, dx, dy, 2.5f * a, darken(ac, 60u));
        AP(0.0f, -4.0f, ax, ay);
        box(ax - 9.0f * a, ay - 2.0f * a, 18.0f * a, 4.0f * a, U_BONE);
        AP(-5.0f, -14.0f, ax, ay);
        star4(ax, ay, 3.0f * a, U_BONE);
        break;
    }
    case ACC_BOW:
        AP(10.0f, -4.0f, ax, ay);
        AP(1.0f, -12.0f, bx, by);
        AP(1.0f, 4.0f, cx, cy);
        tri(ax, ay, bx, by, cx, cy, ac);
        AP(19.0f, -12.0f, bx, by);
        AP(19.0f, 4.0f, cx, cy);
        tri(ax, ay, bx, by, cx, cy, ac);
        disc(ax, ay, 3.2f * a, lighten(ac, 90u));
        break;
    case ACC_GOGGLES: {                           /* VR goggles over the eye */
        AP(4.0f, 16.0f, ax, ay);
        AP(-14.0f, 10.0f, bx, by);
        cap(ax, ay, bx, by, 2.0f * a, U_LINE);
        box(ax - 9.0f * a, ay - 6.0f * a, 20.0f * a, 13.0f * a, U_LINE);
        box(ax - 7.5f * a, ay - 4.5f * a, 17.0f * a, 10.0f * a, ac);
        box(ax - 6.0f * a + (float)((uint32_t)(T / 120.0f) % 8u) * a, ay - 3.0f * a, 3.0f * a, 7.0f * a, lighten(ac, 160u));
        glow(ax, ay, 16.0f * a, scale565(ac, 120u), 150u);
        break;
    }
    case ACC_PARTY:                               /* a striped party hat with a pompom */
        AP(-10.0f, 0.0f, ax, ay);
        AP(10.0f, 0.0f, bx, by);
        AP(0.0f, -26.0f, cx, cy);
        tri(ax, ay, bx, by, cx, cy, ac);
        for (i = 1; i < 3; i++) {
            float u = (float)i / 3.0f, l0x, l0y, l1x, l1y;
            AP(-10.0f * (1.0f - u), -26.0f * u, l0x, l0y);
            AP(10.0f * (1.0f - u), -26.0f * u, l1x, l1y);
            cap(l0x, l0y, l1x, l1y, 1.5f * a, CC_YELLOW);
        }
        disc(cx, cy, 3.5f * a, U_WHITE);
        break;
    case ACC_CROWN:
        AP(-12.0f, -9.0f, ax, ay);
        box(ax, ay, 24.0f * a, 9.0f * a, CC_GOLD);
        for (i = 0; i < 4; i++) {
            float lx = -12.0f + 8.0f * (float)i;
            AP(lx, -9.0f, ax, ay);
            AP(lx + 8.0f, -9.0f, bx, by);
            AP(lx + 4.0f, -18.0f, cx, cy);
            if (i < 3)
                tri(ax, ay, bx, by, cx, cy, CC_GOLD);
        }
        AP(0.0f, -5.0f, ax, ay);
        disc(ax, ay, 2.4f * a, ac);
        break;
    case ACC_PROPELLER: {                         /* a beanie with a spinning propeller */
        float sp = fm_sinf(T / 40.0f);
        AP(0.0f, -4.0f, ax, ay);
        ell(ax, ay, 13.0f * a, 8.0f * a, ac);
        AP(0.0f, -12.0f, bx, by);
        cap(ax, ay - 6.0f * a, bx, by, 1.2f * a, U_LINE);
        ell(bx, by, 12.0f * a * ss_absf(sp) + 1.0f, 2.0f * a, CC_YELLOW);
        disc(bx, by, 2.0f * a, CC_PINK);
        break;
    }
    case ACC_BUNNY:
        for (i = -1; i <= 1; i += 2) {
            AP(6.0f * (float)i, -18.0f + 2.0f * fm_sinf(T / 380.0f + (float)i), ax, ay);
            ell(ax, ay, 4.5f * a, 13.0f * a, U_WHITE);
            ell(ax, ay + 1.0f * a, 2.2f * a, 9.0f * a, CC_BLUSH);
        }
        break;
    default:
        break;
    }
}
#undef AP
