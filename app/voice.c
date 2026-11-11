/* SPDX-License-Identifier: MPL-2.0
 * Spooky Stable v2: the synth (voice.h). Everything below "render side" runs in the audio interrupt. */

static float sintab[1024];
static float midi_hz(float n) { return 440.0f * fm_exp2f((n - 69.0f) / 12.0f); }

typedef struct {
    const patch_t *p;
    note_t n, pend;
    uint16_t handle;
    uint8_t on, gate, steal, has_pend, drum, held;
    uint32_t age;
    float f0;                          /* Hz before the envelopes (a drum's base, or the note's) */
    float glide_from, glide_to, glide_t;   /* semitones */
    float t, rel_t, rel_lvl, env;      /* seconds; the amp envelope's last value */
    float lvl;                         /* the gain applied at the end of the last chunk */
    float steal_g;
    uint32_t ph1, ph2, nz;
    float fb1, fb2;
    float m1, m2, q1, q2;              /* the main filter's and the noise filter's states */
    float crush_h;
    uint32_t crush_c;
} voice_t;
static voice_t vo[NVOICE];
static uint32_t vo_age;
static volatile uint32_t voice_limit = NVOICE;

/* ------------------------------------------------------------- the queue --- */
enum { C_ON = 1, C_OFF, C_GLIDE, C_MOD, C_ALLOFF, C_DRUM, C_REC, C_CLEAR };
typedef struct {
    uint8_t op, key;
    uint16_t handle;
    float a, b;
    note_t n;
} cmd_t;
#define NQ 64u
static cmd_t aq[NQ];
static volatile uint32_t aq_w, aq_r;
static uint16_t snd_handle_next = 1;

static void aq_push(const cmd_t *c)
{
    uint32_t w = aq_w;
    if (w - aq_r >= NQ)
        return;                        /* full: the note is dropped, never the render's time */
    aq[w % NQ] = *c;
    __asm__ volatile("" ::: "memory");
    aq_w = w + 1u;
}

/* -------------------------------------------------------- the FX state --- */
#define DL_LEN 8820u                   /* 200 ms */
#define CH_LEN 1024u
static int16_t dl_buf[DL_LEN] PLAT_POOL;
static float ch_buf[CH_LEN] PLAT_POOL;
static uint32_t dl_w, ch_w;
static float ch_lfo, duck, gain_now;
static volatile float snd_vol = 0.6f, snd_cap = VOL_CAP, snd_crush_g;

/* ------------------------------------------------------------- the main side --- */
static note_t snd_defaults(int patch, float midi, float vel)
{
    note_t n;
    n.patch = (uint8_t)patch;
    n.flags = 0;
    n.handle = 0;
    n.midi = midi;
    n.vel = vel;
    n.tscale = 1.0f;
    n.bend = 1.0f;
    n.crush = 0.0f;
    n.send = 0.0f;
    n.vib = 0.0f;
    n.pulse = 0.0f;
    return n;
}
static uint16_t snd_note(const note_t *n)
{
    cmd_t c;
    c.op = C_ON;
    c.n = *n;
    c.n.handle = snd_handle_next++;
    if (!snd_handle_next)
        snd_handle_next = 1;
    c.handle = c.n.handle;
    aq_push(&c);
    return c.handle;
}
static uint16_t snd_play(int patch, float midi, float vel)
{
    note_t n = snd_defaults(patch, midi, vel);
    n.flags = NF_ONESHOT;
    return snd_note(&n);
}
static void snd_off(uint16_t h)
{
    cmd_t c;
    if (!h)
        return;
    c.op = C_OFF;
    c.handle = h;
    aq_push(&c);
}
static void snd_glide(uint16_t h, float midi)
{
    cmd_t c;
    if (!h)
        return;
    c.op = C_GLIDE;
    c.handle = h;
    c.a = midi;
    aq_push(&c);
}
static void snd_mod(uint16_t h, float vib, float pulse)
{
    cmd_t c;
    if (!h)
        return;
    c.op = C_MOD;
    c.handle = h;
    c.a = vib;
    c.b = pulse;
    aq_push(&c);
}
static void snd_all_off(void)
{
    cmd_t c;
    c.op = C_ALLOFF;
    aq_push(&c);
}
static void snd_volume(float v) { snd_vol = fm_clampf(v, 0.0f, 1.0f); }
static void snd_quiet(int on) { snd_cap = on ? QUIET_CAP : VOL_CAP; }
static void snd_chaos(float crush) { snd_crush_g = fm_clampf(crush, 0.0f, 1.0f); }
static int snd_voices(void)
{
    int i, n = 0;
    for (i = 0; i < NVOICE; i++)
        n += vo[i].on;
    return n;
}

