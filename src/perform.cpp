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

#include "perform.h"
#include "midibus.h"
#include "event.h"
#include <stdio.h>
#include <fstream>
#ifndef __WIN32__
#  include <time.h>
#endif
#include <sched.h>

//For keys
#include <gtkmm/accelkey.h>

static int m_undo_track_count = 0;
static int m_redo_track_count = 0;

using namespace Gtk;

perform::perform()
{
    for (int i=0; i< c_max_track; i++) {

        m_tracks[i] = NULL;
        m_tracks_active[i] = false;
    }

    m_seqlist_open = false;
    m_seqlist_raise = false;
    m_running = false;
    m_looping = false;
    m_inputing = true;
    m_outputing = true;
    m_tick = 0;
    m_midiclockrunning = false;
    m_usemidiclock = false;
    m_midiclocktick = 0;
    m_midiclockpos = -1;

    thread_trigger_width_ms = c_thread_trigger_width_ms;

    m_left_tick = 0;
    m_right_tick = c_ppqn * 16;
    m_starting_tick = 0;

    m_key_bpm_up = GDK_apostrophe;
    m_key_bpm_dn = GDK_semicolon;

    m_key_start  = GDK_space;
    m_key_stop   = GDK_Escape;

    m_key_loop   = GDK_quoteleft;

    m_jack_running = false;
    m_jack_master = false;

    m_out_thread_launched = false;
    m_in_thread_launched = false;
    update_seqlist_on_change = false;
}


void perform::init( void )
{
    m_master_bus.init( );
}


void perform::init_jack( void )
{

#ifdef JACK_SUPPORT

    if ( global_with_jack_transport  && !m_jack_running){

        m_jack_running = true;
        m_jack_master = true;

        //printf ( "init_jack() m_jack_running[%d]\n", m_jack_running );

        do {

            char client_name[100];
            snprintf(client_name, sizeof(client_name), "seq42 (%d)", getpid());

            /* become a new client of the JACK server */
            if ((m_jack_client = jack_client_open(client_name,
                            JackNullOption, NULL)) == 0) {
                printf( "JACK server is not running.\n[JACK sync disabled]\n");
                m_jack_running = false;
                break;
            }

            jack_on_shutdown( m_jack_client, jack_shutdown,(void *) this );
            jack_set_sync_callback(m_jack_client, jack_sync_callback,
                    (void *) this );

            /* true if we want to fail if there is already a master */
            bool cond = global_with_jack_master_cond;

            if ( global_with_jack_master &&
                    jack_set_timebase_callback(m_jack_client, cond,
                        jack_timebase_callback, this) == 0){

                printf("[JACK transport master]\n");
                m_jack_master = true;
            }
            else {
                printf("[JACK transport slave]\n");
                m_jack_master = false;

            }
            if (jack_activate(m_jack_client)) {
                printf("Cannot register as JACK client\n");
                m_jack_running = false;
                break;
            }
        } while (0);
    }

#endif
}


void perform::deinit_jack( void )
{
#ifdef JACK_SUPPORT

    if ( m_jack_running){

        //printf ( "deinit_jack() m_jack_running[%d]\n", m_jack_running );

        m_jack_running = false;
        m_jack_master = false;

        if ( jack_release_timebase(m_jack_client)){
            printf("Cannot release Timebase.\n");
        }

        if (jack_client_close(m_jack_client)) {
            printf("Cannot close JACK client.\n");
        }


    }

    if ( !m_jack_running ){
        printf( "[JACK sync disabled]\n");
    }

#endif
}


void perform::clear_all( void )
{
    reset_sequences();

    for (int i=0; i< c_max_track; i++ ){

        if ( is_active_track(i) )
            delete_track( i );
    }
}

track* perform::get_track( int a_trk )
{
    return m_tracks[a_trk];
}

sequence* perform::get_sequence( int a_trk, int a_seq )
{
    track *a_track = m_tracks[a_trk];
    if(a_track == NULL)
    {
        return NULL;
    }
    else
    {
        return a_track->get_sequence(a_seq);
    }
}


int perform::get_track_index( track *a_track )
{
    for (int i=0; i< c_max_track; i++ )
    {
        if ( is_active_track(i) )
            if(m_tracks[i] == a_track) {
                return i;
            }
    }
    return -1;
}


void perform::set_song_mute( mute_op op  )
{
    for (int i=0; i< c_max_track; i++ )
    {
        if ( is_active_track(i) )
        {
            if(op == MUTE_ON) {
                m_tracks[i]->set_song_mute( true );
            } else if(op == MUTE_OFF) {
                m_tracks[i]->set_song_mute( false );
            } else if(op == MUTE_TOGGLE) {
                m_tracks[i]->set_song_mute( ! m_tracks[i]->get_song_mute() );
            }
        }
    }
}

perform::~perform()
{
    m_inputing = false;
    m_outputing = false;
    m_running = false;

    m_condition_var.signal();

    if (m_out_thread_launched )
        pthread_join( m_out_thread, NULL );

    if (m_in_thread_launched )
        pthread_join( m_in_thread, NULL );

    for (int i=0; i< c_max_track; i++ ){
        if ( is_active_track(i) ){
            delete m_tracks[i];
            m_tracks[i] = NULL;
        }
    }
}


void perform::set_left_tick( long a_tick )
{
    m_left_tick = a_tick;
    m_starting_tick = a_tick;

    if ( m_left_tick >= m_right_tick )
        m_right_tick = m_left_tick + c_ppqn * 4;

}


long perform::get_left_tick( void )
{
    return m_left_tick;
}


void perform::set_starting_tick( long a_tick )
{
    m_starting_tick = a_tick;
}


long perform::get_starting_tick( void )
{
    return m_starting_tick;
}


void perform::set_right_tick( long a_tick )
{
    if ( a_tick >= c_ppqn * 4 ){

        m_right_tick = a_tick;

        if ( m_right_tick <= m_left_tick ){
            m_left_tick = m_right_tick - c_ppqn * 4;
            m_starting_tick = m_left_tick;
        }
    }
}


long perform::get_right_tick( void )
{
    return m_right_tick;
}


void perform::add_track( track *a_track, int a_pref )
{
    /* check for preferred */
    if ( a_pref < c_max_track &&
            is_active_track(a_pref) == false &&
            a_pref >= 0 ){

        m_tracks[a_pref] = a_track;
        set_active(a_pref, true);

    } else {

        for (int i=a_pref; i< c_max_track; i++ ){

            if ( is_active_track(i) == false ){

                m_tracks[i] = a_track;
                set_active(i,true);
                break;
            }
        }
    }
}


