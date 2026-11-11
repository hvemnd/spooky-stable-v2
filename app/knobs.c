/* SPDX-License-Identifier: MPL-2.0
 * Spooky Stable v2: the knobs (knobs.h). */

static void on_mood_landed(int m);        /* app.c: the magic trick */

static struct {
    float ease[NKNOBS];
    uint32_t save_at;
    uint32_t show_until;
    uint8_t show_icon, show_steps, show_val, show_mood;
    int32_t mood_pending;
    uint32_t mood_land_at;
} KN;

static const knob_spec_t KS_VOL = {KI_VOLUME, 11, 0, 6};

static int knob_slot(int k)
{
    switch (k) {
    case K2: return 0;
    case K3: return 1;
    case K4: return 2;
    case K6: return 3;
    case K7: return 4;
    default: return -1;
    }
}
static const knob_spec_t *kspec(int k)
{
    int s = knob_slot(k);
    return s < 0 ? &KS_VOL : &TOY->knobs[s];
}
static int kv(int k)
{
    int s = knob_slot(k);
    if (k == K1)
        return set.vol;
    if (k == K5)
        return set.mood;
    return s < 0 ? 0 : set.knob[cur_toy][s];
}
static float k_target(int k)
{
    const knob_spec_t *sp = k == K5 ? 0 : kspec(k);
    if (k == K5)
        return (float)set.mood / 7.0f;
    return sp->steps > 1 ? (float)kv(k) / (float)(sp->steps - 1) : 0.0f;
}
static float kf(int k) { return KN.ease[k]; }

static void knobs_enter_toy(void)
{
    int k;
    for (k = 0; k < NKNOBS; k++)
        KN.ease[k] = k_target(k);
}

static void knobs_update(float dt)
{
    int k;
    for (k = 0; k < NKNOBS; k++) {
        float t = k_target(k);
        if (k != K1 && k != K5 && kspec(k)->steps <= 8)
            KN.ease[k] = t;                        /* a style snaps */
        else
            KN.ease[k] += (t - KN.ease[k]) * fm_minf(1.0f, dt / 120.0f);
    }
}

static void knobs_show(int icon, int steps, int value, uint32_t ms)
{
    KN.show_icon = (uint8_t)icon;
    KN.show_steps = (uint8_t)steps;
    KN.show_val = (uint8_t)value;
    KN.show_mood = 0;
    KN.show_until = plat_ms() + ms;
}

static void knobs_load(void)
{
    int t, s;
    if (plat_store_load(OBJ_SETTINGS, &set, sizeof set) != (int)sizeof set || set.magic != SET_MAGIC) {
        set.magic = SET_MAGIC;
        set.vol = 6;
        set.quiet = 0;
        set.mood = 0;
        set.hat = set.mane = 0;
        set.unlocked = 0;
        set.actions = 0;
        for (t = 0; t < NTOYS; t++) {
            set.preset[t] = 0;
            for (s = 0; s < NTKNOBS; s++)
                set.knob[t][s] = 255;              /* the toy's default, below */
        }
    }
    if (set.vol > 10u)
        set.vol = 6;
    set.mood %= NMOODS;
    for (t = 0; t < NTOYS; t++) {
        if (set.preset[t] >= TOYREG[t]->preset_count)
            set.preset[t] = 0;
        for (s = 0; s < NTKNOBS; s++)
            if (set.knob[t][s] >= TOYREG[t]->knobs[s].steps)
                set.knob[t][s] = TOYREG[t]->knobs[s].def;
    }
}

static void knobs_save_soon(void) { KN.save_at = plat_ms() + 2500u; }
static void knobs_tick(void)
{
    uint32_t now = plat_ms();
    if (KN.save_at && (int32_t)(now - KN.save_at) >= 0) {
        KN.save_at = 0;
        plat_store_save(OBJ_SETTINGS, &set, sizeof set);
    }
    if (KN.mood_land_at && (int32_t)(now - KN.mood_land_at) >= 0) {   /* the mood knob landed */
        KN.mood_land_at = 0;
        if ((int)set.mood != KN.mood_pending)
            on_mood_landed(KN.mood_pending);
    }
}

