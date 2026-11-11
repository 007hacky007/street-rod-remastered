/* The remastered music's synthesiser (synth.h). */
#include "synth.h"

#include <math.h>
#include <string.h>

#include "arrange.h"

#define VOICES   24
#define DRUMS    8
#define MAX_LINE 4096          /* Karplus-Strong delay line: down to ~11 Hz at 44.1 kHz */
#define TWO_PI   6.28318530717958647692

typedef struct {
    bool on;
    int inst;
    float gl, gr;               /* pan gains * velocity */
    float age;                  /* seconds */
    float fade, fade_step;      /* 1 while held; falls to 0 when stopped */
    /* plucked string */
    float line[MAX_LINE];
    int w;
    float delay, last, loop_gain, bright;
    /* FM */
    double pc, pm;
    float freq, env, env_mul, index, index_mul, ratio;
    float level;                /* |output| follower, for stealing and ending */
} Voice;

typedef struct {
    bool on;
    int kind;
    float t, vel, gl, gr;
    double ph;
    float lp, hp, bp1, bp2;
} Drum;

static int rate = 44100;
static float dt;
static Voice voice[VOICES];
static Drum drum[DRUMS];
static unsigned noise_seed = 0x1234567;
static float comp_env;                  /* the master compressor's level */

/* reverb: four combs and two all-passes per channel (Freeverb-like, short) */
static const int comb_len[4] = { 1116, 1188, 1277, 1356 };
static const int ap_len[2] = { 556, 441 };
static float comb_buf[2][4][1400], comb_lp[2][4], ap_buf[2][2][600];
static int comb_pos[2][4], ap_pos[2][2];

static float noise(void)
{
    noise_seed = noise_seed * 1664525u + 1013904223u;
    return (float)((noise_seed >> 9) & 0x7FFF) / 16384.0f - 1.0f;
}

static float hz(float midi) { return 440.0f * powf(2.0f, (midi - 69.0f) / 12.0f); }

void synth_init(int r)
{
    rate = r > 0 ? r : 44100;
    dt = 1.0f / (float)rate;
    memset(voice, 0, sizeof voice);
    memset(drum, 0, sizeof drum);
    memset(comb_buf, 0, sizeof comb_buf);
    memset(comb_lp, 0, sizeof comb_lp);
    memset(ap_buf, 0, sizeof ap_buf);
    memset(comb_pos, 0, sizeof comb_pos);
    memset(ap_pos, 0, sizeof ap_pos);
    comp_env = 0;
}

static void pan_gains(float pan, float vel, float *l, float *r)
{
    float p = (pan < -1 ? -1 : pan > 1 ? 1 : pan) * 0.5f + 0.5f;
    *l = vel * cosf(p * 1.5707963f);
    *r = vel * sinf(p * 1.5707963f);
}

static void set_pitch(Voice *v, float midi)
{
    v->freq = hz(midi);
    if (v->inst == INST_BASS || v->inst == INST_GUITAR) {
        float d = (float)rate / v->freq - 0.5f;              /* the loop filter adds half a sample */
        v->delay = d < 2 ? 2 : d > MAX_LINE - 2 ? MAX_LINE - 2 : d;
    }
}

