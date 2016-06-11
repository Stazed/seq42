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
#endif // __WIN32__
#include <sched.h>

//For keys
#include <gtkmm/accelkey.h>


using namespace Gtk;

perform::perform()
{
    for (int i=0; i< c_max_track; i++)
    {
        m_tracks[i] = NULL;
        m_tracks_active[i]      = false;
        m_was_active_edit[i]    = false;
        m_was_active_perf[i]    = false;
        m_was_active_names[i]   = false;
    }

    m_seqlist_open = false;
    m_seqlist_raise = false;
    m_looping = false;
    m_reposition = false;
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
    m_key_song   = GDK_F1;
    m_key_jack   = GDK_F2;
    m_key_seqlist   = GDK_F3;
    m_key_follow_trans  = GDK_F4;

    m_jack_running = false;
    m_toggle_jack = false;

    m_jack_master = false;

    m_out_thread_launched = false;
    m_in_thread_launched = false;

    m_playback_mode = false;
    m_follow_transport = true;

    m_bp_measure = 4;
    m_bw = 4;

    m_have_undo = false; // for button sensitive
    m_have_redo = false; // for button sensitive
    m_undo_track_count = 0;
    m_redo_track_count = 0;
    m_undo_perf_count = 0;
    m_redo_perf_count = 0;
}

void perform::init()
{
    m_master_bus.init( );
}

void perform::init_jack()
{

#ifdef JACK_SUPPORT

    if ( global_with_jack_transport  && !m_jack_running)
    {
        m_jack_running = true;
        m_jack_master = true;

        //printf ( "init_jack() m_jack_running[%d]\n", m_jack_running );

        do
        {
            /* become a new client of the JACK server */
#ifdef JACK_SESSION
            if (global_jack_session_uuid.empty())
                m_jack_client = jack_client_open(PACKAGE, JackNullOption, NULL);
            else
                m_jack_client = jack_client_open(PACKAGE, JackSessionID, NULL,
                                                 global_jack_session_uuid.c_str());
#else
            m_jack_client = jack_client_open(PACKAGE, JackNullOption, NULL );
#endif

            if (m_jack_client == 0)
            {
                printf( "JACK server is not running.\n[JACK sync disabled]\n");
                m_jack_running = false;
                break;
            }
            else
                m_jack_frame_rate = jack_get_sample_rate( m_jack_client );

            /*
                The call to jack_timebase_callback() to supply jack with BBT, etc would
                occasionally fail when the *pos information had zero or some garbage in
                the pos.frame_rate variable. This would occur when there was a rapid change
                of frame position by another client... i.e. qjackctl.
                From the jack API:

                "   pos	address of the position structure for the next cycle;
                    pos->frame will be its frame number. If new_pos is FALSE,
                    this structure contains extended position information from the current cycle.
                    If TRUE, it contains whatever was set by the requester.
                    The timebase_callback's task is to update the extended information here."

                The "If TRUE" line seems to be the issue. It seems that qjackctl does not
                always set pos.frame_rate so we get garbage and some strange BBT calculations
                that display in qjackctl. So we need to set it here and just use m_jack_frame_rate
                for calculations instead of pos.frame_rate.
            */

            jack_on_shutdown( m_jack_client, jack_shutdown,(void *) this );
            jack_set_sync_callback(m_jack_client, jack_sync_callback,
                                   (void *) this );
            jack_set_process_callback(m_jack_client, jack_process_callback, NULL);
#ifdef JACK_SESSION
            if (jack_set_session_callback)
                jack_set_session_callback(m_jack_client, jack_session_callback,
                                          (void *) this );
#endif
            /* true if we want to fail if there is already a master */
            bool cond = global_with_jack_master_cond;

            if ( global_with_jack_master &&
                    jack_set_timebase_callback(m_jack_client, cond,
                                               jack_timebase_callback, this) == 0)
            {
                printf("[JACK transport master]\n");
                m_jack_master = true;
            }
            else
            {
                printf("[JACK transport slave]\n");
                m_jack_master = false;
            }
            if (jack_activate(m_jack_client))
            {
                printf("Cannot register as JACK client\n");
                m_jack_running = false;
                break;
            }
        }
        while (0);
    }

#endif // JACK_SUPPORT
}

void perform::deinit_jack()
{
#ifdef JACK_SUPPORT

    if ( m_jack_running)
    {
        //printf ( "deinit_jack() m_jack_running[%d]\n", m_jack_running );

        m_jack_running = false;
        m_jack_master = false;

        if ( jack_release_timebase(m_jack_client))
        {
            printf("Cannot release Timebase.\n");
        }

        if (jack_client_close(m_jack_client))
        {
            printf("Cannot close JACK client.\n");
        }
    }

    if ( !m_jack_running )
    {
        printf( "[JACK sync disabled]\n");
    }

#endif // JACK_SUPPORT
}

void perform::clear_all()
{
    reset_sequences();

    for (int i=0; i< c_max_track; i++ )
    {
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
            if(m_tracks[i] == a_track)
            {
                return i;
            }
    }
    return -1;
}

void perform::set_reposition()
{
    m_reposition = true;
}

void perform::set_song_mute( mute_op op  )
{
    for (int i=0; i< c_max_track; i++ )
    {
        if ( is_active_track(i) )
        {
            if(op == MUTE_ON)
            {
                m_tracks[i]->set_song_mute( true );
            }
            else if(op == MUTE_OFF)
            {
                m_tracks[i]->set_song_mute( false );
            }
            else if(op == MUTE_TOGGLE)
            {
                m_tracks[i]->set_song_mute( ! m_tracks[i]->get_song_mute() );
            }
        }
    }
}

perform::~perform()
{
    m_inputing = false;
    m_outputing = false;

    m_condition_var.signal();

    if (m_out_thread_launched )
        pthread_join( m_out_thread, NULL );

    if (m_in_thread_launched )
        pthread_join( m_in_thread, NULL );

    for (int i=0; i< c_max_track; i++ )
    {
        if ( is_active_track(i) )
        {
            delete m_tracks[i];
            m_tracks[i] = NULL;
        }
    }
}

void
perform::start_playing()
{
    if(global_song_start_mode)   // song mode
    {
        if(m_jack_master)
        {
            position_jack(true, m_left_tick);     // for cosmetic reasons - to stop transport line flicker on start
        }
        start_jack( );
        start( true );           // true for setting song m_playback_mode = true
    }
    else                         // live mode
    {
        if(m_jack_master)
            position_jack(false, 0);   // for cosmetic reasons - to stop transport line flicker on start
        start( false );
        start_jack( );
    }
}

void
perform::stop_playing()
{
    stop_jack();
    stop();
}

void
perform::rewind()
{
    long measure_ticks = (c_ppqn * 4) * m_bp_measure / m_bw;
    long a_tick = m_tick - measure_ticks;
    if(a_tick < 0)
        a_tick = 0;
    if(m_jack_running)
    {
        position_jack(true, a_tick);
    }
    else
    {
        set_starting_tick(a_tick);
        set_reposition();
    }
}

