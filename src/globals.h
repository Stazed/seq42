//----------------------------------------------------------------------------
//
//  This file is part of seq42.
//
//  seq42 is free software; you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation; either version 2 of the License, or
//  (at your option) any later version.
//
//  seq42 is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with seq42; if not, write to the Free Software
//  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//
//-----------------------------------------------------------------------------

#pragma once

#ifdef HAVE_CONFIG_H
#    include "config.h"
#else
#    include "configdefault.h"
#endif

#include <string>
#include <gtkmm/main.h>
#include <gtkmm/drawingarea.h>
//For keys
#include <gtkmm/accelkey.h>

using namespace std;

#define GTKMM_3_SUPPORT 1

const int c_file_version = 7;  // Version of our save file format.  Increment this whenever the format of the save file changes.
/* Version history:
0 - initial seq42 file format
1 - added transposable to track
2 - added swing amount to perform and swing_mode to sequence
3 - added track mute, track index
4 - added beats per measure, beat width
5 - Use int32_t for 32 bit 64 bit compatibility, add file identification, add date & time stamp
6 - Use double for BPM
7 - Use tempo list for BPM
*/

/* for 32 bit & 64 bit compatible - file version 5 */
extern short global_file_int_size;  // default sizeof(int32_t)
extern short global_file_long_int_size; // default sizeof(int32_t) - define in mainwnd
const uint64_t c_file_identification =  0x293A323451455389; // \211 S E Q 42 : )
const short global_VERSION_array_size = 8;
const short global_time_array_size = 32;

const int c_max_track_name = 16;
const int c_max_seq_name = 32;

const int c_max_track = 64;         // FIXME: use a list instead of a fixed number array?
const int c_max_undo_track = 100;   // FIXME how big??
const int c_max_undo_perf  = 40;    // FIXME how big??

const int c_ppqn         = 192;  /* default - doesn't change */
const int c_ppwn         = c_ppqn * 4;  // whole note
const int c_ppen         = c_ppqn / 2;  // eighth note
const int c_ppsn         = c_ppqn / 4;  // 16th note

const int c_note_off_margin = 2;  // # ticks to shave off end of painted notes

const long c_note_on_velocity_default = 100;
const long c_note_off_velocity_default = 64;

const int c_default_trigger_length_in_bars = 1; /* used when adding a new trigger */
const long c_default_trigger_length = c_default_trigger_length_in_bars * 4 * c_ppqn;

const double c_bpm                       = 120.0;   /* default */
const double c_bpm_scale_factor          = 1000.0;  /* used in midifile for doubles */
const double c_bpm_minimum               = 1.0;
const double c_bpm_maximum               = 600.0;

const int c_maxBuses = 32;

const int c_max_swing_amount = 24;
// constants for sequence swing modes (int value is the amount to divide c_ppqn by to obtain note length)
const int c_no_swing = 0;
const int c_swing_eighths  = 2;
const int c_swing_sixteenths = 4;

/* trigger width in milliseconds */
const int c_thread_trigger_width_ms = 4;
const int c_thread_trigger_lookahead_ms = 2;

/* for the seqarea class */
const int c_text_x = 6;
const int c_text_y = 12;
const int c_seqarea_x = c_text_x * 15;
const int c_seqarea_y =  c_text_y * 5;

const int c_mainwid_border = 0;
const int c_mainwid_spacing = 2;

const int c_control_height = 0;

/* data entry area (velocity, aftertouch, etc ) */
const int c_dataarea_y = 128;
/* width of 'bar' */
const int c_data_x = 2;
/* size of handle */
const int c_data_handle_radius = 4;
const char * const c_font = "monospace";
const int c_key_fontsize = 8;

/* keyboard */
const int c_key_x = 16;
const int c_key_y = 8;
const int c_num_keys = 128;
const int c_keyarea_y = c_key_y * c_num_keys + 1;
const int c_keyarea_x = 36;
const int c_keyoffset_x = c_keyarea_x - c_key_x;

/* paino roll */
const int c_rollarea_y = c_keyarea_y;

