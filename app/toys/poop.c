/* SPDX-License-Identifier: MPL-2.0
 * Spooky Stable v2: 2. Unicorn Poop (EDIT). She squats and strains (the freeze), then pops; a cute poop arcs out and
 * splats onto the pile (10 places, then the oldest fades). Each plop is a step up the scale. Every Nth landing the
 * newest stand up in a rainbow tower and dance; every 3rd lets a tiny ghost giggle away (v1's, kept as a prop).
 *   K2 drop rate: hold a key, 1 .. 12 a second (a machine gun)   K3 poop size   K4 how far each plop climbs
 *   K6 style (classic / rainbow / glitter / slime)   K7 gravity, and how tall the pile grows before the tower dance */

#define POOP_X 150.0f
#define POOP_Y 194.0f
#define POOP_FLOOR 206.0f
enum { PS_CLASSIC, PS_RAINBOW, PS_GLITTER, PS_SLIMY };

typedef struct {
    float x, y, vx, vy, ty, t, sc;
    uint32_t seq;
    uint8_t state, slot, style, pad;           /* state: 0 off, 1 falling, 2 landed, 3 fading */
} poop_t;
static const float SLOT_X[10] = {38.0f, 62.0f, 86.0f, 110.0f, 50.0f, 74.0f, 98.0f, 62.0f, 86.0f, 74.0f};
static const float SLOT_DY[10] = {0.0f, 0.0f, 0.0f, 0.0f, -16.0f, -16.0f, -16.0f, -32.0f, -32.0f, -48.0f};

static struct {
    poop_t p[12];
    uint32_t seq, landed;
    float strain_t0, tower_t0;
    uint32_t held, next_at;
    int32_t last_k;
    float last_v;
} poo;

static const preset_t POOP_PRESETS[] = {
    {P_PLOP_MARIMBA, IC_BELLS}, {P_PLOP_WOOD, IC_BLOCK}, {P_PLOP_DROP, IC_DROP}, {P_PLOP_PIZZ, IC_STRING},
    {P_PLOP_BOING, IC_SPRING}};

static float poop_gravity(void) { return 0.3f + 0.67f * kf(K7); }
static uint32_t poop_tower_every(void) { return 3u + (uint32_t)(kf(K7) * 7.0f + 0.5f); }

static void poop_init(void)
{
    int i;
    for (i = 0; i < 12; i++)
        poo.p[i].state = 0;
    poo.strain_t0 = poo.tower_t0 = -10000.0f;
    poo.held = 0;
}
static int poop_count(void)
{
    int n = 0, i;
    for (i = 0; i < 12; i++)
        n += poo.p[i].state == 1u || poo.p[i].state == 2u;
    return n;
}

static void poop_drop(uint32_t k, float vel)
{
    uint8_t used[10] = {0};
    int i, slot = -1, free_i = -1, oldest = -1, n = mood->nscale, deg, oct;
    uint32_t oseq = 0xFFFFFFFFu;
    float rx = POOP_X - 50.0f, ry = POOP_Y - 54.0f, fall;
    note_t nt;
    for (i = 0; i < 12; i++) {
        poop_t *q = &poo.p[i];
        if (q->state == 1u || q->state == 2u) {
            used[q->slot] = 1;
            if (q->seq < oseq) {
                oseq = q->seq;
                oldest = i;
            }
        } else if (q->state == 0u && free_i < 0)
            free_i = i;
    }
    for (i = 0; i < 10; i++)
        if (!used[i]) {
            slot = i;
            break;
        }
    if (slot < 0 && oldest >= 0) {             /* a full pile: the oldest fades and gives its place */
        poo.p[oldest].state = 3;
        poo.p[oldest].t = GT.T;
        slot = poo.p[oldest].slot;
    }
    if (free_i < 0 || slot < 0)
        return;
    {
        poop_t *q = &poo.p[free_i];
        float g = poop_gravity();
        q->state = 1;
        q->slot = (uint8_t)slot;
        q->seq = ++poo.seq;
        q->x = rx;
        q->y = ry;
        q->sc = 0.6f + 1.0f * kf(K3);
        q->ty = POOP_FLOOR + SLOT_DY[slot] * q->sc;
        q->vy = -1.5f;
        fall = (q->vy + fm_sqrtf(q->vy * q->vy + 2.0f * g * fm_maxf(q->ty - ry, 4.0f))) / g;   /* frames to fall */
        q->vx = (SLOT_X[slot] - rx) / fall;
        q->t = GT.T;
        q->style = (uint8_t)kv(K6);
    }
    poo.strain_t0 = GT.T;
    /* the plop: each a step (K4) further up the scale; low keys an octave down, high keys an octave up */
    oct = k < 9u ? 3 : k < 18u ? 4 : 5;
    deg = (int)((poo.seq * (uint32_t)kv(K4)) % (uint32_t)(2 * n)) + (int)(k % 9u) / 3;
    nt = snd_defaults(toy_patch(), (float)mood_degree_midi(oct, deg), vel_f(vel));
    snd_note(&nt);
    impact(0.1f, 3);
    fx_flash(U_WHITE);
}

