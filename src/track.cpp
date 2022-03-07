//----------------------------------------------------------------------------
////
////  This file is part of seq42.
////
////  seq42 is free software; you can redistribute it and/or modify
////  it under the terms of the GNU General Public License as published by
////  the Free Software Foundation; either version 2 of the License, or
////  (at your option) any later version.
////
////  seq42 is distributed in the hope that it will be useful,
////  but WITHOUT ANY WARRANTY; without even the implied warranty of
////  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
////  GNU General Public License for more details.
////
////  You should have received a copy of the GNU General Public License
////  along with seq42; if not, write to the Free Software
////  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
////
////-----------------------------------------------------------------------------
#include "track.h"
#include <stdlib.h>
#include <fstream>
#include <gtkmm.h>

track::track()
{
    m_editing = false;
    m_raise = false;
    m_name = c_dummy;
    m_bus = 0;
    m_midi_channel = 0;
    m_song_mute = false;
    m_song_solo = false;
    m_transposable = true;
    m_trigger_copied = false;
    m_trigger_export = nullptr;

    m_dirty_perf = true;
    m_dirty_names = true;

    m_default_velocity = 100;
    m_is_NULL = false;
}

track::~track()
{
    //printf("in ~track()\n");
    free();
}

void
track::free()
{
    //printf("in free()\n");
    for(unsigned i=0; i<m_vector_sequence.size(); i++)
    {
        delete m_vector_sequence[i];
        m_vector_sequence[i] = NULL;
    }
}

track&
track::operator=(const track& other)
{
    //printf("in track::operator=()\n");
    lock();
    if(this != &other)
    {
        free();

        m_name = other.m_name;
        m_bus = other.m_bus;
        m_midi_channel = other.m_midi_channel;
        m_song_mute = other.m_song_mute;  // for undo/redo and load from file - set to false for user copy/paste
        m_transposable = other.m_transposable;
        m_masterbus = other.m_masterbus;

        m_dirty_perf = false;
        m_dirty_names = false;
        m_is_NULL = other.m_is_NULL;

        m_list_trigger = other.m_list_trigger;
        m_list_trigger_undo = other.m_list_trigger_undo;
        m_list_trigger_redo = other.m_list_trigger_redo;

#if 0
        m_vector_sequence = other.m_vector_sequence;
        for(int i=0; i<m_vector_sequence.size(); i++)
        {
            m_vector_sequence[i]->set_track(this);
        }
#endif
        //printf("copying sequences...\n");
        // Copy the other track's sequences.
        m_vector_sequence.clear();
        for(unsigned i=0; i<other.m_vector_sequence.size(); i++)
        {
            //printf("other.sequence[%d] at %p\n", i, other.m_vector_sequence[i]);
            sequence *a_seq = new sequence();
            *a_seq = *(other.m_vector_sequence[i]);
            a_seq->set_track(this);
            m_vector_sequence.push_back(a_seq);
        }
    }
    unlock();
    return *this;
}

void
track::set_dirty()
{
    m_dirty_names =  m_dirty_perf = global_seqlist_need_update = true;
}

void
track::lock( )
{
    m_mutex.lock();
}

void
track::unlock( )
{
    m_mutex.unlock();
}

const char*
track::get_name()
{
    return m_name.c_str();
}

void
track::set_name( char *a_name )
{
    m_name = a_name;
    set_dirty();
}

void
track::set_name( string a_name )
{
    m_name = a_name;
    set_dirty();
}

void
track::set_midi_channel( unsigned char a_ch )
{
    lock();
    off_playing_notes( );
    m_midi_channel = a_ch;
    set_dirty();
    unlock();
}

unsigned char
track::get_midi_channel( )
{
    return m_midi_channel;
}

void
track::set_midi_bus( char  a_mb )
{
    lock();
    off_playing_notes( );
    m_bus = a_mb;
    set_dirty();
    unlock();
}

char
track::get_midi_bus(  )
{
    return m_bus;
}

unsigned int
track::get_number_of_sequences()
{
    return m_vector_sequence.size();
}

sequence *
track::get_sequence( int a_seq )
{
    if ( a_seq < 0 || a_seq >= (int)m_vector_sequence.size() ) return NULL;
    //m_vector_sequence[a_seq]->set_track(this);
    return m_vector_sequence[a_seq];
}

int
track::get_sequence_index( sequence *a_seq )
{
    for(unsigned i=0; i<m_vector_sequence.size(); i++)
    {
        if(m_vector_sequence[i] == a_seq)
        {
            return i;
        }
    }
    return -1;
}

bool
track::is_dirty_names( )
{
    lock();

    bool ret = m_dirty_names;
    m_dirty_names = false;

    unlock();

    return ret;
}

bool
track::is_dirty_perf( )
{
    lock();

    bool ret = m_dirty_perf;
    if(! ret)
    {
        for(unsigned i=0; i<m_vector_sequence.size(); i++)
        {
            if(m_vector_sequence[i]->is_dirty_perf())
            {
                ret = true;
                break;
            }
        }
    }
    m_dirty_perf = false;

    unlock();

    return ret;
}

