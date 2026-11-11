/* SPDX-License-Identifier: MPL-2.0
 * Spooky Stable v2: drawing primitives (draw.h). From v1's, plus capsules, arcs, stars, hearts, the additive glow
 * sprite and the silhouette mode the impact frames use. */

/* the glow sprite: 16 x 16 radial alpha, made once at boot (design 2.7) */
static uint8_t glow_a[16][16];

static void draw_init(void)
{
    int32_t x, y;
    for (y = 0; y < 16; y++)
        for (x = 0; x < 16; x++) {
            float dx = ((float)x - 7.5f) / 7.5f, dy = ((float)y - 7.5f) / 7.5f, d = dx * dx + dy * dy, a;
            a = d >= 1.0f ? 0.0f : (1.0f - d) * (1.0f - d);
            glow_a[y][x] = (uint8_t)(a * 255.0f);
        }
}

static void dr_solid_begin(uint16_t c)
{
    dr_solid = c;
    dr_solid_on = 1;
}
static void dr_solid_end(void) { dr_solid_on = 0; }

static void hl(int32_t x0, int32_t x1, int32_t y, uint16_t c)
{
    uint16_t sc, *p;
    if ((uint32_t)y >= 240u)
        return;
    if (x0 < 0)
        x0 = 0;
    if (x1 > 239)
        x1 = 239;
    if (x0 > x1)
        return;
    sc = swap16(dr_solid_on ? dr_solid : c);
    p = &cv_px[(uint32_t)y * 240u + (uint32_t)x0];
    for (; x0 <= x1; x0++)
        *p++ = sc;
}

static void hl_a(int32_t x0, int32_t x1, int32_t y, uint16_t c, uint32_t a)
{
    uint16_t *p;
    if (dr_solid_on) {                             /* a silhouette: the faint parts left out, the rest solid */
        if (a > 96u)
            hl(x0, x1, y, c);
        return;
    }
    if (a >= 250u) {
        hl(x0, x1, y, c);
        return;
    }
    if ((uint32_t)y >= 240u || a == 0)
        return;
    if (x0 < 0)
        x0 = 0;
    if (x1 > 239)
        x1 = 239;
    p = &cv_px[(uint32_t)y * 240u];
    for (; x0 <= x1; x0++)
        p[x0] = swap16(mix565(swap16(p[x0]), c, a));
}

static void ell_a(float fcx, float fcy, float frx, float fry, uint16_t c, uint32_t a)   /* filled ellipse */
{
    int32_t cx = (int32_t)(fcx + 0.5f) + dr_ox, cy = (int32_t)(fcy + 0.5f) + dr_oy;
    int32_t rx = (int32_t)(frx + 0.5f), ry = (int32_t)(fry + 0.5f), dy;
    float iry;
    if (rx < 1 || ry < 1 || a == 0) {
        if (frx > 0.3f && fry > 0.3f && a)
            hl_a(cx, cx, cy, c, a);
        return;
    }
    if (cy + ry < 0 || cy - ry > 239 || cx + rx < 0 || cx - rx > 239)
        return;
    iry = 1.0f / (float)ry;
    for (dy = -ry; dy <= ry; dy++) {
        float q = (float)dy * iry;
        int32_t w = (int32_t)((float)rx * fm_sqrtf(fm_maxf(0.0f, 1.0f - q * q)) + 0.5f);
        hl_a(cx - w, cx + w, cy + dy, c, a);
    }
}
static void ell(float x, float y, float rx, float ry, uint16_t c) { ell_a(x, y, rx, ry, c, 256u); }
static void disc(float x, float y, float r, uint16_t c) { ell_a(x, y, r, r, c, 256u); }
static void disc_a(float x, float y, float r, uint16_t c, uint32_t a) { ell_a(x, y, r, r, c, a); }