/* events bar */
const int c_eventarea_y = 16;
const int c_eventevent_y = 10;
const int c_eventevent_x = 5;

/* time scale window on top */
const int c_timearea_y = 18;

/* sequences */
const int c_midi_notes = 256;
const std::string c_dummy( "Untitled" );

/* maximum size of sequence, default size */
const int c_maxbeats     = 0xFFFF;   /* max number of beats in a sequence */

/* midifile tags */
const unsigned long c_midibus      = 0x24240001;
const unsigned long c_midich       = 0x24240002;
const unsigned long c_midiclocks   = 0x24240003;
const unsigned long c_triggers     = 0x24240004;
const unsigned long c_notes        = 0x24240005;
const unsigned long c_timesig      = 0x24240006;
const unsigned long c_bpmtag       = 0x24240007;
const unsigned long c_triggers_new = 0x24240008;
const unsigned long c_mutegroups   = 0x24240009;
const unsigned long c_midictrl     = 0x24240010;

/* From sequencer64 */
//const midilong c_musickey =     0x24240011; /**< The track's key.           */
//const midilong c_musicscale =   0x24240012; /**< The track's scale.         */
//const midilong c_backsequence = 0x24240013; /**< Track background sequence. */

const unsigned long c_transpose    = 0x24240014;  // 7/20/16
const unsigned long c_perf_bp_mes  = 0x24240015;  // perfedit beats per measure
const unsigned long c_perf_bw      = 0x24240016;  // perfedit beat width
const unsigned long c_tempo_map    = 0x24240017;  // perfedit tempo map

/* these are never used */
const char c_font_6_12[] = "-*-fixed-medium-r-*--12-*-*-*-*-*-*";
const char c_font_8_13[] = "-*-fixed-medium-r-*--13-*-*-*-*-*-*";
const char c_font_5_7[]  = "-*-fixed-medium-r-*--7-*-*-*-*-*-*";

/* used in menu to tell setState what to do */
const int c_adding = 0;
const int c_normal = 1;
const int c_paste  = 2;

/* used for undo/redo vector */
const int c_undo_trigger = 0;
const int c_undo_track   = 1;
const int c_undo_perf    = 2;
const int c_undo_collapse_expand = 3;
const int c_undo_bpm     = 4;
const int c_undo_import  = 5;

/* redraw when recording ms */
#ifdef __WIN32__
const int c_redraw_ms = 20;
#else
const int c_redraw_ms = 25;
#endif

/* consts for perform editor */
const int c_names_x = 114;
const int c_names_y = 22;
const int c_perf_scale_x = 32; /*ticks per pixel */
const int c_perf_max_zoom = 8;

extern bool global_showmidi;
extern bool global_priority;
extern bool global_stats;
extern bool global_pass_sysex;

extern bool global_with_jack_transport;
extern bool global_with_jack_master;
extern bool global_with_jack_master_cond;
extern bool global_song_start_mode;
extern bool global_manual_alsa_ports;

/*
    global_is_running:
    initialize in mainwnd.cpp = false:
    set in perform::inner_start() = true:
    set in perform::inner_stop() = false:
    This should not be reset under any other circumstance!
*/
extern bool global_is_running;
extern bool global_is_modified;
extern bool global_seqlist_need_update;
extern bool global_solo_track_set;
#ifdef NSM_SUPPORT
extern bool global_nsm_gui;
#endif

extern Glib::ustring global_filename;
extern Glib::ustring global_jack_session_uuid;
extern Glib::ustring last_used_dir;
extern Glib::ustring last_midi_dir;
/**
 *  Indicates the maximum number of recently-opened .s42 file-names we will
 *  store.
 */
const int c_max_recent_files = 10;

extern bool global_print_keys;

extern Glib::ustring global_client_name;

const int c_max_instruments = 64;

struct user_midi_bus_definition
{
    std::string alias;
    int instrument[16];
};

struct user_instrument_definition
{
    std::string instrument;
    bool controllers_active[128];
    std::string controllers[128];
};