/* ------------------------------------------------------------- render side --- */
static void voice_begin(voice_t *v, const note_t *n)
{
    const patch_t *p = &PATCHES[n->patch % NPATCH];
    v->p = p;
    v->n = *n;
    v->handle = n->handle;
    v->on = 1;
    v->steal = 0;
    v->has_pend = 0;
    v->held = ((p->flags & PA_HOLD) && !(n->flags & NF_ONESHOT)) || (n->flags & NF_HOLD);
    v->gate = 1;
    v->drum = (p->flags & PA_DRUM) != 0;
    v->age = ++vo_age;
    v->f0 = p->base_hz ? (float)p->base_hz * fm_exp2f(n->midi / 12.0f) : midi_hz(n->midi);
    v->glide_from = v->glide_to = 0.0f;
    v->glide_t = 1.0f;
    v->t = 0.0f;
    v->rel_t = 0.0f;
    v->env = 0.0f;
    v->lvl = 0.0f;
    v->ph1 = v->ph2 = 0;
    v->fb1 = v->fb2 = 0.0f;
    v->m1 = v->m2 = v->q1 = v->q2 = 0.0f;
    v->crush_h = 0.0f;
    v->crush_c = 0;
    if (!v->nz)
        v->nz = 0x9E3779B9u ^ (uint32_t)(v - vo) * 2654435761u;
    if (p->flags & PA_KICK)
        duck = 1.0f;
    if (n->flags & NF_RELEASED)                /* let go before it began: a short blip, then its release */
        v->held = 0;
}

static void voice_start(const note_t *n)
{
    uint32_t i, active = 0, oldest = 0xFFFFFFFFu, best = 0, freeing = 0;
    for (i = 0; i < NVOICE; i++)
        if (vo[i].on)
            active++;
    if (active < voice_limit)
        for (i = 0; i < NVOICE; i++)
            if (!vo[i].on) {
                voice_begin(&vo[i], n);
                return;
            }
    for (i = 0; i < NVOICE; i++) {             /* steal the oldest, a held note last */
        uint32_t a = vo[i].age + (vo[i].held ? 0x40000000u : 0u);
        if (vo[i].on && vo[i].steal && !vo[i].has_pend) {
            freeing = 1;
            best = i;
            break;
        }
        if (vo[i].on && a < oldest) {
            oldest = a;
            best = i;
        }
    }
    (void)freeing;
    vo[best].pend = *n;
    vo[best].has_pend = 1;
    if (!vo[best].steal) {
        vo[best].steal = 1;
        vo[best].steal_g = 1.0f;
    }
}

static voice_t *voice_find(uint16_t h)
{
    uint32_t i;
    for (i = 0; i < NVOICE; i++)
        if (vo[i].on && vo[i].handle == h && !vo[i].steal)
            return &vo[i];
    return 0;
}

static void voice_release(voice_t *v)
{
    if (v->gate) {
        v->gate = 0;
        v->rel_t = v->t;
        v->rel_lvl = v->env;
    }
}

/* the sequencer's state (it lives here: the render triggers its steps) */
#define SEQ_STEPS 64
static struct {
    volatile uint8_t on, autoplay, kit, density;
    volatile float bpm, swing, tune, crush;
    volatile uint32_t loop[SEQ_STEPS];
    volatile uint8_t loop_state;
    volatile uint32_t gstep;
    volatile float samp, cur_len;
    uint32_t origin, pend;
    float pend_vel[NKEYS];
    volatile float pitch[NKEYS];
    struct { uint8_t key, src; } ev[32];
    volatile uint32_t ev_w, ev_r;
} seq;
static void seq_trigger(uint32_t k, float vel, uint8_t src);
static void seq_live(uint32_t k, float vel);