static void poop_key(uint32_t k, float vel)
{
    poo.held |= 1u << k;
    poo.last_k = (int32_t)k;
    poo.last_v = vel;
    poo.next_at = plat_ms() + (uint32_t)(1000.0f / (1.0f + 11.0f * kf(K2)));
    poop_drop(k, vel);
}
static void poop_release(uint32_t k, uint32_t held)
{
    (void)held;
    poo.held &= ~(1u << k);
}

static void poop_mega(uint32_t k)             /* an avalanche: a golden fanfare of poops */
{
    int i;
    (void)k;
    for (i = 0; i < 8; i++) {
        particle_t *p = part_spawn(PT_POOPLET, POOP_X - 50.0f, POOP_Y - 54.0f, -1.0f - 4.0f * ss_rndf(), -3.0f - 3.0f * ss_rndf(), 80, 0, 6);
        p->floor = (uint8_t)POOP_FLOOR;
    }
    for (i = 0; i < 5; i++)
        snd_play(toy_patch(), (float)mood_degree_midi(4, i * 2), 0.7f);
    poo.tower_t0 = GT.T;
    part_burst(PT_STAR, 80.0f, 160.0f, 24, 2.0f, 6.0f, 30, CC_GOLD, 4);
    impact(0.7f, 5);
    fx_flash(CC_GOLD);
}

static void poop_floor_hit(particle_t *p)
{
    part_burst(PT_STAR, (float)p->x / 16.0f, (float)p->y / 16.0f, 4, 1.0f, 2.5f, 12, CC_GOLD, 3);
}

static void poop_update(float dt)
{
    int i;
    float k = dt / 16.67f, g = poop_gravity();
    part_on_floor = poop_floor_hit;
    if (poo.held && (int32_t)(plat_ms() - poo.next_at) >= 0) {   /* held: the machine gun */
        poo.next_at = plat_ms() + (uint32_t)(1000.0f / (1.0f + 11.0f * kf(K2)));
        poop_drop((uint32_t)poo.last_k, poo.last_v);
    }
    for (i = 0; i < 12; i++) {
        poop_t *q = &poo.p[i];
        if (q->state == 1u) {
            q->vy += g * k;
            q->y += q->vy * k;
            q->x += q->vx * k;
            if (q->y >= q->ty && q->vy > 0.0f) {
                static const uint16_t PC[4] = {CC_BROWN, CC_GOLD, CC_YELLOW, CC_ORANGE};
                q->y = q->ty;
                q->x = SLOT_X[q->slot];
                q->state = 2;
                q->t = GT.T;
                part_burst(PT_STAR, q->x, q->y, 8, 1.5f, 3.5f, 15, PC[ss_rnd() & 3u], 3);
                part_mood_burst(q->x, q->y - 6.0f, 3, 1.0f, 2.0f);
                if (q->style == PS_SLIMY)
                    part_burst(PT_SLIME, q->x, q->y - 4.0f, 5, 1.0f, 2.5f, 20, 0, 2);
                fx_trauma(0.12f + 0.15f * g);
                ui_led_kick();
                poo.landed++;
                if (poo.landed % poop_tower_every() == 0u) {   /* the rainbow tower dances, with a rising tune */
                    int d;
                    poo.tower_t0 = GT.T;
                    for (d = 0; d < 3; d++)
                        snd_play(P_PLOP_MARIMBA, (float)mood_degree_midi(5, d * 2), 0.6f);
                    part_burst(PT_CONFETTI, 76.0f, POOP_FLOOR - 50.0f, 18, 2.0f, 5.0f, 30, 0, 3);
                }
                if (poo.landed % 3u == 0u)    /* every third: a tiny ghost giggles away */
                    mini_spawn(q->x, q->y - 20.0f);
            }
        } else if (q->state == 3u && GT.T - q->t > 300.0f) {
            q->state = 0;
        }
    }
}