static void snd_apply_volume(void);         /* app.c */

static void knobs_poll(void)
{
    static const uint8_t ENC[NKNOBS] = {EN_K1, EN_K2, EN_K3, EN_K4, EN_SELECT, EN_ALGO, EN_PRESET};
    int k;
    for (k = 0; k < NKNOBS; k++) {
        int32_t e = plat_enc(ENC[k]);
        if (!e)
            continue;
        if (k == K1) {                         /* volume, everywhere */
            set.vol = (uint8_t)clamp((int32_t)set.vol + e, 0, 10);
            snd_apply_volume();
            knobs_show(KI_VOLUME, 11, set.vol, 800u);
        } else if (k == K5) {                  /* the world: shown at once, changed when the knob lands */
            if (!KN.mood_land_at)
                KN.mood_pending = set.mood;
            KN.mood_pending = (int32_t)(((KN.mood_pending + e) % NMOODS + NMOODS) % NMOODS);
            KN.mood_land_at = plat_ms() + 180u;
            KN.show_mood = 1;
            KN.show_icon = KI_MOOD;
            KN.show_steps = NMOODS;
            KN.show_val = (uint8_t)KN.mood_pending;
            KN.show_until = plat_ms() + 900u;
        } else {
            const knob_spec_t *sp = kspec(k);
            int s = knob_slot(k), v = set.knob[cur_toy][s] + (int)e;
            if (sp->wrap)
                v = ((v % sp->steps) + sp->steps) % sp->steps;
            else
                v = clamp(v, 0, sp->steps - 1);
            set.knob[cur_toy][s] = (uint8_t)v;
            knobs_show(sp->icon, sp->steps, v, 800u);
            if (TOY->on_knob)
                TOY->on_knob(k, v);
        }
        knobs_save_soon();
    }
}

