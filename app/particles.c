/* SPDX-License-Identifier: MPL-2.0
 * Spooky Stable v2: particles (particles.h). */

static particle_t parts[NPARTS] PLAT_POOL;
static uint32_t part_cur;
static float part_acc;                 /* ms not yet stepped */
static uint8_t part_sucking;
static int16_t suck_px, suck_py;

#define Q4(v) ((int16_t)((v) * 16.0f))

static uint16_t part_auto_color(int type)
{
    uint32_t r = ss_rnd() & 3u;
    switch (type) {
    case PT_SLIME:
    case PT_SNOT:
        return r & 1u ? CC_SNOT : lighten(CC_SNOT, 60u);
    case PT_DROPLET:
        return r & 1u ? CC_PEE : HEX(0x78E6FF);
    case PT_BAT:
        return mix565(mood->accent, U_LINE, 120u);
    case PT_DUCK:
        return CC_YELLOW;
    case PT_POOPLET:
        return CC_BROWN;
    case PT_CONFETTI:
        return RAINBOW[ss_rnd() % 6u];
    case PT_DUST:
        return lighten(mood->ground, 90u);
    default:
        return r == 0 ? mood->accent : r == 1 ? mood->chr : r == 2 ? lighten(mood->accent, 120u) : U_WHITE;
    }
}

static particle_t *part_spawn(int type, float x, float y, float vx, float vy, int life, uint16_t c, int size)
{
    uint32_t i;
    particle_t *p = 0;
    for (i = 0; i < NPARTS; i++) {
        uint32_t j = (part_cur + i) % NPARTS;
        if (!parts[j].life) {
            p = &parts[j];
            part_cur = j + 1u;
            break;
        }
    }
    if (!p) {                                  /* full: the one nearest its end goes */
        uint32_t best = 0, bl = 999;
        for (i = 0; i < NPARTS; i++)
            if (parts[i].life < bl) {
                bl = parts[i].life;
                best = i;
            }
        p = &parts[best];
    }
    if (life < 1)
        life = 1;
    if (life > 255)
        life = 255;
    p->x = Q4(x);
    p->y = Q4(y);
    p->vx = Q4(vx * 2.0f);                     /* given per 60 Hz frame (v1's), stepped per 30 Hz tick */
    p->vy = Q4(vy * 2.0f);
    p->life = p->max_life = (uint8_t)life;
    p->type = (uint8_t)type;
    p->size = (uint8_t)(size < 1 ? 1 : size > 30 ? 30 : size);
    p->color = c ? c : part_auto_color(type);
    p->floor = 0;
    p->g = 0;
    p->aux = (uint8_t)ss_rnd();
    p->flags = 0;
    switch (type) {                            /* each type's own physics defaults */
    case PT_SLIME:
    case PT_SNOT:
        p->g = 10;                             /* v1's 0.15 px a frame per frame */
        break;
    case PT_DROPLET:
        p->g = 14;                             /* v1's 0.22 */
        break;
    case PT_POOPLET:
        p->g = 22;
        break;
    case PT_DUCK:
        p->g = 12;
        break;
    case PT_CONFETTI:
        p->g = 4;
        break;
    case PT_FLY:                               /* an orbit round where it was spawned */
        p->vx = Q4(x);
        p->vy = Q4(y);
        break;
    case PT_SPARKLE:
    case PT_ORB:
        p->flags = PF_ADD;
        break;
    default:
        break;
    }
    return p;
}

static int part_n(int n)
{
    float f = (float)n * part_mult;
    return f > 60.0f ? 60 : (int)f;
}

static void part_burst(int type, float x, float y, int n, float v0, float v1, int life, uint16_t c, int size)
{
    int i;
    n = part_n(n);
    for (i = 0; i < n; i++) {
        float a = TAU * ((float)i + ss_rndf() * 0.6f) / (float)(n ? n : 1), v = ss_lerp(v0, v1, ss_rndf());
        part_spawn(type, x, y, v * fm_cosf(a), v * fm_sinf(a), (int)((float)life * (0.7f + 0.5f * ss_rndf())), c, size);
    }
}

