/* The remastered music (music_rm.h). */
#include "sound/music_rm.h"

#include "host.h"
#include "arrange.h"
#include "sound/snd_int.h"
#include "synth.h"

bool music_rm_on;
static bool with_host;
static Song song;
static Arrangement arr;
static int last_event = -1;
static bool sounding;

#define UPPER_MAX 6
typedef struct { int voice, ticks; } Held;
static Held bass = { -1, 0 }, upper[UPPER_MAX];
static int lead_voice = -1;             /* the note the vibrato bends */

static void render_host(float *lr, int frames) { synth_render(lr, frames); }

bool music_rm_enable(bool host)
{
    if (!song_parse(SEG(SEG_SONGS_BG), 0, &song)) return false;
    arrange(&song, &arr);
    for (int i = 0; i < UPPER_MAX; i++) upper[i].voice = -1;
    bass.voice = -1;
    last_event = -1;
    sounding = false;
    with_host = host;
    music_rm_on = true;
    if (host) {
        synth_init(host_audio_rate());
        host_set_music_synth(render_host);
    }
    return true;
}

void music_rm_disable(void)
{
    if (with_host) host_set_music_synth(NULL);
    music_rm_on = false;
}

static void sync(void) { if (with_host) host_audio_sync(); }

static float pan_of(int inst)
{
    return inst == INST_EPIANO ? 0.15f : inst == INST_VIBES ? 0.3f : -0.25f;
}

void music_rm_note(u16 off, u16 divisor)
{
    if (!music_rm_on) return;
    int i = song_find(&song, off);
    if (i < 0 || song.ev[i].note != divisor) return;   /* not the tune we read (cannot happen) */
    sync();
    last_event = i;
    sounding = true;
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
    for (int j = 0; j < UPPER_MAX; j++) {               /* a free slot, else the one ending first */
        if (upper[j].voice < 0) { k = j; break; }
        if (upper[j].ticks < upper[k].ticks) k = j;
    }
    synth_release(upper[k].voice);
    upper[k].voice = synth_note(inst, n->midi, (inst == INST_VIBES ? 0.8f : 0.7f) * n->vel, pan_of(inst));
    upper[k].ticks = n->hold;
    lead_voice = upper[k].voice;
}

void music_rm_rest(void) { lead_voice = -1; }

void music_rm_bend(u16 divisor)
{
    if (!music_rm_on || lead_voice < 0 || !divisor) return;
    sync();
    synth_bend(lead_voice, arr_midi(divisor));
}

int music_rm_last_event(void) { return last_event; }

void music_rm_tick(void)
{
    if (!music_rm_on) return;
    bool playing = DSW(DS_music_cur) == DS_bg_song && DSB(DS_bg_song + SS_PLAYING) && DSB(DS_sound_on);
    if (!playing) {
        if (sounding) {
            sync();
            synth_all_off();
            bass.voice = -1;
            for (int i = 0; i < UPPER_MAX; i++) upper[i].voice = -1;
            lead_voice = -1;
            sounding = false;
        }
        return;
    }
    if (bass.voice >= 0 && --bass.ticks <= 0) { sync(); synth_release(bass.voice); bass.voice = -1; }
    for (int i = 0; i < UPPER_MAX; i++)
        if (upper[i].voice >= 0 && --upper[i].ticks <= 0) { sync(); synth_release(upper[i].voice); upper[i].voice = -1; }
}
