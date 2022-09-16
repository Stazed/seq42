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

class track;

#include "trigger.h"
#include "sequence.h"
#include "mutex.h"
#include <vector>

enum trigger_edit
{
    GROW_START = 0, //grow the start of the trigger
    GROW_END = 1, //grow the end of the trigger
    MOVE = 2 //move the entire trigger block
};


class track
{

private:

    /* holds the sequences */

    vector < sequence *> m_vector_sequence;

    /* holds the triggers */
    list < trigger > m_list_trigger;
    trigger m_trigger_clipboard;
    trigger *m_trigger_export;

    stack < list < trigger > >m_list_trigger_undo;
    stack < list < trigger > >m_list_trigger_redo;

    /* markers */
    list < trigger >::iterator m_iterator_play_trigger;
    list < trigger >::iterator m_iterator_draw_trigger;

    /* track name */
    string m_name;

    /* Is the track edit window open? */
    bool m_editing;
    bool m_raise;

    /* contains the proper midi channel */
    char m_midi_channel;
    char m_bus;

    /* song playback mode solo/mute */
    bool m_song_mute;
    bool m_song_solo;

    bool m_transposable;

    /* outputs to sequence to this Bus on midichannel */
    mastermidibus *m_masterbus;

    long m_default_velocity;

    bool m_trigger_copied;

    bool m_dirty_perf;
    bool m_dirty_names;

    seq42::mutex m_mutex;

    void lock ();
    void unlock ();

    void split_trigger( trigger &trig, long a_split_tick);

public:

    track ();
    ~track ();
    track& operator=(const track& other);
    void free ();

    bool m_is_NULL;
    void push_trigger_undo ();
    void pop_trigger_undo ();
    void pop_trigger_redo ();
    void clear_trigger_undo_redo ();

    int new_sequence( );
    void delete_sequence( int a_num );

    sequence *get_sequence( int a_seq );
    int get_sequence_index(const sequence *a_sequence );

    // How many sequences does this track have?
    unsigned int get_number_of_sequences();

    //
    //  Gets and Sets
    //
    void set_name (const string &a_name);
    void set_name (char *a_name);
    /* returns string of name */
    const char *get_name ();

    void set_song_mute (bool a_mute);
    bool get_song_mute ();
    
    void set_song_solo (bool a_solo);
    bool get_song_solo ();

    void set_transposable (bool a_xpose);
    bool get_transposable ();

    /* midi channel */
    unsigned char get_midi_channel ();
    void set_midi_channel (unsigned char a_ch);
    /* sets the midibus to dump to */
    void set_midi_bus (char a_mb);
    char get_midi_bus ();

    void set_master_midi_bus (mastermidibus * a_mmb);
    mastermidibus *get_master_midi_bus ();

    void set_default_velocity(long a_vel);
    long get_default_velocity();

    void set_dirty();
    
    void set_trigger_export( trigger *a_trig);
    trigger *get_trigger_export();
    /* signals that a redraw is needed from recording */
    /* resets flag on call */
    bool is_dirty_perf();
    bool is_dirty_names();

    /* dumps contents to stdout */
    void print ();

    //
    // Selection and Manipulation
    //
    void add_trigger (long a_tick, long a_length, long a_offset = 0, int a_seq = -1);
    void split_trigger( long a_tick );
    void grow_trigger (long a_tick_from, long a_tick_to, long a_length);
    void del_trigger (long a_tick );
    trigger *get_trigger(long a_tick);

    void get_trak_triggers(std::vector<trigger> &trig_vect); //  merge

    bool get_trigger_state (long a_tick);
    sequence *get_trigger_sequence (trigger *a_trigger);
    //void set_trigger_sequence (trigger *a_trigger, sequence *a_sequence);
    void set_trigger_sequence (trigger *a_trigger, int a_sequence);
    void set_trigger_copied ();
    void unset_trigger_copied ();
    bool get_trigger_copied ();
    trigger *get_trigger_clipboard();
    bool select_trigger(long a_tick);
    bool unselect_triggers ();
    bool intersectTriggers( long position, long& start, long& end );
    void del_selected_trigger();
    void cut_selected_trigger();
    void copy_selected_trigger();
    void paste_trigger(long a_tick = -1);
    void move_selected_triggers_to(long a_tick, bool a_adjust_offset, trigger_edit editMode = MOVE);
    long adjust_offset( long a_offset, long a_length );
    void adjust_trigger_offsets_to_length( sequence *a_seq, long a_new_len );
    long get_selected_trigger_start_tick();
    long get_selected_trigger_end_tick();

    long get_max_trigger ();
    int get_trigger_count_for_seqidx(int a_seq);
    int get_track_trigger_count();

    void move_triggers (long a_start_tick, long a_distance, bool a_direction);
    void paste_triggers (long a_start_tick, long a_distance, long a_offset = 0);
    void clear_triggers ();

    void reset_draw_trigger_marker ();

    bool get_next_trigger (long *a_tick_on, long *a_tick_off, bool * a_selected, long *a_tick_offset, int *a_seq_idx);

    /* Return true if at least one of this track's sequences is being edited. */
    bool get_sequence_editing();

    void set_editing( bool a_edit )
    {
        m_editing = a_edit;
    };
    bool get_editing()
    {
        return m_editing;
    };

    void set_raise( bool a_raise )
    {
        m_raise = a_raise;
    };
    bool get_raise()
    {
        return m_raise;
    };

    void reset_sequences(bool a_playback_mode);
    void set_playing_off();

    /* send a note off for all active notes */
    void off_playing_notes ();

    void play( long a_tick, bool a_playback_mode );
    void set_orig_tick (long a_tick);

    bool save( ofstream *file );
    bool load( ifstream *file, int version );

    void delete_unused_sequences();
    void create_triggers(long left_tick, long right_tick);
    void apply_song_transpose ();
};