static void part_fan(int type, float x, float y, int n, float ang, float spread, float v0, float v1, int life,
                     uint16_t c, int size)
{
    int i;
    n = part_n(n);
    for (i = 0; i < n; i++) {
        float a = ang + spread * ss_rnds(), v = ss_lerp(v0, v1, ss_rndf());
        part_spawn(type, x, y, v * fm_cosf(a), v * fm_sinf(a), (int)((float)life * (0.7f + 0.5f * ss_rndf())), c, size);
    }
}

static void part_mood_burst(float x, float y, int n, float v0, float v1)
{
    int t = mood->particle;
    part_burst(t, x, y, n, v0, v1, t == PT_FLY ? 90 : 34, 0, t == PT_STAR ? 4 : 3);
    if (mood_i == MOOD_GLITCH)
        part_burst(PT_CONFETTI, x, y, n / 2, v0, v1, 36, 0, 3);
}

static int part_count(void)
{
    int i, n = 0;
    for (i = 0; i < NPARTS; i++)
        n += parts[i].life != 0;
    return n;
}

static void part_clear(void)
{
    int i;
    for (i = 0; i < NPARTS; i++)
        parts[i].life = 0;
    part_sucking = 0;
}

static void part_suck(float x, float y)
{
    part_sucking = 30;
    suck_px = Q4(x);
    suck_py = Q4(y);
}

/* one tick: 33 ms */
static void part_tick(void)
{
    int i;
    for (i = 0; i < NPARTS; i++) {
        particle_t *p = &parts[i];
        if (!p->life)
            continue;
        if (--p->life == 0) {
            if (p->type == PT_BUBBLE)          /* a bubble pops */
                part_burst(PT_SPARKLE, (float)p->x / 16.0f, (float)p->y / 16.0f, 4, 0.8f, 1.6f, 6, U_WHITE, 1);
            continue;
        }
        if (part_sucking && p->type != PT_FLY) {   /* the toilet's pull: a spiral in */
            int32_t dx = suck_px - p->x, dy = suck_py - p->y;
            p->vx = (int16_t)((p->vx * 3 + dx / 4 - dy / 6) / 4);
            p->vy = (int16_t)((p->vy * 3 + dy / 4 + dx / 6) / 4);
            if (dx * dx + dy * dy < 16 * 16 * 36) {
                p->life = 0;
                continue;
            }
            p->g = 0;
            p->floor = 0;
        }
        if (p->flags & PF_STAIN) {
            if (p->aux-- == 0)
                p->life = 0;
            continue;
        }
        switch (p->type) {
        case PT_STAR:
        case PT_DUST:
            p->vx = (int16_t)(p->vx * 92 / 100);
            p->vy = (int16_t)(p->vy * 92 / 100);
            if (p->type == PT_DUST)
                p->vy -= 1;
            break;
        case PT_SPARKLE:
        case PT_ORB:
            p->vx = (int16_t)(p->vx * 94 / 100);
            p->vy = (int16_t)(p->vy * 94 / 100 - 1);
            break;
        case PT_BUBBLE:
            p->vy = (int16_t)((p->vy * 9 - 20) / 10);
            p->vx = (int16_t)(p->vx * 9 / 10 + (int32_t)(fm_sinf((float)p->life * 0.4f) * 6.0f));
            break;
        case PT_FLY:                           /* round and round its centre, buzzing */
            p->aux += 23;
            p->x = (int16_t)(p->vx + (int32_t)(fm_cosf((float)p->aux * 0.0245f) * (float)(p->size * 48)));
            p->y = (int16_t)(p->vy + (int32_t)(fm_sinf((float)p->aux * 0.049f) * (float)(p->size * 24)));
            p->vy -= 2;
            continue;
        case PT_HEART:
        case PT_NOTE:
        case PT_CRESCENT:
            p->vx = (int16_t)(p->vx * 9 / 10 + (int32_t)(fm_sinf((float)p->life * 0.3f) * 5.0f));
            p->vy = (int16_t)((p->vy * 9 - 14) / 10);
            break;
        case PT_BAT:
            p->vx = (int16_t)(p->vx * 96 / 100);
            p->vy = (int16_t)(p->vy * 9 / 10 + (int32_t)(fm_sinf((float)p->life * 0.5f) * 10.0f) - 2);
            break;
        case PT_PETAL:
            p->vx = (int16_t)(p->vx * 95 / 100 + (int32_t)(fm_sinf((float)p->life * 0.2f) * 6.0f));
            p->vy = (int16_t)((p->vy * 9 + 8) / 10);
            break;
        case PT_CONFETTI:
            p->vx = (int16_t)(p->vx * 95 / 100);
            p->vy = (int16_t)(p->vy * 95 / 100);
            break;
        case PT_PIXEL:
            p->x += (int16_t)(ss_rndi(3) - 1) * 16;
            p->y += (int16_t)(ss_rndi(3) - 1) * 16;
            break;
        case PT_CLOUD:
            p->vx = (int16_t)(p->vx * 9 / 10);
            p->vy = (int16_t)((p->vy * 9 - 6) / 10);
            break;
        default:
            break;
        }
        p->vy += p->g;
        p->x += p->vx;
        p->y += p->vy;
        if (p->type == PT_DUCK) {              /* rubber ducks bounce off everything */
            if (p->x < 16 * 4 || p->x > 16 * 236)
                p->vx = (int16_t)-p->vx;
            if (p->y > 16 * 228 && p->vy > 0)
                p->vy = (int16_t)(-p->vy * 6 / 10);
        }
        if (p->floor && p->y >= (int16_t)(p->floor * 16) && p->vy > 0) {
            p->y = (int16_t)(p->floor * 16);
            if (part_on_floor && (p->type == PT_DROPLET || p->type == PT_POOPLET))
                part_on_floor(p);
            if (p->type == PT_SLIME || p->type == PT_SNOT) {
                p->flags |= PF_STAIN;          /* a splat that stays two frames */
                p->aux = 2;
                p->vx = p->vy = 0;
            } else {
                p->life = 0;
            }
        }
        if (p->y > 16 * 300 || p->y < -16 * 80 || p->x < -16 * 60 || p->x > 16 * 300)
            p->life = 0;
    }
    if (part_sucking)
        part_sucking--;
}

