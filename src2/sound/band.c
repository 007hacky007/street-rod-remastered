/* ENH: the remastered music for Street Rod 2 (band.h). */
#include "sound/band.h"

#include <string.h>

#include "arrange.h"
#include "host.h"
#include "sound/snd_int.h"
#include "synth.h"

bool band_on;
static bool with_host;
static int notes_started;

#define VOICES_ADLIB 11
typedef struct {
    BandPart part;          /* from the voice's timbre */
    int voice;              /* the synth's voice sounding, -1 none */
    int note;
    float bend;             /* semitones */
} AdVoice;
static AdVoice ad[VOICES_ADLIB];
static bool perc;
static int bend_range = 1;

/* The PC speaker tune, arranged as Street Rod 1's (music_rm.c). */
static Song song;
static Arrangement arr;
static bool spk_ok, spk_sounding;
#define UPPER_MAX 6
typedef struct { int voice, ticks; } Held;
static Held bass = { -1, 0 }, upper[UPPER_MAX];
static int lead_voice = -1;                         /* the note the vibrato bends */

static const struct { const char *name; int kind, which; } PARTS[] = {
    { "bass1", BAND_NOTE, INST_BASS },      { "bass2", BAND_NOTE, INST_BASS },
    { "jazzguit", BAND_NOTE, INST_GUITAR }, { "guitar1", BAND_NOTE, INST_GUITAR },
    { "elguit1", BAND_NOTE, INST_GUITAR },  { "fuzguit2", BAND_NOTE, INST_GUITAR },
    { "piano1", BAND_NOTE, INST_EPIANO },   { "elpiano1", BAND_NOTE, INST_EPIANO },
    { "moogsynt", BAND_NOTE, INST_VIBES },  { "syn3", BAND_NOTE, INST_VIBES },
    { "bdrum1", BAND_DRUM, DRUM_KICK },     { "tom1", BAND_DRUM, DRUM_TOM },
    { "logdrum1", BAND_DRUM, DRUM_TOM },    { "logdrum2", BAND_DRUM, DRUM_TOM },
    { "snare1", BAND_DRUM, DRUM_SNARE },    { "crash", BAND_DRUM, DRUM_SNARE },
    { "cymbal1", BAND_DRUM, DRUM_SNARE },   { "hihat1", BAND_DRUM, DRUM_BRUSH },
    { "hihat2", BAND_DRUM, DRUM_BRUSH },
};

BandPart band_part_of(const char *timbre)
{
    BandPart p = { BAND_NONE, 0, 0 };
    for (size_t i = 0; timbre && i < sizeof PARTS / sizeof PARTS[0]; i++)
        if (strcmp(timbre, PARTS[i].name) == 0) {
            p.kind = PARTS[i].kind;
            if (p.kind == BAND_NOTE) p.inst = PARTS[i].which;
            else p.drum = PARTS[i].which;
        }
    return p;
}

BandPart band_perc_part(int voice)
{
    static const int DRUM_OF[5] = { DRUM_KICK, DRUM_SNARE, DRUM_TOM, DRUM_SNARE, DRUM_BRUSH };
    BandPart p = { BAND_NONE, 0, 0 };
    if (voice >= 6 && voice <= 10) { p.kind = BAND_DRUM; p.drum = DRUM_OF[voice - 6]; }
    return p;
}

static void render_host(float *lr, int frames) { synth_render(lr, frames); }
static void sync(void) { if (with_host) host_audio_sync(); }

static void reset_voices(void)
{
    for (int v = 0; v < VOICES_ADLIB; v++) { ad[v].voice = -1; ad[v].bend = 0; }
    bass.voice = -1;
    for (int i = 0; i < UPPER_MAX; i++) upper[i].voice = -1;
    lead_voice = -1;
    spk_sounding = false;
}

bool band_enable(bool host)
{
    synth_init(host_audio_rate());
    memset(ad, 0, sizeof ad);
    reset_voices();
    notes_started = 0;
    spk_ok = song_parse(DGROUP, DS_spk_bg_song, &song);
    if (spk_ok) arrange(&song, &arr);
    with_host = host;
    band_on = true;
    if (host) host_set_music_synth(render_host);
    return spk_ok;
}

void band_disable(void)
{
    if (with_host) host_set_music_synth(NULL);
    band_on = false;
}

/* Pans as Street Rod 1's band (music_rm.c): the bass left of the centre, the piano and the vibes right. */
static float pan_of(int inst)
{
    return inst == INST_EPIANO ? 0.15f : inst == INST_VIBES ? 0.3f : -0.25f;
}

/* The AdLib songs play three voices and the drums at once, denser than the speaker tune's arrangement: at
 * Street Rod 1's levels they reached full scale. ADLIB_LEVEL brings them to the speaker band's loudness. */
#define ADLIB_LEVEL 0.75f

static float level_of(int inst)
{
    return ADLIB_LEVEL * (inst == INST_BASS ? 1.0f : inst == INST_VIBES ? 0.8f : 0.7f);
}