bool
track::get_sequence_editing()
{
    // Return true if at least one of this track's sequences is being edited.
    for(unsigned i=0; i<m_vector_sequence.size(); i++)
    {
        if(m_vector_sequence[i]->get_editing())
        {
            return true;
        }
    }
    return false;
}

void
track::set_playing_off()
{
    // Set playing off for all sequences.
    for(unsigned i=0; i<m_vector_sequence.size(); i++)
    {
        m_vector_sequence[i]->set_playing(false);
    }
}

void
track::reset_sequences(bool a_playback_mode)
{
    for(unsigned i=0; i<m_vector_sequence.size(); i++)
    {
        bool state = m_vector_sequence[i]->get_playing();
        m_vector_sequence[i]->off_playing_notes();
        m_vector_sequence[i]->set_playing(false);
        m_vector_sequence[i]->zero_markers();
        m_vector_sequence[i]->set_playing(state);
    }
}

void
track::push_trigger_undo()
{
    lock();
    m_list_trigger_undo.push( m_list_trigger );

    list<trigger>::iterator i;

    for ( i  = m_list_trigger_undo.top().begin();
            i != m_list_trigger_undo.top().end(); i++ )
    {
        (*i).m_selected = false;
    }

    unlock();
}

void
track::pop_trigger_undo()
{
    lock();

    if (m_list_trigger_undo.size() > 0 )
    {
        m_list_trigger_redo.push( m_list_trigger );
        m_list_trigger = m_list_trigger_undo.top();
        m_list_trigger_undo.pop();
        set_dirty();
    }

    unlock();
}

void
track::pop_trigger_redo()
{
    lock();

    if (m_list_trigger_redo.size() > 0 )
    {
        m_list_trigger_undo.push( m_list_trigger );
        m_list_trigger = m_list_trigger_redo.top();
        m_list_trigger_redo.pop();
        set_dirty();
    }

    unlock();
}

void
track::clear_trigger_undo_redo ()
{
    if(m_list_trigger_undo.size() > 0)
        m_list_trigger_undo.pop();

    if(m_list_trigger_redo.size() > 0)
        m_list_trigger_redo.pop();
}

void
track::set_master_midi_bus( mastermidibus *a_mmb )
{
    lock();

    m_masterbus = a_mmb;

    unlock();
}

mastermidibus *
track::get_master_midi_bus()
{
    return m_masterbus;
}

void
track::set_song_mute( bool a_mute )
{
    m_song_mute = a_mute;
    m_dirty_names = true;
}

bool
track::get_song_mute()
{
    return m_song_mute;
}

void
track::set_song_solo( bool a_solo )
{
    m_song_solo = a_solo;
    m_dirty_names = true;
}

bool
track::get_song_solo()
{
    return m_song_solo;
}

void
track::set_transposable( bool a_xpose )
{
    m_transposable = a_xpose;
}

bool
track::get_transposable()
{
    return m_transposable;
}

int
track::new_sequence( )
{
    sequence *a_seq = new sequence();
    a_seq->set_track(this);
    m_vector_sequence.push_back(a_seq);
    return  m_vector_sequence.size()-1;
}

void track::delete_sequence( int a_num )
{
    sequence *a_seq = m_vector_sequence[a_num];
    if( ! a_seq->get_editing() )
    {
        a_seq->set_playing( false );
        list<trigger>::iterator i = m_list_trigger.begin();
        while ( i != m_list_trigger.end())
        {
            if(i->m_sequence == a_num)
            {
                i->m_sequence = -1;
            }
            else if(i->m_sequence > a_num)
            {
                i->m_sequence--;
            }
            i++;
        }
        delete a_seq;
        a_seq = NULL;
        m_vector_sequence.erase(m_vector_sequence.begin()+a_num);
        set_dirty();
    }
}

/* tick comes in as global tick */
void
track::play( long a_tick, bool a_playback_mode )
{
    //printf( "track::play(a_tick=%ld, a_playback=%d)\n", a_tick, a_playback_mode );
    lock();

    trigger *active_trigger = NULL;
    sequence *trigger_seq = NULL;

    if(a_playback_mode)
    {
        // Song mode
        list<trigger>::iterator i = m_list_trigger.begin();
        while ( i != m_list_trigger.end())
        {
            if ( i->m_tick_start <= a_tick && i->m_tick_end > a_tick)
            {
                if(i->m_sequence > -1)
                {
                    //trigger_seq = i->m_sequence;
                    trigger_seq = get_sequence( i->m_sequence );
                    active_trigger = &(*i);
                    break;
                }
            }

            if ( i->m_tick_start > a_tick )
            {
                break;
            }
            i++;
        }
    }

    for(unsigned i=0; i<m_vector_sequence.size(); i++)
    {
        if(a_playback_mode)
        {
            if(m_vector_sequence[i] == trigger_seq)
            {
                if(global_solo_track_set)
                {
                    m_vector_sequence[i]->set_playing( m_song_solo );
                    m_vector_sequence[i]->play(a_tick, active_trigger);
                }
                else
                {
                    m_vector_sequence[i]->set_playing(! m_song_mute);
                    m_vector_sequence[i]->play(a_tick, active_trigger);
                }
            }
            else
            {
                m_vector_sequence[i]->set_playing(false);
                m_vector_sequence[i]->play(a_tick, NULL);
            }
        }
        else
        {
            m_vector_sequence[i]->play(a_tick, NULL);
        }
    }

    unlock();
}