static void draw_poop(float x, float y, float sc, float sx, float sy, int style, int32_t hue, float T)
{
    static const float TY[3] = {0.0f, -7.0f, -13.0f}, TR[3] = {12.0f, 9.0f, 6.0f}, TH[3] = {6.0f, 5.0f, 4.0f};
    int i;
    uint16_t base = style == PS_SLIMY ? CC_SNOT : CC_BROWN, dk = style == PS_SLIMY ? HEX(0x3F6212) : CC_BROWN_D;
    if (sc < 0.05f)
        return;
    if (style == PS_GLITTER)
        glow(x, y - 6.0f * sc, 22.0f * sc, HEX(0x604010), 150u);
    for (i = 0; i < 3; i++)
        ell(x, y + TY[i] * sc * sy, (TR[i] + 1.5f) * sc * sx, (TH[i] + 1.5f) * sc * sy, dk);
    disc(x + 2.0f * sc * sx, y - 18.0f * sc * sy, 3.5f * sc, dk);
    for (i = 0; i < 3; i++) {
        uint16_t c = style == PS_RAINBOW || hue >= 0 ? hue565((hue >= 0 ? hue : (int32_t)(T / 3.0f)) + i * 300) : base;
        ell(x, y + TY[i] * sc * sy, TR[i] * sc * sx, TH[i] * sc * sy, c);
        ell_a(x - TR[i] * 0.3f * sc * sx, y + (TY[i] - TH[i] * 0.35f) * sc * sy, TR[i] * 0.35f * sc * sx,
              TH[i] * 0.3f * sc * sy, U_WHITE, style == PS_SLIMY ? 150u : 90u);
    }
    disc(x + 2.0f * sc * sx, y - 18.0f * sc * sy, 2.2f * sc, style == PS_RAINBOW || hue >= 0 ? hue565((hue >= 0 ? hue : (int32_t)(T / 3.0f)) + 900) : base);
    if (sc > 0.4f) {
        disc(x - 3.2f * sc * sx, y - 7.0f * sc * sy, 2.4f * sc, U_WHITE);
        disc(x + 3.2f * sc * sx, y - 7.0f * sc * sy, 2.4f * sc, U_WHITE);
        disc(x - 3.0f * sc * sx, y - 6.6f * sc * sy, 1.2f * sc, CC_INK);
        disc(x + 3.4f * sc * sx, y - 6.6f * sc * sy, 1.2f * sc, CC_INK);
        arc_dots(x, y - 4.0f * sc * sy, 3.0f * sc, 0.5f, 2.6f, 1.3f * sc, CC_INK);
        disc_a(x - 6.0f * sc * sx, y - 3.5f * sc * sy, 1.6f * sc, CC_PINK, 170u);
        disc_a(x + 6.0f * sc * sx, y - 3.5f * sc * sy, 1.6f * sc, CC_PINK, 170u);
        if (style == PS_GLITTER || ((uint32_t)(T / 150.0f) + (uint32_t)x) % 3u == 0u)   /* a glint */
            star4(x + 6.0f * sc, y - 9.0f * sc, (style == PS_GLITTER ? 3.5f : 2.0f) * sc, CC_GOLD_L);
    }
}

