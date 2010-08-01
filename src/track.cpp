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

track::track( )
{
    m_name = c_dummy;
    m_bus = 0;
    m_midi_channel = 0;
    m_song_mute = false;

    m_dirty_perf = true;
    m_dirty_names = true;
}

track::~track() {
}

void
track::set_dirty()
{
    m_dirty_names =  m_dirty_perf = true;
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



int
track::get_number_of_sequences(void)
{
    return m_vector_sequence.size();
}

sequence *
track::get_sequence( int a_seq )
{
    if ( a_seq < 0 || a_seq >= (int)m_vector_sequence.size() ) return NULL;
    return &(m_vector_sequence[a_seq]);
}

int
track::get_sequence_index( sequence *a_seq )
{
    int i = 0;
    vector<sequence>::iterator iter = m_vector_sequence.begin();
    while ( iter != m_vector_sequence.end()){
        if(&(*iter) == a_seq) {
            return i;
        }
        iter++;
        i++;
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
    m_dirty_perf = false;

    unlock();

    return ret;
}

bool
track::get_editing( void )
{
    // Return true if at least one of this track's sequences is being edited.
    vector<sequence>::iterator iter = m_vector_sequence.begin();
    while ( iter != m_vector_sequence.end()){
        if(iter->get_editing()) {
            return true;
        }
        iter++;
    }
    return false;
}

void
track::set_playing_off( void )
{
    // Set playing off for all sequences.
    vector<sequence>::iterator iter = m_vector_sequence.begin();
    while ( iter != m_vector_sequence.end()){
        iter->set_playing(false);
        iter++;
    }
}

void
track::reset_sequences(bool a_playback_mode)
{
    vector<sequence>::iterator iter = m_vector_sequence.begin();
    while ( iter != m_vector_sequence.end()){
        bool state = iter->get_playing();
        iter->off_playing_notes();
        iter->set_playing(false);
        iter->zero_markers();
        if (!a_playback_mode) iter->set_playing(state);
        iter++;
    }
}


void
track::push_trigger_undo( void )
{
    lock();
    m_list_trigger_undo.push( m_list_trigger );

    list<trigger>::iterator i;

    for ( i  = m_list_trigger_undo.top().begin();
          i != m_list_trigger_undo.top().end(); i++ ){
      (*i).m_selected = false;
    }


    unlock();
}


void
track::pop_trigger_undo( void )
{
    lock();

    if (m_list_trigger_undo.size() > 0 ){
        m_list_trigger_redo.push( m_list_trigger );
        m_list_trigger = m_list_trigger_undo.top();
        m_list_trigger_undo.pop();
    }

    unlock();
}


void
track::pop_trigger_redo( void )
{
    lock();

    if (m_list_trigger_redo.size() > 0 ){
        m_list_trigger_undo.push( m_list_trigger );
        m_list_trigger = m_list_trigger_redo.top();
        m_list_trigger_redo.pop();
    }

    unlock();
}

void
track::set_master_midi_bus( mastermidibus *a_mmb )
{
    lock();

    m_masterbus = a_mmb;

    unlock();
}


void
track::set_song_mute( bool a_mute )
{
    m_song_mute = a_mute;
}

bool
track::get_song_mute( void )
{
    return m_song_mute;
}

void
track::add_sequence( sequence *a_seq)
{
    a_seq->set_track(this);
    m_vector_sequence.push_back(*a_seq);
}

void track::delete_sequence( int a_num )
{
    sequence a_seq = m_vector_sequence[a_num];
    if( ! a_seq.get_editing() ) {
        a_seq.set_playing( false );
        m_vector_sequence.erase (m_vector_sequence.begin()+a_num);
    }
}

/* tick comes in as global tick */
void
track::play( long a_tick, bool a_playback_mode )
{
    //printf( "track::play(a_tick=%ld, a_playback=%d)\n", a_tick, a_playback_mode );
    lock();

    if(a_playback_mode) {
        // Song mode

        list<trigger>::iterator i = m_list_trigger.begin();
        while ( i != m_list_trigger.end()){
            if ( i->m_tick_end == a_tick ){
                if(i->m_sequence) i->m_sequence->set_playing(false);
            }
            if ( i->m_tick_start <= a_tick && i->m_tick_end > a_tick){
                if(i->m_sequence) {
                    i->m_sequence->set_playing(! m_song_mute);
                    i->m_sequence->play(a_tick, &(*i));
                }
                break;
            }
            if ( i->m_tick_end < a_tick ){
                break;
            }
            i++;
        }

    } else {
        // Live mode
        vector<sequence>::iterator iter = m_vector_sequence.begin();
        while ( iter != m_vector_sequence.end()){
            iter->set_playing(! m_song_mute);
            iter->play(a_tick, NULL);
            iter++;
        }
    }

    unlock();
}


void
track::clear_triggers( void )
{
    lock();
    m_list_trigger.clear();
    unlock();
}


void 
track::add_trigger( long a_tick, long a_length, long a_offset)
{
    lock();

    trigger e;
    
    e.m_offset = a_offset;
    
    e.m_selected = false;

    e.m_tick_start  = a_tick;
    e.m_tick_end    = a_tick + a_length - 1;

    list<trigger>::iterator i = m_list_trigger.begin();

    while ( i != m_list_trigger.end() ){

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
    
    while ( i != m_list_trigger.end() ){
        
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

    while ( i != m_list_trigger.end() ){
        if ((*i).m_tick_start <= a_tick &&
            (*i).m_tick_end   >= a_tick ){
            
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
        add_trigger( new_tick_start, length + 1, trig.m_offset );
    
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
    
    list<trigger>::iterator i = m_list_trigger.begin();
    while(  i != m_list_trigger.end() ){


        
	if ( (*i).m_tick_start >= from_start_tick &&
             (*i).m_tick_start <= from_end_tick )
        {
            trigger e;
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
    while(  i != m_list_trigger.end() ){

        // trigger greater than L and R
        if ( (*i).m_tick_start <= a_tick &&
             (*i).m_tick_end >= a_tick )
        {
            //printf( "split trigger %ld %ld\n", (*i).m_tick_start, (*i).m_tick_end );
            {
                long tick = (*i).m_tick_end - (*i).m_tick_start;
                tick += 1;
                tick /= 2;
                
                split_trigger(*i, (*i).m_tick_start + tick);              
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
    while(  i != m_list_trigger.end() ){

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


    i = m_list_trigger.begin();
    while(  i != m_list_trigger.end() ){

        if ( a_direction ) // forward
        {
            if ( (*i).m_tick_start >= a_start_tick ){
                (*i).m_tick_start += a_distance;
                (*i).m_tick_end   += a_distance;
                
            }         
        }
        else // back
        {
            if ( (*i).m_tick_start >= a_end_tick ){
                (*i).m_tick_start -= a_distance;
                (*i).m_tick_end   -= a_distance;
                
            }  
        }

        ++i;
    }
    

 
    unlock();

}

long
track::get_selected_trigger_start_tick( void )
{
    long ret = -1;
    lock();

    list<trigger>::iterator i = m_list_trigger.begin();
    
    while(  i != m_list_trigger.end() ){
        
	if ( i->m_selected ){
            ret = i->m_tick_start;
        }

        ++i;
    }
    
    unlock();

    return ret;
}

long
track::get_selected_trigger_end_tick( void )
{
    long ret = -1;
    lock();

    list<trigger>::iterator i = m_list_trigger.begin();
    
    while(  i != m_list_trigger.end() ){
        
	if ( i->m_selected ){

            ret = i->m_tick_end;
        }

        ++i;
    }
    
    unlock();

    return ret;
}


void
track::move_selected_triggers_to( long a_tick, int a_which )
{

    lock();

    long min_tick = 0;
    long max_tick = 0x7ffffff;

    list<trigger>::iterator i = m_list_trigger.begin();
    list<trigger>::iterator s = m_list_trigger.begin();


    // min_tick][0                1][max_tick
    //                   2
    
    while(  i != m_list_trigger.end() ){
        
	if ( i->m_selected ){

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

            if ( a_which == 1 )
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

            if ( a_which == 0 )
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
         
            if ( a_which == 2 )
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
            
            if ( a_which == 0 || a_which == 2 )
                s->m_tick_start += a_delta_tick;
            
            if ( a_which == 1 || a_which == 2 )
                s->m_tick_end   += a_delta_tick;
            
            break;
	}
        else {
            min_tick = (*i).m_tick_end + 1;
        }

        ++i;
    }

    unlock();
}


long
track::get_max_trigger( void )
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

bool 
track::get_trigger_state( long a_tick )
{
    lock();

    bool ret = false;
    list<trigger>::iterator i;

    for ( i = m_list_trigger.begin(); i != m_list_trigger.end(); i++ ){

	if ( (*i).m_tick_start <= a_tick &&
             (*i).m_tick_end >= a_tick){
	    ret = true;
            break;
        }
    }

    unlock();

    return ret;
}

bool 
track::get_trigger_sequence( long a_tick )
{
    lock();

    sequence *seq = NULL;
    list<trigger>::iterator i;

    for ( i = m_list_trigger.begin(); i != m_list_trigger.end(); i++ ){

	if ( (*i).m_tick_start <= a_tick &&
             (*i).m_tick_end >= a_tick){
	        seq = i->m_sequence;
            break;
        }
    }

    unlock();

    return seq;
}



bool 
track::select_trigger( long a_tick )
{
    lock();

    bool ret = false;
    list<trigger>::iterator i;
    
    for ( i = m_list_trigger.begin(); i != m_list_trigger.end(); i++ ){

	if ( (*i).m_tick_start <= a_tick &&
             (*i).m_tick_end   >= a_tick){
            
            (*i).m_selected = true;
	    ret = true;
        }        
    }
    
    unlock();
    
    return ret;
}


bool 
track::unselect_triggers( void )
{
    lock();

    bool ret = false;
    list<trigger>::iterator i;

    for ( i = m_list_trigger.begin(); i != m_list_trigger.end(); i++ ){
        (*i).m_selected = false;
    }

    unlock();

    return ret;
}



void
track::del_selected_trigger( void )
{
    lock();
    
    list<trigger>::iterator i;
    
    for ( i = m_list_trigger.begin(); i != m_list_trigger.end(); i++ ){

        if ( i->m_selected ){
            m_list_trigger.erase(i);
            break;
        }
    }
    
    unlock();
}


void
track::cut_selected_trigger( void )
{
    copy_selected_trigger();
    del_selected_trigger();
}


void
track::copy_selected_trigger( void )
{
    lock();
    
    list<trigger>::iterator i;
    
    for ( i = m_list_trigger.begin(); i != m_list_trigger.end(); i++ ){
        
        if ( i->m_selected ){
            m_trigger_clipboard = *i;
            m_trigger_copied = true;
            break;
        }
    }
    
    unlock();
}


void
track::paste_trigger( void )
{
    if ( m_trigger_copied ){
        long length =  m_trigger_clipboard.m_tick_end -
            m_trigger_clipboard.m_tick_start + 1;
        // paste at copy end
        add_trigger( m_trigger_clipboard.m_tick_end + 1,
                     length,
                     m_trigger_clipboard.m_offset);
        
        m_trigger_clipboard.m_tick_start = m_trigger_clipboard.m_tick_end +1;
        m_trigger_clipboard.m_tick_end = m_trigger_clipboard.m_tick_start + length - 1;
        
        m_trigger_clipboard.m_offset += length;
    }
}

void
track::reset_draw_trigger_marker( void )
{
    lock();

    m_iterator_draw_trigger = m_list_trigger.begin();

    unlock();
}


bool 
track::get_next_trigger( long *a_tick_on, long *a_tick_off, bool *a_selected, long *a_offset, sequence **a_seq )
{
    while (  m_iterator_draw_trigger  != m_list_trigger.end() ){ 
	    *a_tick_on  = (*m_iterator_draw_trigger).m_tick_start;
        *a_selected = (*m_iterator_draw_trigger).m_selected;
        *a_offset =   (*m_iterator_draw_trigger).m_offset;
	    *a_tick_off = (*m_iterator_draw_trigger).m_tick_end;
	    *a_seq = (*m_iterator_draw_trigger).m_sequence;
	    m_iterator_draw_trigger++;
	    return true;
    }
    return false;
}

void 
track::print_triggers()
{
    printf("[%s]\n", m_name.c_str()  );

    for( list<trigger>::iterator i = m_list_trigger.begin();
         i != m_list_trigger.end(); i++ ){

        /*long d= c_ppqn / 8;*/
        
        printf ("  tick_start[%ld] tick_end[%ld] off[%ld]\n", (*i).m_tick_start, (*i).m_tick_end, (*i).m_offset );

    }
}


void 
track::set_orig_tick( long a_tick )
{
    vector<sequence>::iterator iter = m_vector_sequence.begin();
    while ( iter != m_vector_sequence.end()){
        iter->set_orig_tick(a_tick);
        iter++;
    }
}

void 
track::off_playing_notes()
{
    vector<sequence>::iterator iter = m_vector_sequence.begin();
    while ( iter != m_vector_sequence.end()){
        iter->off_playing_notes();
        iter++;
    }
}