void perform::set_active( int a_track, bool a_active )
{
    if ( a_track < 0 || a_track >= c_max_track )
        return;

    //printf ("set_active %d\n", a_active );

    if ( m_tracks_active[ a_track ] == true && a_active == false )
    {
        set_was_active(a_track);
    }

    m_tracks_active[ a_track ] = a_active;
}


void perform::set_was_active( int a_track )
{
    if ( a_track < 0 || a_track >= c_max_track )
        return;

    //printf( "was_active true\n" );

    m_was_active_edit[ a_track ] = true;
    m_was_active_perf[ a_track ] = true;
    m_was_active_names[ a_track ] = true;
}



bool perform::is_active_track( int a_track )
{
    if ( a_track < 0 || a_track >= c_max_track )
        return false;

    return m_tracks_active[ a_track ];
}



bool perform::is_dirty_perf (int a_track)
{
    if ( a_track < 0 || a_track >= c_max_track )
        return false;

    if ( is_active_track(a_track) )
    {
        return m_tracks[a_track]->is_dirty_perf();
    }

    bool was_active = m_was_active_perf[ a_track ];
    m_was_active_perf[ a_track ] = false;

    return was_active;
}


bool perform::is_dirty_names (int a_track)
{
    if ( a_track < 0 || a_track >= c_max_track )
        return false;

    if ( is_active_track(a_track) )
    {
        return m_tracks[a_track]->is_dirty_names();
    }

    bool was_active = m_was_active_names[ a_track ];
    m_was_active_names[ a_track ] = false;

    return was_active;
}

mastermidibus* perform::get_master_midi_bus( )
{
    return &m_master_bus;
}


void perform::set_running( bool a_running )
{
    m_running = a_running;
}


bool perform::is_running( void )
{
    return m_running;
}

void perform::set_bpm(int a_bpm)
{
    if ( a_bpm < 20 )  a_bpm = 20;
    if ( a_bpm > 500 ) a_bpm = 500;

    if ( ! (m_jack_running && m_running )){
        m_master_bus.set_bpm( a_bpm );
    }
}


int  perform::get_bpm( )
{
    return  m_master_bus.get_bpm( );
}

void perform::set_swing_amount8(int a_swing_amount)
{
    m_master_bus.set_swing_amount8( a_swing_amount );
}


int  perform::get_swing_amount8( )
{
    return m_master_bus.get_swing_amount8();
}

void perform::set_swing_amount16(int a_swing_amount)
{
    m_master_bus.set_swing_amount16( a_swing_amount );
}


int  perform::get_swing_amount16( )
{
    return m_master_bus.get_swing_amount16();
}


void perform::delete_track( int a_num )
{
    if ( m_tracks[a_num] != NULL &&
            !is_track_in_edit(a_num) ){
        set_active(a_num, false);
        m_tracks[a_num]->set_playing_off( );
        delete m_tracks[a_num];
        m_tracks[a_num] = NULL;
    }
}


bool perform::is_track_in_edit( int a_num )
{
    return ( (m_tracks[a_num] != NULL) &&
             ( m_tracks[a_num]->get_editing() ||  m_tracks[a_num]->get_sequence_editing() )
           );

}


void perform::new_track( int a_track )
{
    m_tracks[ a_track ] = new track();
    m_tracks[ a_track ]->set_master_midi_bus( &m_master_bus );
    set_active(a_track, true);
}



void perform::print()
{
    printf( "Dumping track data...\n");
    printf("bpm[%d]\n", get_bpm());
    printf("swing8[%d]\n", get_swing_amount8());
    printf("swing16[%d]\n", get_swing_amount16());
    for (int i = 0; i < c_max_track; i++) {
        if (is_active_track(i)) {
            printf("--------------------\n");
            printf("track[%d] at %p\n", i, &(m_tracks[i]));
            m_tracks[i]->print();
        }
    }
    //m_master_bus.print();
}


void perform::play( long a_tick )
{

    /* just run down the list of sequences and have them dump */

    //printf( "play [%d]\n", a_tick );

    m_tick = a_tick;
    for (int i=0; i< c_max_track; i++ ){

        if ( is_active_track(i) ){
            assert( m_tracks[i] );
            m_tracks[i]->play( a_tick, m_playback_mode );
        }
    }

    /* flush the bus */
    m_master_bus.flush();
}


void perform::set_orig_ticks( long a_tick  )
{
    for (int i=0; i< c_max_track; i++ ){

        if ( is_active_track(i) == true ){
            assert( m_tracks[i] );
            m_tracks[i]->set_orig_tick( a_tick );
        }
    }
}


void perform::clear_track_triggers( int a_trk  )
{
    if ( is_active_track(a_trk) == true ){
        assert( m_tracks[a_trk] );
        m_tracks[a_trk]->clear_triggers( );
    }
}


void perform::move_triggers( bool a_direction )
{
    if ( m_left_tick < m_right_tick ){

        long distance = m_right_tick - m_left_tick;

        for (int i=0; i< c_max_track; i++ ){

            if ( is_active_track(i) == true ){
                assert( m_tracks[i] );
                m_tracks[i]->move_triggers( m_left_tick, distance, a_direction );
            }
        }
    }
}

// collapse and expand - all tracks
void perform::push_trigger_undo( void )
{
    for (int i=0; i< c_max_track; i++ ){

        if ( is_active_track(i) == true ){
            assert( m_tracks[i] );
            m_tracks[i]->push_trigger_undo( );
        }
    }
    undo_type a_undo;
    a_undo.track = -1; // all tracks
    a_undo.type = c_undo_collapse_expand;

    undo_vect.push_back(a_undo);
    redo_vect.clear();
    update_seqlist_on_change = true;
}

void perform::pop_trigger_undo( void )
{
    for (int i=0; i< c_max_track; i++ ){

        if ( is_active_track(i) == true ){
            assert( m_tracks[i] );
            m_tracks[i]->pop_trigger_undo( );
        }
    }

    undo_type a_undo;
    a_undo.track = -1; // all tracks
    a_undo.type = c_undo_collapse_expand;

    undo_vect.pop_back();
    redo_vect.push_back(a_undo);
    update_seqlist_on_change = true;
}

void perform::pop_trigger_redo( void )
{
    for (int i=0; i< c_max_track; i++ ){

        if ( is_active_track(i) == true ){
            assert( m_tracks[i] );
            m_tracks[i]->pop_trigger_redo( );
        }
    }

    undo_type a_undo;
    a_undo.track = -1; // all tracks
    a_undo.type = c_undo_collapse_expand;

    redo_vect.pop_back();
    undo_vect.push_back(a_undo);
    update_seqlist_on_change = true;
}