static void ring_a(float fcx, float fcy, float frx, float fry, float th, uint16_t c, uint32_t a)   /* outline */
{
    int32_t cx = (int32_t)(fcx + 0.5f) + dr_ox, cy = (int32_t)(fcy + 0.5f) + dr_oy;
    int32_t rx = (int32_t)(frx + 0.5f), ry = (int32_t)(fry + 0.5f), dy;
    int32_t rix = (int32_t)(frx - th + 0.5f), riy = (int32_t)(fry - th + 0.5f);
    if (rx < 1 || ry < 1 || a == 0)
        return;
    if (cy + ry < 0 || cy - ry > 239 || cx + rx < 0 || cx - rx > 239)
        return;
    for (dy = -ry; dy <= ry; dy++) {
        float q = (float)dy / (float)ry;
        int32_t wo = (int32_t)((float)rx * fm_sqrtf(fm_maxf(0.0f, 1.0f - q * q)) + 0.5f);
        if (rix > 0 && riy > 0 && (dy < 0 ? -dy : dy) < riy) {
            float qi = (float)dy / (float)riy;
            int32_t wi = (int32_t)((float)rix * fm_sqrtf(fm_maxf(0.0f, 1.0f - qi * qi)) + 0.5f);
            hl_a(cx - wo, cx - wi, cy + dy, c, a);
            hl_a(cx + wi, cx + wo, cy + dy, c, a);
        } else {
            hl_a(cx - wo, cx + wo, cy + dy, c, a);
        }
    }
}

static void tri_a(float x0, float y0, float x1, float y1, float x2, float y2, uint16_t c, uint32_t a)
{
    float t, xa, xb;
    int32_t y, ys, ye;
    x0 += (float)dr_ox; x1 += (float)dr_ox; x2 += (float)dr_ox;
    y0 += (float)dr_oy; y1 += (float)dr_oy; y2 += (float)dr_oy;
    if (y0 > y1) { t = y0; y0 = y1; y1 = t; t = x0; x0 = x1; x1 = t; }
    if (y1 > y2) { t = y1; y1 = y2; y2 = t; t = x1; x1 = x2; x2 = t; }
    if (y0 > y1) { t = y0; y0 = y1; y1 = t; t = x0; x0 = x1; x1 = t; }
    if (y2 - y0 < 0.5f)
        return;
    ys = (int32_t)(y0 + 0.5f);
    ye = (int32_t)(y2 + 0.5f);
    if (ys < 0)
        ys = 0;
    if (ye > 240)
        ye = 240;
    for (y = ys; y < ye; y++) {
        float fy = (float)y;                   /* (each edge's parameter clamped: a near-flat edge never extrapolates) */
        xa = x0 + (x2 - x0) * ss_clamp01((fy - y0) / (y2 - y0));
        if (fy < y1)
            xb = y1 - y0 > 0.001f ? x0 + (x1 - x0) * ss_clamp01((fy - y0) / (y1 - y0)) : x1;
        else
            xb = y2 - y1 > 0.001f ? x1 + (x2 - x1) * ss_clamp01((fy - y1) / (y2 - y1)) : x1;
        if (xa > xb) { t = xa; xa = xb; xb = t; }
        hl_a((int32_t)(xa + 0.5f), (int32_t)(xb - 0.5f), y, c, a);
    }
}
static void tri(float x0, float y0, float x1, float y1, float x2, float y2, uint16_t c)
{
    tri_a(x0, y0, x1, y1, x2, y2, c, 256u);
}
static void quad(float x0, float y0, float x1, float y1, float x2, float y2, float x3, float y3, uint16_t c)
{
    tri(x0, y0, x1, y1, x2, y2, c);
    tri(x0, y0, x2, y2, x3, y3, c);
}

static void box_a(float x, float y, float w, float h, uint16_t c, uint32_t a)
{
    int32_t x0 = (int32_t)(x + 0.5f) + dr_ox, y0 = (int32_t)(y + 0.5f) + dr_oy;
    int32_t x1 = x0 + (int32_t)(w + 0.5f) - 1, y1 = y0 + (int32_t)(h + 0.5f);
    if (y0 < 0)
        y0 = 0;
    if (y1 > 240)
        y1 = 240;
    for (; y0 < y1; y0++)
        hl_a(x0, x1, y0, c, a);
}
static void box(float x, float y, float w, float h, uint16_t c) { box_a(x, y, w, h, c, 256u); }