static void particles_update(float dt)
{
    part_acc += dt;
    if (part_acc > 200.0f)
        part_acc = 200.0f;
    while (part_acc >= 33.3f) {
        part_acc -= 33.3f;
        part_tick();
    }
}

static void draw_duck(float x, float y, float s, uint16_t c)
{
    ell(x, y + 2.0f * s, 6.0f * s, 4.0f * s, c);
    disc(x + 4.0f * s, y - 3.0f * s, 3.2f * s, c);
    tri(x + 6.5f * s, y - 4.0f * s, x + 6.5f * s, y - 1.5f * s, x + 9.5f * s, y - 2.5f * s, CC_ORANGE);
    disc(x + 4.8f * s, y - 3.8f * s, fm_maxf(0.8f * s, 0.6f), CC_INK);
}

static void draw_pooplet(float x, float y, float s)
{
    ell(x, y, 6.0f * s, 3.0f * s, CC_BROWN);
    ell(x, y - 3.0f * s, 4.5f * s, 2.5f * s, CC_BROWN);
    ell(x + 0.5f * s, y - 5.5f * s, 2.8f * s, 2.0f * s, CC_BROWN);
    disc(x - 1.5f * s, y - 1.0f * s, fm_maxf(0.8f * s, 0.6f), U_WHITE);
    disc(x + 1.5f * s, y - 1.0f * s, fm_maxf(0.8f * s, 0.6f), U_WHITE);
}

