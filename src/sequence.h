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

class sequence;

#include <string>
#include <list>
#include <stack>

#include "event.h"
#include "midibus.h"
#include "globals.h"
#include "mutex.h"
#include "track.h"
#include "trigger.h"

enum draw_type
{
    DRAW_FIN = 0,
    DRAW_NORMAL_LINKED,
    DRAW_NOTE_ON,
    DRAW_NOTE_OFF
};

enum note_play_state
{
    NOTE_PLAY = 0,
    NOTE_MUTE,
    NOTE_SOLO
};

using std::list;

class sequence
{

private:

    track *m_track;

    /* holds the events */
    list < event > m_list_event;
    static list < event > m_list_clipboard;

    list < event > m_list_undo_hold;

    stack < list < event > >m_list_undo;
    stack < list < event > >m_list_redo;

    /* markers */
    list < event >::iterator m_iterator_play;
    list < event >::iterator m_iterator_draw;

    /* polyphonic step edit note counter */
    int m_notes_on;

    /* map for noteon, used when muting, to shut off current
       messages */
    int m_playing_notes[c_midi_notes];
    
    /* flags for solo/mute notes */
    bool m_have_solo;
    note_play_state m_mute_solo_notes[c_midi_notes];

    /* states */
    bool m_playing;
    bool m_recording;
    bool m_quanized_rec;
    bool m_thru;
    bool m_overwrite_recording;
    
    /* flag to indicate play marker has gone to beginning of sequence on looping */
    bool m_loop_reset;

    /* flag indicates that contents has changed from
       a recording */
    bool m_dirty_edit;
    bool m_dirty_perf;

    /* anything editing currently ? */
    bool m_editing;
    bool m_raise;

    /* named sequence */
    string m_name;

    int m_swing_mode;

    /* where were we */
    long m_last_tick;

    long m_trigger_offset;

    /* length of sequence in pulses
       should be powers of two in bars */
    long m_length;
    long m_snap_tick;
    long m_unit_measure;

    /* these are just for the editor to mark things
       in correct time */
    //long m_length_measures;
    long m_time_beats_per_measure;
    long m_time_beat_width;

    /* locking */
    seq42::mutex m_mutex;
    void lock ();
    void unlock ();

    /* used to idenfity which events are ours in the out queue */
    //unsigned char m_tag;

    /* takes an event this sequence is holding and
       places it on our midibus */
    void put_event_on_bus (event * a_e);
    
    /* remove all events from sequence */
    void remove_all ();

    /* sets m_trigger_offset and wraps it to length */
    void set_trigger_offset (long a_trigger_offset);
    long get_trigger_offset ();

    void remove( list<event>::iterator i );
    void remove(const event* e );

public:

    sequence ();
    ~sequence ();

    /* seqdata & lfownd hold for undo */
    void set_hold_undo (bool a_hold);
    int get_hold_undo ();

    bool m_have_undo;
    bool m_have_redo;

    void push_undo (bool a_hold = false);
    void pop_undo ();
    void pop_redo ();

    //
    //  Gets and Sets
    //

    void set_have_undo();
    void set_have_redo();

    void set_track (track *a_track);
    track *get_track ();

    void set_name (const string &a_name);
    void set_name (char *a_name);
    /* returns string of name */
    const char *get_name ();

    void set_swing_mode (int a_mode)
    {
        m_swing_mode = a_mode;
    }

    int get_swing_mode ()
    {
        return m_swing_mode;
    }

    void set_unit_measure ();
    long get_unit_measure ();

    void set_bp_measure (long a_beats_per_measure);
    long get_bp_measure ();

    void set_bw (long a_beat_width);
    long get_bw ();
    void set_rec_vol (long a_rec_vol);

    void set_editing (bool a_edit)
    {
        m_editing = a_edit;
    };
    bool get_editing ()
    {
        return m_editing;
    };

