/* SPDX-License-Identifier: MPL-2.0
 * Spooky Stable v2: 8. Booger Flick (SEQ). Press a key: her hoof goes up to her nose (a slurp, at once) and out comes
 * a glittery booger, growing while the key is held. Let go: flick! It arcs across to the wall (low keys low, high keys
 * high), sticks with a squashed splat and a note, and a few sparkles drip off. The wall holds up to K7's worth; then
 * the oldest slides down, wobbling, and pops. When it gets too full a sponge wipes it clean.
 *   K2 flick power   K3 booger size   K4 splat pitch   K6 booger style (green glitter / rainbow / glow / gold)
 *   K7 how many the wall holds (4 - 12) */

#define BG_X 70.0f
#define BG_Y 196.0f
#define WALL_X 182.0f
enum { BS_GREEN, BS_RAINBOW, BS_GLOW, BS_GOLD };

typedef struct {
    float x, y, vx, vy, r, t, wob;
    uint32_t seq;
    uint8_t state, style, key, pad;            /* 0 off, 1 flying, 2 stuck, 3 sliding */
} booger_t;
#define NBOOG 14

static struct {
    booger_t b[NBOOG];
    uint32_t seq, held, stuck_since_wipe;
    uint8_t picking;
    float pick_t0, flick_t0, grow, wipe_t0;
    uint32_t key;
    float vel;
} bg;

static const preset_t BOOGER_PRESETS[] = {
    {P_PLOP_BOING, IC_SPRING}, {P_FLICK_TWANG, IC_STRING}, {P_FLICK_ZIP, IC_SLIDE}, {P_FLICK_SPROING, IC_WAVE}};

static int bg_max(void) { return 4 + (int)(kf(K7) * 8.0f + 0.5f); }
static float bg_nose_x(void) { return BG_X + 68.0f; }
static float bg_nose_y(void) { return BG_Y - 75.0f; }

static void booger_init(void)
{
    int i;
    for (i = 0; i < NBOOG; i++)
        bg.b[i].state = 0;
    bg.held = 0;
    bg.picking = 0;
    bg.pick_t0 = bg.flick_t0 = bg.wipe_t0 = -10000.0f;
}

static int bg_stuck(void)
{
    int i, n = 0;
    for (i = 0; i < NBOOG; i++)
        n += bg.b[i].state == 2u;
    return n;
}

static void booger_key(uint32_t k, float vel)
{
    bg.held |= 1u << k;
    bg.key = k;
    bg.vel = vel;
    if (!bg.picking) {                         /* up to the nose: a slurp, at once */
        bg.picking = 1;
        bg.pick_t0 = GT.T;
        bg.grow = 0.0f;
        snd_play(P_SLURP, (float)mood_key_midi(k, 4), 0.7f);
    }
}

static void bg_flick(uint32_t k, float power, int style)
{
    int i;
    booger_t *b = 0;
    float r = (2.5f + 6.0f * kf(K3)) * (0.5f + 0.5f * bg.grow), ty = 40.0f + key_big(k) * 150.0f;
    float sx = bg_nose_x() + 4.0f, sy = bg_nose_y() + 4.0f, frames;
    for (i = 0; i < NBOOG; i++)
        if (!bg.b[i].state) {
            b = &bg.b[i];
            break;
        }
    if (!b)
        return;
    b->state = 1;
    b->style = (uint8_t)style;
    b->key = (uint8_t)k;
    b->r = r;
    b->x = sx;
    b->y = sy;
    b->seq = ++bg.seq;
    frames = 26.0f - 14.0f * power;            /* a stronger flick: a flatter, quicker arc */
    b->vx = (WALL_X - sx + 6.0f) / frames;
    b->vy = (ty - sy) / frames - 0.25f * frames * 0.5f;
    b->t = GT.T;
    b->wob = 0.0f;
    {
        note_t n = snd_defaults(toy_patch(), (float)mood_key_midi(k, 5), vel_f(bg.vel));
        n.tscale = 0.7f + 0.6f * power;
        snd_note(&n);
    }
    bg.flick_t0 = GT.T;
}

static void booger_release(uint32_t k, uint32_t held)
{
    (void)held;
    bg.held &= ~(1u << k);
    if (bg.held || !bg.picking)
        return;
    bg.picking = 0;
    bg_flick(bg.key, 0.3f + 0.7f * kf(K2), kv(K6));
    fx_trauma(0.08f);
}

static void booger_mega(uint32_t k)            /* a booger storm */
{
    int i;
    for (i = 0; i < 6; i++) {
        bg.grow = 1.0f;
        bg_flick((k + (uint32_t)i * 4u) % NKEYS, 1.0f, i % 4);
    }
    part_burst(PT_CONFETTI, bg_nose_x(), bg_nose_y(), 30, 2.0f, 6.0f, 36, 0, 3);
    impact(0.6f, 4);
}