int
track::get_trigger_count_for_seqidx(int a_seq)
{
    int count = 0;
    list<trigger>::iterator i = m_list_trigger.begin();
    while ( i != m_list_trigger.end())
    {
        if(i->m_sequence == a_seq)
        {
            count++;
        }
        i++;
    }
    return count;
}

int
track::get_track_trigger_count()
{
    return m_list_trigger.size();
}

void
track::clear_triggers()
{
    lock();
    m_list_trigger.clear();
    unlock();
}

void
track::add_trigger( long a_tick, long a_length, long a_offset, int a_seq )
{
    lock();

    trigger e;

    e.m_offset = a_offset;
    e.m_sequence = a_seq;

    e.m_selected = false;

    e.m_tick_start  = a_tick;
    e.m_tick_end    = a_tick + a_length - 1;

    list<trigger>::iterator i = m_list_trigger.begin();

    while ( i != m_list_trigger.end() )
    {
        // Is it inside the new one? erase
        if ((*i).m_tick_start >= e.m_tick_start &&
                (*i).m_tick_end   <= e.m_tick_end  )
        {
            //printf ( "erase start[%d] end[%d]\n", (*i).m_tick_start, (*i).m_tick_end );
            m_list_trigger.erase(i);
            i = m_list_trigger.begin();
            continue;
        }
        // Is the e's end inside  ?
        else if ( (*i).m_tick_end   >= e.m_tick_end &&
                  (*i).m_tick_start <= e.m_tick_end )
        {
            (*i).m_tick_start = e.m_tick_end + 1;
            //printf ( "mvstart start[%d] end[%d]\n", (*i).m_tick_start, (*i).m_tick_end );
        }
        // Is the last start inside the new end ?
        else if ((*i).m_tick_end   >= e.m_tick_start &&
                 (*i).m_tick_start <= e.m_tick_start )
        {
            (*i).m_tick_end = e.m_tick_start - 1;
            //printf ( "mvend start[%d] end[%d]\n", (*i).m_tick_start, (*i).m_tick_end );
        }

        ++i;
    }

    m_list_trigger.push_front( e );
    m_list_trigger.sort();

    unlock();
}

bool track::intersectTriggers( long position, long& start, long& end )
{
    lock();

    list<trigger>::iterator i = m_list_trigger.begin();
    while ( i != m_list_trigger.end() )
    {
        if ((*i).m_tick_start <= position && position <= (*i).m_tick_end)
        {
            start = (*i).m_tick_start;
            end = (*i).m_tick_end;
            unlock();
            return true;
        }
        ++i;
    }

    unlock();
    return false;
}

void
track::grow_trigger (long a_tick_from, long a_tick_to, long a_length)
{
    lock();

    list<trigger>::iterator i = m_list_trigger.begin();

    while ( i != m_list_trigger.end() )
    {
        // Find our pair
        if ((*i).m_tick_start <= a_tick_from &&
                (*i).m_tick_end   >= a_tick_from  )
        {
            long start = (*i).m_tick_start;
            long end   = (*i).m_tick_end;

            if ( a_tick_to < start )
            {
                start = a_tick_to;
            }

            if ( (a_tick_to + a_length - 1) > end )
            {
                end = (a_tick_to + a_length - 1);
            }

            add_trigger( start, end - start + 1, (*i).m_offset );
            break;
        }
        ++i;
    }

    unlock();
}


void
track::del_trigger( long a_tick )
{
    lock();

    list<trigger>::iterator i = m_list_trigger.begin();

    while ( i != m_list_trigger.end() )
    {
        if ((*i).m_tick_start <= a_tick &&
                (*i).m_tick_end   >= a_tick )
        {
            m_list_trigger.erase(i);
            break;
        }
        ++i;
    }

    unlock();
}

void
track::split_trigger( trigger &trig, long a_split_tick)
{
    lock();

    long new_tick_end   = trig.m_tick_end;
    long new_tick_start = a_split_tick;

    trig.m_tick_end = a_split_tick - 1;

    long length = new_tick_end - new_tick_start;
    if ( length > 1 )
        add_trigger( new_tick_start, length + 1, trig.m_offset, trig.m_sequence );

    unlock();
}