extern user_midi_bus_definition   global_user_midi_bus_definitions[c_maxBuses];
extern user_instrument_definition global_user_instrument_definitions[c_max_instruments];

/* scales */
enum c_music_scales
{
    c_scale_off,
    c_scale_major,
    c_scale_minor,
    c_scale_harmonic_minor,
    c_scale_melodic_minor,
    c_scale_c_whole_tone,
    c_scale_size            // a "maximum" or "size of set" value.
};

const bool c_scales_policy[c_scale_size][12] =
{
    {                                                       /* off = chromatic */
        true, true, true, true, true, true,
        true, true, true, true, true, true
    },
    {                                                       /* major           */
        true, false, true, false, true, true,
        false, true, false, true, false, true
    },
    {                                                       /* minor           */
        true, false, true, true, false, true,
        false, true, true, false, true, false
    },
    {                                                       /* harmonic minor  */
        true, false, true, true, false, true,
        false, true, true, false, false, true
    },
    {                                                       /* melodic minor   */
        true, false, true, true, false, true,
        false, true, false, true, false, true
    },
    {                                                       /* whole tone      */
        true, false, true, false, true, false,
        true, false, true, false, true, false
    },
};

const int c_scales_transpose_up[c_scale_size][12] =
{
    { 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},              /* off = chromatic */
    { 2, 0, 2, 0, 1, 2, 0, 2, 0, 2, 0, 1},              /* major           */
    { 2, 0, 1, 2, 0, 2, 0, 1, 2, 0, 2, 0},              /* minor           */
    { 2, 0, 1, 2, 0, 2, 0, 1, 3, 0, 0, 1},              /* harmonic minor  */
    { 2, 0, 1, 2, 0, 2, 0, 2, 0, 2, 0, 1},              /* melodic minor   */
    { 2, 0, 2, 0, 2, 0, 2, 0, 2, 0, 2, 0},              /* C whole tone    */
};

const int c_scales_transpose_dn[c_scale_size][12] =
{
    { -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},  /* off = chromatic */
    { -1, 0, -2, 0, -2, -1, 0, -2, 0, -2, 0, -2},       /* major           */
    { -2, 0, -2, -1, 0, -2, 0, -2, -1, 0, -2, 0},       /* minor           */
    { -1, -0, -2, -1, 0, -2, 0, -2, -1, 0, 0, -3},      /* harmonic minor  */
    { -1, 0, -2, -1, 0, -2, 0, -2, 0, -2, 0, -2},       /* melodic minor   */
    { -2, 0, -2, 0, -2, 0, -2, 0, -2, 0, -2, 0},        /* C whole tone    */
};

const int c_scales_symbol[c_scale_size][12] =
{
    /* off = chromatic */
    { 32,32,32,32,32,32,32,32,32,32,32,32},

    /* major */
    { 32,32,32,32,32,32,32,32,32,32,32,32},

    /* minor */
    { 32,32,32,32,32,32,32,32,129,128,129,128},
};

// up 128
// down 129

const char c_scales_text[c_scale_size][32] =
{
    "Off (chromatic)",
    "Major",
    "Minor",
    "Harmonic Minor",
    "Melodic Minor",
    "Whole Tone",
};

const char c_key_text[][3] =
{
    "C",
    "C#",
    "D",
    "D#",
    "E",
    "F",
    "F#",
    "G",
    "G#",
    "A",
    "A#",
    "B"
};

const char c_interval_text[][3] =
{
    "P1",
    "m2",
    "M2",
    "m3",
    "M3",
    "P4",
    "TT",
    "P5",
    "m6",
    "M6",
    "m7",
    "M7",
    "P8",
    "m9",
    "M9",
    ""
};

const char c_chord_text[][5] =
{
    "I",
    "II",
    "III",
    "IV",
    "V",
    "VI",
    "VII",
    "VIII"
};

const int c_chord_number = 40;

