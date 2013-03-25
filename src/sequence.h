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


#ifndef SEQ42_SEQUENCE
#define SEQ42_SEQUENCE

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

using std::list;

class sequence
{

  private:

    track *m_track;

    /* holds the events */
    list < event > m_list_event;
    static list < event > m_list_clipboard;

    stack < list < event > >m_list_undo;
    stack < list < event > >m_list_redo;

    /* markers */
    list < event >::iterator m_iterator_play;
    list < event >::iterator m_iterator_draw;

    /* map for noteon, used when muting, to shut off current 
       messages */
    int m_playing_notes[c_midi_notes];

    /* states */
    bool m_playing;
    bool m_recording;
    bool m_quanized_rec;
    bool m_thru;

    /* flag indicates that contents has changed from
       a recording */
    bool m_dirty_edit;
    bool m_dirty_perf;
    bool m_dirty_seqlist;

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

    /* these are just for the editor to mark things
       in correct time */
    //long m_length_measures;
    long m_time_beats_per_measure;
    long m_time_beat_width;

    /* locking */
    mutex m_mutex;
    void lock ();
    void unlock ();

    /* used to idenfity which events are ours in the out queue */
    //unsigned char m_tag;

    /* takes an event this sequence is holding and
       places it on our midibus */
    void put_event_on_bus (event * a_e);

    /* resets the location counters */
    void reset_loop (void);

    void remove_all (void);

    /* sets m_trigger_offset and wraps it to length */
    void set_trigger_offset (long a_trigger_offset);
    long get_trigger_offset (void);

    void remove( list<event>::iterator i );
    void remove( event* e );

  public:

      sequence ();
     ~sequence ();


    void push_undo (void);
    void pop_undo (void);
    void pop_redo (void);

    //
    //  Gets and Sets
    //
    void set_track (track *a_track);
    track *get_track (void);


    void set_name (string a_name);
    void set_name (char *a_name);
    /* returns string of name */
    const char *get_name (void);

    void set_swing_mode (int a_mode) {
        m_swing_mode = a_mode;
    }
    int get_swing_mode (void) {
        return m_swing_mode;
    }

    void set_measures (long a_length_measures);
    long get_measures (void);

    void set_bpm (long a_beats_per_measure);
    long get_bpm (void);

    void set_bw (long a_beat_width);
    long get_bw (void);
    void set_rec_vol (long a_rec_vol);


    void set_editing (bool a_edit)
    {
	m_editing = a_edit;
    };
    bool get_editing (void)
    {
	return m_editing;
    };
    void set_raise (bool a_edit)
    {
	m_raise = a_edit;
    };
    bool get_raise (void)
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

    void set_recording (bool);
    bool get_recording ();
    void set_snap_tick( int a_st );
    void set_quanized_rec( bool a_qr );
    bool get_quanidez_rec( );

    void set_thru (bool);
    bool get_thru ();

    /* signals that a redraw is needed from recording */
    /* resets flag on call */
    bool is_dirty_edit ();
    bool is_dirty_perf ();
    bool is_dirty_seqlist ();
    
    void set_dirty();

    /* dumps contents to stdout */
    void print ();
    void print_triggers();

    /* dumps notes from tick and prebuffers to
       ahead.  Called by sequencer thread - performance */
    void play (long a_tick, trigger *a_trigger);
    void set_orig_tick (long a_tick);

    //
    //  Selection and Manipulation
    //

    /* adds event to internal list */
    void add_event (const event * a_e);

    bool intersectNotes( long position, long position_note, long& start, long& end, long& note );
    bool intersectEvents( long posstart, long posend, long status, long& start );

    char get_midi_bus (void);
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

    int get_num_selected_notes ();
    int get_num_selected_events (unsigned char a_status, unsigned char a_cc);

    void select_all (void);

    /* given a note length (in ticks) and a boolean indicating even or odd,
    select all notes where the note on event occurs exactly on an even (or odd)
    multiple of note length.
    Example use: select every note that starts on an even eigth note beat.
    */
    int select_even_or_odd_notes(int note_len, bool even);

    void copy_selected (void);
    void paste_selected (long a_tick, int a_note);

    /* returns the 'box' of selected items */
    void get_selected_box (long *a_tick_s, int *a_note_h,
			   long *a_tick_f, int *a_note_l);

    /* returns the 'box' of selected items */
    void get_clipboard_box (long *a_tick_s, int *a_note_h,
			    long *a_tick_f, int *a_note_l);

    /* removes and adds readds selected in position */
    void move_selected_notes (long a_delta_tick, int a_delta_note);

    /* adds a single note on / note off pair */
    void add_note (long a_tick, long a_length, int a_note, bool a_paint = false);

    void add_event (long a_tick,
		    unsigned char a_status,
		    unsigned char a_d0, unsigned char a_d1, bool a_paint = false);

    void stream_event (event * a_ev);

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

    /* moves note off event */
    void grow_selected (long a_delta_tick);
    void stretch_selected(long a_delta_tick);
    
    /* deletes events */
    void remove_marked();
    void mark_selected();
    void unpaint_all();
    
    /* unselects every event */
    void unselect ();

    /* verfies state, all noteons have an off,
       links noteoffs with their ons */
    void verify_and_link ();
    void link_new ();

    /* resets everything to zero, used when
       sequencer stops */
    void zero_markers (void);

    /* flushes a note to the midibus to preview its 
       sound, used by the virtual paino */
    void play_note_on (int a_note);
    void play_note_off (int a_note);

    /* send a note off for all active notes */
    void off_playing_notes (void);

    //
    // Drawing functions
    //

    /* resets draw marker so calls to getNextnoteEvent
       will start from the first */
    void reset_draw_marker (void);

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
			 unsigned char *a_D1, bool * a_selected);

    bool get_next_event (unsigned char *a_status, unsigned char *a_cc);

    sequence & operator= (const sequence & a_rhs);

    void fill_list (list < char >*a_list, int a_pos);

    void select_events (unsigned char a_status, unsigned char a_cc,
			bool a_inverse = false);
    void quanize_events (unsigned char a_status, unsigned char a_cc,
			 long a_snap_tick, int a_divide, bool a_linked =
			 false);
    void transpose_notes (int a_steps, int a_scale);
    void shift_notes (int a_ticks);  // move selected notes later/earlier in time
    void multiply_pattern( float a_multiplier );

    bool save( ofstream *file );
    bool load( ifstream *file, int version );

    void apply_song_transpose (void);
};

#endif