static void audio_cmds(void)
{
    while (aq_r != aq_w) {
        cmd_t *c = &aq[aq_r % NQ];
        voice_t *v;
        uint32_t i;
        switch (c->op) {
        case C_ON:
            voice_start(&c->n);
            break;
        case C_OFF:
            if ((v = voice_find(c->handle)) != 0)
                voice_release(v);
            for (i = 0; i < NVOICE; i++)       /* (a note still waiting for its stolen voice) */
                if (vo[i].has_pend && vo[i].pend.handle == c->handle)
                    vo[i].pend.flags = (uint8_t)((vo[i].pend.flags & ~NF_HOLD) | NF_RELEASED);
            break;
        case C_GLIDE:
            if ((v = voice_find(c->handle)) != 0) {
                float now = ss_lerp(v->glide_from, v->glide_to, fm_minf(v->glide_t / 0.06f, 1.0f));
                v->glide_from = now;
                v->glide_to = c->a - v->n.midi;
                v->glide_t = 0.0f;
            }
            break;
        case C_MOD:
            if ((v = voice_find(c->handle)) != 0) {
                v->n.vib = c->a;
                v->n.pulse = c->b;
            }
            break;
        case C_ALLOFF:
            for (i = 0; i < NVOICE; i++)
                if (vo[i].on) {
                    vo[i].held = 0;
                    voice_release(&vo[i]);
                }
            break;
        case C_DRUM:
            seq_live(c->key, c->a);
            break;
        case C_REC:
            for (i = 0; i < SEQ_STEPS; i++)
                seq.loop[i] = 0;
            seq.origin = seq.gstep - seq.gstep % 16u;
            seq.loop_state = LOOP_REC;
            break;
        case C_CLEAR:
            for (i = 0; i < SEQ_STEPS; i++)
                seq.loop[i] = 0;
            seq.loop_state = LOOP_NONE;
            break;
        default:
            break;
        }
        __asm__ volatile("" ::: "memory");
        aq_r++;
    }
}

static float shape_of(int sh, float x)
{
    switch (sh) {
    case SH_INQ: return x * x;
    case SH_EXP: { float u = 1.0f - x; return 1.0f - u * u * u; }
    case SH_ELASTIC: return ease_out_elastic(x);
    default: return x;
    }
}
static float decay_of(float x, int linear)   /* 1 -> 0 as x goes 0 -> 1 */
{
    if (x >= 1.0f)
        return 0.0f;
    if (linear)
        return 1.0f - x;
    return (fm_expf(-5.0f * x) - 0.006738f) * 1.006784f;
}

typedef struct {
    float amp, freq, tone, noise, cutoff, mod2;
    int done;
} ev_t;