void
track::adjust_trigger_offsets_to_length( sequence *a_seq, long a_new_len )
{
    lock();

    int seq_idx = get_sequence_index(a_seq);
    long seq_length = a_seq->get_length();

    // for all triggers, and undo triggers
    list<trigger>::iterator i = m_list_trigger.begin();

    while ( i != m_list_trigger.end() )
    {
        if(i->m_sequence == seq_idx)
        {
            i->m_offset = adjust_offset( i->m_offset, seq_length );
            i->m_offset = seq_length - i->m_offset; // flip
            long inverse_offset = seq_length - (i->m_tick_start % seq_length);

            long local_offset = (inverse_offset - i->m_offset);
            local_offset %= seq_length;

            long inverse_offset_new = a_new_len - (i->m_tick_start % a_new_len);

            long new_offset = inverse_offset_new - local_offset;

            i->m_offset = (new_offset % a_new_len);
            i->m_offset = a_new_len - i->m_offset;
        }

        ++i;
    }

    unlock();
}

void
track::copy_triggers( long a_start_tick,
                      long a_distance  )
{
    long from_start_tick = a_start_tick + a_distance;
    long from_end_tick = from_start_tick + a_distance - 1;

    lock();

    move_triggers( a_start_tick,
                   a_distance,
                   true );

    sequence *a_seq;
    long seq_length = 0L;

    list<trigger>::iterator i = m_list_trigger.begin();
    while(  i != m_list_trigger.end() )
    {
        a_seq = get_trigger_sequence(&(*i));
        if(a_seq)
        {
            seq_length = a_seq->get_length();
        }
        else
        {
            seq_length = 0L;
        }

        if ( (*i).m_tick_start >= from_start_tick &&
                (*i).m_tick_start <= from_end_tick )
        {
            trigger e;
            e.m_sequence = (*i).m_sequence;
            e.m_offset = (*i).m_offset;
            e.m_selected = false;

            e.m_tick_start  = (*i).m_tick_start - a_distance;

            if ((*i).m_tick_end   <= from_end_tick )
            {
                e.m_tick_end  = (*i).m_tick_end - a_distance;
            }

            if ((*i).m_tick_end   > from_end_tick )
            {
                e.m_tick_end = from_start_tick -1;
            }

            if(seq_length)
            {
                e.m_offset += (seq_length - (a_distance % seq_length));
                e.m_offset %= seq_length;

                if ( e.m_offset < 0 )
                    e.m_offset += seq_length;
            }
            else
            {
                e.m_offset = 0L;
            }

            m_list_trigger.push_front( e );
        }

        ++i;
    }

    m_list_trigger.sort();

    unlock();
}

void
track::paste_triggers (long a_start_tick,
                      long a_distance, long a_offset)
{
    long end_tick = a_start_tick + a_distance - 1;

    lock();

  //  printf("offset = %ld: distance = %ld \n", a_offset, a_distance);
    
    /* if we are pasting before the 'L' 'R' markers */
    if( a_offset < 0 )
    {
        a_start_tick += a_distance;
        end_tick += a_distance;
        
        a_distance = 0;
    }
    
  //  printf("PASTE from_start = %ld: from_end = %ld\n", a_start_tick, end_tick );

    sequence *a_seq;
    long seq_length = 0L;

    list<trigger>::iterator i = m_list_trigger.begin();
    while(  i != m_list_trigger.end() )
    {
        a_seq = get_trigger_sequence(&(*i));
        if(a_seq)
        {
            seq_length = a_seq->get_length();
        }
        else
        {
            seq_length = 0L;
        }

        if ( (*i).m_tick_start >= a_start_tick &&
                (*i).m_tick_start <= end_tick )
        {
       //     printf("PASTE m_tick_start = %ld: from_start = %ld: from_end = %ld\n",(*i).m_tick_start, a_start_tick, end_tick );
            
            trigger e;
            e.m_sequence = (*i).m_sequence;
            e.m_offset = (*i).m_offset;
            e.m_selected = false;

            e.m_tick_start  = (*i).m_tick_start + a_offset + a_distance;

            if ((*i).m_tick_end <= end_tick )
            {
                e.m_tick_end  = (*i).m_tick_end + a_offset + a_distance;
            }

            if ((*i).m_tick_end > end_tick )
            {
                e.m_tick_end = end_tick + a_offset + a_distance;
            }

            if(seq_length)
            {
                e.m_offset += (seq_length - (a_distance % seq_length));
                e.m_offset %= seq_length;

                if ( e.m_offset < 0 )
                    e.m_offset += seq_length;
            }
            else
            {
                e.m_offset = 0L;
            }

            m_list_trigger.push_front( e );
        }

        ++i;
    }

    m_list_trigger.sort();

    unlock();
}

void
track::split_trigger( long a_tick )
{
    lock();

    list<trigger>::iterator i = m_list_trigger.begin();
    while(  i != m_list_trigger.end() )
    {
        // trigger greater than L and R
        if ( (*i).m_tick_start <= a_tick &&
                (*i).m_tick_end >= a_tick )
        {
            //printf( "split trigger %ld %ld\n", (*i).m_tick_start, (*i).m_tick_end );
            {
                split_trigger(*i, a_tick);
                break;
            }
        }
        ++i;
    }
    unlock();
}