    void set_raise (bool a_edit)
    {
        m_raise = a_edit;
    };
    bool get_raise ()
    {
        return m_raise;
    };

    /* length in ticks */
    void set_length (long a_len, bool a_adjust_triggers = true);
    long get_length ();

    /* returns last tick played..  used by
       editors idle function */
    long get_last_tick ();

    /* sets state.  when playing,
       and sequencer is running, notes
       get dumped to the alsa buffers */
    void set_playing (bool a_p, bool set_dirty_seqlist=true);
    bool get_playing ();
    void toggle_playing ();
    
    /* for solo / muting of notes */
    bool check_any_solo_notes();
    void set_solo_note(int a_note);
    void set_mute_note(int a_note);
    bool is_note_solo(int a_note);
    bool is_note_mute(int a_note);

    void set_recording (bool);
    bool get_recording ();
    void set_snap_tick( int a_st );
    void set_quanized_rec( bool a_qr );
    bool get_quanidez_rec( );
    void set_overwrite_rec( bool a_ov );
    bool get_overwrite_rec( );    
    void set_loop_reset( bool a_reset);
    bool get_loop_reset( );

    void set_thru (bool);
    bool get_thru ();

    /* signals that a redraw is needed from recording */
    /* resets flag on call */
    bool is_dirty_edit ();
    bool is_dirty_perf ();

    void set_dirty();

    /* dumps contents to stdout */
    void print ();

    /* dumps notes from tick and prebuffers to
       ahead.  Called by sequencer thread - performance */
    void play (long a_tick, trigger *a_trigger);
    void send_note_to_bus(int transpose, event transposed_event, event note);
    
    void set_orig_tick (long a_tick);

    //
    //  Selection and Manipulation
    //

    /* adds event to internal list */
    void add_event (const event * a_e);
    
    /* for speed on file loading & midi import these are used to great benefit */
    void add_event_no_sort( const event *a_e );     // all events are added first
    void sort_events();                             // called after all events added, once
    
    bool intersectNotes( long position, long position_note, long& start, long& end, long& note );
    bool intersectEvents( long posstart, long posend, long status, long& start );

    char get_midi_bus ();
    unsigned char get_midi_channel ();

    mastermidibus * get_master_midi_bus ();

    enum select_action_e
    {
        e_select,
        e_select_one,
        e_is_selected,
        e_would_select,
        e_deselect, // deselect under cursor
        e_toggle_selection, // sel/deselect under cursor
        e_remove_one // remove one note under cursor
    };

    /* select note events in range, returns number
       selected */
    int select_note_events (long a_tick_s, int a_note_h,
                            long a_tick_f, int a_note_l, select_action_e a_action );

    /* select events in range, returns number
       selected */
    int select_events (long a_tick_s, long a_tick_f,
                       unsigned char a_status, unsigned char a_cc, select_action_e a_action);

    int select_linked (long a_tick_s, long a_tick_f, unsigned char a_status);

    int select_event_handle( long a_tick_s, long a_tick_f,
                         unsigned char a_status,
                         unsigned char a_cc, int a_data_s);

    int get_num_selected_notes ();
    int get_num_selected_events (unsigned char a_status, unsigned char a_cc);

    void select_all ();

    /* given a note length (in ticks) and a boolean indicating even or odd,
    select all notes where the note on event occurs exactly on an even (or odd)
    multiple of note length.
    Example use: select every note that starts on an even eigth note beat.
    */
    int select_even_or_odd_notes(int note_len, bool even);

    void copy_selected ();
    void paste_selected (long a_tick, int a_note);

    /* returns the 'box' of selected items */
    void get_selected_box (long *a_tick_s, int *a_note_h,
                           long *a_tick_f, int *a_note_l);

    /* returns the 'box' of selected items */
    bool get_clipboard_box (long *a_tick_s, int *a_note_h,
                            long *a_tick_f, int *a_note_l);

    /* removes and adds readds selected in position */
    void move_selected_notes (long a_delta_tick, int a_delta_note);