/* ------------------------------------------------------------------ icons --- */
static void draw_kicon(int icon, float x, float y, float s, uint16_t c)
{
    int i;
    switch (icon) {
    case KI_VOLUME:                            /* a speaker */
        box(x - 8.0f * s, y - 4.0f * s, 6.0f * s, 8.0f * s, c);
        tri(x - 4.0f * s, y, x + 4.0f * s, y - 9.0f * s, x + 4.0f * s, y + 9.0f * s, c);
        arc_dots(x + 5.0f * s, y, 6.0f * s, -0.9f, 0.9f, 1.6f * s, c);
        break;
    case KI_MOOD:                              /* a little world: sun and moon */
        disc(x - 2.0f * s, y, 8.0f * s, c);
        disc(x + 3.0f * s, y - 3.0f * s, 6.0f * s, darken(c, 160u));
        break;
    case KI_CLOCK:
    case KI_SPEED:
        ring_a(x, y, 9.0f * s, 9.0f * s, 2.0f * s, c, 256u);
        cap(x, y, x, y - 6.0f * s, 1.2f * s, c);
        cap(x, y, x + 4.5f * s, y + 1.5f * s, 1.2f * s, c);
        if (icon == KI_SPEED)
            for (i = 0; i < 3; i++)
                cap(x - 16.0f * s, y - 4.0f * s + 4.0f * (float)i * s, x - 11.0f * s, y - 4.0f * s + 4.0f * (float)i * s, 0.9f * s, c);
        break;
    case KI_SIZE:                              /* small, middle, big */
        disc(x - 9.0f * s, y + 4.0f * s, 3.0f * s, c);
        disc(x - 1.0f * s, y + 2.0f * s, 5.0f * s, c);
        disc(x + 9.0f * s, y - 1.0f * s, 8.0f * s, c);
        break;
    case KI_PITCH:                             /* a note */
        ell(x - 3.0f * s, y + 6.0f * s, 4.5f * s, 3.5f * s, c);
        box(x + 0.5f * s, y - 9.0f * s, 2.0f * s, 15.0f * s, c);
        tri(x + 2.5f * s, y - 9.0f * s, x + 2.5f * s, y - 3.0f * s, x + 9.0f * s, y - 4.0f * s, c);
        break;
    case KI_BEND:                              /* a wave */
        for (i = 0; i < 12; i++) {
            float x0 = x - 12.0f * s + (float)i * 2.0f * s;
            cap(x0, y + 5.0f * s * fm_sinf((float)i * 0.6f), x0 + 2.0f * s, y + 5.0f * s * fm_sinf((float)(i + 1) * 0.6f), 1.2f * s, c);
        }
        break;
    case KI_STYLE:                             /* a rainbow */
        for (i = 0; i < 4; i++)
            arc_dots(x, y + 6.0f * s, (10.0f - 2.5f * (float)i) * s, 3.3f, 6.1f, 2.4f * s, RAINBOW[i]);
        break;
    case KI_CHAOS:                             /* a flame */
        disc(x, y + 3.0f * s, 7.0f * s, CC_ORANGE);
        tri(x - 7.0f * s, y + 2.0f * s, x + 7.0f * s, y + 2.0f * s, x + 1.0f * s, y - 12.0f * s, CC_ORANGE);
        disc(x, y + 5.0f * s, 4.0f * s, CC_YELLOW);
        tri(x - 4.0f * s, y + 4.0f * s, x + 4.0f * s, y + 4.0f * s, x, y - 4.0f * s, CC_YELLOW);
        break;
    case KI_HEIGHT:                            /* an arc and its arrow */
        arc_dots(x, y + 8.0f * s, 11.0f * s, 3.4f, 6.0f, 2.0f * s, c);
        tri(x + 9.0f * s, y + 1.0f * s, x + 13.0f * s, y + 8.0f * s, x + 6.0f * s, y + 7.0f * s, c);
        break;
    case KI_FORCE:                             /* a burst */
        for (i = 0; i < 8; i++) {
            float a = (float)i * 0.785f;
            cap(x + 4.0f * s * fm_cosf(a), y + 4.0f * s * fm_sinf(a), x + 11.0f * s * fm_cosf(a), y + 11.0f * s * fm_sinf(a), 1.3f * s, c);
        }
        break;
    case KI_SWING:                             /* two notes, one late */
        disc(x - 6.0f * s, y + 5.0f * s, 3.5f * s, c);
        box(x - 3.5f * s, y - 7.0f * s, 1.6f * s, 12.0f * s, c);
        disc(x + 6.0f * s, y + 7.0f * s, 3.5f * s, c);
        box(x + 8.5f * s, y - 5.0f * s, 1.6f * s, 12.0f * s, c);
        break;
    case KI_DRUM:
        ell(x, y + 5.0f * s, 11.0f * s, 4.0f * s, darken(c, 60u));
        box(x - 11.0f * s, y - 3.0f * s, 22.0f * s, 8.0f * s, darken(c, 60u));
        ell(x, y - 3.0f * s, 11.0f * s, 4.0f * s, c);
        cap(x - 4.0f * s, y - 6.0f * s, x - 12.0f * s, y - 13.0f * s, 1.2f * s, c);
        break;
    case KI_WATER:
        disc(x, y + 3.0f * s, 7.0f * s, c);
        tri(x - 6.5f * s, y + 1.0f * s, x + 6.5f * s, y + 1.0f * s, x, y - 11.0f * s, c);
        disc(x - 2.5f * s, y + 2.0f * s, 2.0f * s, U_WHITE);
        break;
    case KI_BUBBLES:
        ring_a(x - 5.0f * s, y + 4.0f * s, 5.0f * s, 5.0f * s, 1.5f * s, c, 256u);
        ring_a(x + 6.0f * s, y - 2.0f * s, 4.0f * s, 4.0f * s, 1.5f * s, c, 256u);
        ring_a(x - 1.0f * s, y - 8.0f * s, 3.0f * s, 3.0f * s, 1.2f * s, c, 256u);
        break;
    case KI_GRAVITY:                           /* a weight falling */
        box(x - 7.0f * s, y - 10.0f * s, 14.0f * s, 9.0f * s, c);
        tri(x - 6.0f * s, y + 1.0f * s, x + 6.0f * s, y + 1.0f * s, x, y + 11.0f * s, c);
        break;
    case KI_ECHO:
        for (i = 0; i < 3; i++)
            arc_dots(x - 8.0f * s, y, (5.0f + 5.0f * (float)i) * s, -0.8f, 0.8f, 1.8f * s, mix565(c, U_LINE, (uint32_t)(i * 60)));
        break;
    case KI_GLOW:                              /* a sun */
        disc(x, y, 5.0f * s, c);
        for (i = 0; i < 8; i++) {
            float a = (float)i * 0.785f;
            cap(x + 8.0f * s * fm_cosf(a), y + 8.0f * s * fm_sinf(a), x + 11.0f * s * fm_cosf(a), y + 11.0f * s * fm_sinf(a), 1.2f * s, c);
        }
        break;
    case KI_WHIRL:
        for (i = 0; i < 3; i++)
            arc_dots(x, y, (3.0f + 4.0f * (float)i) * s, (float)i * 1.4f, (float)i * 1.4f + 4.0f, 1.8f * s, c);
        break;
    case KI_WALL:                              /* bricks */
        for (i = 0; i < 3; i++) {
            box(x - 12.0f * s + (i & 1 ? 4.0f * s : 0.0f), y - 8.0f * s + (float)i * 6.0f * s, 10.0f * s, 5.0f * s, c);
            box(x + (i & 1 ? 4.0f * s : 0.0f), y - 8.0f * s + (float)i * 6.0f * s, 10.0f * s, 5.0f * s, c);
        }
        break;
    default:                                   /* KI_CHARGE: a bolt */
        tri(x + 2.0f * s, y - 12.0f * s, x - 7.0f * s, y + 2.0f * s, x + 1.0f * s, y + 1.0f * s, CC_YELLOW);
        tri(x - 2.0f * s, y + 12.0f * s, x + 7.0f * s, y - 2.0f * s, x - 1.0f * s, y - 1.0f * s, CC_YELLOW);
        break;
    }
}