void
track::move_triggers( long a_start_tick,
                      long a_distance,
                      bool a_direction )
{
    long a_end_tick = a_start_tick + a_distance;
    //printf( "move_triggers() a_start_tick[%d] a_distance[%d] a_direction[%d]\n",
    //        a_start_tick, a_distance, a_direction );

    lock();

    list<trigger>::iterator i = m_list_trigger.begin();
    while(  i != m_list_trigger.end() )
    {
        // trigger greater than L and R
        if ( (*i).m_tick_start < a_start_tick &&
                (*i).m_tick_end > a_start_tick )
        {
            if ( a_direction ) // forward
            {
                split_trigger(*i,a_start_tick);
            }
            else // back
            {
                split_trigger(*i,a_end_tick);
            }
        }

        // triggers on L
        if ( (*i).m_tick_start < a_start_tick &&
                (*i).m_tick_end > a_start_tick )
        {
            if ( a_direction ) // forward
            {
                split_trigger(*i,a_start_tick);
            }
            else // back
            {
                (*i).m_tick_end = a_start_tick - 1;
            }
        }

        // In betweens
        if ( (*i).m_tick_start >= a_start_tick &&
                (*i).m_tick_end <= a_end_tick &&
                !a_direction )
        {
            m_list_trigger.erase(i);
            i = m_list_trigger.begin();
        }

        // triggers on R
        if ( (*i).m_tick_start < a_end_tick &&
                (*i).m_tick_end > a_end_tick )
        {
            if ( !a_direction ) // forward
            {
                (*i).m_tick_start = a_end_tick;
            }
        }

        ++i;
    }

    sequence *a_seq;
    long seq_length = 0L;

    i = m_list_trigger.begin();
    while(  i != m_list_trigger.end() )
    {
        a_seq = get_trigger_sequence(&(*i));

        if(a_seq)
        {
            seq_length = a_seq->get_length();
        }
        else
        {
            seq_length = 0L;
        }

        if ( a_direction ) // forward
        {
            if ( (*i).m_tick_start >= a_start_tick )
            {
                (*i).m_tick_start += a_distance;
                (*i).m_tick_end   += a_distance;

                if(seq_length)
                {
                    (*i).m_offset += a_distance;
                    (*i).m_offset %= seq_length;
                }
            }
        }
        else // back
        {
            if ( (*i).m_tick_start >= a_end_tick )
            {
                (*i).m_tick_start -= a_distance;
                (*i).m_tick_end   -= a_distance;

                if(seq_length)
                {
                    (*i).m_offset += (seq_length - (a_distance % seq_length));
                    (*i).m_offset %= seq_length;
                }
            }
        }

        if(seq_length)
        {
            (*i).m_offset = adjust_offset( (*i).m_offset, seq_length );
        }
        else
        {
            (*i).m_offset = 0L;
        }

        ++i;
    }

    unlock();
}

long
track::get_selected_trigger_start_tick()
{
    long ret = -1;
    lock();

    list<trigger>::iterator i = m_list_trigger.begin();

    while(  i != m_list_trigger.end() )
    {
        if ( i->m_selected )
        {
            ret = i->m_tick_start;
        }

        ++i;
    }

    unlock();

    return ret;
}

long
track::get_selected_trigger_end_tick()
{
    long ret = -1;
    lock();

    list<trigger>::iterator i = m_list_trigger.begin();

    while(  i != m_list_trigger.end() )
    {
        if ( i->m_selected )
        {
            ret = i->m_tick_end;
        }

        ++i;
    }

    unlock();

    return ret;
}

