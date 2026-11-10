/* The remastered music's score (arrange.h). */
#include "sound/arrange.h"

#include <math.h>
#include <string.h>

#include "mem.h"

bool song_parse(u16 seg, u16 base, Song *out)
{
    out->n = 0;
    out->nsec = 1;
    u16 o = (u16)(base + 4);
    for (int guard = 0; guard < 4 * SONG_MAX_EVENTS; guard++) {
        u16 at = o, dur = rd16(seg, o);
        o = (u16)(o + 2);
        if (dur == 0) {
            u16 m = rd16(seg, o);
            o = (u16)(o + 2);
            if (m == 0) return out->n > 0;
            if (out->nsec < SONG_MAX_SECTIONS) out->nsec++;
            continue;
        }
        if (out->n == SONG_MAX_EVENTS) return false;
        SongEvent *e = &out->ev[out->n++];
        e->off = at;
        e->dur = dur;
        e->note = rd16(seg, o);
        e->section = (u8)(out->nsec - 1);
        o = (u16)(o + 2);
    }
    return false;
}

int song_find(const Song *s, u16 off)
{
    int lo = 0, hi = s->n - 1;
    while (lo <= hi) {
        int m = (lo + hi) / 2;
        if (s->ev[m].off == off) return m;
        if (s->ev[m].off < off) lo = m + 1;
        else hi = m - 1;
    }
    return -1;
}

float arr_midi(u16 d)
{
    return d ? (float)(69.0 + 12.0 * log2(1193182.0 / d / 440.0)) : 0;
}

#define MAX_HOLD_BASS  40       /* ticks (0.55 s) */
#define MAX_HOLD_UPPER 48
#define REST_CUT       10       /* a rest this long ends the notes before it */
#define KICK_GAP       12       /* ticks between bass drum hits at least */
#define BRUSH_GAP      10

/* Split pitch of a section: notes below it are the bass. Largest gap (>= 5 semitones) between used pitches
 * with at least 15% of the notes on either side; else one voice, by the median. */
static float split_of(const Song *s, int first, int end, bool *one_voice, float *median)
{
    int count[128] = { 0 }, n = 0;
    for (int i = first; i < end; i++)
        if (s->ev[i].note) {
            int m = (int)lroundf(arr_midi(s->ev[i].note));
            if (m >= 0 && m < 128) { count[m]++; n++; }
        }
    *one_voice = true;
    *median = 60;
    if (n == 0) return 0;
    int acc = 0;
    for (int m = 0; m < 128; m++) {
        acc += count[m];
        if (2 * acc >= n) { *median = (float)m; break; }
    }
    int best_gap = 0;
    float split = 0;
    int below = 0, prev = -1;
    for (int m = 0; m < 128; m++) {
        if (!count[m]) continue;
        if (prev >= 0 && m - prev >= 5 && m - prev > best_gap && below * 100 >= 15 * n && (n - below) * 100 >= 15 * n) {
            best_gap = m - prev;
            split = (float)(prev + m) / 2;
        }
        below += count[m];
        prev = m;
    }
    if (best_gap) *one_voice = false;
    return split;
}

void arrange(const Song *s, Arrangement *a)
{
    memset(a, 0, sizeof *a);
    int first = 0;
    for (int sec = 0; sec < s->nsec && first <= s->n; sec++) {
        int end = first;
        while (end < s->n && s->ev[end].section == sec) end++;
        bool one;
        float median, split = split_of(s, first, end, &one, &median);
        /* voices */
        int nupper = 0;
        float upper_sum = 0;
        for (int i = first; i < end; i++) {
            ArrNote *n = &a->note[i];
            if (!s->ev[i].note) continue;
            n->midi = arr_midi(s->ev[i].note);
            n->voice = one ? (median < 55 ? ARR_BASS : ARR_UPPER) : (n->midi < split ? ARR_BASS : ARR_UPPER);
            n->vel = 0.75f + 0.25f * fminf(1.0f, s->ev[i].dur / 16.0f);
            if (n->voice == ARR_UPPER) { nupper++; upper_sum += n->midi; }
        }
        a->sec_inst[sec] = nupper && upper_sum / nupper >= 76 ? INST_VIBES : INST_GUITAR;
        /* a shuffle (the 5 / 11 tick pairs of the boogie) gets the piano */
        int sh = 0, nn = 0;
        for (int i = first; i < end; i++)
            if (s->ev[i].note) { nn++; sh += s->ev[i].dur == 5 || s->ev[i].dur == 11 || s->ev[i].dur == 16; }
        if (a->sec_inst[sec] == INST_GUITAR && !one && nn && sh * 10 >= nn * 8) a->sec_inst[sec] = INST_EPIANO;
        /* holds: until the next note of the same voice (bass) or two upper onsets on (upper), cut by rests */
        for (int i = first; i < end; i++) {
            ArrNote *n = &a->note[i];
            if (!s->ev[i].note) continue;
            int t = 0, seen = 0, hold = 0;
            int max = n->voice == ARR_BASS ? MAX_HOLD_BASS : MAX_HOLD_UPPER;
            for (int j = i; j < end && t < max; j++) {
                if (j > i && !s->ev[j].note && s->ev[j].dur >= REST_CUT) break;
                if (j > i && s->ev[j].note && a->note[j].voice == n->voice && ++seen == (n->voice == ARR_BASS ? 1 : 2)) break;
                t += s->ev[j].dur;
                hold = t;
            }
            if (hold < s->ev[i].dur) hold = s->ev[i].dur;
            n->hold = (u16)(hold > max ? max : hold);
        }
        /* drums: kick and snare by turns on the bass notes, brushes on every onset */
        bool has_bass = false;
        for (int i = first; i < end; i++) has_bass |= s->ev[i].note && a->note[i].voice == ARR_BASS;
        int since_kick = 1 << 20, since_brush = 1 << 20, k = 0;
        bool jingle = a->sec_inst[sec] == INST_VIBES;
        for (int i = first; i < end; i++) {
            ArrNote *n = &a->note[i];
            if (s->ev[i].note && !jingle) {
                if (has_bass && n->voice == ARR_BASS && since_kick >= KICK_GAP) {
                    n->drums |= k++ & 1 ? ARR_SNARE : ARR_KICK;
                    since_kick = 0;
                }
                if (since_brush >= BRUSH_GAP) { n->drums |= ARR_BRUSH; since_brush = 0; }
            }
            since_kick += s->ev[i].dur;
            since_brush += s->ev[i].dur;
        }
        first = end;
    }
}