// single track items
void perform::push_trigger_undo( int a_track )
{
    if ( is_active_track(a_track) == true ){
        assert( m_tracks[a_track] );
        m_tracks[a_track]->push_trigger_undo( );
    }
    undo_type a_undo;
    a_undo.track = a_track;
    a_undo.type = c_undo_trigger;

    undo_vect.push_back(a_undo);
    redo_vect.clear();
}

void perform::pop_trigger_undo( int a_track )
{
    if ( is_active_track(a_track) == true ){
        assert( m_tracks[a_track] );
        m_tracks[a_track]->pop_trigger_undo( );
    }

    undo_type a_undo;
    a_undo.track = a_track;
    a_undo.type = c_undo_trigger;

    undo_vect.pop_back();
    redo_vect.push_back(a_undo);
    update_seqlist_on_change = true;
}

void perform::pop_trigger_redo( int a_track )
{
    if ( is_active_track(a_track) == true ){
        assert( m_tracks[a_track] );
        m_tracks[a_track]->pop_trigger_redo( );
    }

    undo_type a_undo;
    a_undo.track = a_track;
    a_undo.type = c_undo_trigger;

    redo_vect.pop_back();
    undo_vect.push_back(a_undo);
    update_seqlist_on_change = true;
}

// tracks - merge sequence, cross track trigger, track cut, track paste
void perform::push_track_undo( int a_track )
{
    if ( is_active_track(a_track) == true ) // merge, cross track, track cut
    {
        assert( m_tracks[a_track] );

        m_undo_tracks[m_undo_track_count] = *(m_tracks[a_track]);
        m_undo_track_count++;
    }
    else // track paste - set name to null for later delete
    {
        m_undo_tracks[m_undo_track_count].set_name((char*)"***NULL***"); // FIXME
        m_undo_track_count++;
    }
    undo_type a_undo;
    a_undo.track = a_track;
    a_undo.type = c_undo_track;

    undo_vect.push_back(a_undo);
    redo_vect.clear();
}

void perform::pop_track_undo( int a_track )
{
    std::string name = "***NULL***"; // FIXME

    if ( is_active_track(a_track) == true ) // cross track, merge seq, paste track
    {
        if(is_track_in_edit(a_track)) // don't allow since track will be changed
            return;

        assert( m_tracks[a_track] );
        bool mute = m_tracks[a_track]->get_song_mute();

        m_redo_tracks[m_redo_track_count] = *(get_track(a_track ));
        m_redo_track_count++;

        delete_track(a_track); // must delete or junk leftover on copy - also for paste track undo

        if(m_undo_tracks[m_undo_track_count - 1].get_name() != name) // cross track, merge track
        {
            new_track( a_track );
            *(get_track( a_track )) = m_undo_tracks[m_undo_track_count - 1];
            assert( m_tracks[a_track] );
            get_track(a_track)->set_song_mute(mute); // don't change mute status on undo
            get_track( a_track )->set_dirty();
        }

        m_undo_track_count--;

    }else // cut track - set NULL name to redo, create new track for undo
    {
        m_redo_tracks[m_redo_track_count].set_name(name); // FIXME
        m_redo_track_count++;

        new_track( a_track  );
        assert( m_tracks[a_track] );
        *(get_track(a_track )) = m_undo_tracks[m_undo_track_count - 1];
        get_track( a_track )->set_dirty();
        m_undo_track_count--;
    }

    undo_type a_undo;
    a_undo.track = a_track;
    a_undo.type = c_undo_track;

    undo_vect.pop_back();
    redo_vect.push_back(a_undo);
    update_seqlist_on_change = true; // in case the seqlist is open
}

void perform::pop_track_redo( int a_track )
{
    std::string name = "***NULL***";

    if ( is_active_track(a_track) == true ) // cross track, merge seq, cut track
    {
        if(is_track_in_edit(a_track)) // don't allow since track will be changed
            return;

        assert( m_tracks[a_track] );
        bool mute = m_tracks[a_track]->get_song_mute();

        m_undo_tracks[m_undo_track_count] = *(get_track(a_track ));
        m_undo_track_count++;

        delete_track(a_track); // must delete or junk leftover on copy - also for cut track redo

        if(m_redo_tracks[m_redo_track_count - 1].get_name() != name) // cross track, merge track
        {
            new_track( a_track );
            *(get_track( a_track )) = m_redo_tracks[m_redo_track_count - 1];
            get_track(a_track)->set_song_mute(mute); // don't change mute status on undo
            get_track( a_track )->set_dirty();
        }

        m_redo_track_count--;

    }else  // paste track - set NULL to undo, create track for redo
    {
        m_undo_tracks[m_undo_track_count].set_name(name);
        m_undo_track_count++;

        new_track( a_track  );
        assert( m_tracks[a_track] );
        *(get_track(a_track )) = m_redo_tracks[m_redo_track_count - 1];
        get_track( a_track )->set_dirty();
        m_redo_track_count--;
    }

    undo_type a_undo;
    a_undo.track = a_track;
    a_undo.type = c_undo_track;

    redo_vect.pop_back();
    undo_vect.push_back(a_undo);
    update_seqlist_on_change = true; // in case the seqlist is open
}

void
perform::push_track_clipboard_undo(track a_track, int trk_idx)
{
    //printf("in push_track_clipboard_undo...\n");
    m_undo_tracks[m_undo_track_count] = a_track;
    m_undo_track_count++;

    undo_type a_undo;
    a_undo.track = trk_idx;
    a_undo.type = c_undo_track;

    undo_vect.push_back(a_undo);
    redo_vect.clear();
}

void
perform::set_undo_clipboard(int a_track)
{
    //printf("in set_undo_clipboard...\n");
    m_undo_clipboard = *(m_tracks[a_track]);
    track_index = a_track;
}

track
perform::get_undo_clipboard(void)
{
    return m_undo_clipboard;
}

/* copies between L and R -> R */
void perform::copy_triggers( )
{
    if ( m_left_tick < m_right_tick ){

        long distance = m_right_tick - m_left_tick;

        for (int i=0; i< c_max_track; i++ ){

            if ( is_active_track(i) == true ){
                assert( m_tracks[i] );
                m_tracks[i]->copy_triggers( m_left_tick, distance );
            }
        }
    }
}


void perform::start_jack(  )
{
    //printf( "perform::start_jack()\n" );
#ifdef JACK_SUPPORT
    if ( m_jack_running)
        jack_transport_start (m_jack_client );
#endif
}


