/* SPDX-License-Identifier: MPL-2.0
 * Spooky Stable v2: 7. Toilet Flush (ARP). A big friendly toilet with a face. Any key drops something in (K6: poops,
 * rubber ducks, toilet rolls, rainbow fish) and flushes: a whirlpool spins up in the bowl, and everything on the screen
 * that isn't the toilet is pulled in, spiralling, and vanishes with a bloop (each a note lower, in the scale); the
 * water drains and gurgles back in rings. The little unicorn watches and waves bye-bye. Hold ARP (or charge a key):
 * the big flush, which leaves the whole screen clean.
 *   K2 flush speed   K3 whirlpool size   K4 gurgle pitch   K6 what gets flushed   K7 the water's colour and glow */

#define BOWL_X 150.0f
#define BOWL_Y 168.0f
enum { TI_POOP, TI_DUCK, TI_ROLL, TI_FISH };

typedef struct {
    float x, y, vx, vy, t, rot, r0;
    uint8_t state, kind;                       /* 0 off, 1 falling, 2 swirling */
} titem_t;
#define NTITEMS 10

static struct {
    titem_t it[NTITEMS];
    float flush_t0, flush_ms, mega_t0, lid_t0;
    uint16_t gurgle;
    uint8_t bloops;
    uint32_t swallowed;
} toil;

static const preset_t TOILET_PRESETS[] = {
    {P_GURGLE, IC_DROP}, {P_GURGLE_BIG, IC_GIANT}, {P_GURGLE_BUBBLY, IC_BUBBLES}, {P_GURGLE_WHOOSH, IC_WAVE}};

static uint16_t toil_water(void)
{
    int32_t shift = (int32_t)(kf(K7) * 600.0f) - 100;
    return mix565(mood->accent, hue565(900 + shift), 110u);
}
static int toil_flushing(void) { return GT.T - toil.flush_t0 < toil.flush_ms; }

static void toilet_init(void)
{
    int i;
    for (i = 0; i < NTITEMS; i++)
        toil.it[i].state = 0;
    toil.flush_t0 = toil.mega_t0 = toil.lid_t0 = -10000.0f;
    toil.flush_ms = 1200.0f;
}

static void toil_flush(int big)
{
    int fresh = !toil_flushing();
    note_t n = snd_defaults(toy_patch(), (float)(kv(K4) - 10), 0.9f);
    toil.flush_ms = (1500.0f - 800.0f * kf(K2)) * (big ? 1.8f : 1.0f);
    toil.flush_t0 = GT.T;
    if (fresh || big) {                        /* the gurgle: once a flush (mashing only adds bloops) */
        n.tscale = toil.flush_ms / 900.0f;
        n.midi = (float)(kv(K4) - 10) * 0.6f;
        toil.gurgle = snd_note(&n);
        toil.lid_t0 = GT.T;
        impact(big ? 0.7f : 0.25f, big ? 5 : 3);
    }
    mood_props_suck(BOWL_X, BOWL_Y);           /* everything that isn't the toilet: in */
    part_suck(BOWL_X, BOWL_Y);
    if (big) {
        toil.mega_t0 = GT.T;
        minis_clear();
        ui_led_flash_all(300u);
        fx_flash(toil_water());
    }
}

static void toilet_key(uint32_t k, float vel)
{
    int i;
    for (i = 0; i < NTITEMS; i++)
        if (!toil.it[i].state)
            break;
    if (i == NTITEMS)
        i = (int)(toil.swallowed % NTITEMS);
    {
        titem_t *t = &toil.it[i];
        t->kind = (uint8_t)kv(K6);
        t->x = 16.0f + (float)k * 8.0f;
        t->y = -14.0f;
        t->vx = (BOWL_X - t->x) / 26.0f;
        t->vy = 1.0f + 2.0f * vel;
        t->state = 1;
        t->t = GT.T;
        t->rot = ss_rnds();
    }
    snd_play(P_POP, (float)mood_key_midi(k, 5), 0.5f);   /* at once: a plink as it drops */
    toil_flush(0);
}
static void toilet_release(uint32_t k, uint32_t held) { (void)k; (void)held; }
static void toilet_mega(uint32_t k)
{
    (void)k;
    toil_flush(1);
}