static void voice_eval(voice_t *v, ev_t *e)
{
    const patch_t *p = v->p;
    const note_t *n = &v->n;
    float t = v->t, ts = n->tscale, a = fm_maxf((float)p->a_ms * 0.001f, 0.0005f);
    float d = fm_maxf((float)p->d_ms * 0.001f * ts, 0.002f), S = (float)p->s_level * 0.01f, env, st, x;
    int lin = (p->flags & PA_LINEAR) != 0;
    e->done = 0;
    /* a one-shot with a sustain is released after its gate */
    if (v->gate && !v->held && S > 0.0f) {
        float g = (p->gate_ms ? (float)p->gate_ms * 0.001f : (p->flags & PA_HOLD) ? 0.35f : 0.0f) * ts;
        if (t >= a + d + g)
            voice_release(v);
    }
    if (t < a)
        env = t / a;
    else if (t < a + d)
        env = S + (1.0f - S) * decay_of((t - a) / d, lin);
    else
        env = S;
    if (!v->gate) {
        float r = fm_maxf((float)p->r_ms * 0.001f * ts, 0.005f), u = (t - v->rel_t) / r;
        env = fm_minf(env, v->rel_lvl * decay_of(u, 0));
        if (u >= 1.0f)
            e->done = 1;
    } else if (!v->held && S <= 0.0f && t >= a + d) {
        e->done = 1;
    }
    v->env = env;
    if (p->trem_depth) {
        float tr = 0.5f + 0.5f * fm_sinf(TAU * (float)p->trem_rate * t);
        env *= 1.0f - (float)p->trem_depth * 0.01f * tr;
    }
    if (n->pulse > 0.0f) {
        float q = 0.5f + 0.5f * fm_sinf(TAU * n->pulse * t);
        env *= 0.3f + 0.7f * q * q;
    }
    e->amp = env * n->vel * (p->level ? (float)p->level * 0.01f : 1.0f);
    /* the tone's own level */
    e->tone = p->tone == 255 ? 0.0f : p->tone ? (float)p->tone * 0.01f : 1.0f;
    if (p->tone_d_ms)
        e->tone *= decay_of(t / ((float)p->tone_d_ms * 0.001f * ts), 0);
    /* pitch: the two-stage envelope, the vibrato, the glide */
    {
        float t1 = (float)p->pt1 * 0.001f * ts, t2 = (float)p->pt2 * 0.001f * ts;
        if (t < t1)
            st = ss_lerp((float)p->pe0, (float)p->pe1, shape_of(p->pshape, t / t1));
        else if (p->pt2 && t < t1 + t2)
            st = ss_lerp((float)p->pe1, (float)p->pe2, shape_of(p->pshape, (t - t1) / t2));
        else
            st = (float)(p->pt2 ? p->pe2 : p->pe1);
        st *= 0.1f * n->bend;
    }
    if (p->vib_rate) {
        float dep = (float)p->vib_depth * 0.1f + n->vib, dl = (float)p->vib_delay_ms * 0.001f;
        float fade = ss_clamp01((t - dl) / 0.08f);
        st += dep * fade * fm_sinf(TAU * (float)p->vib_rate * t);
    } else if (n->vib > 0.0f) {
        st += n->vib * fm_sinf(TAU * 5.0f * t);
    }
    if (v->glide_t < 0.06f || v->glide_to != 0.0f)
        st += ss_lerp(v->glide_from, v->glide_to, fm_minf(v->glide_t / 0.06f, 1.0f));
    e->freq = v->f0 * fm_exp2f(st / 12.0f);
    /* the noise layer */
    e->noise = (float)p->noise_amt * 0.01f;
    if (e->noise > 0.0f && p->noise_d_ms) {
        float na = fm_maxf((float)p->noise_a_ms * 0.001f, 0.0005f), nd = (float)p->noise_d_ms * 0.001f * ts, ne;
        if (p->flags & PA_CLAP) {               /* three bursts, 12 ms apart */
            float best = 0.0f;
            int k;
            for (k = 0; k < 3; k++) {
                float tk = t - 0.012f * (float)k;
                if (tk >= 0.0f)
                    best = fm_maxf(best, tk < na ? tk / na : decay_of((tk - na) / nd, 0));
            }
            ne = best;
        } else {
            ne = t < na ? t / na : decay_of((t - na) / nd, 0);
        }
        e->noise *= ne;
    } else if (e->noise > 0.0f && p->noise_a_ms) {
        e->noise *= ss_clamp01(t / ((float)p->noise_a_ms * 0.001f));
    }
    /* the filter */
    e->cutoff = (float)p->cutoff_hz;
    if (p->flags & PA_GURGLE) {                 /* 300 -> 1200 -> 200 Hz over the decay */
        float u = ss_clamp01(t / (a + d)), base = (float)p->cutoff_hz / 300.0f;
        e->cutoff = base * (u < 0.45f ? ss_lerp(300.0f, 1200.0f, u / 0.45f) : ss_lerp(1200.0f, 200.0f, (u - 0.45f) / 0.55f));
    } else if (p->fenv_amt && p->fenv_ms) {
        x = ss_clamp01(t / ((float)p->fenv_ms * 0.001f * ts));
        e->cutoff *= fm_exp2f((float)p->fenv_amt * 0.1f * (1.0f - x));
    }
    e->mod2 = 0.0f;
}

#define CHUNK 16u
static float bus_mel[CHUNK], bus_drm[CHUNK], bus_dl[CHUNK], bus_ch[CHUNK];

static void svf_coef(float fc, float res, float *a1, float *a2, float *a3, float *k)
{
    float g = fm_tanf(3.14159265f * fm_clampf(fc, 20.0f, 15000.0f) / AFS);
    *k = 2.0f - 1.9f * res;
    *a1 = 1.0f / (1.0f + g * (g + *k));
    *a2 = g * *a1;
    *a3 = g * *a2;
}