void perform::stop_jack(  )
{
    //printf( "perform::stop_jack()\n" );
#ifdef JACK_SUPPORT
    if( m_jack_running )
        jack_transport_stop (m_jack_client);
#endif
}


void perform::position_jack( bool a_state )
{

    //printf( "perform::position_jack()\n" );

#ifdef JACK_SUPPORT

    if ( m_jack_running ){
        jack_transport_locate( m_jack_client, 0 );
    }
    return;


    jack_nframes_t rate = jack_get_sample_rate( m_jack_client ) ;

    long current_tick = 0;

    if ( a_state ){
        current_tick = m_left_tick;
    }

    jack_position_t pos;

    pos.valid = JackPositionBBT;
    pos.beats_per_bar = 4;
    pos.beat_type = 4;
    pos.ticks_per_beat = c_ppqn * 10;
    pos.beats_per_minute =  m_master_bus.get_bpm();

    /* Compute BBT info from frame number.  This is relatively
     * simple here, but would become complex if we supported tempo
     * or time signature changes at specific locations in the
     * transport timeline. */

    current_tick *= 10;

    pos.bar = (int32_t) (current_tick / (long) pos.ticks_per_beat
            / pos.beats_per_bar);
    pos.beat = (int32_t) ((current_tick / (long) pos.ticks_per_beat) % 4);
    pos.tick = (int32_t) (current_tick % (c_ppqn * 10));

    pos.bar_start_tick = pos.bar * pos.beats_per_bar * pos.ticks_per_beat;
    pos.frame_rate = rate;
    pos.frame = (jack_nframes_t) ( (current_tick * rate * 60.0)
            / (pos.ticks_per_beat * pos.beats_per_minute) );

    /*
       ticks * 10 = jack ticks;
       jack ticks / ticks per beat = num beats;
       num beats / beats per minute = num minutes
       num minutes * 60 = num seconds
       num secords * frame_rate  = frame */

    pos.bar++;
    pos.beat++;

    //printf( "position bbb[%d:%d:%4d]\n", pos.bar, pos.beat, pos.tick );

    jack_transport_reposition( m_jack_client, &pos );

#endif
}


void perform::start(bool a_state)
{
    if (m_jack_running) {
        return;
    }

    inner_start(a_state);
}


void perform::stop()
{
    if (m_jack_running) {
        return;
    }

    inner_stop();
}


void perform::inner_start(bool a_state)
{
    m_condition_var.lock();

    if (!is_running()) {

        set_playback_mode( a_state );

        if (a_state)
            off_sequences();

        set_running(true);
        m_condition_var.signal();
    }

    m_condition_var.unlock();
}


void perform::inner_stop()
{
    set_running(false);
    //off_sequences();
    reset_sequences();
    m_usemidiclock = false;
}


void perform::off_sequences(void)
{
    for (int i = 0; i < c_max_track; i++) {
        if (is_active_track(i)) {
            assert(m_tracks[i]);
            m_tracks[i]->set_playing_off();
        }
    }
}


void perform::all_notes_off( void )
{
    for (int i=0; i< c_max_track; i++) {

        if (is_active_track(i)) {
            assert(m_tracks[i]);
            m_tracks[i]->off_playing_notes();
        }
    }
    /* flush the bus */
    m_master_bus.flush();
}


void perform::reset_sequences(void)
{
    for (int i=0; i< c_max_track; i++) {
        if (is_active_track(i)) {
            assert( m_tracks[i] );
            m_tracks[i]->reset_sequences(m_playback_mode);
        }
    }
    /* flush the bus */
    m_master_bus.flush();
}


void perform::launch_output_thread(void)
{
    int err;

    err = pthread_create(&m_out_thread, NULL, output_thread_func, this);
    if (err != 0) {
        /*TODO: error handling*/
    }
    else
        m_out_thread_launched= true;
}


void perform::set_playback_mode( bool a_playback_mode )
{
    m_playback_mode = a_playback_mode;
}


void perform::launch_input_thread()
{
    int err;

    err = pthread_create(&m_in_thread, NULL, input_thread_func, this);
    if (err != 0) {
        /*TODO: error handling*/
    }
    else
    m_in_thread_launched = true;
}


long perform::get_max_trigger( void )
{
    long ret = 0, t;

    for (int i=0; i< c_max_track; i++ ){

        if ( is_active_track(i) == true ){
            assert( m_tracks[i] );

            t = m_tracks[i]->get_max_trigger( );
            if ( t > ret )
                ret = t;
        }
    }

    return ret;
}


void* output_thread_func(void *a_pef )
{
    /* set our performance */
    perform *p = (perform *) a_pef;
    assert(p);

    struct sched_param *schp = new sched_param;
    /*
     * set the process to realtime privs
     */

    if ( global_priority ){

        memset(schp, 0, sizeof(sched_param));
        schp->sched_priority = 1;

#ifndef __WIN32__
        // Not in MinGW RCB
        if (sched_setscheduler(0, SCHED_FIFO, schp) != 0) 	{
            printf("output_thread_func: couldnt sched_setscheduler"
                    " (FIFO), you need to be root.\n");
            pthread_exit(0);
        }
#endif
    }

#ifdef __WIN32__
    timeBeginPeriod(1);
#endif
    p->output_func();
#ifdef __WIN32__
    timeEndPeriod(1);
#endif

    return 0;
}



#ifdef JACK_SUPPORT

int jack_sync_callback(jack_transport_state_t state,
        jack_position_t *pos, void *arg)
{
    //printf( "jack_sync_callback() " );

    perform *p = (perform *) arg;

    p->m_jack_frame_current = jack_get_current_transport_frame(p->m_jack_client);

    p->m_jack_tick =
        p->m_jack_frame_current *
        p->m_jack_pos.ticks_per_beat *
        p->m_jack_pos.beats_per_minute / (p->m_jack_pos.frame_rate * 60.0);

    p->m_jack_frame_last = p->m_jack_frame_current;

    p->m_jack_transport_state_last =
        p->m_jack_transport_state =
        state;


    switch (state) {

        case JackTransportStopped:
            //printf( "[JackTransportStopped]\n" );
            break;

        case JackTransportRolling:
            //printf( "[JackTransportRolling]\n" );
            break;

        case JackTransportStarting:
            //printf( "[JackTransportStarting]\n" );
            p->inner_start( global_jack_start_mode );
            break;

        case JackTransportLooping:
            //printf( "[JackTransportLooping]" );
            break;

        default:
            break;
    }

    //printf( "starting frame[%d] tick[%8.2f]\n", p->m_jack_frame_current, p->m_jack_tick );

    print_jack_pos( pos );
    return 1;
}