static void knobs_draw(void)
{
    int i, n;
    float x0, gap;
    if ((int32_t)(KN.show_until - plat_ms()) <= 0)
        return;
    dr_ox = dr_oy = 0;
    box_a(30.0f, 6.0f, 180.0f, 30.0f, CC_INK, 200u);
    ring_a(120.0f, 21.0f, 92.0f, 18.0f, 1.0f, mood->accent, 90u);
    if (KN.show_mood) {                        /* the worlds: each its own accent, the chosen one big */
        draw_kicon(KI_MOOD, 50.0f, 21.0f, 0.9f, MOODS[KN.show_val].accent);
        for (i = 0; i < NMOODS; i++)
            disc(76.0f + (float)i * 17.0f, 21.0f, i == KN.show_val ? 7.0f : 3.5f, MOODS[i].accent);
        return;
    }
    draw_kicon(KN.show_icon, 50.0f, 21.0f, 0.9f, U_WHITE);
    n = KN.show_steps;
    if (n <= 8) {                              /* a style: one dot per choice, the chosen big */
        gap = 104.0f / (float)(n > 1 ? n - 1 : 1);
        for (i = 0; i < n; i++)
            disc(80.0f + (float)i * gap, 21.0f, i == KN.show_val ? 7.0f : 3.5f, i == KN.show_val ? mood->accent : HEX(0x5A5070));
    } else {                                   /* a level: ten dots filling up */
        int on = (KN.show_val * 10 + (n - 2)) / (n - 1);
        x0 = 74.0f;
        for (i = 0; i < 10; i++)
            disc(x0 + (float)i * 12.6f, 21.0f, 4.5f, i < on ? RAINBOW[i % 6] : HEX(0x463C5A));
    }
}