const char c_chord_table_text[c_chord_number][32] =
{
    "Off",
    "Major",
    "Majb5",
    "minor",
    "minb5",
    "sus2",
    "sus4",
    "aug",
    "augsus4",
    "tri",

    "6",
    "6sus4",
    "6add9",
    "m6",
    "m6add9",

    "7",
    "7sus4",
    "7#5",
    "7b5",
    "7#9",
    "7b9",
    "7#5#9",
    "7#5b9",
    "7b5b9",
    "7add11",
    "7add13",
    "7#11",
    "Maj7",
    "Maj7b5",
    "Maj7#5",
    "Maj7#11",
    "Maj7add13",
    "m7",
    "m7b5",
    "m7b9",
    "m7add11",
    "m7add13",
    "m-Maj7",
    "m-Maj7add11",
    "m-Maj7add13"
};

const int c_chord_size = 6;

const int c_chord_table[c_chord_number][c_chord_size] =
{
    { 0, -1, 0, 0, 0, 0 },
    { 0, 4, 7, -1, 0, 0 },
    { 0, 4, 6, -1, 0, 0 },
    { 0, 3, 7, -1, 0, 0 },
    { 0, 3, 6, -1, 0, 0 },
    { 0, 2, 7, -1, 0, 0 },
    { 0, 5, 7, -1, 0, 0 },
    { 0, 4, 8, -1, 0, 0 },
    { 0, 5, 8, -1, 0, 0 },
    { 0, 3, 6, 9, -1, 0 },

    { 0, 4, 7, 9, -1, 0 },
    { 0, 5, 7, 9, -1, 0 },
    { 0, 4, 7, 9, 14, -1 },
    { 0, 3, 7, 9, -1, 0 },
    { 0, 3, 7, 9, 14, -1 },

    { 0, 4, 7, 10, -1, 0 },
    { 0, 5, 7, 10, -1, 0 },
    { 0, 4, 8, 10, -1, 0 },
    { 0, 4, 6, 10, -1, 0 },
    { 0, 4, 7, 10, 15, -1 },
    { 0, 4, 7, 10, 13, -1 },
    { 0, 4, 8, 10, 15, -1 },
    { 0, 4, 8, 10, 13, -1 },
    { 0, 4, 6, 10, 13, -1 },
    { 0, 4, 7, 10, 17, -1 },
    { 0, 4, 7, 10, 21, -1 },
    { 0, 4, 7, 10, 18, -1 },
    { 0, 4, 7, 11, -1, 0 },
    { 0, 4, 6, 11, -1, 0 },
    { 0, 4, 8, 11, -1, 0 },
    { 0, 4, 7, 11, 18, -1 },
    { 0, 4, 7, 11, 21, -1 },
    { 0, 3, 7, 10, -1, 0 },
    { 0, 3, 6, 10, -1, 0 },
    { 0, 3, 7, 10, 13, -1 },
    { 0, 3, 7, 10, 17, -1 },
    { 0, 3, 7, 10, 21, -1 },
    { 0, 3, 7, 11, -1, 0 },
    { 0, 3, 7, 11, 17, -1 },
    { 0, 3, 7, 11, 21, -1 }
};

enum mouse_action_e
{
    e_action_select,
    e_action_draw,
    e_action_grow
};

enum interaction_method_e
{
    e_seq42_interaction,
    e_fruity_interaction,
    e_number_of_interactions // keep this one last...
};

const char* const c_interaction_method_names[] =
{
    "seq42",
    "fruity",
    NULL
};

const char* const c_interaction_method_descs[] =
{
    "original seq42 method",
    "similar to a certain fruity sequencer we like",
    NULL
};

extern interaction_method_e global_interactionmethod;
extern bool global_interaction_method_change;

template <typename T>
string NumberToString ( T Number )
{
    stringstream ss;
    ss << Number;
    return ss.str();
}

#define SEQ24_SCREEN_SET_SIZE (32)

enum file_type_e
{
    E_SEQ42_NATIVE_FILE,
    E_MIDI_SEQ24_FORMAT,
    E_MIDI_SONG_FORMAT,
    E_MIDI_SOLO_SEQUENCE,
    E_MIDI_SOLO_TRIGGER,
    E_MIDI_SOLO_TRACK
};

int FF_RW_timeout(void *arg);