#endif


void perform::output_func(void)
{
    while (m_outputing) {

        //printf ("waiting for signal\n");

        m_condition_var.lock();

        while (!m_running) {

            m_condition_var.wait();

            /* if stopping, then kill thread */
            if (!m_outputing)
                break;
        }

        m_condition_var.unlock();

        //printf( "signaled [%d]\n", m_playback_mode );

#ifndef __WIN32__
        /* begning time */
        struct timespec last;
        /* current time */
        struct timespec current;

        struct timespec stats_loop_start;
        struct timespec stats_loop_finish;


        /* difference between last and current */
        struct timespec delta;
#else
        /* begning time */
        long last;
        /* current time */
        long current;

        long stats_loop_start = 0;
        long stats_loop_finish = 0;


        /* difference between last and current */
        long delta;
#endif

        /* tick and tick fraction */
        double current_tick   = 0.0;
        double total_tick   = 0.0;
        double clock_tick = 0.0;

        long stats_total_tick = 0;

        long stats_loop_index = 0;
        long stats_min = 0x7FFFFFFF;
        long stats_max = 0;
        long stats_avg = 0;
        long stats_last_clock_us = 0;
        long stats_clock_width_us = 0;

        long stats_all[100];
        long stats_clock[100];

        bool jack_stopped = false;
        bool dumping = false;

        bool init_clock = true;

#ifdef JACK_SUPPORT
        double jack_ticks_converted = 0.0;
        double jack_ticks_converted_last = 0.0;
        double jack_ticks_delta = 0.0;
#endif
        for( int i=0; i<100; i++ ){
            stats_all[i] = 0;
            stats_clock[i] = 0;
        }

        /* if we are in the performance view, we care
           about starting from the offset */
        if ( m_playback_mode && !m_jack_running){

            current_tick = m_starting_tick;
            clock_tick = m_starting_tick;
            set_orig_ticks( m_starting_tick );
        }


        int ppqn = m_master_bus.get_ppqn();
#ifndef __WIN32__
        /* get start time position */
        clock_gettime(CLOCK_REALTIME, &last);

        if ( global_stats )
            stats_last_clock_us= (last.tv_sec * 1000000) + (last.tv_nsec / 1000);
#else
        /* get start time position */
        last = timeGetTime();

        if ( global_stats )
            stats_last_clock_us= last * 1000;
#endif

        while( m_running ){

            /************************************

              Get delta time ( current - last )
              Get delta ticks from time
              Add to current_ticks
              Compute prebuffer ticks
              play from current tick to prebuffer

             **************************************/

            if ( global_stats ){
#ifndef __WIN32__
                clock_gettime(CLOCK_REALTIME, &stats_loop_start);
#else
                stats_loop_start = timeGetTime();
#endif
            }


            /* delta time */
#ifndef __WIN32__
            clock_gettime(CLOCK_REALTIME, &current);
            delta.tv_sec  =  (current.tv_sec  - last.tv_sec  );
            delta.tv_nsec =  (current.tv_nsec - last.tv_nsec );
            long delta_us = (delta.tv_sec * 1000000) + (delta.tv_nsec / 1000);
#else
            current = timeGetTime();
            //printf( "current [0x%x]\n", current );
            delta = current - last;
            long delta_us = delta * 1000;
            //printf( "  delta [0x%x]\n", delta );
#endif

            /* delta time to ticks */
            /* bpm */
            int bpm  = m_master_bus.get_bpm();

            /* get delta ticks, delta_ticks_f is in 1000th of a tick */
            double delta_tick = (double) (bpm * ppqn * (delta_us/60000000.0f));

            if (m_usemidiclock) {
                delta_tick = m_midiclocktick;
                m_midiclocktick = 0;
            }
            if (0 <= m_midiclockpos) {
                delta_tick = 0;
                clock_tick     = m_midiclockpos;
                current_tick   = m_midiclockpos;
                total_tick     = m_midiclockpos;
                m_midiclockpos = -1;
                //init_clock = true;
            }

            //printf ( "    delta_tick[%lf]\n", delta_tick  );
#ifdef JACK_SUPPORT

            // no init until we get a good lock

            if ( m_jack_running ){

                init_clock = false;

                m_jack_transport_state = jack_transport_query( m_jack_client, &m_jack_pos );
                m_jack_frame_current =  jack_get_current_transport_frame( m_jack_client );

                if ( m_jack_transport_state_last  ==  JackTransportStarting &&
                        m_jack_transport_state       == JackTransportRolling ){

                    m_jack_frame_last = m_jack_frame_current;


                    //printf ("[Start Playback]\n" );
                    dumping = true;
                    m_jack_tick =
                        m_jack_pos.frame *
                        m_jack_pos.ticks_per_beat *
                        m_jack_pos.beats_per_minute / (m_jack_pos.frame_rate * 60.0);


                    /* convert ticks */
                    jack_ticks_converted =
                        m_jack_tick * ((double) c_ppqn /
                                (m_jack_pos.ticks_per_beat *
                                 m_jack_pos.beat_type / 4.0  ));

                    set_orig_ticks( (long) jack_ticks_converted );
                    current_tick = clock_tick = total_tick = jack_ticks_converted_last = jack_ticks_converted;
                    init_clock = true;

                    if ( m_looping && m_playback_mode ){

                        //printf( "left[%lf] right[%lf]\n", (double) get_left_tick(), (double) get_right_tick() );

                        if ( current_tick >= get_right_tick() ){

                            while ( current_tick >= get_right_tick() ){

                                double size = get_right_tick() - get_left_tick();
                                current_tick = current_tick - size;

                                //printf( "> current_tick[%lf]\n", current_tick );
                            }
                            reset_sequences();
                            set_orig_ticks( (long)current_tick );
                        }
                    }
                }

                if ( m_jack_transport_state_last  ==  JackTransportRolling &&
                        m_jack_transport_state  == JackTransportStopped ){

                    m_jack_transport_state_last = JackTransportStopped;
                    //printf ("[Stop Playback]\n" );
                    jack_stopped = true;
                }

                //-----  Jack transport is Rolling Now ---------

                /* transport is in a sane state if dumping == true */
                if ( dumping )
                {
                    m_jack_frame_current =  jack_get_current_transport_frame( m_jack_client );

                    //printf( " frame[%7d]", m_jack_pos.frame );
                    //printf( " current_transport_frame[%7d]", m_jack_frame_current );

                    // if we are moving ahead
                    if ( (m_jack_frame_current > m_jack_frame_last)){


                        m_jack_tick +=
                            (m_jack_frame_current - m_jack_frame_last)  *
                            m_jack_pos.ticks_per_beat *
                            m_jack_pos.beats_per_minute / (m_jack_pos.frame_rate * 60.0);


                        //printf ( "m_jack_tick += (m_jack_frame_current[%lf] - m_jack_frame_last[%lf]) *\n",
                        //        (double) m_jack_frame_current, (double) m_jack_frame_last );
                        //printf(  "m_jack_pos.ticks_per_beat[%lf] * m_jack_pos.beats_per_minute[%lf] / \n(m_jack_pos.frame_rate[%lf] * 60.0\n", (double) m_jack_pos.ticks_per_beat, (double) m_jack_pos.beats_per_minute, (double) m_jack_pos.frame_rate);


                        m_jack_frame_last = m_jack_frame_current;
                    }

                    /* convert ticks */
                    jack_ticks_converted =
                        m_jack_tick * ((double) c_ppqn /
                                (m_jack_pos.ticks_per_beat * m_jack_pos.beat_type / 4.0  ));

                    //printf ( "jack_ticks_conv[%lf] = \n",  jack_ticks_converted );
                    //printf ( "    m_jack_tick[%lf] * ((double) c_ppqn[%lf] / \n", m_jack_tick, (double) c_ppqn );
                    //printf ( "   (m_jack_pos.ticks_per_beat[%lf] * m_jack_pos.beat_type[%lf] / 4.0  )\n",
                    //        m_jack_pos.ticks_per_beat, m_jack_pos.beat_type );




                    jack_ticks_delta = jack_ticks_converted - jack_ticks_converted_last;

                    clock_tick     += jack_ticks_delta;
                    current_tick   += jack_ticks_delta;
                    total_tick     += jack_ticks_delta;

                    m_jack_transport_state_last = m_jack_transport_state;
                    jack_ticks_converted_last = jack_ticks_converted;

                    /* printf( "current_tick[%lf] delta[%lf]\n", current_tick, jack_ticks_delta ); */


                    //long ptick, pbeat, pbar;

                    //pbar  = (long) ((long) m_jack_tick / (m_jack_pos.ticks_per_beat *  m_jack_pos.beats_per_bar ));

                    long pbeat = (long) ((long) m_jack_tick % (long)
                                     (m_jack_pos.ticks_per_beat *  m_jack_pos.beats_per_bar ));
                    pbeat = pbeat / (long) m_jack_pos.ticks_per_beat;

                    //ptick = (long) m_jack_tick % (long) m_jack_pos.ticks_per_beat;


                    //printf( " bbb [%2d:%2d:%4d]", pbar+1, pbeat+1, ptick );
                    //printf( " bbb [%2d:%2d:%4d]", m_jack_pos.bar, m_jack_pos.beat, m_jack_pos.tick );

                    /*double jack_tick = (m_jack_pos.bar-1) * (m_jack_pos.ticks_per_beat *  m_jack_pos.beats_per_bar ) +
                      (m_jack_pos.beat-1) * m_jack_pos.ticks_per_beat + m_jack_pos.tick;*/

                    //printf( " jtick[%8.3f]", m_jack_tick );
                    //printf( " mtick[%8.3f]", jack_tick );

                    //printf( " delta[%8.3f]", m_jack_tick - jack_tick );

                    //printf( "\n");

                } /* end if dumping / sane state */

            } /* if jack running */
            else
            {
#endif
                /* default if jack is not compiled in, or not running */
                /* add delta to current ticks */
                clock_tick     += delta_tick;
                current_tick   += delta_tick;
                total_tick     += delta_tick;
                dumping = true;

#ifdef JACK_SUPPORT
            }
#endif

            /* init_clock will be true when we run for the first time, or
             * as soon as jack gets a good lock on playback */

            if (init_clock) {
                m_master_bus.init_clock( (long)clock_tick );
                init_clock = false;
            }

            if (dumping) {
                if ( m_looping && m_playback_mode )
                {
                    if ( current_tick >= get_right_tick() )
                    {
                        double leftover_tick = current_tick - (get_right_tick());

                        play( get_right_tick() - 1 );
                        reset_sequences();

                        set_orig_ticks( get_left_tick() );
                        current_tick = (double) get_left_tick() + leftover_tick;
                    }
                }

                /* play */
                play( (long) current_tick );
                //printf( "play[%d]\n", current_tick );

                /* midi clock */
                m_master_bus.clock( (long) clock_tick );


                if ( global_stats ){

                    while ( stats_total_tick <= total_tick ){

                        /* was there a tick ? */
                        if ( stats_total_tick % (c_ppqn / 24) == 0 ){

#ifndef __WIN32__
                            long current_us = (current.tv_sec * 1000000) + (current.tv_nsec / 1000);
#else
                            long current_us = current * 1000;
#endif
                            stats_clock_width_us = current_us - stats_last_clock_us;
                            stats_last_clock_us = current_us;

                            int index = stats_clock_width_us / 300;
                            if ( index >= 100 ) index = 99;
                            stats_clock[index]++;

                        }
                        stats_total_tick++;
                    }
                }
            }

            /***********************************

              Figure out how much time
              we need to sleep, and do it

             ************************************/

            /* set last */
            last = current;

#ifndef __WIN32__
            clock_gettime(CLOCK_REALTIME, &current);
            delta.tv_sec  =  (current.tv_sec  - last.tv_sec  );
            delta.tv_nsec =  (current.tv_nsec - last.tv_nsec );
            long elapsed_us = (delta.tv_sec * 1000000) + (delta.tv_nsec / 1000);
            //printf( "elapsed_us[%ld]\n", elapsed_us );
#else
            current = timeGetTime();
            delta = current - last;
            long elapsed_us = delta * 1000;
            //printf( "        elapsed_us[%ld]\n", elapsed_us );
#endif

            /* now, we want to trigger every c_thread_trigger_width_ms,
               and it took us delta_us to play() */

            delta_us = (c_thread_trigger_width_ms * 1000) - elapsed_us;
            //printf( "sleeping_us[%ld]\n", delta_us );


            /* check midi clock adjustment */

            double next_total_tick = (total_tick + (c_ppqn / 24.0));
            double next_clock_delta   = (next_total_tick - total_tick - 1);


            double next_clock_delta_us =  (( next_clock_delta ) * 60000000.0f / c_ppqn  / bpm );

            if ( next_clock_delta_us < (c_thread_trigger_width_ms * 1000.0f * 2.0f) ){
                delta_us = (long)next_clock_delta_us;
            }

#ifndef __WIN32__
            if ( delta_us > 0.0 ){

                delta.tv_sec =  (delta_us / 1000000);
                delta.tv_nsec = (delta_us % 1000000) * 1000;

                //printf("sleeping() ");
                nanosleep( &delta, NULL );

            }
#else
            if ( delta_us > 0 ){

                delta =  (delta_us / 1000);

                //printf("           sleeping() [0x%x]\n", delta);
                Sleep(delta);

            }
#endif

            else {

                if ( global_stats )
                    printf ("underrun\n" );
            }

            if ( global_stats ){
#ifndef __WIN32__
                clock_gettime(CLOCK_REALTIME, &stats_loop_finish);
#else
                stats_loop_finish = timeGetTime();
#endif
            }

            if ( global_stats ){

#ifndef __WIN32__
                delta.tv_sec  =  (stats_loop_finish.tv_sec  - stats_loop_start.tv_sec  );
                delta.tv_nsec =  (stats_loop_finish.tv_nsec - stats_loop_start.tv_nsec );
                long delta_us = (delta.tv_sec * 1000000) + (delta.tv_nsec / 1000);
#else
                delta = stats_loop_finish - stats_loop_start;
                long delta_us = delta * 1000;
#endif

                int index = delta_us / 100;
                if ( index >= 100  ) index = 99;

                stats_all[index]++;

                if ( delta_us > stats_max )
                    stats_max = delta_us;
                if ( delta_us < stats_min )
                    stats_min = delta_us;

                stats_avg += delta_us;
                stats_loop_index++;

                if ( stats_loop_index > 200 ){

                    stats_loop_index = 0;
                    stats_avg /= 200;

                    printf("stats_avg[%ld]us stats_min[%ld]us"
                            " stats_max[%ld]us\n", stats_avg,
                            stats_min, stats_max);

                    stats_min = 0x7FFFFFFF;
                    stats_max = 0;
                    stats_avg = 0;
                }
            }

            if (jack_stopped)
                inner_stop();
        }


        if (global_stats){

            printf("\n\n-- trigger width --\n");
            for (int i=0; i<100; i++ ){
                printf( "[%3d][%8ld]\n", i * 100, stats_all[i] );
            }
            printf("\n\n-- clock width --\n" );
            int bpm  = m_master_bus.get_bpm();

            printf("optimal: [%d]us\n", ((c_ppqn / 24)* 60000000 / c_ppqn / bpm));

            for ( int i=0; i<100; i++ ){
                printf( "[%3d][%8ld]\n", i * 300, stats_clock[i] );
            }
        }

        m_tick = 0;
        m_master_bus.flush();
        m_master_bus.stop();
    }

    pthread_exit(0);
}