static void render_voice(voice_t *v, uint32_t m)
{
    const patch_t *p = v->p;
    ev_t e;
    uint32_t k, i1, i2, hold = 1;
    float a0 = v->lvl, a1, da, idx, fbk, ma1 = 0, ma2 = 0, ma3 = 0, mk = 0, na1 = 0, na2 = 0, na3 = 0, nk = 0;
    float drive = (float)p->drive * 0.01f, dgain = 1.0f + 4.0f * drive, dnorm = 1.0f / (1.0f + 0.8f * drive);
    float crush = fm_minf(1.0f, (float)p->crush * 0.01f + v->n.crush + (v->drum ? 0.0f : snd_crush_g));
    float levels = 0.0f, ds = fm_minf(1.0f, (float)p->delay_send * 0.01f + v->n.send), cs = (float)p->chorus_send * 0.01f;
    float *bus = v->drum ? bus_drm : bus_mel;
    int ft = p->ft, nft = p->noise_ft, dual = (p->flags & PA_DUAL) != 0;
    voice_eval(v, &e);
    if (e.done && !v->steal) {
        v->on = 0;
        v->lvl = 0.0f;
        return;
    }
    a1 = e.done ? 0.0f : e.amp;
    if (v->steal) {                            /* stolen: fade out over 5 ms, then the new note */
        v->steal_g -= (float)m / (0.005f * AFS);
        if (v->steal_g <= 0.0f) {
            if (v->has_pend) {
                note_t n = v->pend;
                voice_begin(v, &n);
            } else {
                v->on = 0;
            }
            v->lvl = 0.0f;
            return;
        }
        a1 *= v->steal_g;
    }
    da = (a1 - a0) / (float)m;
    i1 = (uint32_t)(fm_minf(e.freq, 18000.0f) * (4294967296.0f / AFS));
    i2 = (uint32_t)(fm_minf(e.freq * (float)p->ratio_num / (float)p->ratio_den, 20000.0f) * (4294967296.0f / AFS));
    idx = (float)p->mod_level * 0.04f * (1073741824.0f / TAU);   /* quarter scale: the int32 never overflows */
    fbk = (float)p->feedback * 0.015f * (1073741824.0f / TAU);
    if (ft)
        svf_coef(e.cutoff, (float)p->res * 0.01f, &ma1, &ma2, &ma3, &mk);
    if (nft && e.noise > 0.0f)
        svf_coef((float)p->noise_hz, 0.3f, &na1, &na2, &na3, &nk);
    if (crush > 0.02f) {
        hold = 1u + (uint32_t)(crush * 10.0f);
        levels = fm_exp2f(9.0f - 6.0f * crush);
    }
    for (k = 0; k < m; k++) {
        float s = 0.0f, mod;
        if (e.tone > 0.0f) {
            mod = sintab[(v->ph2 + ((uint32_t)(int32_t)((v->fb1 + v->fb2) * 0.5f * fbk) << 2)) >> 22];
            v->fb2 = v->fb1;
            v->fb1 = mod;
            if (dual)
                s = (sintab[v->ph1 >> 22] + 0.8f * mod) * e.tone;
            else
                s = sintab[(v->ph1 + ((uint32_t)(int32_t)(mod * idx) << 2)) >> 22] * e.tone;
            v->ph1 += i1;
            v->ph2 += i2;
        }
        if (drive > 0.0f) {
            float x = fm_clampf(s * dgain, -3.0f, 3.0f);
            s = x * (27.0f + x * x) / (27.0f + 9.0f * x * x) * dnorm;
        }
        if (e.noise > 0.0f) {
            uint32_t x = v->nz;
            float nn;
            x ^= x << 13;
            x ^= x >> 17;
            x ^= x << 5;
            v->nz = x;
            nn = (float)(int32_t)x * (1.0f / 2147483648.0f);
            if (nft) {
                float v3 = nn - v->q2, v1 = na1 * v->q1 + na2 * v3, v2 = v->q2 + na2 * v->q1 + na3 * v3;
                v->q1 = 2.0f * v1 - v->q1;
                v->q2 = 2.0f * v2 - v->q2;
                nn = nft == FT_LP ? v2 : nft == FT_BP ? v1 * 2.0f : nn - nk * v1 - v2;
            }
            s += e.noise * nn;
        }
        if (ft) {
            float v3 = s - v->m2, v1 = ma1 * v->m1 + ma2 * v3, v2 = v->m2 + ma2 * v->m1 + ma3 * v3;
            v->m1 = 2.0f * v1 - v->m1;
            v->m2 = 2.0f * v2 - v->m2;
            s = ft == FT_LP ? v2 : ft == FT_BP ? v1 * 1.6f : s - mk * v1 - v2;
        }
        s *= a0;
        a0 += da;
        if (hold > 1u || levels > 0.0f) {      /* bitcrush: fewer samples, fewer steps */
            if (v->crush_c++ % hold == 0u)
                v->crush_h = (float)(int32_t)(s * levels) / levels;
            s = v->crush_h;
        }
        bus[k] += s;
        bus_dl[k] += s * ds;
        bus_ch[k] += s * cs;
    }
    v->m1 = fm_flush(v->m1);
    v->m2 = fm_flush(v->m2);
    v->q1 = fm_flush(v->q1);
    v->q2 = fm_flush(v->q2);
    v->lvl = a1;
    v->t += (float)m / AFS;
    v->glide_t += (float)m / AFS;
}