static void toil_bloop(titem_t *t)
{
    int deg = 8 - (int)(toil.bloops++ % 6u) + (kv(K4) - 10) / 3;   /* each one a step lower */
    snd_play(P_BLOOP, (float)mood_degree_midi(5, deg), 0.75f);
    part_burst(PT_BUBBLE, BOWL_X, BOWL_Y - 4.0f, 4, 0.8f, 2.0f, 20, lighten(toil_water(), 120u), 3);
    ring_spawn(BOWL_X, BOWL_Y, 4.0f, 2.0f, 0.35f, 300.0f, 2.0f, U_WHITE);
    toil.swallowed++;
    t->state = 0;
}

static void toilet_update(float dt)
{
    int i;
    float k = dt / 16.67f, fa = GT.T - toil.flush_t0;
    for (i = 0; i < NTITEMS; i++) {
        titem_t *t = &toil.it[i];
        if (t->state == 1u) {                  /* falling toward the bowl */
            t->vy += 0.35f * k;
            t->x += t->vx * k;
            t->y += t->vy * k;
            t->rot += 0.05f * k;
            if (t->y >= BOWL_Y - 18.0f) {
                t->state = 2;
                t->t = GT.T;
                t->r0 = fm_sqrtf((t->x - BOWL_X) * (t->x - BOWL_X) + 1.0f) + 16.0f;
            }
        } else if (t->state == 2u) {           /* round the whirlpool, faster and smaller, then gone */
            float u = (GT.T - t->t) / (toil.flush_ms * 0.5f + 200.0f);
            if (u >= 1.0f)
                toil_bloop(t);
        }
    }
    if (fa > toil.flush_ms * 0.85f && fa - dt <= toil.flush_ms * 0.85f) {   /* the end: rings of gurgle */
        ring_spawn(BOWL_X, BOWL_Y, 6.0f, 2.0f, 0.4f, 600.0f, 2.0f, lighten(toil_water(), 100u));
        ring_spawn(BOWL_X, BOWL_Y, 2.0f, 1.6f, 0.4f, 700.0f, 2.0f, U_WHITE);
    }
    if (toil_flushing() && fa < toil.flush_ms * 0.7f)
        part_suck(BOWL_X, BOWL_Y);
}

static void draw_titem(int kind, float x, float y, float s, float rot, float T)
{
    switch (kind) {
    case TI_DUCK:
        draw_duck(x, y, s * 1.4f, CC_YELLOW);
        break;
    case TI_ROLL:                              /* a toilet roll, a tail of paper fluttering */
        cap(x - 7.0f * s * fm_cosf(rot), y - 7.0f * s * fm_sinf(rot), x + 7.0f * s * fm_cosf(rot), y + 7.0f * s * fm_sinf(rot), 6.0f * s, U_LINE);
        cap(x - 7.0f * s * fm_cosf(rot), y - 7.0f * s * fm_sinf(rot), x + 7.0f * s * fm_cosf(rot), y + 7.0f * s * fm_sinf(rot), 5.0f * s, U_WHITE);
        disc(x + 7.0f * s * fm_cosf(rot), y + 7.0f * s * fm_sinf(rot), 2.0f * s, HEX(0xC8C0D0));
        cap(x, y + 5.0f * s, x - 6.0f * s, y + 14.0f * s + 3.0f * fm_sinf(T / 60.0f), 2.0f * s, U_WHITE);
        break;
    case TI_FISH: {
        uint16_t c = hue565((int32_t)(T / 2.0f) + (int32_t)(x * 4.0f));
        ell(x, y, 8.0f * s, 5.0f * s, c);
        tri(x - 7.0f * s, y, x - 13.0f * s, y - 5.0f * s, x - 13.0f * s, y + 5.0f * s, c);
        disc(x + 4.0f * s, y - 1.0f * s, 1.4f * s, CC_INK);
        break;
    }
    default:
        draw_pooplet(x, y + 3.0f * s, s * 1.4f);
        break;
    }
}