void
perform::fast_forward()
{
    long measure_ticks = (c_ppqn * 4) * m_bp_measure / m_bw;
    long a_tick = m_tick + measure_ticks;
    if(m_jack_running)
    {
        position_jack(true, a_tick);
    }
    else
    {
        set_starting_tick(a_tick);
        set_reposition();
    }
}

void perform::toggle_song_mode()
{
    if(global_song_start_mode)
        global_song_start_mode = false;
    else
    {
        global_song_start_mode = true;
    }
}

void perform::toggle_jack_mode()
{
    set_jack_mode(!m_jack_running);
}

void perform::set_jack_mode(bool a_mode)
{
    m_toggle_jack = a_mode;
}

bool perform::get_toggle_jack()
{
    return m_toggle_jack;
}

bool perform::is_jack_running()
{
    return m_jack_running;
}

void perform::set_follow_transport(bool a_set)
{
    m_follow_transport = a_set;
}

bool perform::get_follow_transport()
{
    return m_follow_transport;
}

void perform::toggle_follow_transport()
{
    set_follow_transport(!m_follow_transport);
}

void perform::set_left_tick( long a_tick )
{
    m_left_tick = a_tick;
    m_starting_tick = a_tick;

    if ( m_left_tick >= m_right_tick )
        m_right_tick = m_left_tick + c_ppqn * 4;
}

long perform::get_left_tick()
{
    return m_left_tick;
}

void perform::set_starting_tick( long a_tick )
{
    m_starting_tick = a_tick;
}

long perform::get_starting_tick()
{
    return m_starting_tick;
}

void perform::set_right_tick( long a_tick )
{
    if ( a_tick >= c_ppqn * 4 )
    {
        m_right_tick = a_tick;

        if ( m_right_tick <= m_left_tick )
        {
            m_left_tick = m_right_tick - c_ppqn * 4;
            m_starting_tick = m_left_tick;
        }
    }
}

long perform::get_right_tick()
{
    return m_right_tick;
}