/* ------------------------------------------------------------ the sequencer --- */
/* Drum Jam's keys: low third drums, middle third cymbals and shakers, top third the silly melodic keys */
static const uint8_t DRUM_PATCH[NKEYS] = {
    P_KICK, P_LTOM, P_SNARE, P_MTOM, P_KICK, P_HTOM, P_SNARE, P_KICK, P_CLAP,
    P_CHAT, P_OHAT, P_CHAT, P_SHAKER, P_CRASH, P_CHAT, P_COWBELL, P_SHAKER, P_CRASH,
    P_PLOP_BOING, P_HONK, P_SLIDEWH, P_OW, P_PLOP_BOING, P_HONK, P_SLIDEWH, P_OW, P_PLOP_BOING};
static const int8_t DRUM_DETUNE[NKEYS] = {0, 0, 0, 0, 2, 0, 2, -2, 0, 0, 0, 3, 0, 0, -2, 0, 3, -3, 0, 0, 0, 0, 0, 0, 0, 0, 0};

static void seq_trigger(uint32_t k, float vel, uint8_t src)
{
    const kit_t *kit = &KITS[seq.kit % NKITS];
    uint32_t w;
    note_t n;
    if (k >= NKEYS)
        return;
    n = snd_defaults(DRUM_PATCH[k], 0.0f, vel);
    if (PATCHES[DRUM_PATCH[k]].base_hz)
        n.midi = (float)kit->tune + seq.tune + (float)DRUM_DETUNE[k];
    else
        n.midi = seq.pitch[k];
    n.tscale = (float)kit->decay * 0.1f;
    n.crush = (float)kit->crush * 0.01f + seq.crush;
    n.send = (float)kit->delay * 0.01f;
    n.handle = 0;
    voice_start(&n);
    w = seq.ev_w;
    if (w - seq.ev_r < 32u) {
        seq.ev[w % 32u].key = (uint8_t)k;
        seq.ev[w % 32u].src = src;
        __asm__ volatile("" ::: "memory");
        seq.ev_w = w + 1u;
    }
}

static void seq_record(uint32_t k, uint32_t step)
{
    if (seq.loop_state == LOOP_REC && step >= seq.origin)
        seq.loop[(step - seq.origin) % SEQ_STEPS] |= 1u << k;
}

/* a hit she played: now, if it is within the first 30 % of a step (on time enough), else at the next step */
static void seq_live(uint32_t k, float vel)
{
    if (k >= NKEYS)
        return;
    if (!seq.on || seq.samp < 0.3f * seq.cur_len) {
        seq_trigger(k, vel, HIT_LIVE);
        seq_record(k, seq.gstep);
    } else {
        seq.pend |= 1u << k;
        seq.pend_vel[k] = vel;
    }
}