static void drum_hit(int drum, int note, float vel)
{
    static const float LEVEL[4] = { 0.55f, 0.35f, 0.16f, 0.45f };   /* KICK, SNARE, BRUSH, TOM */
    float v = ADLIB_LEVEL * LEVEL[drum] * vel / 0.75f;               /* the songs' usual velocity 95 = SR1's level */
    if (drum == DRUM_TOM) synth_drum_pitched(DRUM_TOM, (float)note, v);
    else synth_drum(drum, v);
}

void band_adlib_program(int v, int index)
{
    if (!band_on || v < 0 || v >= VOICES_ADLIB) return;
    ad[v].part = band_part_of(snd_timbre_name(index));
}

void band_adlib_note_on(int v, int note, int vel)
{
    if (!band_on || v < 0 || v >= VOICES_ADLIB || !DSB(DS_sound_on)) return;
    sync();
    float f = (float)vel / 127.0f;
    BandPart p = perc && v >= 6 ? band_perc_part(v) : ad[v].part;
    if (p.kind == BAND_DRUM) {
        drum_hit(p.drum, note, f);
        notes_started++;
        return;
    }
    synth_release(ad[v].voice);                     /* one note per voice, as on the chip */
    ad[v].voice = -1;
    if (p.kind != BAND_NOTE) return;
    ad[v].note = note;
    ad[v].voice = synth_note(p.inst, (float)note + ad[v].bend, level_of(p.inst) * f, pan_of(p.inst));
    notes_started++;
}

void band_adlib_note_off(int v)
{
    if (!band_on || v < 0 || v >= VOICES_ADLIB || ad[v].voice < 0) return;
    sync();
    synth_release(ad[v].voice);
    ad[v].voice = -1;
}

void band_adlib_bend(int v, u16 bend)
{
    if (!band_on || v < 0 || v >= VOICES_ADLIB) return;
    if (bend > 0x3FFF) bend = 0x3FFF;
    ad[v].bend = ((float)bend - 0x2000) / 0x2000 * (float)bend_range;
    if (ad[v].voice >= 0) { sync(); synth_bend(ad[v].voice, (float)ad[v].note + ad[v].bend); }
}

void band_adlib_bend_range(int semitones) { bend_range = semitones > 0 ? semitones : 1; }

void band_adlib_perc(bool on) { perc = on; }

void band_adlib_all_off(void)
{
    if (!band_on) return;
    sync();
    synth_all_off();
    reset_voices();
}

void band_spk_note(u16 off, u16 divisor)
{
    if (!band_on || !spk_ok) return;
    int i = song_find(&song, off);
    if (i < 0 || song.ev[i].note != divisor) return;   /* not the tune that was read (cannot happen) */
    sync();
    spk_sounding = true;
    notes_started++;
    const ArrNote *n = &arr.note[i];
    if (n->drums & ARR_KICK) synth_drum(DRUM_KICK, 0.55f);
    if (n->drums & ARR_SNARE) synth_drum(DRUM_SNARE, 0.35f);
    if (n->drums & ARR_BRUSH) synth_drum(DRUM_BRUSH, 0.12f + 0.06f * (n->vel - 0.75f) * 4);
    if (n->voice == ARR_BASS) {
        synth_release(bass.voice);
        bass.voice = synth_note(INST_BASS, n->midi, 1.0f * n->vel, 0);
        bass.ticks = n->hold;
        lead_voice = bass.voice;
        return;
    }
    int inst = arr.sec_inst[song.ev[i].section], k = 0;
    for (int j = 0; j < UPPER_MAX; j++) {           /* a free slot, else the one ending first */
        if (upper[j].voice < 0) { k = j; break; }
        if (upper[j].ticks < upper[k].ticks) k = j;
    }
    synth_release(upper[k].voice);
    upper[k].voice = synth_note(inst, n->midi, (inst == INST_VIBES ? 0.8f : 0.7f) * n->vel, pan_of(inst));
    upper[k].ticks = n->hold;
    lead_voice = upper[k].voice;
}

void band_spk_rest(void) { lead_voice = -1; }

void band_spk_bend(u16 divisor)
{
    if (!band_on || lead_voice < 0 || !divisor) return;
    sync();
    synth_bend(lead_voice, arr_midi(divisor));
}

void band_tick(void)
{
    static bool was_on = true;
    if (!band_on) return;
    bool on = DSB(DS_sound_on) != 0;
    if (!on && was_on) band_adlib_all_off();        /* Ctrl: the sound off */
    was_on = on;
    if (DSW(DS_snd_device) != 1 || !spk_sounding) return;
    bool playing = DSW(DS_music_cur) == DS_bg_song && DSW(DS_bg_song + SS_PLAYING) && on;
    if (!playing) { band_adlib_all_off(); return; }
    if (bass.voice >= 0 && --bass.ticks <= 0) { sync(); synth_release(bass.voice); bass.voice = -1; }
    for (int i = 0; i < UPPER_MAX; i++)
        if (upper[i].voice >= 0 && --upper[i].ticks <= 0) { sync(); synth_release(upper[i].voice); upper[i].voice = -1; }
}

int band_notes_started(void) { return notes_started; }
