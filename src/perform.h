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

class perform;

#include "globals.h"
#include "event.h"
#include "midibus.h"
#include "midifile.h"
#include "sequence.h"
#include "track.h"
#include "mutex.h"
#ifndef __WIN32__
#   include <unistd.h>
#endif
#include <pthread.h>

/* if we have jack, include the jack headers */
#ifdef JACK_SUPPORT
#include <jack/jack.h>
#include <jack/transport.h>
#endif

#undef RDEBUG

#undef USE_MODIFIABLE_JACK_TEMPO            // EXPERIMENTAL SEQUENCER64 - won't work with tempo markers
#undef USE_JACK_BBT_POSITION                // old code could be used for debug

#ifdef MIDI_CONTROL_SUPPORT
class midi_control
{
public:

    bool m_active;
    bool m_inverse_active;
    long m_status;
    long m_data;
    long m_min_value;
    long m_max_value;
};

#define NONE -1                             // indicates no data value sent when using midi control toggle group
#endif // MIDI_CONTROL_SUPPORT


enum mute_op
{
    MUTE_TOGGLE     = -1,
    MUTE_OFF        =  0,
    MUTE_ON         =  1
};

enum ff_rw_type_e
{
    FF_RW_REWIND    = -1,
    FF_RW_RELEASE   =  0,
    FF_RW_FORWARD   =  1
};

struct undo_type
{
    char type;
    int track;
    
    undo_type() :
        type(0),
        track(-1) {}
};

struct undo_redo_perf_tracks
{
    track perf_tracks[c_max_track];
};

struct tempo_mark
{
    uint64_t tick;
    double bpm;
    uint32_t bw;            // not used
    uint32_t bp_measure;    // not used
    uint32_t start;         // calculated frame offset start - jack_nframes_t
    double microseconds_start; // calculated offset for clock display
    
    tempo_mark () :
        tick (0),
        bpm (0.0),
        bw (0),
        bp_measure (0),
        start (0),
        microseconds_start(0.0) {}
};

#ifdef JACK_SUPPORT
/*  Bar and beat start at 1. */
struct BBT
{
    unsigned short bar;
    unsigned char beat;
    unsigned short tick;

    BBT () :
        bar(0),
        beat(0),
        tick(0) {}
};


struct position_info
{
    jack_nframes_t frame;

    float tempo;
    int beats_per_bar;
    int beat_type;
    BBT bbt;
    
    position_info() :
        frame(0),
        tempo(0.0),
        beats_per_bar(0),
        beat_type(0),
        bbt() {}
};

struct time_sig
{
    int beats_per_bar;
    int beat_type;

    time_sig () :
        beats_per_bar(0),
        beat_type(0) {}

    time_sig ( int bpb, int note ) :
        beats_per_bar(bpb),
        beat_type(note) {}
};

#endif // JACK_SUPPORT

#define STOP_MARKER         0.0
#define STARTING_MARKER     0

#define PLAYLIST_ZERO       0
#define PLAYLIST_NEXT       1
#define PLAYLIST_PREVIOUS   -1

#ifdef MIDI_CONTROL_SUPPORT

const int c_midi_total_ctrl = 0;
const int c_midi_control_play       = c_midi_total_ctrl;
const int c_midi_control_record     = c_midi_total_ctrl + 1;
const int c_midi_control_FF         = c_midi_total_ctrl + 2;
const int c_midi_control_rewind     = c_midi_total_ctrl + 3;
const int c_midi_control_top        = c_midi_total_ctrl + 4;
const int c_midi_control_playlist   = c_midi_total_ctrl + 5;
const int c_midi_control_reserved1  = c_midi_total_ctrl + 6;    // if this becomes used, you must adjust the offset in check_midi_control()
const int c_midi_control_reserved2  = c_midi_total_ctrl + 7;    // if this becomes used, you must adjust the offset in check_midi_control()
const int c_midi_controls           = c_midi_total_ctrl + 8;

#endif // MIDI_CONTROL_SUPPORT

class perform
{
public:

    //Playlist mode
    void 	set_playlist_mode(bool mode);
    bool 	get_playlist_mode();
    void 	set_playlist_file(const Glib::ustring& fn);
    Glib::ustring get_playlist_current_file();
    int		get_playlist_index();
    bool 	set_playlist_index(int index);

    unsigned int 	m_key_playlist_next;
    unsigned int 	m_key_playlist_prev;
    unsigned int        m_playlist_midi_jump_value;
    bool                m_playlist_midi_control_set;
    bool                m_playlist_stop_mark;
    // end playlist public
private:

    //Playlist mode
    bool m_playlist_mode;
    Glib::ustring m_playlist_file;
    int m_playlist_nfiles;
    int m_playlist_current_idx;
    std::vector<Glib::ustring> m_playlist_fileset;
    // end playlist private

    /* vector of tracks */
    track *m_tracks[c_max_track];
//    track m_clipboard;
    track m_undo_tracks[c_max_undo_track];
    track m_redo_tracks[c_max_undo_track];
    int m_undo_track_count;
    int m_redo_track_count;

    undo_redo_perf_tracks undo_perf[c_max_undo_perf];
    undo_redo_perf_tracks redo_perf[c_max_undo_perf];
    int m_undo_perf_count;
    int m_redo_perf_count;

    bool m_tracks_active[ c_max_track ];
    bool m_seqlist_open;
    bool m_seqlist_toggle;

    bool m_was_active_edit[ c_max_track ];
    bool m_was_active_perf[ c_max_track ];
    bool m_was_active_names[ c_max_track ];
    bool m_is_focus_track[ c_max_track ];

    /* our midibus */
    mastermidibus m_master_bus;

    /* pthread info */
    pthread_t m_out_thread;
    pthread_t m_in_thread;
    bool m_out_thread_launched;
    bool m_in_thread_launched;

    bool m_inputing;
    bool m_outputing;
    bool m_looping;
    bool m_reposition;

    bool m_playback_mode;
    bool m_follow_transport;

    int thread_trigger_width_ms;

    long m_left_tick;
    long m_right_tick;
    long m_starting_tick;

    long m_tick;
    bool m_usemidiclock;
    bool m_midiclockrunning; // stopped or started
    int  m_midiclocktick;
    long  m_midiclockpos;

    int m_bp_measure;
    int m_bw;
    
#ifdef MIDI_CONTROL_SUPPORT
    midi_control m_midi_cc_toggle[ c_midi_controls ];
    midi_control m_midi_cc_on[ c_midi_controls ];
    midi_control m_midi_cc_off[ c_midi_controls ];
    
    int m_note_is_used[c_midi_notes];   // Used to exclude linked note offs from being recorded
    
    bool m_recording_set;           // flag for notifying seqedit to toggle record midi control
    int m_list_sequence_editing;    // the number of sequences to toggle recording
    int m_list_sequence_recording_set;  // the number of sequences that have been set
#endif // MIDI_CONTROL_SUPPORT

    seq42::condition_var m_condition_var;
    seq42::mutex m_mutex;

#ifdef JACK_SUPPORT

    jack_client_t *m_jack_client;
    jack_nframes_t m_jack_frame_current,
                   m_jack_frame_last,
                   m_jack_frame_rate;
    jack_position_t m_jack_pos;
    jack_transport_state_t m_jack_transport_state;
    jack_transport_state_t m_jack_transport_state_last;
    double m_jack_tick;

#endif  // JACK_SUPPORT

    bool m_jack_running;
    bool m_toggle_jack;
    bool m_jack_master;
    long m_jack_stop_tick;

    bool m_load_tempo_list;
    
    /* Allow continue on stop  */
    bool m_continue;
    
    /**
     *  Holds a few .s42 file-names most recently used.  Although this is a
     *  vector, we do not let it grow past c_max_recent_files.
     *
     *  New feature from Oli Kester's kepler34 project via sequencer64
     */

    std::vector<std::string> m_recent_files;

    void inner_start( bool a_state );
    void inner_stop(bool a_midi_clock = false);

public:

    /* used for undo/redo vector */
    vector<undo_type> undo_vect;
    vector<undo_type> redo_vect;

    track m_tracks_clipboard[c_max_track];
    
    /* m_list_play_marker is used to trigger bpm or stops when running in play().
     * As each marker is encountered, it's value is used, then it is erased.
     * It is reset at stop, or when any new marker is set or removed by user.
     * Reset value = m_list_marker in the tempo() class. */
    list < tempo_mark > m_list_play_marker;
    
    /* m_list_total_marker contains all markers including stops.
     * Used for file saving and loading. Contains stop markers.
     * Should always = m_list_marker in the tempo() class.
     * Only adjusted when new marker is set or removed by user  */
    list < tempo_mark > m_list_total_marker;
    