void
track::move_selected_triggers_to( long a_tick, bool a_adjust_offset, trigger_edit editMode )
{
    lock();

    long min_tick = 0;
    long max_tick = 0x7ffffff;

    list<trigger>::iterator i = m_list_trigger.begin();
    list<trigger>::iterator s = m_list_trigger.begin();

    // min_tick][0                1][max_tick
    //                   2

    while(  i != m_list_trigger.end() )
    {
        if ( i->m_selected )
        {
            s = i;

            if (     i != m_list_trigger.end() &&
                     ++i != m_list_trigger.end())
            {
                max_tick = (*i).m_tick_start - 1;
            }

            // if we are moving the 0, use first as offset
            // if we are moving the 1, use the last as the offset
            // if we are moving both (2), use first as offset

            long a_delta_tick = 0;

            if ( editMode == GROW_END )
            {
                a_delta_tick = a_tick - s->m_tick_end;

                if (  a_delta_tick > 0 &&
                        (a_delta_tick + s->m_tick_end) > max_tick )
                {
                    a_delta_tick = ((max_tick) - s->m_tick_end);
                }

                // not past first
                if (  a_delta_tick < 0 &&
                        (a_delta_tick + s->m_tick_end <= (s->m_tick_start + c_ppqn / 8 )))
                {
                    a_delta_tick = ((s->m_tick_start + c_ppqn / 8 ) - s->m_tick_end);
                }
            }

            if ( editMode == GROW_START )
            {
                a_delta_tick = a_tick - s->m_tick_start;

                if (  a_delta_tick < 0 &&
                        (a_delta_tick + s->m_tick_start) < min_tick )
                {
                    a_delta_tick = ((min_tick) - s->m_tick_start);
                }

                // not past last
                if (  a_delta_tick > 0 &&
                        (a_delta_tick + s->m_tick_start >= (s->m_tick_end - c_ppqn / 8 )))
                {
                    a_delta_tick = ((s->m_tick_end - c_ppqn / 8 ) - s->m_tick_start);
                }
            }

            if ( editMode == MOVE )
            {
                a_delta_tick = a_tick - s->m_tick_start;

                if (  a_delta_tick < 0 &&
                        (a_delta_tick + s->m_tick_start) < min_tick )
                {
                    a_delta_tick = ((min_tick) - s->m_tick_start);
                }

                if ( a_delta_tick > 0 &&
                        (a_delta_tick + s->m_tick_end) > max_tick )
                {
                    a_delta_tick = ((max_tick) - s->m_tick_end);
                }
            }

            if ( editMode == GROW_START || editMode == MOVE )
                s->m_tick_start += a_delta_tick;

            if ( editMode == GROW_END || editMode == MOVE )
                s->m_tick_end   += a_delta_tick;

            if ( a_adjust_offset )
            {
                int seq_idx = s->m_sequence;
                if(seq_idx != -1)
                {
                    long a_length = get_sequence(seq_idx)->get_length();
                    s->m_offset += a_delta_tick;
                    s->m_offset = adjust_offset( s->m_offset, a_length );
                }
            }

            break;
        }
        else
        {
            min_tick = (*i).m_tick_end + 1;
        }

        ++i;
    }

    unlock();
}


long
track::get_max_trigger()
{
    lock();

    long ret;

    if ( m_list_trigger.size() > 0 )
        ret = m_list_trigger.back().m_tick_end;
    else
        ret = 0;

    unlock();

    return ret;
}

long
track::adjust_offset( long a_offset, long a_length )
{
    a_offset %= a_length;

    if ( a_offset < 0 )
        a_offset += a_length;

    return a_offset;
}

trigger *
track::get_trigger( long a_tick )
{
    lock();

    trigger *ret = NULL;
    list<trigger>::iterator i;

    for ( i = m_list_trigger.begin(); i != m_list_trigger.end(); i++ )
    {
        if ( (*i).m_tick_start <= a_tick &&
                (*i).m_tick_end >= a_tick)
        {
            ret = &(*i);
            break;
        }
    }

    unlock();

    return ret;
}

void
track::get_trak_triggers(std::vector<trigger> &trig_vect)
{
    lock();

    trig_vect.assign(m_list_trigger.begin(),m_list_trigger.end());

    unlock();
}

bool
track::get_trigger_state( long a_tick )
{
    trigger *a_trigger = get_trigger(a_tick);
    return a_trigger != NULL;
}

sequence *
track::get_trigger_sequence( trigger *a_trigger )
{
    if(a_trigger == NULL)
    {
        return NULL;
    }
    else
    {
        return get_sequence(a_trigger->m_sequence);
    }
}

void track::set_trigger_sequence( trigger *a_trigger, int a_sequence )
{
    // to make all newly added sequences align seq start with trigger start
    a_trigger->m_offset = a_trigger->m_tick_start;

    if(a_trigger != NULL)
    {
        a_trigger->m_sequence = a_sequence;
        set_dirty();
    }
}

void track::set_trigger_copied ()
{
    m_trigger_copied = true;
}

void track::unset_trigger_copied ()
{
    m_trigger_copied = false;
}

bool track::get_trigger_copied ()
{
    return m_trigger_copied;
}

trigger*
track::get_trigger_clipboard()
{
    return &m_trigger_clipboard;
}

bool
track::select_trigger( long a_tick )
{
    lock();

    bool ret = false;
    list<trigger>::iterator i;

    for ( i = m_list_trigger.begin(); i != m_list_trigger.end(); i++ )
    {
        if ( (*i).m_tick_start <= a_tick &&
                (*i).m_tick_end   >= a_tick)
        {
            (*i).m_selected = true;
            ret = true;
        }
    }

    unlock();

    return ret;
}

bool
track::unselect_triggers()
{
    lock();

    bool ret = false;
    list<trigger>::iterator i;

    for ( i = m_list_trigger.begin(); i != m_list_trigger.end(); i++ )
    {
        (*i).m_selected = false;
    }

    unlock();

    return ret;
}

void
track::del_selected_trigger()
{
    lock();

    list<trigger>::iterator i;

    for ( i = m_list_trigger.begin(); i != m_list_trigger.end(); i++ )
    {
        if ( i->m_selected )
        {
            m_list_trigger.erase(i);
            break;
        }
    }

    unlock();
}