static void bg_pop(booger_t *b)
{
    part_burst(PT_STAR, b->x, b->y, 8, 1.0f, 3.0f, 14, CC_SNOT, 3);
    snd_play(P_POP, (float)mood_degree_midi(6, ss_rndi(5)), 0.5f);
    b->state = 0;
}

static void booger_update(float dt)
{
    int i, max = bg_max();
    float k = dt / 16.67f;
    if (bg.picking)
        bg.grow = fm_minf(1.0f, bg.grow + dt / 700.0f);
    if (GT.T - bg.wipe_t0 < 600.0f) {          /* the sponge: everything below it goes */
        float wy = 240.0f * ease_in_out_sine((GT.T - bg.wipe_t0) / 600.0f);
        for (i = 0; i < NBOOG; i++)
            if (bg.b[i].state >= 2u && bg.b[i].y < wy)
                bg.b[i].state = 0;
    }
    for (i = 0; i < NBOOG; i++) {
        booger_t *b = &bg.b[i];
        if (b->state == 1u) {                  /* flying */
            b->vy += 0.25f * k;
            b->x += b->vx * k;
            b->y += b->vy * k;
            if (b->x >= WALL_X + 6.0f) {       /* SPLAT: it sticks */
                int deg = (int)((215.0f - b->y) / 14.0f) + (kv(K4) - 10) / 2, n, j, oldest = -1;
                uint32_t oseq = 0xFFFFFFFFu;
                note_t nt = snd_defaults(P_SPLAT, (float)mood_degree_midi(4, deg), 0.8f);
                b->x = WALL_X + 6.0f + ss_rndf() * 30.0f;
                b->y = fm_clampf(b->y, 20.0f, 196.0f);
                b->state = 2;
                b->t = GT.T;
                snd_note(&nt);
                fx_trauma(0.12f);
                ui_led_snare();
                for (j = 0; j < 2 + ss_rndi(2); j++) {   /* 2 or 3 sparkles drip off */
                    particle_t *p = part_spawn(PT_SPARKLE, b->x, b->y + b->r, ss_rnds() * 0.4f, 0.4f, 30, CC_GOLD_L, 1);
                    p->g = 3;
                }
                part_mood_burst(b->x, b->y, 4, 1.0f, 2.5f);
                bg.stuck_since_wipe++;
                n = bg_stuck();
                if (n > max) {                 /* too many: the oldest slides down */
                    for (j = 0; j < NBOOG; j++)
                        if (bg.b[j].state == 2u && bg.b[j].seq < oseq) {
                            oseq = bg.b[j].seq;
                            oldest = j;
                        }
                    if (oldest >= 0) {
                        bg.b[oldest].state = 3;
                        bg.b[oldest].t = GT.T;
                    }
                }
                if (bg.stuck_since_wipe >= (uint32_t)(2 * max + 2)) {   /* too full: the wipe */
                    bg.stuck_since_wipe = 0;
                    bg.wipe_t0 = GT.T;
                    snd_play(P_SWOOSH, 0.0f, 0.7f);
                }
            }
        } else if (b->state == 3u) {           /* sliding down, wobbling, then pop */
            b->y += 0.9f * k;
            b->wob = fm_sinf((GT.T - b->t) / 60.0f) * 3.0f;
            if (b->y > 200.0f)
                bg_pop(b);
        }
    }
}

static void draw_booger(float x, float y, float r, float sx, float sy, int style, float T)
{
    uint16_t c = style == BS_RAINBOW ? hue565((int32_t)(T / 2.0f + x * 6.0f)) : style == BS_GLOW ? lighten(mood->accent, 60u)
                 : style == BS_GOLD ? CC_GOLD : CC_SNOT;
    int k;
    if (style == BS_GLOW)
        glow(x, y, r * 3.0f, scale565(mood->accent, 180u), 200u);
    ell(x, y, r * sx + 1.0f, r * sy + 1.0f, darken(c, 110u));
    ell(x, y, r * sx, r * sy, c);
    disc(x - r * 0.35f * sx, y - r * 0.35f * sy, fm_maxf(r * 0.3f, 0.8f), lighten(c, 160u));
    for (k = 0; k < 3; k++) {                  /* its three sparkle children */
        float an = T / 300.0f + (float)k * 2.094f;
        star4(x + r * 1.5f * fm_cosf(an), y + r * 1.5f * fm_sinf(an), fm_maxf(r * 0.45f, 1.5f), style == BS_GOLD ? U_WHITE : CC_GOLD_L);
    }
}