    /* m_list_no_stop_markers contains only playing markers (no stops).
     * It is used by the render_tempomap() (jack) when playing and position_jack().
     * Also used in output_func() when jack is running.
     * Only adjusted when new marker is set or removed by user */
    list < tempo_mark > m_list_no_stop_markers;

    /* for undo/redo */
    stack < list < tempo_mark > >m_list_undo;
    stack < list < tempo_mark > >m_list_redo;

    float m_excell_FF_RW;
    bool m_have_undo;
    bool m_have_redo;

    unsigned int m_key_bpm_up;
    unsigned int m_key_bpm_dn;
    unsigned int m_key_tap_bpm;

    unsigned int m_key_start;
    unsigned int m_key_stop;
    unsigned int m_key_forward;
    unsigned int m_key_rewind;
    unsigned int m_key_pointer;

    unsigned int m_key_loop;
    unsigned int m_key_song;
    unsigned int m_key_jack;
    unsigned int m_key_seqlist;
    unsigned int m_key_follow_trans;
    
    perform();
    ~perform();

    void start_playing();
    void stop_playing();
    void set_continue(bool a_set);

    void FF_rewind();

    void toggle_song_mode();
    void set_playback_mode( bool a_playback_mode );
    bool get_playback_mode();

    void toggle_jack_mode();
    void set_jack_mode(bool a_mode);
    bool get_toggle_jack();
    bool is_jack_running();
    bool is_jack_master();

    void set_follow_transport(bool a_set);
    bool get_follow_transport();
    void toggle_follow_transport();

    void init();

    bool clear_all();

    void launch_input_thread();
    void launch_output_thread();
    void init_jack();
    void deinit_jack();

    void add_track( track *a_track, int a_pref );
    void delete_track( int a_num );

    bool is_track_in_edit( int a_num );
    int get_track_index(const track * a_track );

    void clear_track_triggers( int a_num  );

    long get_tick( )
    {
        return m_tick;
    };

    void set_jack_stop_tick(long a_tick);
    long get_jack_stop_tick( )
    {
        return m_jack_stop_tick;
    };

    void set_left_tick( long a_tick );
    long get_left_tick();

    void set_starting_tick( long a_tick );
    long get_starting_tick();

    void set_right_tick( long a_tick );
    long get_right_tick();

    void move_triggers( bool a_direction, uint64_t location );
    void copy_triggers(  );
    bool paste_triggers (long paste_tick, bool overwrite);
    
    void push_bpm_undo();
    void pop_bpm_undo();
    void pop_bpm_redo();
    // collapse and expand - all tracks
    void push_trigger_undo();
    void pop_trigger_undo();
    void pop_trigger_redo();
    // single track items
    void push_trigger_undo( int a_track );
    void pop_trigger_undo( int a_track );
    void pop_trigger_redo( int a_track );
    // tracks - merge sequence, cross track trigger, track cut, track paste, sequence adds & deletes
    void push_track_undo(int a_track );
    void pop_track_undo(int a_track );
    void pop_track_redo(int a_track );
    // row insert/delete, track pack, midi import
    void push_perf_undo(bool a_import = false);
    void pop_perf_undo(bool a_import = false);
    void pop_perf_redo(bool a_import = false);

    void check_max_undo_redo();
    void set_have_undo();
    void set_have_redo();

    void print();
    void error_message_gtk( Glib::ustring message);
    
#ifdef MIDI_CONTROL_SUPPORT
    midi_control *get_midi_control_toggle( unsigned int a_control );
    midi_control *get_midi_control_on( unsigned int a_control );
    midi_control *get_midi_control_off( unsigned int a_control );
    
    bool check_midi_control(event ev, bool is_recording);
    void handle_midi_control( int a_control, bool a_state, int a_value = NONE );
    void set_sequence_editing_list(bool a_set);
    void set_sequence_record(bool a_record);
    bool get_sequence_record();
#endif // MIDI_CONTROL_SUPPORT
    
    void start( bool a_state );
    void stop();

    void reset_tempo_play_marker_list();
    bool get_tempo_load();
    void set_tempo_load(bool a_load);
    double get_start_tempo();
    void set_start_tempo(double a_bpm);

    void start_jack();
    void stop_jack();
    void position_jack( bool a_state, long a_tick );

    void off_sequences();
    void all_notes_off();

    void set_active(int a_track, bool a_active);
    void set_was_active( int a_track );
    bool is_active_track(int a_track);
    bool is_dirty_perf (int a_sequence);
    bool is_dirty_names (int a_sequence);
    void set_focus_track(int a_track);
    bool is_focus_track(int a_track);