void
track::cut_selected_trigger()
{
    copy_selected_trigger();
    del_selected_trigger();
}

void
track::copy_selected_trigger()
{
    lock();

    list<trigger>::iterator i;

    for ( i = m_list_trigger.begin(); i != m_list_trigger.end(); i++ )
    {
        if ( i->m_selected )
        {
            m_trigger_clipboard = *i;
            m_trigger_copied = true;
            break;
        }
    }

    unlock();
}

void
track::paste_trigger(long a_tick)
{
    // empty trigger = segfault via get_length - don't allow w/o sequence
    if (m_trigger_clipboard.m_sequence < 0)
        return;

    if ( m_trigger_copied )
    {
        long length =  m_trigger_clipboard.m_tick_end -
                       m_trigger_clipboard.m_tick_start + 1;
        // paste at copy end or a_tick
        if(a_tick < 0)    // < 0 means no paste tick set so use default
        {
            add_trigger( m_trigger_clipboard.m_tick_end + 1,
                         length,
                         m_trigger_clipboard.m_offset + length,
                         m_trigger_clipboard.m_sequence );

            m_trigger_clipboard.m_tick_start = m_trigger_clipboard.m_tick_end +1;
            m_trigger_clipboard.m_tick_end = m_trigger_clipboard.m_tick_start + length - 1;

            m_trigger_clipboard.m_offset += length;

            long a_length = get_sequence(m_trigger_clipboard.m_sequence)->get_length();

            m_trigger_clipboard.m_offset = adjust_offset(m_trigger_clipboard.m_offset,a_length);
        }
        else
        {
            long offset_adjust = a_tick - m_trigger_clipboard.m_tick_start;
            add_trigger(a_tick, length,
                        m_trigger_clipboard.m_offset + offset_adjust,
                        m_trigger_clipboard.m_sequence); // +/- distance to paste tick from start

            m_trigger_clipboard.m_tick_start = a_tick;
            m_trigger_clipboard.m_tick_end = m_trigger_clipboard.m_tick_start + length - 1;
            m_trigger_clipboard.m_offset += offset_adjust;

            long a_length = get_sequence(m_trigger_clipboard.m_sequence)->get_length();
            m_trigger_clipboard.m_offset = adjust_offset(m_trigger_clipboard.m_offset,a_length);
        }
    }
}

void
track::reset_draw_trigger_marker()
{
    lock();

    m_iterator_draw_trigger = m_list_trigger.begin();

    unlock();
}

bool
track::get_next_trigger( long *a_tick_on, long *a_tick_off, bool *a_selected, long *a_offset, int *a_seq_idx )
{
    while (  m_iterator_draw_trigger  != m_list_trigger.end() )
    {
        *a_tick_on  = (*m_iterator_draw_trigger).m_tick_start;
        *a_selected = (*m_iterator_draw_trigger).m_selected;
        *a_offset =   (*m_iterator_draw_trigger).m_offset;
        *a_tick_off = (*m_iterator_draw_trigger).m_tick_end;
        *a_seq_idx =  (*m_iterator_draw_trigger).m_sequence;
        m_iterator_draw_trigger++;
        return true;
    }
    return false;
}

void
track::print()
{
    printf("name=[%s]\n", m_name.c_str()  );
    printf("midi bus=[%d]\n", m_bus );
    printf("midi channel=[%d]\n", m_midi_channel  );
    printf("mute=[%d]\n", m_song_mute  );
    printf("transposable=[%d]\n", m_transposable  );

    for(unsigned i=0; i<m_vector_sequence.size(); i++)
    {
        printf("sequence[%d] address=%p name=\"%s\"\n", i, m_vector_sequence[i], m_vector_sequence[i]->get_name() );
        m_vector_sequence[i]->print();
    }
    int i=0;
    for( list<trigger>::iterator iter = m_list_trigger.begin();
            iter != m_list_trigger.end(); iter++ )
    {
        /*long d= c_ppqn / 8;*/
        //printf ("trigger[%d] address=%p tick_start[%ld] tick_end[%ld] off[%ld] sequence=[%p]\n", i, &(*iter), (*iter).m_tick_start, (*iter).m_tick_end, (*iter).m_offset, (*iter).m_sequence );
        printf ("trigger[%d] address=%p tick_start[%ld] tick_end[%ld] off[%ld] sequence=[%d]\n", i, &(*iter), (*iter).m_tick_start, (*iter).m_tick_end, (*iter).m_offset, (*iter).m_sequence );
        i++;
    }
}

void
track::set_orig_tick( long a_tick )
{
    for(unsigned i=0; i<m_vector_sequence.size(); i++)
    {
        m_vector_sequence[i]->set_orig_tick(a_tick);
    }
}

void
track::off_playing_notes()
{
    for(unsigned i=0; i<m_vector_sequence.size(); i++)
    {
        m_vector_sequence[i]->off_playing_notes();
    }
}