    /* adds a single note on / note off pair */
    void add_note (long a_tick, long a_length, int a_note, bool a_paint = false);

    void add_event (long a_tick,
                    unsigned char a_status,
                    unsigned char a_d0, unsigned char a_d1, bool a_paint = false);

    bool stream_event (const event * a_ev);

    /* changes velocities in a ramping way from vel_s to vel_f  */
    void change_event_data_range (long a_tick_s, long a_tick_f,
                                  unsigned char a_status,
                                  unsigned char a_cc,
                                  int a_d_s, int a_d_f);
    //unsigned char a_d_s, unsigned char a_d_f);

    /* lfo tool */
    void change_event_data_lfo(double a_value, double a_range,
                               double a_speed, double a_phase, int a_wave,
                               unsigned char a_status, unsigned char a_cc);

    /* moves note off event */
    void increment_selected (unsigned char a_status, unsigned char a_control);
    void decrement_selected (unsigned char a_status, unsigned char a_control);

    void randomize_selected( unsigned char a_status, unsigned char a_control, int a_plus_minus );

    void adjust_data_handle( unsigned char a_status, int a_data );

    /* moves note off event */
    void grow_selected (long a_delta_tick);
    void stretch_selected(long a_delta_tick);

    /* deletes events */
    void remove_marked();
    bool mark_selected();
    void unpaint_all();

    /* unselects every event */
    void unselect ();

    /* verfies state, all noteons have an off,
       links noteoffs with their ons */
    void verify_and_link ();
    void link_new ();

    /* resets everything to zero, used when
       sequencer stops */
    void zero_markers ();

    /* flushes a note to the midibus to preview its
       sound, used by the virtual paino */
    void play_note_on (int a_note);
    void play_note_off (int a_note);

    /* send a note off for all active notes */
    void off_playing_notes ();

    //
    // Drawing functions
    //

    /* resets draw marker so calls to getNextnoteEvent
       will start from the first */
    void reset_draw_marker ();

    /* each call seqdata( sequence *a_seq, int a_scale );fills the passed refrences with a
       events elements, and returns true.  When it
       has no more events, returns a false */
    draw_type get_next_note_event (long *a_tick_s,
                                   long *a_tick_f,
                                   int *a_note,
                                   bool * a_selected, int *a_velocity);

    int get_lowest_note_event ();
    int get_highest_note_event ();

    bool get_next_event (unsigned char a_status,
                         unsigned char a_cc,
                         long *a_tick,
                         unsigned char *a_D0,
                         unsigned char *a_D1, bool * a_selected, int type = ALL_EVENTS);

    bool get_next_event (unsigned char *a_status, unsigned char *a_cc);

    sequence & operator= (const sequence & a_rhs);

    void seq_number_fill_list( list<char> *a_list, int a_pos );
    void seq_name_fill_list( list<char> *a_list );
    void fill_proprietary_list(list < char >*a_list);
    void meta_track_end( list<char> *a_list, long delta_time);
    void fill_list(list < char >*a_list, int a_pos, bool write_triggers = true);

    long song_fill_list_seq_event( list<char> *a_list, trigger *a_trig, long prev_timestamp, file_type_e type );
    void song_fill_list_seq_trigger( list<char> *a_list, trigger *a_trig, long a_length, long prev_timestamp );

    void select_events (unsigned char a_status, unsigned char a_cc,
                        bool a_inverse = false);
    void quanize_events (unsigned char a_status, unsigned char a_cc,
                         long a_snap_tick, int a_divide, bool a_linked =
                             false);
    void transpose_notes (int a_steps, int a_scale);
    void shift_notes (int a_ticks);  // move selected notes later/earlier in time
    void multiply_pattern( float a_multiplier );
    void reverse_pattern();
    void calulate_reverse(event &a_e);

    bool save( ofstream *file );
    bool load( ifstream *file, int version );

    void apply_song_transpose ();
};