    void new_track( int a_track );

    /* plays all notes to Current tick */
    void play( long a_tick );
    void set_orig_ticks( long a_tick  );

    void tempo_change();

    track *get_track( int a_trk );
    sequence *get_sequence( int a_trk, int a_seq );

    void reset_sequences();

    void set_bpm(double a_bpm);
    double  get_bpm( );

    void set_bp_measure(int a_bp_mes);
    int get_bp_measure( );

    void set_bw(int a_bw);
    int get_bw( );

    void set_swing_amount8(int a_swing_amount);
    int  get_swing_amount8( );
    void set_swing_amount16(int a_swing_amount);
    int  get_swing_amount16( );

    void set_looping( bool a_looping )
    {
        m_looping = a_looping;
    };

    void set_reposition(bool a_pos_type = true);
    bool get_reposition();
    void set_song_mute( mute_op op );

    mastermidibus* get_master_midi_bus( );

    void output_func();
    void input_func();

    unsigned short combine_bytes(unsigned char First, unsigned char Second);
    
    long get_max_trigger();

    bool track_is_song_exportable(int a_track);

    friend class midifile;
    friend class optionsfile;
    friend class options;

    void set_seqlist_open( bool a_edit )
    {
        m_seqlist_open = a_edit;
    };

    bool get_seqlist_open()
    {
        return m_seqlist_open;
    };

    void set_seqlist_toggle( bool a_raise )
    {
        m_seqlist_toggle = a_raise;
    };

    bool get_seqlist_toggle()
    {
        return m_seqlist_toggle;
    };

    void delete_unused_sequences();
    void create_triggers();
    void apply_song_transpose ();
    
    std::string recent_file (int index, bool shorten = true) const;
    void add_recent_file (const std::string & filename, bool file_loading = false);
    /**
     * \getter m_recent_files.size()
     */

    int recent_file_count () const
    {
        return int(m_recent_files.size());
    }

#ifdef JACK_SUPPORT
#ifdef USE_JACK_BBT_POSITION
    void jack_BBT_position(jack_position_t &pos, double jack_tick);

    /* now using jack_process_callback() ca. 7/10/16    */
    friend int jack_sync_callback(jack_transport_state_t state,
                                  jack_position_t *pos, void *arg);
#endif // USE_JACK_BBT_POSITION
    friend position_info solve_tempomap ( jack_nframes_t frame, void *arg );
    friend position_info render_tempomap( jack_nframes_t start, jack_nframes_t length, void *cb, void *arg );
    friend jack_nframes_t tick_to_jack_frame(uint64_t a_tick, double a_bpm, void *arg);
    friend void jack_shutdown(void *arg);
    friend void jack_timebase_callback(jack_transport_state_t state, jack_nframes_t nframes,
                                       jack_position_t *pos, int new_pos, void *arg);
    friend int jack_process_callback(jack_nframes_t nframes, void* arg);
    friend long convert_jack_frame_to_s42_tick(jack_nframes_t a_frame, double a_bpm, void *arg);
    friend long get_current_jack_position(jack_nframes_t a_frame, void *arg);
#endif // JACK_SUPPORT
};

/* located in perform.C */
extern void *output_thread_func(void *a_p);
extern void *input_thread_func(void *a_p);

/* located in mainwnd.h */
extern ff_rw_type_e FF_RW_button_type;

#ifdef JACK_SUPPORT
#ifdef USE_JACK_BBT_POSITION
int jack_sync_callback(jack_transport_state_t state,
                       jack_position_t *pos, void *arg);
#endif // USE_JACK_BBT_POSITION
position_info solve_tempomap ( jack_nframes_t frame, void *arg );
position_info render_tempomap( jack_nframes_t start, jack_nframes_t length, void *cb, void *arg );
jack_nframes_t tick_to_jack_frame(uint64_t a_tick, double a_bpm, void *arg);
void print_jack_pos( jack_position_t* jack_pos );
void jack_shutdown(void *arg);
void jack_timebase_callback(jack_transport_state_t state, jack_nframes_t nframes,
                            jack_position_t *pos, int new_pos, void *arg);
int jack_process_callback(jack_nframes_t nframes, void* arg);
long convert_jack_frame_to_s42_tick(jack_nframes_t a_frame, double a_bpm, void *arg);
long get_current_jack_position(jack_nframes_t a_frame, void *arg);
#endif // JACK_SUPPORT