static uint32_t seq_auto_mask(uint32_t step, uint32_t d)
{
    uint32_t s = step % 16u, bar = (step / 16u) % 4u, m = 0;
    if (!d)
        return 0;
    if (s == 0u || s == 8u)
        m |= 1u << 0;                          /* kick */
    if (d >= 2u && (s == 4u || s == 12u))
        m |= 1u << 2;                          /* snare */
    if (d >= 3u && (s & 1u) == 0u)
        m |= 1u << 9;                          /* closed hat, eighths */
    if (d >= 4u && s == 10u)
        m |= 1u << 4;
    if (d >= 5u && s == 14u)
        m |= 1u << 10;                         /* open hat */
    if (d >= 5u && bar == 3u && s == 12u)
        m |= 1u << 8;                          /* clap */
    if (d >= 6u && (s & 1u))
        m |= 1u << 11;                         /* sixteenths */
    if (d >= 7u && (s == 3u || s == 7u))
        m |= 1u << 12;                         /* shaker */
    if (d >= 8u && bar == 3u && s >= 12u)
        m |= 1u << (s == 12u ? 5 : s == 13u ? 3 : 1);   /* a tom fill */
    if (d >= 9u && bar == 0u && s == 0u)
        m |= 1u << 13;                         /* crash */
    if (d >= 10u && (s == 2u || s == 6u || s == 11u))
        m |= 1u << 15;                         /* cowbell */
    return m;
}

static void seq_step(uint32_t step)
{
    uint32_t m = 0, k;
    if (seq.loop_state == LOOP_REC && step >= seq.origin + SEQ_STEPS)
        seq.loop_state = LOOP_PLAY;
    if (seq.loop_state == LOOP_PLAY) {
        uint32_t lm = seq.loop[(step - seq.origin) % SEQ_STEPS];
        for (k = 0; k < NKEYS; k++)
            if (lm >> k & 1u)
                seq_trigger(k, 0.85f, HIT_LOOP);
    }
    if (seq.autoplay)
        m = seq_auto_mask(step, seq.density);
    for (k = 0; k < NKEYS; k++)
        if (m >> k & 1u)
            seq_trigger(k, (step & 3u) == 0u ? 0.95f : 0.75f, HIT_AUTO);
    if (seq.pend) {
        for (k = 0; k < NKEYS; k++)
            if (seq.pend >> k & 1u) {
                seq_trigger(k, seq.pend_vel[k], HIT_LIVE);
                seq_record(k, step);
            }
        seq.pend = 0;
    }
}

static void seq_advance(uint32_t m)
{
    float L, sw;
    if (!seq.on)
        return;
    L = 60.0f / fm_clampf(seq.bpm, 40.0f, 240.0f) / 4.0f * AFS;
    sw = fm_clampf(seq.swing, 0.0f, 0.6f) * 0.5f;
    seq.cur_len = (seq.gstep & 1u) ? L * (1.0f - sw) : L * (1.0f + sw);
    seq.samp += (float)m;
    if (seq.samp >= seq.cur_len) {
        seq.samp -= seq.cur_len;
        seq.gstep++;
        seq_step(seq.gstep);
    }
}

/* the main side of the sequencer */
static void seq_enable(int on)
{
    if (on && !seq.on) {
        seq.samp = 0.0f;
        seq.cur_len = 6000.0f;
    }
    seq.on = (uint8_t)on;
}
static void seq_config(float bpm, float swing, float tune, int density, int autoplay, int kit, float crush)
{
    seq.bpm = bpm;
    seq.swing = swing;
    seq.tune = tune;
    seq.density = (uint8_t)density;
    seq.autoplay = (uint8_t)autoplay;
    seq.kit = (uint8_t)kit;
    seq.crush = crush;
}
static void seq_hit(uint32_t key, float vel)
{
    cmd_t c;
    c.op = C_DRUM;
    c.key = (uint8_t)key;
    c.a = vel;
    aq_push(&c);
}
static void seq_rec(void)
{
    cmd_t c;
    c.op = C_REC;
    aq_push(&c);
}
static void seq_clear(void)
{
    cmd_t c;
    c.op = C_CLEAR;
    aq_push(&c);
}
static int seq_pop(uint8_t *key, uint8_t *src)
{
    uint32_t r = seq.ev_r;
    if (r == seq.ev_w)
        return 0;
    *key = seq.ev[r % 32u].key;
    *src = seq.ev[r % 32u].src;
    __asm__ volatile("" ::: "memory");
    seq.ev_r = r + 1u;
    return 1;
}
static float seq_beat(void) { return ((float)seq.gstep + seq.samp / fm_maxf(seq.cur_len, 1.0f)) * 0.25f; }
static int seq_loop_state(void) { return seq.loop_state; }
static uint32_t seq_loop_mask(int step) { return seq.loop[(uint32_t)step % SEQ_STEPS]; }
static int seq_loop_pos(void) { return (int)((seq.gstep - seq.origin) % SEQ_STEPS); }
static void seq_set_pitch(uint32_t key, float midi)
{
    if (key < NKEYS)
        seq.pitch[key] = midi;
}