/* a capsule: a thick line with round ends, as a quad and two discs */
static void cap(float x0, float y0, float x1, float y1, float r, uint16_t c)
{
    float dx = x1 - x0, dy = y1 - y0, l = fm_sqrtf(dx * dx + dy * dy), nx, ny;
    if (r < 0.75f) {
        int32_t n = (int32_t)l + 1, i;
        for (i = 0; i <= n; i++) {
            float t = (float)i / (float)n;
            int32_t px = (int32_t)(x0 + dx * t + 0.5f) + dr_ox;
            hl(px, px, (int32_t)(y0 + dy * t + 0.5f) + dr_oy, c);
        }
        return;
    }
    if (l > 0.5f) {
        nx = -dy / l * r;
        ny = dx / l * r;
        quad(x0 + nx, y0 + ny, x1 + nx, y1 + ny, x1 - nx, y1 - ny, x0 - nx, y0 - ny, c);
    }
    disc(x0, y0, r, c);
    disc(x1, y1, r, c);
}

/* an arc as a row of dots (whirlpools, spirals, smiles): angles in radians, screen y down */
static void arc_dots(float x, float y, float r, float a0, float a1, float th, uint16_t c)
{
    float span = a1 - a0, step;
    int32_t n, i;
    if (r < 0.5f)
        return;
    n = (int32_t)(ss_absf(span) * r / fm_maxf(th * 0.8f, 0.8f)) + 1;
    if (n > 160)
        n = 160;
    step = span / (float)n;
    for (i = 0; i <= n; i++) {
        float a = a0 + step * (float)i;
        disc(x + r * fm_cosf(a), y + r * fm_sinf(a), th * 0.5f, c);
    }
}

/* a four-point star */
static void star4(float x, float y, float r, uint16_t c)
{
    float w = fm_maxf(r * 0.32f, 0.8f);
    tri(x - w, y, x + w, y, x, y - r, c);
    tri(x - w, y, x + w, y, x, y + r, c);
    tri(x, y - w, x, y + w, x - r, y, c);
    tri(x, y - w, x, y + w, x + r, y, c);
}

/* a heart, s = half its width */
static void heart(float x, float y, float s, uint16_t c)
{
    disc(x - s * 0.5f, y - s * 0.15f, s * 0.55f, c);
    disc(x + s * 0.5f, y - s * 0.15f, s * 0.55f, c);
    tri(x - s * 1.02f, y + s * 0.05f, x + s * 1.02f, y + s * 0.05f, x, y + s * 1.05f, c);
}

/* the additive glow: the 16 x 16 sprite stretched to radius r, tinted, added (no alpha blend over big areas) */
static void glow(float fx, float fy, float r, uint16_t tint, uint32_t amt)
{
    int32_t cx = (int32_t)fx + dr_ox, cy = (int32_t)fy + dr_oy, ri = (int32_t)r, x, y;
    float inv;
    if (ri < 2 || amt == 0 || dr_glows >= DR_GLOW_MAX || dr_solid_on)
        return;
    if (ri > 40)
        ri = 40;
    dr_glows++;
    inv = 8.0f / (float)ri;
    for (y = -ri; y < ri; y++) {
        int32_t py = cy + y, gy = (int32_t)((float)(y + ri) * inv);
        uint16_t *row;
        if ((uint32_t)py >= 240u)
            continue;
        if (gy > 15)
            gy = 15;
        row = &cv_px[(uint32_t)py * 240u];
        for (x = -ri; x < ri; x++) {
            int32_t px = cx + x, gx = (int32_t)((float)(x + ri) * inv);
            uint32_t a;
            if ((uint32_t)px >= 240u)
                continue;
            if (gx > 15)
                gx = 15;
            a = (uint32_t)glow_a[gy][gx] * amt >> 8;
            if (a > 4u)
                row[px] = swap16(add565(swap16(row[px]), scale565(tint, a)));
        }
    }
}