bool
track::save(ofstream *file)
{
    char name[c_max_track_name];

_Pragma("GCC diagnostic push")
_Pragma("GCC diagnostic ignored \"-Wstringop-truncation\"")
    (strncpy(name, m_name.c_str(), c_max_track_name));
_Pragma("GCC diagnostic pop")

    file->write(name, sizeof(char)*c_max_track_name);

    file->write((const char *) &m_bus, sizeof(char));

    file->write((const char *) &m_midi_channel, sizeof(char));

    file->write((const char *) &m_transposable, sizeof(bool));

    file->write((const char *) &m_song_mute, sizeof(bool));

    unsigned int num_seqs = get_number_of_sequences();
    file->write((const char *) &num_seqs, global_file_int_size);

    for(unsigned int i=0; i<m_vector_sequence.size(); i++)
    {
        if(!  m_vector_sequence[i]->save(file))
        {
            return false;
        }
    }

    unsigned int num_triggers = m_list_trigger.size();
    file->write((const char *) &num_triggers, global_file_int_size);

    for( list<trigger>::iterator iter = m_list_trigger.begin();
            iter != m_list_trigger.end(); iter++ )
    {
        file->write((const char *) &(iter->m_tick_start), global_file_long_int_size);
        file->write((const char *) &(iter->m_tick_end), global_file_long_int_size);
        file->write((const char *) &(iter->m_offset), global_file_long_int_size);
        file->write((const char *) &(iter->m_sequence), global_file_int_size);
    }
    return true;
}

bool
track::load(ifstream *file, int version)
{
    char name[c_max_track_name+1];
    file->read(name, sizeof(char)*c_max_track_name);
    name[c_max_track_name] = '\0';
    set_name(name);

    file->read((char *) &m_bus, sizeof(char));

    file->read((char *) &m_midi_channel, sizeof(char));

    // The m_transposable didn't exist for version==0 files.
    if(version > 0)
    {
        file->read((char *) &m_transposable, sizeof(bool));
    }

    if(version > 2)
    {
        file->read((char *) &m_song_mute, sizeof(bool));
    }

    unsigned int num_seqs;
    file->read((char *) &num_seqs, global_file_int_size);

    for (unsigned int i=0; i< num_seqs; i++ )
    {
        new_sequence();
        if(! get_sequence(i)->load(file, version))
        {
            return false;
        }
    }

    unsigned int num_triggers;
    file->read((char *) &num_triggers, global_file_int_size);

    for (unsigned int i=0; i< num_triggers; i++ )
    {
        trigger e;
        file->read((char *) &(e.m_tick_start), global_file_long_int_size);
        file->read((char *) &(e.m_tick_end), global_file_long_int_size);
        file->read((char *) &(e.m_offset), global_file_long_int_size);
        file->read((char *) &(e.m_sequence), global_file_int_size);
        m_list_trigger.push_back(e);
    }

    for (unsigned int i=0; i< num_seqs; i++ )
    {
        get_sequence(i)->verify_and_link();
    }

    return true;
}

void
track::set_default_velocity( long a_vel )
{
    lock();

    if((int)a_vel != 0)
        m_default_velocity = a_vel;
    else
        m_default_velocity = c_note_on_velocity_default; //default = 100

    unlock();
}

long
track::get_default_velocity( )
{
    return m_default_velocity;
}

void
track::set_trigger_export( trigger *a_trig)
{
    m_trigger_export = a_trig;
}
trigger *
track::get_trigger_export()
{
    return m_trigger_export;
}

void
track::delete_unused_sequences()
{
    for(unsigned int i=0; i<m_vector_sequence.size(); i++)
    {
        if(get_trigger_count_for_seqidx(i) == 0)
        {
            Glib::ustring message = "From track: ";
            message += get_name();
            message += "\nSequence:  ";
            message += get_sequence(i)->get_name();
            message += "\n\nWill be deleted!\nAre you sure?";

            Gtk::MessageDialog warning(message,
                                   false,
                                   Gtk::MESSAGE_WARNING, Gtk::BUTTONS_YES_NO, true);

            auto result = warning.run();

            if (result == Gtk::RESPONSE_NO || result == Gtk::RESPONSE_DELETE_EVENT)
            {
                continue; // next sequence
            }

            delete_sequence(i);
        }
    }
}

void
track::create_triggers(long left_tick, long right_tick)
{
    for(unsigned int i=0; i<m_vector_sequence.size(); i++)
    {
        if(m_vector_sequence[i]->get_playing())
        {
            add_trigger (left_tick, right_tick - left_tick, 0, i);
            m_vector_sequence[i]->set_dirty();
        }
    }
}

void
track::apply_song_transpose()
{
    if(m_transposable)
    {
        for(unsigned int i=0; i<m_vector_sequence.size(); i++)
        {
            m_vector_sequence[i]->apply_song_transpose();
        }
    }
}