void* input_thread_func(void *a_pef )
{

    /* set our performance */
    perform *p = (perform *) a_pef;
    assert(p);


    struct sched_param *schp = new sched_param;
    /*
     * set the process to realtime privs
     */

    if ( global_priority ){

        memset(schp, 0, sizeof(sched_param));
        schp->sched_priority = 1;

#ifndef __WIN32__
        // MinGW RCB
        if (sched_setscheduler(0, SCHED_FIFO, schp) != 0) 	{

            printf("input_thread_func: couldnt sched_setscheduler"
                    " (FIFO), you need to be root.\n");
            pthread_exit(0);
        }
#endif
    }
#ifdef __WIN32__
    timeBeginPeriod(1);
#endif

    p->input_func();

#ifdef __WIN32__
    timeEndPeriod(1);
#endif

    return 0;
}



void perform::input_func(void)
{
    event ev;

    while (m_inputing) {

        if (m_master_bus.poll_for_midi() > 0) {

            do {

                if (m_master_bus.get_midi_event(&ev)) {

                    /* start propagation if not already running*/
                    if (ev.get_status() == EVENT_MIDI_CLOCK)
                    {
                        if (m_midiclockrunning)
                            m_midiclocktick += 8;
                        else if (m_usemidiclock) {
                            start(false);
                            m_midiclockrunning = true;
                        }
                    }

                    /*prapare for MIDI clock usage at song position 0*/
                    else if (ev.get_status() == EVENT_MIDI_START)
                    {
                        if (!m_midiclockrunning) {
                            m_usemidiclock = true;
                            m_midiclocktick = 0;
                            m_midiclockpos = 0;
                        }
                    }

                    /*prapare for MIDI clock usage at current song position*/
                    else if (ev.get_status() == EVENT_MIDI_CONTINUE)
                    {
                        if (!m_midiclockrunning) {
                            m_usemidiclock = true;
                        }
                    }

                    /*stop MIDI clock usage*/
                    else if (ev.get_status() == EVENT_MIDI_STOP)
                    {
                        if (m_midiclockrunning) {
                            m_midiclockrunning = false;
                            m_usemidiclock = false;
                            all_notes_off();
                        }
                    }

                    /*adjust position if not in MIDI clock run mode*/
                    else if (ev.get_status() == EVENT_MIDI_SONG_POS)
                    {
                        if (!m_midiclockrunning) {
                            unsigned char a, b;
                            ev.get_data(&a, &b);
                            m_midiclockpos = ((unsigned int)a << 7) | b;
                        }
                    }

                    /* filter system wide messages */
                    if (ev.get_status() <= EVENT_SYSEX) {

                        if( global_showmidi)
                            ev.print();

                        /* is there a sequence set? */
                        if (m_master_bus.is_dumping()) {

                            ev.set_timestamp(m_tick);

                            /* dump to it */
                            (m_master_bus.get_sequence())->stream_event(&ev);
                        }


                    }

                    if (ev.get_status() == EVENT_SYSEX) {

                        if (global_showmidi)
                            ev.print();

                        if (global_pass_sysex)
                            m_master_bus.sysex(&ev);
                    }
                }

            } while (m_master_bus.is_more_input());
        }
    }
    pthread_exit(0);
}