/* ---------------------------------------------------------------- render --- */
void app_render(int32_t *lr, uint32_t n, uint32_t master_q12)
{
    uint32_t i, j, v;
    float gain = (float)master_q12 * (1.0f / 4096.0f) * snd_cap * 8300000.0f;
    float vol = snd_vol, duck_k = 1.0f - 1.0f / (0.12f * AFS);
    audio_cmds();
    for (i = 0; i < n; i += CHUNK) {
        uint32_t m = n - i < CHUNK ? n - i : CHUNK;
        seq_advance(m);
        for (j = 0; j < m; j++)
            bus_mel[j] = bus_drm[j] = bus_dl[j] = bus_ch[j] = 0.0f;
        for (v = 0; v < NVOICE; v++)
            if (vo[v].on)
                render_voice(&vo[v], m);
        for (j = 0; j < m; j++) {
            float d, c, x, ax, rd;
            uint32_t r0;
            /* the delay: 200 ms, feedback 0.35 */
            d = (float)dl_buf[dl_w] * (1.0f / 16000.0f);
            x = fm_clampf(bus_dl[j] + d * 0.35f, -2.0f, 2.0f);
            dl_buf[dl_w] = (int16_t)(x * 16000.0f);
            dl_w = dl_w + 1u >= DL_LEN ? 0u : dl_w + 1u;
            /* the chorus: one 15 ms line swept +-3 ms */
            ch_buf[ch_w] = bus_ch[j];
            ch_lfo += 0.8f * TAU / AFS;
            if (ch_lfo > TAU)
                ch_lfo -= TAU;
            rd = 661.5f + 132.0f * sintab[(uint32_t)(ch_lfo * (1024.0f / TAU)) & 1023u];
            r0 = (uint32_t)rd;
            {
                float fr = rd - (float)r0, s0 = ch_buf[(ch_w + CH_LEN - r0) % CH_LEN], s1 = ch_buf[(ch_w + CH_LEN - r0 - 1u) % CH_LEN];
                c = s0 + (s1 - s0) * fr;
            }
            ch_w = (ch_w + 1u) % CH_LEN;
            /* the mix: the kick ducks the melodic bus by 4 dB */
            duck *= duck_k;
            x = bus_mel[j] * (1.0f - 0.37f * duck) + bus_drm[j] + d * 0.8f + c * 0.7f;
            x *= 0.5f * vol;
            /* the soft-knee limiter: straight to -6 dBFS, then bending to full scale */
            ax = x < 0.0f ? -x : x;
            if (ax > 0.5f) {
                float u = fm_minf((ax - 0.5f) * 2.0f, 3.0f);
                ax = 0.5f + 0.5f * (u * (27.0f + u * u) / (27.0f + 9.0f * u * u));
                x = x < 0.0f ? -ax : ax;
            }
            gain_now += (gain - gain_now) * 0.004f;
            lr[2u * (i + j)] = lr[2u * (i + j) + 1u] = (int32_t)(x * gain_now);
        }
    }
}

void app_panic(void)
{
    uint32_t v;
    for (v = 0; v < NVOICE; v++) {
        vo[v].on = 0;
        vo[v].lvl = 0.0f;
    }
}

void app_overload(void)                        /* the render ran late: fewer voices from now on */
{
    if (voice_limit > 6u)
        voice_limit--;
}

static void voice_init(void)
{
    uint32_t i;
    for (i = 0; i < 1024u; i++)
        sintab[i] = fm_sinf(TAU * (float)i / 1024.0f);
    seq.bpm = 110.0f;
    seq.cur_len = 6000.0f;
}