static void poop_draw(float T)
{
    int i, j, tower = T - poo.tower_t0 < 1600.0f, rank;
    float a = T - poo.strain_t0;
    pose_t p;
    mood_draw_ground(198);
    for (i = 0; i < 18; i++) {                 /* a sparkly floor */
        uint32_t h = ss_hash((uint32_t)i * 13u + 5u);
        float fx = (float)(h % 236u) + 2.0f, fy = 204.0f + (float)((h >> 9) % 34u), tw = fm_sinf(T / 200.0f + (float)(h & 63u));
        if (tw > 0.3f)
            star4(fx, fy, 3.0f * tw, (h & 1u) ? CC_GOLD_L : U_WHITE);
    }
    p = uni_pose(POOP_X, POOP_Y, 1.0f);
    if (a >= 0.0f && a < 900.0f) {
        if (a < 1.0f && fx_frozen()) {         /* the squat: braced, straining, tail high */
            p.scale_y *= 0.84f;
            p.scale_x *= 1.13f;
            p.spread = 0.6f;
            p.tail_phase = 1.0f;
            p.eye_state = EY_SQUINT;
            p.mouth_state = MO_STRAIN;
            p.cheeks = 0.8f;
            p.rot_deg = -3.0f;
        } else {                               /* the pop: up, over, settling */
            float e = ease_out_elastic(a / 480.0f), u = 1.0f - e;
            p.scale_y *= 1.0f + 0.13f * u;
            p.scale_x *= 1.0f - 0.09f * u;
            p.tail_phase = a < 400.0f ? 1.0f : ss_clamp01(1.0f - (a - 400.0f) / 300.0f);
            p.eye_state = a < 260.0f ? EY_WIDE : a < 700.0f ? EY_HAPPY : EY_OPEN;
            p.mouth_state = a < 260.0f ? MO_O : MO_GRIN;
            p.blush = ss_pulse(a, 100.0f, 900.0f);
            p.look_x = -0.8f;                  /* she looks back at what she did */
        }
    }
    if (poo.held)                              /* the machine gun: she stays squatting, buzzing */
        p.spread = 0.6f, p.vibrate = 1.0f, p.tail_phase = 1.0f, p.mouth_state = MO_GRIN;
    uni_draw(&p);
    /* the pile; while the tower dances, the five newest stand in a rainbow column */
    for (i = 0; i < 12; i++) {
        poop_t *q = &poo.p[i];
        float sx = 1.0f, sy = 1.0f, sc = q->sc;
        if (!q->state)
            continue;
        rank = 0;
        if (tower && q->state == 2u) {
            for (j = 0; j < 12; j++)
                rank += poo.p[j].state == 2u && poo.p[j].seq > q->seq;
            if (rank < 5) {
                float ta = T - poo.tower_t0, rise = ease_out_back(ta / 300.0f);
                float tx = 76.0f + fm_sinf(T / 110.0f + (float)rank * 0.9f) * (3.0f + 2.5f * (float)(4 - rank));
                float ty = POOP_FLOOR - (float)(4 - rank) * 15.0f * rise - ss_absf(fm_sinf(T / 160.0f)) * 6.0f;
                draw_poop(tx, ty, 1.0f, 1.0f + 0.1f * fm_sinf(T / 80.0f), 1.0f, q->style, (int32_t)(T / 3.0f) + rank * 200, T);
                continue;
            }
        }
        if (q->state == 1u) {                  /* falling: stretched by its speed, spinning a little */
            sy = 1.0f + fm_minf(q->vy * 0.05f, 0.3f);
            sx = 1.0f / sy;
        }
        if (q->state == 2u && T - q->t < 400.0f) {   /* the splat: squashed wide, springing back */
            float e = ease_out_elastic((T - q->t) / 400.0f);
            sx = 1.5f - 0.5f * e;
            sy = 0.5f + 0.5f * e;
        }
        if (q->state == 3u)
            sc *= ss_clamp01(1.0f - (T - q->t) / 300.0f);
        draw_poop(q->x, q->y, sc, sx, sy, q->style, -1, T);
    }
}

static void poop_clean(void) { poop_init(); }

static const toy_def_t TOY_POOP_DEF = {
    "UNICORN POOP", B_EDIT, TF_HOLD, poop_init, 0, poop_key, poop_release, poop_mega, 0, poop_update, poop_draw, poop_clean,
    POOP_PRESETS, 5,
    {{KI_SPEED, 21, 0, 2}, {KI_SIZE, 21, 0, 8}, {KI_PITCH, 4, 0, 1}, {KI_STYLE, 4, 1, 0}, {KI_GRAVITY, 21, 0, 6}},
};