#ifdef JACK_SUPPORT
void jack_timebase_callback(jack_transport_state_t state,
        jack_nframes_t nframes,
        jack_position_t *pos, int new_pos, void *arg)
{

    static double jack_tick;
    static jack_nframes_t last_frame;
    static jack_nframes_t current_frame;
    static jack_transport_state_t state_current;
    static jack_transport_state_t state_last;

    state_current = state;

    perform *p = (perform *) arg;
    current_frame = jack_get_current_transport_frame( p->m_jack_client );

    //printf( "jack_timebase_callback() [%d] [%d] [%d]", state, new_pos, current_frame);

    pos->valid = JackPositionBBT;
    pos->beats_per_bar = 4;
    pos->beat_type = 4;
    pos->ticks_per_beat = c_ppqn * 10;
    pos->beats_per_minute = p->get_bpm();


    /* Compute BBT info from frame number.  This is relatively
     * simple here, but would become complex if we supported tempo
     * or time signature changes at specific locations in the
     * transport timeline. */

    // if we are in a new position
    if (  state_last    ==  JackTransportStarting &&
            state_current ==  JackTransportRolling ){

        //printf ( "Starting [%d] [%d]\n", last_frame, current_frame );

        jack_tick = 0.0;
        last_frame = current_frame;
    }

    if ( current_frame > last_frame ){

        double jack_delta_tick =
            (current_frame - last_frame) *
            pos->ticks_per_beat *
            pos->beats_per_minute / (pos->frame_rate * 60.0);

        jack_tick += jack_delta_tick;

        last_frame = current_frame;
    }

    long ptick = 0, pbeat = 0, pbar = 0;

    pbar  = (long) ((long) jack_tick / (pos->ticks_per_beat *  pos->beats_per_bar ));

    pbeat = (long) ((long) jack_tick % (long) (pos->ticks_per_beat *  pos->beats_per_bar ));
    pbeat = pbeat / (long) pos->ticks_per_beat;

    ptick = (long) jack_tick % (long) pos->ticks_per_beat;

    pos->bar = pbar + 1;
    pos->beat = pbeat + 1;
    pos->tick = ptick;;
    pos->bar_start_tick = pos->bar * pos->beats_per_bar *
        pos->ticks_per_beat;

    //printf( " bbb [%2d:%2d:%4d]\n", pos->bar, pos->beat, pos->tick );

    state_last = state_current;
}