int synth_note(int inst, float midi, float vel, float pan)
{
    if (inst < 0 || inst >= INST_COUNT) return -1;
    int k = -1;
    float quiet = 1e9f;
    for (int i = 0; i < VOICES; i++) {
        if (!voice[i].on) { k = i; break; }
        float q = voice[i].level * voice[i].fade - voice[i].age * 0.01f;   /* the quietest, older first */
        if (q < quiet) { quiet = q; k = i; }
    }
    Voice *v = &voice[k];
    memset(v, 0, sizeof *v);
    v->on = true;
    v->inst = inst;
    v->fade = 1;
    v->level = 1;
    pan_gains(pan, vel, &v->gl, &v->gr);
    set_pitch(v, midi);
    switch (inst) {
    case INST_BASS:
    case INST_GUITAR: {
        int n = (int)v->delay + 2;
        float lp = 0, a = inst == INST_BASS ? 0.35f : 0.85f;      /* the pluck: low-passed noise */
        for (int i = 0; i < n; i++) {
            lp += a * (noise() - lp);
            v->line[i] = lp * (inst == INST_BASS ? 1.6f : 0.9f);
        }
        v->w = n;
        v->bright = inst == INST_BASS ? 0.45f : 0.62f;
        /* loop gain per period for a decay time of about 1.6 s (bass) / 1.2 s (guitar) */
        float t60 = inst == INST_BASS ? 1.6f : 1.2f;
        v->loop_gain = powf(10.0f, -3.0f / (t60 * v->freq));
        if (v->loop_gain > 0.9995f) v->loop_gain = 0.9995f;
        break;
    }
    case INST_EPIANO:
        v->ratio = 1.0f;
        v->index = 1.8f;
        v->index_mul = powf(0.25f, dt / 0.4f);
        v->env = 1;
        v->env_mul = powf(0.001f, dt / 2.2f);
        break;
    case INST_VIBES:
        v->ratio = 4.0f;
        v->index = 1.1f;
        v->index_mul = powf(0.1f, dt / 0.15f);
        v->env = 1;
        v->env_mul = powf(0.001f, dt / 3.0f);
        break;
    }
    return k;
}

void synth_release(int k)
{
    if (k < 0 || k >= VOICES || !voice[k].on) return;
    Voice *v = &voice[k];
    if (v->fade_step == 0) v->fade_step = dt / 0.12f;          /* 120 ms */
}

void synth_bend(int k, float midi)
{
    if (k < 0 || k >= VOICES || !voice[k].on) return;
    set_pitch(&voice[k], midi);
}

void synth_drum(int kind, float vel)
{
    int k = 0;
    for (int i = 0; i < DRUMS; i++) {
        if (!drum[i].on) { k = i; break; }
        if (drum[i].t > drum[k].t) k = i;
    }
    Drum *d = &drum[k];
    memset(d, 0, sizeof *d);
    d->on = true;
    d->kind = kind;
    d->vel = vel;
    pan_gains(kind == DRUM_SNARE ? 0.2f : kind == DRUM_BRUSH ? -0.35f : 0, 1, &d->gl, &d->gr);
}

void synth_all_off(void)
{
    for (int i = 0; i < VOICES; i++)
        if (voice[i].on) voice[i].fade_step = dt / 0.03f;
    for (int i = 0; i < DRUMS; i++) drum[i].on = false;
}

static float voice_sample(Voice *v)
{
    float out;
    if (v->inst == INST_BASS || v->inst == INST_GUITAR) {
        float rp = (float)v->w - v->delay;                     /* read position, fractional */
        while (rp < 0) rp += MAX_LINE;
        int i0 = (int)rp;
        float fr = rp - (float)i0;
        float a = v->line[i0 % MAX_LINE], b = v->line[(i0 + 1) % MAX_LINE];
        float x = a + fr * (b - a);
        float y = v->bright * x + (1 - v->bright) * v->last;   /* the loop's low-pass */
        v->last = y;
        float g = v->fade_step > 0 ? 0.990f : v->loop_gain;    /* dampened when released */
        v->line[v->w % MAX_LINE] = y * g;
        v->w = (v->w + 1) % MAX_LINE;
        out = v->inst == INST_GUITAR ? tanhf(1.6f * x) * 0.75f : x;   /* a little twang */
    } else {
        v->pm += TWO_PI * v->freq * v->ratio * dt;
        v->pc += TWO_PI * v->freq * dt;
        if (v->pm > TWO_PI) v->pm -= TWO_PI;
        if (v->pc > TWO_PI) v->pc -= TWO_PI;
        float m = v->index * sinf((float)v->pm);
        out = sinf((float)v->pc + m) * v->env;
        if (v->inst == INST_VIBES) out *= 1.0f - 0.22f * (0.5f + 0.5f * sinf(TWO_PI * 5.5f * v->age));
        float atk = v->age < 0.003f ? v->age / 0.003f : 1;
        out *= atk * 0.7f;
        v->env *= v->env_mul;
        v->index *= v->index_mul;
    }
    v->age += dt;
    if (v->fade_step > 0) {
        v->fade -= v->fade_step;
        if (v->fade <= 0) { v->on = false; return 0; }
    }
    out *= v->fade;
    v->level += 0.001f * (fabsf(out) - v->level);
    if (v->age > 0.5f && v->level < 1e-4f) v->on = false;
    return out;
}