static void booger_draw(float T)
{
    float pa = T - bg.pick_t0, fa = T - bg.flick_t0;
    int i;
    pose_t p;
    uint16_t wall = mix565(mood->prop1, mood->ground, 110u);
    mood_draw_ground(198);
    /* the booger wall: tiles */
    box(WALL_X, 10.0f, 240.0f - WALL_X, 190.0f, darken(wall, 40u));
    box(WALL_X + 3.0f, 12.0f, 240.0f - WALL_X - 3.0f, 186.0f, wall);
    for (i = 0; i < 8; i++)
        box(WALL_X + 3.0f, 12.0f + (float)i * 24.0f, 240.0f - WALL_X, 1.5f, darken(wall, 60u));
    for (i = 0; i < 3; i++)
        box(WALL_X + 3.0f + (float)i * 20.0f, 12.0f, 1.5f, 186.0f, darken(wall, 60u));
    /* the unicorn */
    p = uni_pose(BG_X, BG_Y, 1.0f);
    if (bg.picking) {
        p.pick = ease_out_back(pa / 160.0f);
        p.eye_state = EY_SQUINT;
        p.mouth_state = MO_OPEN;
        p.blush = 0.8f;
        p.head_rot = -4.0f + 2.0f * fm_sinf(T / 90.0f);
        p.look_x = 0.6f;
        p.look_y = 0.6f;
    } else if (fa >= 0.0f && fa < 600.0f) {    /* the flick: arm out, grin */
        p.arm_l = 0.9f * (1.0f - ease_out_cubic(fa / 600.0f));
        p.mouth_state = MO_GRIN;
        p.eye_state = fa < 300.0f ? EY_WIDE : EY_HAPPY;
        p.look_x = 1.0f;
        p.rot_deg = 4.0f * (1.0f - ease_out_elastic(fa / 500.0f));
    }
    uni_draw(&p);
    /* the booger being pulled out, on a stretchy string */
    if (bg.picking && pa > 120.0f) {
        float r = (2.5f + 6.0f * kf(K3)) * (0.4f + 0.6f * bg.grow), nx = bg_nose_x(), ny = bg_nose_y();
        float bx = nx + 6.0f + 8.0f * bg.grow, by = ny + 6.0f + 6.0f * bg.grow;
        cap(nx, ny, bx, by, fm_maxf(1.2f - 0.6f * bg.grow, 0.6f), lighten(CC_SNOT, 60u));
        draw_booger(bx, by, r, 1.0f + 0.1f * fm_sinf(T / 60.0f), 1.0f, kv(K6), T);
    }
    /* the boogers: flying, stuck (squashed when new), sliding */
    for (i = 0; i < NBOOG; i++) {
        booger_t *b = &bg.b[i];
        float sx = 1.0f, sy = 1.0f;
        if (!b->state)
            continue;
        if (b->state == 1u) {
            sx = 1.2f;
            sy = 0.85f;
        } else if (GT.T - b->t < 400.0f && b->state == 2u) {
            float e = ease_out_elastic((GT.T - b->t) / 400.0f);
            sx = 0.5f + 0.5f * e;
            sy = 1.6f - 0.6f * e;
        }
        if (b->state == 3u)                    /* a drip trail behind the slide */
            cap(b->x, b->y - 16.0f, b->x, b->y, b->r * 0.4f, lighten(CC_SNOT, 80u));
        draw_booger(b->x + b->wob, b->y, b->r, sx, sy, b->style, T);
    }
    /* the sponge wiping the wall */
    if (T - bg.wipe_t0 < 600.0f) {
        float wy = 240.0f * ease_in_out_sine((T - bg.wipe_t0) / 600.0f);
        box(WALL_X - 4.0f, wy - 14.0f, 240.0f - WALL_X + 8.0f, 16.0f, U_LINE);
        box(WALL_X - 3.0f, wy - 13.0f, 240.0f - WALL_X + 6.0f, 14.0f, CC_YELLOW);
        for (i = 0; i < 6; i++)
            disc(WALL_X + 6.0f + (float)i * 10.0f, wy - 6.0f, 2.0f, darken(CC_YELLOW, 50u));
        part_spawn(PT_BUBBLE, WALL_X + ss_rndf() * 58.0f, wy - 12.0f, ss_rnds(), -0.5f, 30, U_WHITE, 2);
    }
}

static void booger_clean(void)
{
    bg.wipe_t0 = GT.T;
    bg.stuck_since_wipe = 0;
}

static int booger_count(void)
{
    int i, n = 0;
    for (i = 0; i < NBOOG; i++)
        n += bg.b[i].state >= 2u;
    return n;
}

static const toy_def_t TOY_BOOGER_DEF = {
    "BOOGER FLICK", B_SEQ, TF_HOLD, booger_init, 0, booger_key, booger_release, booger_mega, 0, booger_update,
    booger_draw, booger_clean, BOOGER_PRESETS, 4,
    {{KI_FORCE, 21, 0, 10}, {KI_SIZE, 21, 0, 10}, {KI_PITCH, 21, 0, 10}, {KI_STYLE, 4, 1, 0}, {KI_WALL, 21, 0, 20}},
};