void perform::add_track( track *a_track, int a_pref )
{
    /* check for preferred */
    if ( a_pref < c_max_track &&
            is_active_track(a_pref) == false &&
            a_pref >= 0 )
    {
        m_tracks[a_pref] = a_track;
        set_active(a_pref, true);

    }
    else
    {
        for (int i=a_pref; i< c_max_track; i++ )
        {
            if ( is_active_track(i) == false )
            {
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

void perform::set_bpm(int a_bpm)
{
    if ( a_bpm < 20 )  a_bpm = 20;
    if ( a_bpm > 500 ) a_bpm = 500;

    if ( ! (m_jack_running && global_is_running ))
    {
        m_master_bus.set_bpm( a_bpm );
    }
}

int  perform::get_bpm( )
{
    return  m_master_bus.get_bpm( );
}

void perform::set_bp_measure(int a_bp_mes)
{
    m_bp_measure = a_bp_mes;
}

int perform::get_bp_measure( )
{
    return m_bp_measure;
}

void perform::set_bw(int a_bw)
{
    m_bw = a_bw;
}

int perform::get_bw( )
{
    return m_bw;
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
            !is_track_in_edit(a_num) )
    {
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
    for (int i = 0; i < c_max_track; i++)
    {
        if (is_active_track(i))
        {
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

    //printf( "play [%ld]\n", a_tick );

    m_tick = a_tick;
    for (int i=0; i< c_max_track; i++ )
    {
        if ( is_active_track(i) )
        {
            assert( m_tracks[i] );
            m_tracks[i]->play( a_tick, m_playback_mode );
        }
    }

    /* flush the bus */
    m_master_bus.flush();
}

void perform::set_orig_ticks( long a_tick  )
{
    for (int i=0; i< c_max_track; i++ )
    {
        if ( is_active_track(i) == true )
        {
            assert( m_tracks[i] );
            m_tracks[i]->set_orig_tick( a_tick );
        }
    }
}

void perform::clear_track_triggers( int a_trk  )
{
    if ( is_active_track(a_trk) == true )
    {
        assert( m_tracks[a_trk] );
        m_tracks[a_trk]->clear_triggers( );
    }
}

void perform::move_triggers( bool a_direction )
{
    if ( m_left_tick < m_right_tick )
    {
        long distance = m_right_tick - m_left_tick;

        for (int i=0; i< c_max_track; i++ )
        {
            if ( is_active_track(i) == true )
            {
                assert( m_tracks[i] );
                m_tracks[i]->move_triggers( m_left_tick, distance, a_direction );
            }
        }
    }
}

// collapse and expand - all tracks
void perform::push_trigger_undo()
{
    m_mutex.lock();
    for (int i=0; i< c_max_track; i++ )
    {
        if ( is_active_track(i) == true )
        {
            assert( m_tracks[i] );
            m_tracks[i]->push_trigger_undo( );
        }
    }
    undo_type a_undo;
    a_undo.track = -1; // all tracks
    a_undo.type = c_undo_collapse_expand;

    undo_vect.push_back(a_undo);
    redo_vect.clear();
    global_seqlist_need_update = true;
    set_have_undo();
    set_have_redo();
    m_mutex.unlock();
}

void perform::pop_trigger_undo()
{
    m_mutex.lock();
    for (int i=0; i< c_max_track; i++ )
    {
        if ( is_active_track(i) == true )
        {
            assert( m_tracks[i] );
            m_tracks[i]->pop_trigger_undo( );
        }
    }

    undo_type a_undo;
    a_undo.track = -1; // all tracks
    a_undo.type = c_undo_collapse_expand;

    undo_vect.pop_back();
    redo_vect.push_back(a_undo);
    global_seqlist_need_update = true;
    set_have_redo();
    m_mutex.unlock();
}

void perform::pop_trigger_redo()
{
    m_mutex.lock();
    for (int i=0; i< c_max_track; i++ )
    {
        if ( is_active_track(i) == true )
        {
            assert( m_tracks[i] );
            m_tracks[i]->pop_trigger_redo( );
        }
    }

    undo_type a_undo;
    a_undo.track = -1; // all tracks
    a_undo.type = c_undo_collapse_expand;

    redo_vect.pop_back();
    undo_vect.push_back(a_undo);
    global_seqlist_need_update = true;
    set_have_undo();
    m_mutex.unlock();
}

// single track items
void perform::push_trigger_undo( int a_track )
{
    m_mutex.lock();
    if ( is_active_track(a_track) == true )
    {
        assert( m_tracks[a_track] );
        m_tracks[a_track]->push_trigger_undo( );
    }
    undo_type a_undo;
    a_undo.track = a_track;
    a_undo.type = c_undo_trigger;

    undo_vect.push_back(a_undo);
    redo_vect.clear();
    global_seqlist_need_update = true;
    set_have_undo();
    set_have_redo();
    m_mutex.unlock();
}

void perform::pop_trigger_undo( int a_track )
{
    m_mutex.lock();
    if ( is_active_track(a_track) == true )
    {
        assert( m_tracks[a_track] );
        m_tracks[a_track]->pop_trigger_undo( );
    }

    undo_type a_undo;
    a_undo.track = a_track;
    a_undo.type = c_undo_trigger;

    undo_vect.pop_back();
    redo_vect.push_back(a_undo);
    global_seqlist_need_update = true;
    set_have_redo();
    m_mutex.unlock();
}

void perform::pop_trigger_redo( int a_track )
{
    m_mutex.lock();
    if ( is_active_track(a_track) == true )
    {
        assert( m_tracks[a_track] );
        m_tracks[a_track]->pop_trigger_redo( );
    }

    undo_type a_undo;
    a_undo.track = a_track;
    a_undo.type = c_undo_trigger;

    redo_vect.pop_back();
    undo_vect.push_back(a_undo);
    global_seqlist_need_update = true;
    set_have_undo();
    m_mutex.unlock();
}

// tracks - merge sequence, cross track trigger, track cut, track paste
void perform::push_track_undo( int a_track )
{
    m_mutex.lock();
    if ( is_active_track(a_track) == true ) // merge, cross track, track cut
    {
        assert( m_tracks[a_track] );

        m_undo_tracks[m_undo_track_count] = *(m_tracks[a_track]);
        m_undo_track_count++;
    }
    else // track paste - set name to null for later delete
    {
        m_undo_tracks[m_undo_track_count].m_is_NULL = true;
        m_undo_track_count++;
    }
    undo_type a_undo;
    a_undo.track = a_track;
    a_undo.type = c_undo_track;

    undo_vect.push_back(a_undo);
    redo_vect.clear();
    set_have_undo();
    set_have_redo();
    global_seqlist_need_update = true;
    m_mutex.unlock();
}

void perform::pop_track_undo( int a_track )
{
    m_mutex.lock();
    if ( is_active_track(a_track) == true ) // cross track, merge seq, paste track
    {
        if(is_track_in_edit(a_track)) // don't allow since track will be changed
            return;

        assert( m_tracks[a_track] );

        /* The track edit items should not be changed on redo/undo */
        bool mute = m_tracks[a_track]->get_song_mute();
        std::string name = m_tracks[a_track]->get_name();
        unsigned char midi_channel = m_tracks[a_track]->get_midi_channel();
        char midi_bus = m_tracks[a_track]->get_midi_bus();
        bool transpose = m_tracks[a_track]->get_transposable();
        /* end track edit items */

        m_redo_tracks[m_redo_track_count] = *(get_track(a_track ));

        delete_track(a_track); // must delete or junk leftover on copy - also for paste track undo

        if(!m_undo_tracks[m_undo_track_count - 1].m_is_NULL) // cross track, merge track
        {
            new_track( a_track );
            *(get_track( a_track )) = m_undo_tracks[m_undo_track_count - 1];
            assert( m_tracks[a_track] );

            /* reset the track edit items */
            get_track(a_track)->set_song_mute(mute);
            get_track(a_track)->set_name(name);
            get_track(a_track)->set_midi_channel(midi_channel);
            get_track(a_track)->set_midi_bus(midi_bus);
            get_track(a_track)->set_transposable(transpose);
            /* end reset track items */

            get_track( a_track )->set_dirty();
        }
    }
    else  // cut track - set NULL name to redo, create new track for undo
    {
        m_redo_tracks[m_redo_track_count].m_is_NULL = true;

        new_track( a_track  );
        assert( m_tracks[a_track] );
        *(get_track(a_track )) = m_undo_tracks[m_undo_track_count - 1];
        get_track( a_track )->set_dirty();
    }

    m_undo_track_count--;
    m_redo_track_count++;

    undo_type a_undo;
    a_undo.track = a_track;
    a_undo.type = c_undo_track;

    undo_vect.pop_back();
    redo_vect.push_back(a_undo);
    global_seqlist_need_update = true; // in case track set_dirty() is not triggered above
    set_have_redo();
    m_mutex.unlock();
}

void perform::pop_track_redo( int a_track )
{
    m_mutex.lock();
    if ( is_active_track(a_track) == true ) // cross track, merge seq, cut track
    {
        if(is_track_in_edit(a_track)) // don't allow since track will be changed
            return;

        assert( m_tracks[a_track] );

        /* The track edit items should not be changed on redo/undo */
        bool mute = m_tracks[a_track]->get_song_mute();
        std::string name = m_tracks[a_track]->get_name();
        unsigned char midi_channel = m_tracks[a_track]->get_midi_channel();
        char midi_bus = m_tracks[a_track]->get_midi_bus();
        bool transpose = m_tracks[a_track]->get_transposable();
        /* end track edit items */

        m_undo_tracks[m_undo_track_count] = *(get_track(a_track ));

        delete_track(a_track); // must delete or junk leftover on copy - also for cut track redo

        if(!m_redo_tracks[m_redo_track_count - 1].m_is_NULL) // cross track, merge track
        {
            new_track( a_track );
            *(get_track( a_track )) = m_redo_tracks[m_redo_track_count - 1];

            /* reset the track edit items */
            get_track(a_track)->set_song_mute(mute);
            get_track(a_track)->set_name(name);
            get_track(a_track)->set_midi_channel(midi_channel);
            get_track(a_track)->set_midi_bus(midi_bus);
            get_track(a_track)->set_transposable(transpose);
            /* end reset track items */

            get_track( a_track )->set_dirty();
        }
    }
    else   // paste track - set NULL to undo, create track for redo
    {
        m_undo_tracks[m_undo_track_count].m_is_NULL = true;

        new_track( a_track  );
        assert( m_tracks[a_track] );
        *(get_track(a_track )) = m_redo_tracks[m_redo_track_count - 1];
        get_track( a_track )->set_dirty();
    }

    m_undo_track_count++;
    m_redo_track_count--;

    undo_type a_undo;
    a_undo.track = a_track;
    a_undo.type = c_undo_track;

    redo_vect.pop_back();
    undo_vect.push_back(a_undo);
    global_seqlist_need_update = true; // in case track set_dirty() is not triggered above
    set_have_undo();
    m_mutex.unlock();
}

void
perform::push_perf_undo()
{
    m_mutex.lock();
    for(int i = 0; i < c_max_track; i++)
    {
        if ( is_active_track(i) == true )
        {
            undo_perf[m_undo_perf_count].perf_tracks[i] = *get_track(i);
        }
        else
        {
            undo_perf[m_undo_perf_count].perf_tracks[i].m_is_NULL = true;
        }
    }

    m_undo_perf_count++;

    undo_type a_undo;
    a_undo.track = -1;
    a_undo.type = c_undo_perf;

    undo_vect.push_back(a_undo);
    redo_vect.clear();
    set_have_undo();
    set_have_redo();
    m_mutex.unlock();
}

void
perform::pop_perf_undo()
{
    m_mutex.lock();
    for(int i = 0; i < c_max_track; i++)//now delete and replace
    {
        if ( is_active_track(i) == true )
        {
            redo_perf[m_redo_perf_count].perf_tracks[i] = *get_track(i);// push redo

            delete_track(i);
            if(!undo_perf[m_undo_perf_count-1].perf_tracks[i].m_is_NULL)
            {
                new_track(i);
                *get_track(i) = undo_perf[m_undo_perf_count-1].perf_tracks[i];
                get_track(i)->set_dirty();
            }
        }
        else
        {
            redo_perf[m_redo_perf_count].perf_tracks[i].m_is_NULL = true; // push redo

            if(!undo_perf[m_undo_perf_count-1].perf_tracks[i].m_is_NULL)
            {
                new_track(i);
                *get_track(i) = undo_perf[m_undo_perf_count-1].perf_tracks[i];
                get_track(i)->set_dirty();
            }
        }
    }

    m_undo_perf_count--;
    m_redo_perf_count++;

    undo_type a_undo;
    a_undo.track = -1;
    a_undo.type = c_undo_perf;

    undo_vect.pop_back();
    redo_vect.push_back(a_undo);
    global_seqlist_need_update = true; // in case track set_dirty() is not triggered above
    set_have_redo();
    m_mutex.unlock();
}

void
perform::pop_perf_redo()
{
    m_mutex.lock();
    for(int i = 0; i < c_max_track; i++)//now delete and replace
    {
        if ( is_active_track(i) == true )
        {
            undo_perf[m_undo_perf_count].perf_tracks[i] = *get_track(i); // push undo

            delete_track(i);
            if(!redo_perf[m_redo_perf_count-1].perf_tracks[i].m_is_NULL)
            {
                new_track(i);
                *get_track(i) = redo_perf[m_redo_perf_count-1].perf_tracks[i];
                get_track(i)->set_dirty();
            }
        }
        else
        {
            undo_perf[m_undo_perf_count].perf_tracks[i].m_is_NULL = true; // push undo

            if(!redo_perf[m_redo_perf_count-1].perf_tracks[i].m_is_NULL)
            {
                new_track(i);
                *get_track(i) = redo_perf[m_redo_perf_count-1].perf_tracks[i];
                get_track(i)->set_dirty();
            }
        }
    }

    m_undo_perf_count++;
    m_redo_perf_count--;

    undo_type a_undo;
    a_undo.track = -1;
    a_undo.type = c_undo_perf;

    redo_vect.pop_back();
    undo_vect.push_back(a_undo);
    global_seqlist_need_update = true; // in case track set_dirty() is not triggered above
    set_have_undo();
    m_mutex.unlock();
}

void
perform::check_max_undo_redo()
{
    m_mutex.lock();
    if(m_undo_track_count > 100 || m_redo_track_count > 100 ||
            m_undo_perf_count > 40 || m_redo_perf_count > 40 )
    {
        m_undo_track_count = 0;
        m_redo_track_count = 0;
        m_undo_perf_count = 0;
        m_redo_perf_count = 0;
        undo_vect.clear();
        redo_vect.clear();
        for(int i = 0; i < c_max_track; i++)
        {
            if ( is_active_track(i) == true )
                m_tracks[i]->clear_trigger_undo_redo();
        }
    }
    m_mutex.unlock();
}

void
perform::set_have_undo()
{
    check_max_undo_redo();
    if(undo_vect.size() > 0)
    {
        m_have_undo = true;
        //printf("m_undo_track_count[%d]\n",m_undo_track_count);
    }
    else
    {
        m_have_undo = false;
    }
    global_is_modified = true; // once true, always true unless file save
}

void
perform::set_have_redo()
{
    check_max_undo_redo();
    if(redo_vect.size() > 0)
    {
        m_have_redo = true;
    }
    else
    {
        m_have_redo = false;
    }
}

/* copies between L and R -> R */
void perform::copy_triggers( )
{
    if ( m_left_tick < m_right_tick )
    {
        long distance = m_right_tick - m_left_tick;

        for (int i=0; i< c_max_track; i++ )
        {
            if ( is_active_track(i) == true )
            {
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
#endif // JACK_SUPPORT
}

void perform::stop_jack(  )
{
    //printf( "perform::stop_jack()\n" );
#ifdef JACK_SUPPORT
    if( m_jack_running )
        jack_transport_stop (m_jack_client);
#endif // JACK_SUPPORT
}

void perform::position_jack( bool a_state, long a_tick )
{
    //printf( "perform::position_jack()\n" );

#ifdef JACK_SUPPORT

    long current_tick = 0;

    if(a_state) // master in song mode
    {
        current_tick = a_tick;
    }

    current_tick *= 10;

    /*  This jack_frame calculation is all that is needed to change jack position
        The BBT calc below can be sent but will be overridden by the first call to
        jack_timebase_callback() of any master set. If no master is set, then the
        BBT will display the new position but will not change even if the transport
        is rolling. I'm going with the KISS (keep it simple stupid) rule here.
        There is no need to send BBT on position change - the fact that the function
        jack_transport_locate() exists and only uses the frame position is proof that
        BBT is not needed!
     */
    int ticks_per_beat = c_ppqn * 10; // 192 * 10 = 1920
    int beats_per_minute =  m_master_bus.get_bpm();

    uint64_t tick_rate = ((uint64_t)m_jack_frame_rate * current_tick * 60.0);
    long tpb_bpm = ticks_per_beat * beats_per_minute;
    uint64_t jack_frame = tick_rate / tpb_bpm;

    jack_transport_locate(m_jack_client,jack_frame);

    /* The below calculations for BBT are not necessary to change jack position!!! */

//    jack_position_t pos;

//    pos.valid = JackPositionBBT;
//    pos.beats_per_bar = m_bp_measure;
//    pos.beat_type = m_bw;
//    pos.ticks_per_beat = c_ppqn * 10; // 192 * 10 = 1920
//    pos.beats_per_minute =  m_master_bus.get_bpm();

    /* Compute BBT info from frame number.  This is relatively
     * simple here, but would become complex if we supported tempo
     * or time signature changes at specific locations in the
     * transport timeline. */

//    current_tick *= 10;

    /* this BBT stuff is not necessary for position change */
//    pos.bar = (int32_t) (current_tick / (long) pos.ticks_per_beat
//                         / pos.beats_per_bar);
//    pos.beat = (int32_t) ((current_tick / (long) pos.ticks_per_beat) % 4);
//    pos.tick = (int32_t) (current_tick % (c_ppqn * 10));

//    pos.bar_start_tick = pos.bar * pos.beats_per_bar * pos.ticks_per_beat;
//    pos.frame_rate =  m_jack_frame_rate; // comes from init_jack()

    /* this calculates jack frame to put into pos.frame
       it is what really matters for position change */

//    uint64_t tick_rate = ((uint64_t)pos.frame_rate * current_tick * 60.0);
//    long tpb_bpm = pos.ticks_per_beat * pos.beats_per_minute;
//    pos.frame = tick_rate / tpb_bpm;

    //pos.frame = (jack_nframes_t) ( (current_tick *  m_jack_frame_rate * 60.0)
    //        / (pos.ticks_per_beat * pos.beats_per_minute) );

    /*
       ticks * 10 = jack ticks;
       jack ticks / ticks per beat = num beats;
       num beats / beats per minute = num minutes
       num minutes * 60 = num seconds
       num secords * frame_rate  = frame */

//    pos.bar++;
//    pos.beat++;

    //printf( "position bbb[%d:%d:%4d]\n", pos.bar, pos.beat, pos.tick );

    //jack_transport_reposition( m_jack_client, &pos );

#endif // JACK_SUPPORT
}

void perform::start(bool a_state)
{
    if (m_jack_running)
    {
        return;
    }

    inner_start(a_state);
}

/*
    stop(); This function's sole purpose was to prevent inner_stop() from being called
    internally when jack was running...potentially twice?. inner_stop() was called by output_func()
    when jack sent a JackTransportStopped message. If seq42 initiated the stop, then
    stop_jack() was called which then triggered the JackTransportStopped message
    to output_func() which then triggered the bool stop_jack to call inner_stop().
    The output_func() call to inner_stop() is only necessary when some other jack
    client sends a jack_transport_stop message to jack, not when it is initiated
    by seq42.  The method of relying on jack to call inner_stop() when internally initiated
    caused a (very) obscure apparent freeze if you press and hold the start/stop key
    if set to toggle. This occurs because of the delay between JackTransportStarting and
    JackTransportStopped if both triggered in rapid succession by holding the toggle key
    down.  The variable global_is_running gets set false by a delayed inner_stop()
    from jack after the start (true) is already sent. This means the global is set to true
    when jack is actually off (false). Any subsequent presses to the toggle key send a
    stop message because the global is set to true. Because jack is not running,
    output_func() is not running to send the inner_stop() call which resets the global
    to false. Thus an apparent freeze as the toggle key endlessly sends a stop, but
    inner_stop() never gets called to reset. Whoo! So, to fix this we just need to call
    inner_stop() directly rather than wait for jack to send a delayed stop, only when
    running. This makes the whole purpose of this stop() function unneeded. The check
    for m_jack_running is commented out and this function could be removed. It is
    being left for future generations to ponder!!!
*/
void perform::stop()
{
//    if (m_jack_running)
//    {
//        return;
//   }

    inner_stop();
}

void perform::inner_start(bool a_state)
{
    m_condition_var.lock();

    if (!global_is_running)
    {
        set_playback_mode( a_state );

        if (a_state)
            off_sequences();

        global_is_running = true;
        m_condition_var.signal();
    }

    m_condition_var.unlock();
}

void perform::inner_stop()
{
    global_is_running = false;
    //off_sequences();
    reset_sequences();
    m_usemidiclock = false;
}

void perform::off_sequences()
{
    for (int i = 0; i < c_max_track; i++)
    {
        if (is_active_track(i))
        {
            assert(m_tracks[i]);
            m_tracks[i]->set_playing_off();
        }
    }
}

void perform::all_notes_off()
{
    for (int i=0; i< c_max_track; i++)
    {
        if (is_active_track(i))
        {
            assert(m_tracks[i]);
            m_tracks[i]->off_playing_notes();
        }
    }
    /* flush the bus */
    m_master_bus.flush();
}

void perform::reset_sequences()
{
    for (int i=0; i< c_max_track; i++)
    {
        if (is_active_track(i))
        {
            assert( m_tracks[i] );
            m_tracks[i]->reset_sequences(m_playback_mode);
        }
    }
    /* flush the bus */
    m_master_bus.flush();
}

void perform::launch_output_thread()
{
    int err;

    err = pthread_create(&m_out_thread, NULL, output_thread_func, this);
    if (err != 0)
    {
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
    if (err != 0)
    {
        /*TODO: error handling*/
    }
    else
        m_in_thread_launched = true;
}

long perform::get_max_trigger()
{
    long ret = 0, t;

    for (int i=0; i< c_max_track; i++ )
    {
        if ( is_active_track(i) == true )
        {
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

    struct sched_param schp;
    /*
     * set the process to realtime privs
     */

    if ( global_priority )
    {
        memset(&schp, 0, sizeof(sched_param));
        schp.sched_priority = 1;

#ifndef __WIN32__
        // Not in MinGW RCB
        if (sched_setscheduler(0, SCHED_FIFO, &schp) != 0)
        {
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

int jack_process_callback(jack_nframes_t nframes, void* arg)
{
    return 0;
}

int jack_sync_callback(jack_transport_state_t state,
                       jack_position_t *pos, void *arg)
{
    //printf( "jack_sync_callback() " );

    perform *p = (perform *) arg;

//    p->m_jack_frame_current = jack_get_current_transport_frame(p->m_jack_client);

//    p->m_jack_tick =
//        p->m_jack_frame_current *
//        p->m_jack_pos.ticks_per_beat *
//        p->m_jack_pos.beats_per_minute / (p->m_jack_pos.frame_rate * 60.0);

//    p->m_jack_frame_last = p->m_jack_frame_current;

    p->m_jack_transport_state_last =
        p->m_jack_transport_state =
            state;

    switch (state)
    {
    case JackTransportStopped:
        //printf( "[JackTransportStopped]\n" );
        break;
    case JackTransportRolling:
        //printf( "[JackTransportRolling]\n" );
        break;
    case JackTransportStarting:
        //printf( "[JackTransportStarting]\n" );
        p->inner_start( global_song_start_mode );
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

#ifdef JACK_SESSION

bool perform::jack_session_event()
{
    Glib::ustring fname( m_jsession_ev->session_dir );

    fname += "file.s42";

    Glib::ustring cmd( "seq42 \"${SESSION_DIR}file.s42\" --jack_session_uuid " );
    cmd += m_jsession_ev->client_uuid;

    save(fname);

    m_jsession_ev->command_line = strdup( cmd.c_str() );

    jack_session_reply( m_jack_client, m_jsession_ev );

    if( m_jsession_ev->type == JackSessionSaveAndQuit )
        Gtk::Main::quit();

    jack_session_event_free (m_jsession_ev);

    return false;
}

void jack_session_callback(jack_session_event_t *event, void *arg )
{
    perform *p = (perform *) arg;
    p->m_jsession_ev = event;
    Glib::signal_idle().connect( sigc::mem_fun( *p, &perform::jack_session_event) );
}

#endif // JACK_SESSION
#endif // JACK_SUPPORT

void perform::output_func()
{
    while (m_outputing)
    {
        //printf ("waiting for signal\n");

        m_condition_var.lock();

        while (!global_is_running)
        {
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
#endif // __WIN32__

        /* tick and tick fraction */
        double current_tick   = 0.0;
        double total_tick   = 0.0;
        long clock_tick = 0;
        long delta_tick_frac = 0;

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

        /* A note about jack:  For some reason the position_jack() info does not seem to register with some
        other programs (non-timeline) when sent without jack running. Thus the need for the entries below.
        This is somewhat redundant since the position was previously sent by start_playing(). The jack_position()
        set in start_playing() was set for cosmetic reasons to stop start up flicker when the transport
        position is different from the left frame start position and there was a brief flash of the transport
        line before being set correctly here. The below position_jack() settings also serves to allow seq42
        to correctly start as master when another program is used to start the transport rolling. */

        if(m_jack_running && m_jack_master && m_playback_mode) // song mode master start left tick marker
            position_jack(true, m_left_tick);
        if(m_jack_running && m_jack_master && !m_playback_mode)// live mode master start at zero
            position_jack(false, 0);

#endif // JACK_SUPPORT

        for( int i=0; i<100; i++ )
        {
            stats_all[i] = 0;
            stats_clock[i] = 0;
        }

        /* if we are in the performance view, we care
           about starting from the offset */
        if ( m_playback_mode && !m_jack_running)
        {
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
#endif // __WIN32__

        while( global_is_running )
        {
            /************************************

              Get delta time ( current - last )
              Get delta ticks from time
              Add to current_ticks
              Compute prebuffer ticks
              play from current tick to prebuffer

             **************************************/

            if ( global_stats )
            {
#ifndef __WIN32__
                clock_gettime(CLOCK_REALTIME, &stats_loop_start);
#else
                stats_loop_start = timeGetTime();
#endif // __WIN32__
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
#endif // __WIN32__

            /* delta time to ticks */
            /* bpm */
            int bpm  = m_master_bus.get_bpm();

            /* get delta ticks, delta_ticks_f is in 1000th of a tick */
            long long delta_tick_num = bpm * ppqn * delta_us + delta_tick_frac;
            long long delta_tick_denom = 60000000;
            long delta_tick = (long)(delta_tick_num / delta_tick_denom);
            delta_tick_frac = (long)(delta_tick_num % delta_tick_denom);

            if (m_usemidiclock)
            {
                delta_tick = m_midiclocktick;
                m_midiclocktick = 0;
            }
            if (0 <= m_midiclockpos)
            {
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

            if ( m_jack_running )
            {
                init_clock = false;

                m_jack_transport_state = jack_transport_query( m_jack_client, &m_jack_pos );
                m_jack_frame_current =  jack_get_current_transport_frame( m_jack_client );

                /*
                    Another note about jack....
                    If another jack client is supplying tempo/BBT info that is different from seq42 (as Master),
                    the perfroll grid will be incorrect. Perfroll uses internal temp/BBT and cannot update on
                    the fly. Even if seq42 could support tempo/BBT changes, all info would have to be available
                    before the transport start, to work. For this reason, the tempo/BBT info will be plugged from
                    the seq42 internal settings here... always. This is the method used by probably all other jack
                    clients with some sort of time-line. The jack API indicates that BBT is optional and AFIK,
                    other sequencers only use frame & frame_rate from jack for internal calculations. The tempo
                    and BBT info is always internal. Also, if there is no Master set, then we would need to plug
                    it here to follow the jack frame anyways.
                */

                m_jack_pos.beats_per_bar = m_bp_measure;
                m_jack_pos.beat_type = m_bw;
                m_jack_pos.ticks_per_beat = c_ppqn * 10;
                m_jack_pos.beats_per_minute =  m_master_bus.get_bpm();

                if ( m_jack_transport_state_last  ==  JackTransportStarting &&
                        m_jack_transport_state       == JackTransportRolling )
                {
                    m_jack_frame_last = m_jack_frame_current;

                    //printf ("[Start Playback]\n" );
                    dumping = true;
                    m_jack_tick =
                        m_jack_pos.frame *
                        m_jack_pos.ticks_per_beat *
                        m_jack_pos.beats_per_minute / (m_jack_frame_rate * 60.0);

                    /* convert ticks */
                    jack_ticks_converted =
                        m_jack_tick * ((double) c_ppqn /
                                       (m_jack_pos.ticks_per_beat *
                                        m_jack_pos.beat_type / 4.0  ));

                    set_orig_ticks( (long) jack_ticks_converted );
                    current_tick = clock_tick = total_tick = jack_ticks_converted_last = jack_ticks_converted;
                    init_clock = true;

                    if ( m_looping && m_playback_mode )
                    {
                        //printf( "left[%lf] right[%lf]\n", (double) get_left_tick(), (double) get_right_tick() );

                        if ( current_tick >= get_right_tick() )
                        {
                            while ( current_tick >= get_right_tick() )
                            {
                                double size = get_right_tick() - get_left_tick();
                                current_tick = current_tick - size;

                                //printf( "> current_tick[%lf]\n", current_tick );
                            }
                               // reset_sequences();
                            off_sequences();
                            set_orig_ticks( (long)current_tick );
                        }
                    }
                }

                if ( m_jack_transport_state_last  ==  JackTransportRolling &&
                        m_jack_transport_state  == JackTransportStopped )
                {
                    m_jack_transport_state_last = JackTransportStopped;
                    //printf ("[Stop Playback]\n" );
                    jack_stopped = true;
                }

                //-----  Jack transport is Rolling Now ---------

                /* transport is in a sane state if dumping == true */
                if ( dumping )
                {
                    m_jack_frame_current =  jack_get_current_transport_frame( m_jack_client );

                    //printf( " frame[%7d]\n", m_jack_pos.frame );
                    //printf( " current_transport_frame[%7d]", m_jack_frame_current );

                    // if we are moving ahead
                    if ( (m_jack_frame_current > m_jack_frame_last))
                    {
                        m_jack_tick +=
                            (m_jack_frame_current - m_jack_frame_last)  *
                            m_jack_pos.ticks_per_beat *
                            m_jack_pos.beats_per_minute / (m_jack_frame_rate * 60.0);

                        //printf ( "m_jack_tick += (m_jack_frame_current[%lf] - m_jack_frame_last[%lf]) *\n",
                        //        (double) m_jack_frame_current, (double) m_jack_frame_last );
                        //printf(  "m_jack_pos.ticks_per_beat[%lf] * m_jack_pos.beats_per_minute[%lf] / \n(m_jack_frame_rate[%lf] * 60.0\n", (double) m_jack_pos.ticks_per_beat, (double) m_jack_pos.beats_per_minute, (double) m_jack_frame_rate);

                        m_jack_frame_last = m_jack_frame_current;
                    }

                    /* convert ticks */
                    jack_ticks_converted =
                        m_jack_tick * ((double) c_ppqn /
                                       (m_jack_pos.ticks_per_beat *
                                        m_jack_pos.beat_type / 4.0  ));

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

                } /* end if dumping / sane state */
            } /* if jack running */
            else
            {
#endif // JACK_SUPPORT
                /* default if jack is not compiled in, or not running */
                /* add delta to current ticks */
                clock_tick     += delta_tick;
                current_tick   += delta_tick;
                total_tick     += delta_tick;
                dumping = true;

            /* if we reposition key-p from perfroll
               then reset to adjusted starting  */
            if ( m_playback_mode && !m_jack_running && !m_usemidiclock && m_reposition)
            {
                current_tick = m_starting_tick;
                set_orig_ticks( m_starting_tick );
                m_reposition = false;
            }

#ifdef JACK_SUPPORT
            }
#endif // JACK_SUPPORT

            /* init_clock will be true when we run for the first time, or
             * as soon as jack gets a good lock on playback */

            if (init_clock)
            {
                m_master_bus.init_clock( clock_tick );
                init_clock = false;
            }

            if (dumping)
            {
                if ( m_looping && m_playback_mode )
                {
                    static bool jack_position_once = false;
                    if ( current_tick >= get_right_tick() )
                    {
                        //printf("current_tick [%f]: right_tick [%ld]\n", current_tick, get_right_tick());
#ifdef JACK_SUPPORT
                        if(m_jack_running && m_jack_master && !jack_position_once)
                        {
                            position_jack(true, m_left_tick);
                            jack_position_once = true;
                        }
#endif // JACK_SUPPORT
                        double leftover_tick = current_tick - (get_right_tick());

                        play( get_right_tick() - 1 );
                        reset_sequences();

                        set_orig_ticks( get_left_tick() );
                        current_tick = (double) get_left_tick() + leftover_tick;
                    }
                    else
                        jack_position_once = false;
                }
                /* play */
                play( (long) current_tick );
                //printf( "play[%f]\n", current_tick );

                /* midi clock */
                m_master_bus.clock( clock_tick );

                if ( global_stats )
                {
                    while ( stats_total_tick <= total_tick )
                    {
                        /* was there a tick ? */
                        if ( stats_total_tick % (c_ppqn / 24) == 0 )
                        {
#ifndef __WIN32__
                            long current_us = (current.tv_sec * 1000000) + (current.tv_nsec / 1000);
#else
                            long current_us = current * 1000;
#endif // __WIN32__
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
#endif // __WIN32__

            /* now, we want to trigger every c_thread_trigger_width_ms,
               and it took us delta_us to play() */

            delta_us = (c_thread_trigger_width_ms * 1000) - elapsed_us;
            //printf( "sleeping_us[%ld]\n", delta_us );

            /* check midi clock adjustment */

            double next_total_tick = (total_tick + (c_ppqn / 24.0));
            double next_clock_delta   = (next_total_tick - total_tick - 1);

            double next_clock_delta_us =  (( next_clock_delta ) * 60000000.0f / c_ppqn  / bpm );

            if ( next_clock_delta_us < (c_thread_trigger_width_ms * 1000.0f * 2.0f) )
            {
                delta_us = (long)next_clock_delta_us;
            }

#ifndef __WIN32__
            if ( delta_us > 0.0 )
            {
                delta.tv_sec =  (delta_us / 1000000);
                delta.tv_nsec = (delta_us % 1000000) * 1000;

                //printf("sleeping() ");
                nanosleep( &delta, NULL );
            }
#else
            if ( delta_us > 0 )
            {
                delta =  (delta_us / 1000);

                //printf("           sleeping() [0x%x]\n", delta);
                Sleep(delta);
            }
#endif // __WIN32__

            else
            {
                if ( global_stats )
                    printf ("underrun\n" );
            }

            if ( global_stats )
            {
#ifndef __WIN32__
                clock_gettime(CLOCK_REALTIME, &stats_loop_finish);
#else
                stats_loop_finish = timeGetTime();
#endif // __WIN32__
            }

            if ( global_stats )
            {
#ifndef __WIN32__
                delta.tv_sec  =  (stats_loop_finish.tv_sec  - stats_loop_start.tv_sec  );
                delta.tv_nsec =  (stats_loop_finish.tv_nsec - stats_loop_start.tv_nsec );
                long delta_us = (delta.tv_sec * 1000000) + (delta.tv_nsec / 1000);
#else
                delta = stats_loop_finish - stats_loop_start;
                long delta_us = delta * 1000;
#endif // __WIN32__

                int index = delta_us / 100;
                if ( index >= 100  ) index = 99;

                stats_all[index]++;

                if ( delta_us > stats_max )
                    stats_max = delta_us;
                if ( delta_us < stats_min )
                    stats_min = delta_us;

                stats_avg += delta_us;
                stats_loop_index++;

                if ( stats_loop_index > 200 )
                {
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

        if (global_stats)
        {
            printf("\n\n-- trigger width --\n");
            for (int i=0; i<100; i++ )
            {
                printf( "[%3d][%8ld]\n", i * 100, stats_all[i] );
            }
            printf("\n\n-- clock width --\n" );
            int bpm  = m_master_bus.get_bpm();

            printf("optimal: [%d]us\n", ((c_ppqn / 24)* 60000000 / c_ppqn / bpm));

            for ( int i=0; i<100; i++ )
            {
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

    struct sched_param schp;
    /*
     * set the process to realtime privs
     */

    if ( global_priority )
    {
        memset(&schp, 0, sizeof(sched_param));
        schp.sched_priority = 1;

#ifndef __WIN32__
        // MinGW RCB
        if (sched_setscheduler(0, SCHED_FIFO, &schp) != 0)
        {
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

void perform::input_func()
{
    event ev;

    while (m_inputing)
    {
        if ( m_master_bus.poll_for_midi() > 0 )
        {
            do
            {
                if (m_master_bus.get_midi_event(&ev) )
                {
                    // Obey MidiTimeClock:
                    if (ev.get_status() == EVENT_MIDI_START)
                    {
                        stop();
                        start(false);
                        m_midiclockrunning = true;
                        m_usemidiclock = true;
                        m_midiclocktick = 0;
                        m_midiclockpos = 0;
                    }
                    // midi continue: start from current pos.
                    else if (ev.get_status() == EVENT_MIDI_CONTINUE)
                    {
                        m_midiclockrunning = true;
                        start(false);   // the false sets playback_mode to live
                        //m_usemidiclock = true;
                    }
                    else if (ev.get_status() == EVENT_MIDI_STOP)
                    {
                        // do nothing, just let the system pause
                        // since we're not getting ticks after the stop, the song wont advance
                        // when start is recieved, we'll reset the position, or
                        // when continue is recieved, we wont
                        m_midiclockrunning = false;
                        all_notes_off();
                    }
                    else if (ev.get_status() == EVENT_MIDI_CLOCK)
                    {
                        if (m_midiclockrunning)
                            m_midiclocktick += 8;
                    }
                    // not tested (todo: test it!)
                    else if (ev.get_status() == EVENT_MIDI_SONG_POS)
                    {
                        unsigned char a, b;
                        ev.get_data(&a, &b);
                        m_midiclockpos = ((int)a << 7) && (int)b;
                    }

                    /* filter system wide messages */
                    if (ev.get_status() <= EVENT_SYSEX)
                    {
                        if( global_showmidi)
                            ev.print();

                        /* is there a sequence set ? */
                        if (m_master_bus.is_dumping())
                        {
                            ev.set_timestamp(m_tick);

                            /* dump to it */
                            (m_master_bus.get_sequence())->stream_event(&ev);
                        }
                    }

                    if (ev.get_status() == EVENT_SYSEX)
                    {
                        if (global_showmidi)
                            ev.print();

                        if (global_pass_sysex)
                            m_master_bus.sysex(&ev);
                    }
                }
            }
            while (m_master_bus.is_more_input());
        }
    }
    pthread_exit(0);
}

#ifdef JACK_SUPPORT

/*
    This callback is only called by jack when seq42 is Master and is used to supply jack
    with BBT information based on frame position and frame_rate.
*/
void jack_timebase_callback(jack_transport_state_t state,
                            jack_nframes_t nframes,
                            jack_position_t *pos, int new_pos, void *arg)
{
    static double jack_tick;
    static jack_nframes_t current_frame;

    perform *p = (perform *) arg;
    current_frame = jack_get_current_transport_frame( p->m_jack_client );

    //printf( "jack_timebase_callback() [%d] [%d] [%d]\n", state, new_pos, current_frame);

    pos->valid = JackPositionBBT;
    pos->beats_per_bar = p->m_bp_measure;
    pos->beat_type = p->m_bw;
    pos->ticks_per_beat = c_ppqn * 10;
    pos->beats_per_minute = p->get_bpm();

    /* Compute BBT info from frame number.  This is relatively
     * simple here, but would become complex if we supported tempo
     * or time signature changes at specific locations in the
     * transport timeline. */

    jack_tick =
        (current_frame) *
        pos->ticks_per_beat *
        pos->beats_per_minute / ( p->m_jack_frame_rate* 60.0);

    long ptick = 0, pbeat = 0, pbar = 0;

    pbar  = (long) ((long) jack_tick / (pos->ticks_per_beat *  pos->beats_per_bar ));

    pbeat = (long) ((long) jack_tick % (long) (pos->ticks_per_beat *  pos->beats_per_bar ));
    pbeat = pbeat / (long) pos->ticks_per_beat;

    ptick = (long) jack_tick % (long) pos->ticks_per_beat;

    pos->bar = pbar + 1;
    pos->beat = pbeat + 1;
    pos->tick = ptick;
    pos->bar_start_tick = pos->bar * pos->beats_per_bar *
                          pos->ticks_per_beat;

    //printf( " bbb [%2d:%2d:%4d]\n", pos->bar, pos->beat, pos->tick );
}


void jack_shutdown(void *arg)
{
    perform *p = (perform *) arg;
    p->m_jack_running = false;

    printf("JACK shut down.\nJACK sync Disabled.\n");
}


void print_jack_pos( jack_position_t* jack_pos )
{
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

int main ()
{
    jack_client_t *client;

    /* become a new client of the JACK server */
    if ((client = jack_client_new("transport tester")) == 0)
    {
        fprintf(stderr, "jack server not running?\n");
        return 1;
    }

    jack_on_shutdown(client, jack_shutdown, 0);
    jack_set_sync_callback(client, jack_sync_callback, NULL);

    if (jack_activate(client))
    {
        fprintf(stderr, "cannot activate client");
        return 1;
    }

    bool cond = false; /* true if we want to fail if there is already a master */
    if (jack_set_timebase_callback(client, cond, timebase, NULL) != 0)
    {
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

#endif // 0


#endif // JACK_SUPPORT


bool
perform::save( const Glib::ustring& a_filename )
{
    m_mutex.lock();
    ofstream file (a_filename.c_str (), ios::out | ios::binary | ios::trunc);

    if (!file.is_open ()) return false;

    file.write((const char *) &c_file_version, sizeof(int));

    int bpm = get_bpm();
    file.write((const char *) &bpm, sizeof(int));

    int bp_measure = get_bp_measure(); // version 4
    file.write((const char *) &bp_measure, sizeof(int));

    int bw = get_bw();                 // version 4
    file.write((const char *) &bw, sizeof(int));

    int swing_amount8 = get_swing_amount8();
    file.write((const char *) &swing_amount8, sizeof(int));
    int swing_amount16 = get_swing_amount16();
    file.write((const char *) &swing_amount16, sizeof(int));

    int active_tracks = 0;
    for (int i=0; i< c_max_track; i++ )
    {
        if ( is_active_track(i) ) active_tracks++;
    }
    file.write((const char *) &active_tracks, sizeof(int));

    for (int i=0; i< c_max_track; i++ )
    {
        if ( is_active_track(i) )
        {
            int trk_idx = i; // file version 3
            file.write((const char *) &trk_idx, sizeof(int));

            if(! get_track(i)->save(&file))
            {
                return false;
            }
        }
    }

    file.close();

    m_mutex.unlock();
    return true;
}

bool
perform::load( const Glib::ustring& a_filename )
{
    ifstream file (a_filename.c_str (), ios::in | ios::binary);

    if (!file.is_open ()) return false;

    int version;
    file.read((char *) &version, sizeof(int));

    if (version < 0 || version > c_file_version)
    {
        fprintf(stderr, "Invalid file version detected: %d\n", version);
        return false;
    }

    int bpm;
    file.read((char *) &bpm, sizeof(int));
    set_bpm(bpm);

    int bp_measure = 4;
    if(version > 3)
    {
        file.read((char *) &bp_measure, sizeof(int));
    }

    set_bp_measure(bp_measure);

    int bw = 4;
    if(version > 3)
    {
        file.read((char *) &bw, sizeof(int));
    }

    set_bw(bw);

    int swing_amount8 = 0;
    if(version > 1)
    {
        file.read((char *) &swing_amount8, sizeof(int));
    }

    set_swing_amount8(swing_amount8);
    int swing_amount16 = 0;
    if(version > 1)
    {
        file.read((char *) &swing_amount16, sizeof(int));
    }

    set_swing_amount16(swing_amount16);

    int active_tracks;
    file.read((char *) &active_tracks, sizeof(int));

    int trk_index = 0;
    for (int i=0; i< active_tracks; i++ )
    {
        trk_index = i;
        if(version > 2)
        {
            file.read((char *) &trk_index, sizeof(int));
        }

        new_track(trk_index);
        if(! get_track(trk_index)->load(&file, version))
        {
            return false;
        }
    }

    file.close();
    return true;
}

void
perform::delete_unused_sequences()
{
    push_perf_undo();
    for (int i=0; i< c_max_track; i++ )
    {
        if ( is_active_track(i) )
        {
            get_track(i)->delete_unused_sequences();
        }
    }
}

void
perform::create_triggers()
{
    push_perf_undo();
    for (int i=0; i< c_max_track; i++ )
    {
        if ( is_active_track(i) )
        {
            get_track(i)->create_triggers(m_left_tick, m_right_tick);
        }
    }
}

void
perform::apply_song_transpose()
{
    for (int i=0; i< c_max_track; i++ )
    {
        if ( is_active_track(i) )
        {
            get_track(i)->apply_song_transpose();
        }
    }
}