static void toilet_draw(float T)
{
    float fa = T - toil.flush_t0, fl = toil_flushing() ? ss_clamp01(fa / toil.flush_ms) : 1.0f, spin, lid, wsz;
    float lev = toil_flushing() ? 4.0f * fm_sinf(3.14159f * fl) - 3.0f * ss_ramp(fl, 0.5f, 1.0f) : 0.0f;
    uint16_t water = toil_water(), porc = HEX(0xFFFFFF), shade = HEX(0xD8D4E8);
    int i;
    pose_t p;
    /* a tiled floor */
    mood_draw_ground(206);
    for (i = 0; i < 9; i++)
        cap((float)i * 30.0f, 206.0f, (float)i * 30.0f - 40.0f + (float)i * 10.0f, 240.0f, 0.6f, mood->ground2);
    lid = toil.lid_t0 > -1000.0f && T - toil.lid_t0 < 500.0f ? 1.0f - ease_out_elastic((T - toil.lid_t0) / 500.0f) : 0.0f;
    if (fl < 1.0f && toil_flushing())
        glow(BOWL_X, BOWL_Y, 40.0f, scale565(water, (uint32_t)(60.0f + 160.0f * kf(K7))), 200u);
    /* the toilet: tank with a face, the bowl, the seat; it squashes when it flushes */
    {
        float sq = 1.0f + 0.08f * lid, tx = BOWL_X + 4.0f, ty = 124.0f;
        box(tx - 30.0f, ty - 30.0f * sq, 60.0f, 56.0f * sq, U_LINE);
        box(tx - 28.5f, ty - 28.5f * sq, 57.0f, 53.0f * sq, porc);
        box(tx + 16.0f, ty - 28.0f * sq, 10.0f, 52.0f * sq, shade);
        box(tx - 32.0f, ty - 34.0f * sq, 64.0f, 8.0f, U_LINE);
        box(tx - 30.5f, ty - 32.5f * sq, 61.0f, 5.0f, porc);
        cap(tx + 20.0f, ty - 18.0f, tx + 30.0f + 4.0f * lid, ty - 18.0f - 6.0f * lid, 2.5f, HEX(0xA0AEC0));   /* the handle */
        /* its face: big eyes (wide while it flushes), a smile */
        {
            float ew = toil_flushing() ? 1.3f : 1.0f, lx = (BOWL_X - tx) * 0.02f;
            disc(tx - 12.0f, ty - 6.0f, 7.5f * ew, U_LINE);
            disc(tx + 6.0f, ty - 6.0f, 7.5f * ew, U_LINE);
            disc(tx - 12.0f, ty - 6.0f, 6.5f * ew, U_WHITE);
            disc(tx + 6.0f, ty - 6.0f, 6.5f * ew, U_WHITE);
            if (toil_flushing()) {             /* its eyes go round with the whirlpool */
                float an = T / 90.0f;
                disc(tx - 12.0f + 2.5f * fm_cosf(an), ty - 6.0f + 2.5f * fm_sinf(an), 3.2f, CC_INK);
                disc(tx + 6.0f + 2.5f * fm_cosf(an), ty - 6.0f + 2.5f * fm_sinf(an), 3.2f, CC_INK);
            } else {
                disc(tx - 12.0f + lx, ty - 4.0f, 3.2f, CC_INK);
                disc(tx + 6.0f + lx, ty - 4.0f, 3.2f, CC_INK);
            }
            arc_dots(tx - 3.0f, ty + 4.0f, 8.0f, 0.4f, 2.7f, 2.2f, U_LINE);
            disc_a(tx - 20.0f, ty + 6.0f, 4.0f, CC_BLUSH, 160u);
            disc_a(tx + 14.0f, ty + 6.0f, 4.0f, CC_BLUSH, 160u);
        }
        /* the bowl and its base */
        quad(BOWL_X - 18.0f, BOWL_Y + 6.0f, BOWL_X + 18.0f, BOWL_Y + 6.0f, BOWL_X + 14.0f, 208.0f, BOWL_X - 14.0f, 208.0f, U_LINE);
        quad(BOWL_X - 16.5f, BOWL_Y + 6.0f, BOWL_X + 16.5f, BOWL_Y + 6.0f, BOWL_X + 12.5f, 207.0f, BOWL_X - 12.5f, 207.0f, porc);
        ell(BOWL_X, BOWL_Y + 8.0f, 44.0f * sq, 26.0f / sq, U_LINE);
        ell(BOWL_X, BOWL_Y + 8.0f, 42.5f * sq, 24.5f / sq, porc);
        ell(BOWL_X + 8.0f, BOWL_Y + 14.0f, 28.0f, 14.0f, shade);
        ell(BOWL_X, BOWL_Y - 2.0f, 42.0f * sq, 15.0f, U_LINE);
        ell(BOWL_X, BOWL_Y - 2.0f, 40.0f * sq, 13.5f, porc);              /* the seat */
        ell(BOWL_X, BOWL_Y - 1.0f, 31.0f, 9.5f, U_LINE);
        ell(BOWL_X, BOWL_Y - 1.0f, 30.0f, 8.5f + lev * 0.3f, water);       /* the water */
        ell_a(BOWL_X - 10.0f, BOWL_Y - 4.0f, 10.0f, 2.0f, U_WHITE, 120u);
    }
    /* the whirlpool: three arcs, spinning faster and faster */
    if (toil_flushing()) {
        spin = fa * (0.004f + 0.02f * fl * fl);
        wsz = (0.6f + 0.6f * kf(K3)) * (1.0f - 0.5f * ss_ramp(fl, 0.6f, 1.0f));
        for (i = 0; i < 3; i++) {
            float r = (8.0f + 8.0f * (float)i) * wsz;
            arc_dots(BOWL_X, BOWL_Y - 1.0f, r, spin * (1.0f + 0.4f * (float)i) + (float)i * 2.1f,
                     spin * (1.0f + 0.4f * (float)i) + (float)i * 2.1f + 3.6f, 2.2f, i & 1 ? U_WHITE : lighten(water, 120u));
        }
        disc(BOWL_X, BOWL_Y - 1.0f, 3.0f + 3.0f * fl, darken(water, 120u));
    }
    /* the things on their way in */
    for (i = 0; i < NTITEMS; i++) {
        titem_t *t = &toil.it[i];
        if (t->state == 1u) {
            draw_titem(t->kind, t->x, t->y, 1.0f, t->rot, T);
        } else if (t->state == 2u) {
            float u = (T - t->t) / (toil.flush_ms * 0.5f + 200.0f), e = ease_in_quad(u), ang = u * 12.0f + (float)i;
            float r = t->r0 * (1.0f - e);
            draw_titem(t->kind, BOWL_X + r * fm_cosf(ang), BOWL_Y - 1.0f + r * 0.3f * fm_sinf(ang), 1.0f - 0.85f * e, ang, T);
        }
    }
    /* the little unicorn watching, waving bye-bye */
    p = uni_pose(48.0f, 210.0f, 0.62f);
    p.look_x = 0.9f;
    if (toil_flushing()) {
        p.arm_l = 0.75f + 0.2f * fm_sinf(T / 110.0f);
        p.mouth_state = MO_GRIN;
        p.eye_state = EY_WIDE;
        p.head_rot = 4.0f * fm_sinf(T / 200.0f);
    }
    if (T - toil.mega_t0 < 1500.0f)
        p.lift = 14.0f * ss_absf(fm_sinf(T / 120.0f)), p.eye_state = EY_HAPPY;
    uni_draw(&p);
}

static void toilet_clean(void)
{
    toil_flush(1);
}

static const toy_def_t TOY_TOILET_DEF = {
    "TOILET FLUSH", B_ARP, 0, toilet_init, 0, toilet_key, toilet_release, toilet_mega, 0, toilet_update, toilet_draw,
    toilet_clean, TOILET_PRESETS, 4,
    {{KI_SPEED, 21, 0, 8}, {KI_WHIRL, 21, 0, 10}, {KI_PITCH, 21, 0, 10}, {KI_STYLE, 4, 1, 0}, {KI_GLOW, 21, 0, 6}},
};