static void particles_draw(void)
{
    int i;
    for (i = 0; i < NPARTS; i++) {
        particle_t *p = &parts[i];
        float x, y, f, s;
        if (!p->life)
            continue;
        x = (float)p->x / 16.0f;
        y = (float)p->y / 16.0f;
        f = (float)p->life / (float)p->max_life;
        s = (float)p->size;
        if (p->flags & PF_STAIN) {
            ell(x, y, s * 1.8f, s * 0.6f, p->color);
            continue;
        }
        switch (p->type) {
        case PT_STAR: {
            float r = s * (0.4f + 0.6f * f) + (float)((p->life >> 1) & 1u);   /* twinkle */
            star4(x, y, r + 1.0f, p->color);
            break;
        }
        case PT_SPARKLE:
            box(x - s, y, 2.0f * s + 1.0f, 1.0f, p->color);
            box(x, y - s, 1.0f, 2.0f * s + 1.0f, p->color);
            disc(x, y, 1.0f, U_WHITE);
            break;
        case PT_ORB:
            disc_a(x, y, s + 2.0f, p->color, (uint32_t)(60.0f + 80.0f * f));
            disc_a(x, y, s, lighten(p->color, 120u), (uint32_t)(120.0f + 130.0f * f));
            break;
        case PT_SLIME:
        case PT_SNOT:
            disc(x, y, s, p->color);
            disc(x - s * 0.3f, y - s * 0.3f, fm_maxf(s * 0.3f, 0.6f), lighten(p->color, 150u));
            break;
        case PT_BUBBLE:
            ring_a(x, y, s + 1.0f, s + 1.0f, 1.0f, p->color, 230u);
            disc(x - s * 0.35f, y - s * 0.35f, fm_maxf(s * 0.25f, 0.7f), U_WHITE);
            break;
        case PT_FLY: {
            uint32_t wing = (uint32_t)p->life & 1u;
            disc(x, y, 1.6f, U_LINE);
            ell_a(x - 1.5f, y - 1.5f - (float)wing, 1.6f, 1.0f, U_WHITE, 200u);
            ell_a(x + 1.5f, y - 1.5f - (float)wing, 1.6f, 1.0f, U_WHITE, 200u);
            break;
        }
        case PT_HEART:
            heart(x, y, s * (0.6f + 0.4f * f), p->color);
            break;
        case PT_BAT: {                         /* a little round bat, its wings flapping */
            float w = (p->life & 2u) ? 1.0f : -1.0f;
            tri(x - 1.0f, y, x - 4.0f * s / 3.0f - 3.0f, y - 2.0f * w, x - 2.0f, y + 2.0f, p->color);
            tri(x + 1.0f, y, x + 4.0f * s / 3.0f + 3.0f, y - 2.0f * w, x + 2.0f, y + 2.0f, p->color);
            disc(x, y, 2.0f, p->color);
            disc(x - 0.8f, y - 0.5f, 0.6f, U_BONE);
            disc(x + 0.8f, y - 0.5f, 0.6f, U_BONE);
            break;
        }
        case PT_PETAL: {
            float a = (float)p->life * 0.35f, cx = fm_cosf(a) * s, sy = fm_sinf(a) * s * 0.5f;
            ell(x, y, ss_absf(cx) + 0.8f, ss_absf(sy) + 1.0f, p->color);
            break;
        }
        case PT_CONFETTI:
            if (p->life & 2u)
                box(x - s * 0.5f, y - 0.5f, s, 1.5f, p->color);
            else
                box(x - 0.5f, y - s * 0.5f, 1.5f, s, p->color);
            break;
        case PT_PIXEL:
            box(x - 1.0f, y, 2.0f, 2.0f, HEX(0xFF0040));
            box(x + 1.0f, y, 2.0f, 2.0f, HEX(0x00FFC0));
            box(x, y, 2.0f, 2.0f, p->color);
            break;
        case PT_CLOUD: {
            float r = s * (1.0f + 1.4f * (1.0f - f));
            uint32_t a = (uint32_t)(230.0f * f);
            disc_a(x - r * 0.5f, y + r * 0.1f, r * 0.7f, p->color, a);
            disc_a(x + r * 0.5f, y + r * 0.1f, r * 0.7f, p->color, a);
            disc_a(x, y - r * 0.25f, r * 0.8f, p->color, a);
            break;
        }
        case PT_DROPLET:
            disc(x, y, s, p->color);
            if (s > 1.5f)
                disc(x - s * 0.3f, y - s * 0.3f, fm_maxf(s * 0.3f, 0.6f), U_WHITE);
            break;
        case PT_NOTE:
            disc(x, y, 2.8f, p->color);
            box(x + 1.8f, y - 9.0f, 1.5f, 9.0f, p->color);
            box(x + 1.8f, y - 9.0f, 4.5f, 2.0f, p->color);
            break;
        case PT_CRESCENT:
            arc_dots(x, y, s + 1.5f, 1.9f, 5.0f, s * 0.9f + 1.0f, p->color);
            break;
        case PT_DUCK:
            draw_duck(x, y, s / 4.0f, p->color);
            break;
        case PT_POOPLET:
            draw_pooplet(x, y, s / 4.0f);
            break;
        default:                               /* PT_DUST */
            disc_a(x, y, s * (1.0f + (1.0f - f)), p->color, (uint32_t)(200.0f * f));
            break;
        }
    }
}