static float drum_sample(Drum *d)
{
    float t = d->t, out = 0;
    switch (d->kind) {
    case DRUM_KICK: {
        float f = 48 + 95 * expf(-t / 0.035f);
        d->ph += TWO_PI * f * dt;
        out = sinf((float)d->ph) * expf(-t / 0.22f) * 0.9f + noise() * expf(-t / 0.004f) * 0.15f;
        if (t > 1.2f) d->on = false;
        break;
    }
    case DRUM_SNARE: {
        float n = noise();
        d->bp1 += 0.45f * (n - d->bp1);                        /* band: low-pass minus a slower low-pass */
        d->bp2 += 0.06f * (d->bp1 - d->bp2);
        d->ph += TWO_PI * 185 * dt;
        out = (d->bp1 - d->bp2) * expf(-t / 0.11f) * 0.7f + sinf((float)d->ph) * expf(-t / 0.05f) * 0.35f;
        if (t > 0.8f) d->on = false;
        break;
    }
    case DRUM_BRUSH: {
        float n = noise();
        d->lp += 0.5f * (n - d->lp);
        float hp = n - d->lp;
        float env = t < 0.004f ? t / 0.004f : expf(-(t - 0.004f) / 0.06f);
        out = hp * env * 0.35f;
        if (t > 0.5f) d->on = false;
        break;
    }
    }
    d->t += dt;
    return out * d->vel;
}

static float reverb_channel(int c, float x)
{
    float s = 0;
    for (int i = 0; i < 4; i++) {
        int len = comb_len[i] * rate / 44100 + c * 23;
        if (len > 1400) len = 1400;
        float *b = comb_buf[c][i];
        float y = b[comb_pos[c][i]];
        comb_lp[c][i] = y * 0.7f + comb_lp[c][i] * 0.3f;       /* damping */
        b[comb_pos[c][i]] = x * 0.05f + comb_lp[c][i] * 0.78f;   /* four combs gain ~4.5x: wet ~20% */
        if (++comb_pos[c][i] >= len) comb_pos[c][i] = 0;
        s += y;
    }
    for (int i = 0; i < 2; i++) {
        int len = ap_len[i] * rate / 44100 + c * 13;
        if (len > 600) len = 600;
        float *b = ap_buf[c][i];
        float bo = b[ap_pos[c][i]];
        float y = -s + bo;
        b[ap_pos[c][i]] = s + bo * 0.5f;
        if (++ap_pos[c][i] >= len) ap_pos[c][i] = 0;
        s = y;
    }
    return s * 0.25f;
}

static float soft_clip(float x)
{
    if (x > 3) return 1;
    if (x < -3) return -1;
    return x * (27 + x * x) / (27 + 9 * x * x);
}

void synth_render(float *lr, int frames)
{
    const float atk = 1 - expf(-1.0f / (0.005f * rate)), rel = 1 - expf(-1.0f / (0.15f * rate));
    for (int f = 0; f < frames; f++) {
        float l = 0, r = 0;
        for (int i = 0; i < VOICES; i++) {
            Voice *v = &voice[i];
            if (!v->on) continue;
            float s = voice_sample(v);
            l += s * v->gl;
            r += s * v->gr;
        }
        for (int i = 0; i < DRUMS; i++) {
            Drum *d = &drum[i];
            if (!d->on) continue;
            float s = drum_sample(d);
            l += s * d->gl;
            r += s * d->gr;
        }
        float wet_in = (l + r) * 0.5f;
        float wl = reverb_channel(0, wet_in), wr = reverb_channel(1, wet_in);
        l += wl;
        r += wr;
        float a = fmaxf(fabsf(l), fabsf(r));                   /* a gentle compressor: 3:1 above 0.4 */
        comp_env += (a > comp_env ? atk : rel) * (a - comp_env);
        float g = comp_env > 0.4f ? (0.4f + (comp_env - 0.4f) / 3) / comp_env : 1;
        lr[2 * f] = soft_clip(l * g);
        lr[2 * f + 1] = soft_clip(r * g);
    }
}