void jack_shutdown(void *arg)
{
    perform *p = (perform *) arg;
    p->m_jack_running = false;

    printf("JACK shut down.\nJACK sync Disabled.\n");
}


void print_jack_pos( jack_position_t* jack_pos ){

    return;
    printf( "print_jack_pos()\n" );
    printf( "    bar  [%d]\n", jack_pos->bar  );
    printf( "    beat [%d]\n", jack_pos->beat );
    printf( "    tick [%d]\n", jack_pos->tick );
    printf( "    bar_start_tick   [%lf]\n", jack_pos->bar_start_tick );
    printf( "    beats_per_bar    [%f]\n", jack_pos->beats_per_bar );
    printf( "    beat_type        [%f]\n", jack_pos->beat_type );
    printf( "    ticks_per_beat   [%lf]\n", jack_pos->ticks_per_beat );
    printf( "    beats_per_minute [%lf]\n", jack_pos->beats_per_minute );
    printf( "    frame_time       [%lf]\n", jack_pos->frame_time );
    printf( "    next_time        [%lf]\n", jack_pos->next_time );
}


#if 0

int main ( void )
{
    jack_client_t *client;

    /* become a new client of the JACK server */
    if ((client = jack_client_new("transport tester")) == 0) {
        fprintf(stderr, "jack server not running?\n");
        return 1;
    }

    jack_on_shutdown(client, jack_shutdown, 0);
    jack_set_sync_callback(client, jack_sync_callback, NULL);

    if (jack_activate(client)) {
        fprintf(stderr, "cannot activate client");
        return 1;
    }

    bool cond = false; /* true if we want to fail if there is already a master */
    if (jack_set_timebase_callback(client, cond, timebase, NULL) != 0){
        printf("Unable to take over timebase or there is already a master.\n");
        exit(1);
    }

    jack_position_t pos;

    pos.valid = JackPositionBBT;

    pos.bar = 0;
    pos.beat = 0;
    pos.tick = 0;

    pos.beats_per_bar = time_beats_per_bar;
    pos.beat_type = time_beat_type;
    pos.ticks_per_beat = time_ticks_per_beat;
    pos.beats_per_minute = time_beats_per_minute;
    pos.bar_start_tick = 0.0;


    //jack_transport_reposition( client, &pos );

    jack_transport_start (client);

    //void jack_transport_stop (jack_client_t *client);

    int bob;
    scanf ("%d", &bob);

    jack_transport_stop (client);
    jack_release_timebase(client);
    jack_client_close(client);

    return 0;
}

#endif


#endif


bool
perform::save( const Glib::ustring& a_filename )
{
    ofstream file (a_filename.c_str (), ios::out | ios::binary | ios::trunc);

    if (!file.is_open ()) return false;

    file.write((const char *) &c_file_version, sizeof(int));

    int bpm = get_bpm();
    file.write((const char *) &bpm, sizeof(int));

    int swing_amount8 = get_swing_amount8();
    file.write((const char *) &swing_amount8, sizeof(int));
    int swing_amount16 = get_swing_amount16();
    file.write((const char *) &swing_amount16, sizeof(int));

    int active_tracks = 0;
    for (int i=0; i< c_max_track; i++ ){
        if ( is_active_track(i) ) active_tracks++;
    }
    file.write((const char *) &active_tracks, sizeof(int));

    for (int i=0; i< c_max_track; i++ ){
        if ( is_active_track(i) )
        {
            if(! get_track(i)->save(&file)) {
                return false;
            }
        }
    }

    file.close();
    return true;
}

bool
perform::load( const Glib::ustring& a_filename )
{
    ifstream file (a_filename.c_str (), ios::in | ios::binary);

    if (!file.is_open ()) return false;

    int version;
    file.read((char *) &version, sizeof(int));

    int bpm;
    file.read((char *) &bpm, sizeof(int));
    set_bpm(bpm);

    int swing_amount8 = 0;
    if(version > 1) {
        file.read((char *) &swing_amount8, sizeof(int));
    }
    set_swing_amount8(swing_amount8);
    int swing_amount16 = 0;
    if(version > 1) {
        file.read((char *) &swing_amount16, sizeof(int));
    }
    set_swing_amount16(swing_amount16);

    int active_tracks;
    file.read((char *) &active_tracks, sizeof(int));

    for (int i=0; i< active_tracks; i++ ){
        new_track(i);
        if(! get_track(i)->load(&file, version)) {
            return false;
        }
    }

    file.close();
    return true;
}

void
perform::apply_song_transpose()
{
    for (int i=0; i< c_max_track; i++ ){
        if ( is_active_track(i) )
        {
            get_track(i)->apply_song_transpose();
        }
    }
}
