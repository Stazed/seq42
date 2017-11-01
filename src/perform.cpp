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
#include <gtkmm/messagedialog.h>

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

    m_setlist_stop_mark = false;
    m_setlist_mode = false;
    m_setlist_file = "";
    m_setlist_nfiles = 0;
    m_setlist_current_idx = 0;
    
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
    m_key_tap_bpm = GDK_F9;

    m_key_start  = GDK_space;
    m_key_stop   = GDK_Escape;
    m_key_forward   = GDK_f;
    m_key_rewind   = GDK_r;
    m_key_pointer   = GDK_p;

    m_key_loop   = GDK_quoteleft;
    m_key_song   = GDK_F1;
    m_key_jack   = GDK_F2;
    m_key_seqlist   = GDK_F3;
    m_key_follow_trans  = GDK_F4;
    
    //setlist next/prev keys:
    m_key_setlist_next = GDK_Right;
    m_key_setlist_prev = GDK_Left;
    
    m_jack_stop_tick = 0;
    m_reset_tempo_list = false;
    m_load_tempo_list = false;

    m_jack_running = false;
    m_toggle_jack = false;

    m_jack_master = false;

    m_out_thread_launched = false;
    m_in_thread_launched = false;

    m_playback_mode = false;
    m_follow_transport = true;

    m_bp_measure = 4;
    m_bw = 4;
    m_excell_FF_RW = 1.0;

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

                The "If TRUE" line seems to be the issue. Any client that uses jack_transport_locate()
                to change jack position will only send frame, not BBT or any other information.
                Screen dumps indicated that pos.frame_rate would contain garbage, while tempo and time
                signature fields would be zero filled. This resulted in the strange BBT calculations
                that display in qjackctl. So we are setting frame_rate here and just use m_jack_frame_rate
                for calculations instead of pos.frame_rate.
            */

            jack_on_shutdown( m_jack_client, jack_shutdown,(void *) this );

            /* now using jack_process_callback() ca. 7/10/16    */
            /*
                jack_set_sync_callback(m_jack_client, jack_sync_callback,
                                   (void *) this );
            */

            jack_set_process_callback(m_jack_client, jack_process_callback, (void *) this);

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

bool perform::clear_all()
{
    for (int i=0; i< c_max_track; i++ )
    {
        if ( is_active_track(i) )
        {
            if ( m_tracks[i] != NULL && is_track_in_edit(i) )
            {
                return false;
            }
        }
    }

    reset_sequences();

    for (int i=0; i< c_max_track; i++ )
    {
        if ( is_active_track(i) )
            delete_track( i );
    }

    undo_vect.clear();
    redo_vect.clear();
    set_have_undo();
    set_have_redo();
    m_list_play_marker.clear();
    m_list_total_marker.clear();
    m_list_no_stop_markers.clear();

    return true;
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

void perform::set_reposition(bool a_pos_type)
{
    m_reposition = a_pos_type;
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
            if(!m_reposition)    // allow to start at key-p position if set
                position_jack(global_song_start_mode, m_left_tick);     // for cosmetic reasons - to stop transport line flicker on start
        }
        start_jack( );
        start( global_song_start_mode );             // true for setting song m_playback_mode = true
    }
    else                         // live mode
    {
        if(m_jack_master)
            position_jack(global_song_start_mode, 0);   // for cosmetic reasons - to stop transport line flicker on start
        start( global_song_start_mode );
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
perform::FF_rewind()
{
    if(FF_RW_button_type == FF_RW_RELEASE)
        return;

    if(global_song_start_mode)                  // don't allow in live mode
    {
        long a_tick = 0;
        long measure_ticks = (c_ppqn * 4) * m_bp_measure / m_bw;
        measure_ticks /= 4;
        measure_ticks *= m_excell_FF_RW;

        if(FF_RW_button_type == FF_RW_REWIND)   // Rewind
        {
            a_tick = m_tick - measure_ticks;
            if(a_tick < 0)
            {
                a_tick = 0;
                FF_RW_button_type = FF_RW_RELEASE;  // in the case of sysex control
                                                    // user may forget to send second (release)
                                                    // sysex because it's all the way to beginning
                                                    // already. So the timeout keeps running.
                                                    // So we shut it off for them (by them I mean me).
            }
        }
        else                                    // Fast Forward
            a_tick = m_tick + measure_ticks;

        if(m_jack_running && global_song_start_mode)
        {
            position_jack(global_song_start_mode, a_tick);
        }
        else
        {
            set_starting_tick(a_tick);          // this will set progress line
            set_reposition();
        }
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

void perform::set_jack_stop_tick(long a_tick)
{
    m_jack_stop_tick = a_tick;
}

void perform::set_left_tick( long a_tick )
{
    m_left_tick = a_tick;
    m_starting_tick = a_tick;

    if(global_song_start_mode && (m_jack_master || !m_jack_running))
    {
        if(m_jack_master && global_song_start_mode && m_jack_running)
            position_jack(global_song_start_mode, a_tick);
        else
            m_tick = a_tick;
    }

    m_reposition = false;

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
    m_tick = a_tick;    // set progress line
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

        if ( m_right_tick <= m_left_tick ) // don't allow right tick to be left of left tick
        {
            m_left_tick = m_right_tick - c_ppqn * 4;
            m_starting_tick = m_left_tick;

            if(global_song_start_mode && (m_jack_master || !m_jack_running))
            {
                if(m_jack_master && global_song_start_mode && m_jack_running)
                    position_jack(global_song_start_mode, m_left_tick);
                else
                    m_tick = m_left_tick;
            }

            m_reposition = false;
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
    else if(a_pref >= 0)
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

void perform::set_bpm(double a_bpm)
{
    if ( a_bpm < c_bpm_minimum ) a_bpm = c_bpm_minimum;
    if ( a_bpm > c_bpm_maximum ) a_bpm = c_bpm_maximum;

    if ( ! (m_jack_running && global_is_running ))
    {
        m_master_bus.set_bpm( a_bpm );
    }
}

double  perform::get_bpm( )
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
    printf("bpm[%f]\n", get_bpm());
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

void perform::error_message_gtk( Glib::ustring message)
{
    Gtk::MessageDialog errdialog
    (
        message,
        false,
        Gtk::MESSAGE_ERROR,
        Gtk::BUTTONS_OK,
        true
    );
    errdialog.run();
}

void perform::play( long a_tick )
{
    /* just run down the list of sequences and have them dump */

    if(global_song_start_mode && !m_usemidiclock)  // only allow in song mode when not following midi clock
        tempo_change();

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

void perform::tempo_change()
{
    list<tempo_mark>::iterator i;

    for ( i = m_list_play_marker.begin(); i != m_list_play_marker.end(); i++ )
    {
        if((uint64_t)m_tick >= (i)->tick)
        {
            if((i)->bpm == STOP_MARKER)
            {
                stop_playing();
                if(m_setlist_mode) // if we are in set list mode then increment the file on stop marker
                    m_setlist_stop_mark = true;
            }
            else
            {
                m_master_bus.set_bpm((i)->bpm);
                m_list_play_marker.erase(i);
                break;
            }
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

/* setting the undo_vect for proper location - called by tempo */
void perform::push_bpm_undo()
{
    undo_type a_undo;
    a_undo.track = -1; // does not matter - not used
    a_undo.type = c_undo_bpm;

    undo_vect.push_back(a_undo);
    redo_vect.clear();
    set_have_undo();
    set_have_redo();
}

void perform::pop_bpm_undo()
{
    undo_type a_undo;
    a_undo.track = -1; // does not matter - not used
    a_undo.type = c_undo_bpm;

    undo_vect.pop_back();
    redo_vect.push_back(a_undo);
    set_have_redo();
}

void perform::pop_bpm_redo()
{
    undo_type a_undo;
    a_undo.track = -1; // does not matter - not used
    a_undo.type = c_undo_bpm;

    redo_vect.pop_back();
    undo_vect.push_back(a_undo);
    set_have_undo();
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
perform::push_perf_undo(bool a_import)
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

    undo_type a_undo;
    a_undo.type = c_undo_perf;

    if(a_import)
    {
        a_undo.type = c_undo_import;
        m_list_undo.push( m_list_total_marker );
    }

    m_undo_perf_count++;
    a_undo.track = -1;

    undo_vect.push_back(a_undo);
    redo_vect.clear();
    set_have_undo();
    set_have_redo();
    m_mutex.unlock();
}

void
perform::pop_perf_undo(bool a_import)
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

    undo_type a_undo;
    a_undo.type = c_undo_perf;

    if(a_import)
    {
        a_undo.type = c_undo_import;
        m_list_redo.push( m_list_total_marker );
        m_list_total_marker = m_list_undo.top();
        m_list_undo.pop();
        set_tempo_load(true);// used by mainwnd timeout to call m_tempo->load_tempo_list();
    }

    m_undo_perf_count--;
    m_redo_perf_count++;

    a_undo.track = -1;

    undo_vect.pop_back();
    redo_vect.push_back(a_undo);
    global_seqlist_need_update = true; // in case track set_dirty() is not triggered above
    set_have_redo();
    m_mutex.unlock();
}

void
perform::pop_perf_redo(bool a_import)
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

    undo_type a_undo;
    a_undo.type = c_undo_perf;

    if(a_import)
    {
        a_undo.type = c_undo_import;
        m_list_undo.push( m_list_total_marker );
        m_list_total_marker = m_list_redo.top();
        m_list_redo.pop();
        set_tempo_load(true); // used by mainwnd timeout to call m_tempo->load_tempo_list();
    }

    m_undo_perf_count++;
    m_redo_perf_count--;

    a_undo.track = -1;

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
    if(m_undo_track_count > c_max_undo_track || m_redo_track_count > c_max_undo_track ||
            m_undo_perf_count > c_max_undo_perf || m_redo_perf_count > c_max_undo_perf )
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

#ifdef JACK_SUPPORT
jack_nframes_t tick_to_jack_frame(uint64_t a_tick, double a_bpm, void *arg)
{
    perform *perf = (perform *) arg;

    long current_tick = a_tick;
    current_tick *= 10;

    int ticks_per_beat = c_ppqn * 10; // 192 * 10 = 1920
    double beats_per_minute =  a_bpm;

    uint64_t tick_rate = ((uint64_t)perf->m_jack_frame_rate * current_tick * 60.0);
    long tpb_bpm = ticks_per_beat * beats_per_minute * 4.0 / perf->m_bw;
    jack_nframes_t jack_frame = tick_rate / tpb_bpm;
    return jack_frame;
}

/** return a stucture containing the BBT info which applies at /frame/ */
position_info solve_tempomap ( jack_nframes_t frame, void *arg )
{
    return render_tempomap( frame, 0, 0, arg );
}

/* From non-timeline - modified */
position_info render_tempomap( jack_nframes_t start, jack_nframes_t length, void *cb, void *arg )
{
#ifdef RDEBUG
    printf("start %u\n", start);
#endif
    perform *perf = (perform *) arg;
    const jack_nframes_t end = start + length;

    position_info pos;
    memset( &pos, 0, sizeof( pos ) );

    BBT &bbt = pos.bbt;

    /* default values */
    pos.beat_type = 4;
    pos.beats_per_bar = 4;
    pos.tempo = 120.0;

    const jack_nframes_t samples_per_minute = perf->m_jack_frame_rate * 60;

    float bpm = 120.0f;

    time_sig sig;

    sig.beats_per_bar = 4;
    sig.beat_type = 4;

    jack_nframes_t frame = 0;
    jack_nframes_t next = 0;

    jack_nframes_t frames_per_beat = samples_per_minute / bpm;

    if ( ! perf->m_list_no_stop_markers.size() )
       return pos;

    list<tempo_mark>::iterator i;

    for ( i = perf->m_list_no_stop_markers.begin(); i != perf->m_list_no_stop_markers.end(); ++i )
    {
        tempo_mark p = (*i);
        bpm = p.bpm;

        frames_per_beat = samples_per_minute / bpm;

        sig.beat_type = perf->m_bw;
        sig.beats_per_bar = perf->m_bp_measure;

#ifdef RDEBUG
        printf("bpm %f: frames_per_beat %u: TOP frames %u\n",bpm, frames_per_beat,f);
#endif
            /* Time point resets beat */
//            bbt.beat = 0; // timeline needed to, because it supported multiple sig markers -- we don't

        {
            list<tempo_mark>::iterator n = i;
            ++n;

            if ( n == perf->m_list_no_stop_markers.end())
            {
                next = end;
            }
            else
            {
                jack_nframes_t end_frame = (*i).start;
                jack_nframes_t start_frame = (*n).start;
#ifdef RDEBUG
                printf("(*n).tick %ld: (*i).tick %ld\n", (*n).tick, (*i).tick);
                printf("start_frame(n) %u: end_frame(i) %u\n", start_frame,end_frame);
#endif
                /* points may not always be aligned with beat boundaries, so we must align here */
                next = start_frame - ( ( start_frame - end_frame ) % frames_per_beat );
            }
#ifdef RDEBUG
            printf("next %u: end %u\n",next,end);
#endif
        }

        for ( ; frame <= next; ++bbt.beat, frame += frames_per_beat )
        {
            if ( bbt.beat == sig.beats_per_bar )
            {
                bbt.beat = 0;
                ++bbt.bar;
            }
#ifdef RDEBUG
            printf("frames %u: next %u: end %u: frames_per_beat %u\n", f, next,end,frames_per_beat);
            printf("bbt,beat %u: bbt.bar %u: frame %u\n", bbt.beat, bbt.bar, f);
#endif
            /* ugliness to avoid failing out at -1 */
            if ( end > frames_per_beat )
            {
                if ( frame > end - frames_per_beat )
                    goto done;
            }
            else if ( frame + frames_per_beat > end )
                goto done;
        }
        /* when frame is == next && not goto done: then one extra frame & beat are added - so subtract them here */
        frame -= frames_per_beat;
        --bbt.beat;
    }

done:

    pos.frame = frame;
    pos.tempo = bpm;
    pos.beats_per_bar = sig.beats_per_bar;
    pos.beat_type = sig.beat_type;

    assert( frame <= end );

    assert( end - frame <= frames_per_beat );


    double ticks_per_beat = c_ppqn * 10; // 192 * 10 = 1920
    const double frames_per_tick = frames_per_beat / ticks_per_beat;
    bbt.tick = ( end - frame ) / frames_per_tick;

    return pos;
}
#endif // JACK_SUPPORT

void perform::position_jack( bool a_state, long a_tick )
{
    //printf( "perform::position_jack()\n" );

#ifdef JACK_SUPPORT

    uint64_t current_tick = 0;

    if(a_state) // master in song mode
    {
        current_tick = a_tick;
    }

    uint32_t hold_frame = 0;

    list<tempo_mark>::iterator i;
    tempo_mark last_tempo = (*--m_list_no_stop_markers.end());

    for ( i = ++m_list_no_stop_markers.begin(); i != m_list_no_stop_markers.end(); ++i )
    {
        if( current_tick >= (*i).tick )
        {
            hold_frame = (*i).start;
        }
        else
        {
            last_tempo = (*--i);
            break;
        }
    }

    uint32_t end_tick = current_tick - last_tempo.tick;
    uint64_t jack_frame = hold_frame + tick_to_jack_frame(end_tick, last_tempo.bpm, this);

    //printf("end_tick %d: current_tick %d: last tempo.tick %d, bpm %f\n", end_tick, current_tick, last_tempo.tick, last_tempo.bpm);
    //printf("jack_frame %d: hold_frame %d\n", jack_frame, hold_frame);

    jack_transport_locate(m_jack_client,jack_frame);


 #ifdef USE_JACK_BBT_POSITION
    current_tick *= 10;

    /*  This jack_frame calculation is all that is needed to change jack position
        The BBT calc can be sent but will be overridden by the first call to
        jack_timebase_callback() of any master set. If no master is set, then the
        BBT will display the new position but will not change even if the transport
        is rolling. There is no need to send BBT on position change - the fact that
        the function jack_transport_locate() exists and only uses the frame position
        is proof that BBT is not needed! Upon further reflection, why not send BBT?
        Because other programs do not.... lets follow convention.
        The below calculation for jack_transport_locate(), works, is simpler and
        does not send BBT. The calc for jack_transport_reposition() will be commented
        out again....
    */

    int ticks_per_beat = c_ppqn * 10; // 192 * 10 = 1920
    double beats_per_minute =  m_master_bus.get_bpm();

    uint64_t tick_rate = ((uint64_t)m_jack_frame_rate * current_tick * 60.0);
    long tpb_bpm = ticks_per_beat * beats_per_minute * 4.0 / m_bw;
    uint64_t jack_frame = tick_rate / tpb_bpm;

    jack_transport_locate(m_jack_client,jack_frame);


    /* The below BBT call to jack_BBT_position() is not necessary to change jack position!!! */

    jack_position_t pos;
    double jack_tick = current_tick * 4 / m_bw;

    /* gotta set these here since they are set in timebase */
    pos.ticks_per_beat = c_ppqn * 10; // 192 * 10 = 1920
    pos.beats_per_minute =  m_master_bus.get_bpm();

    jack_BBT_position(pos, jack_tick);

    /* this calculates jack frame to put into pos.frame.
       it is what really matters for position change */

    uint64_t tick_rate = ((uint64_t)pos.frame_rate * current_tick * 60.0);
    long tpb_bpm = pos.ticks_per_beat * pos.beats_per_minute / (pos.beat_type / 4.0 );
    pos.frame = tick_rate / tpb_bpm; // pos.frame is all that is needed for position change!

    /*
       ticks * 10 = jack ticks;
       jack ticks / ticks per beat = num beats;
       num beats / beats per minute = num minutes
       num minutes * 60 = num seconds
       num secords * frame_rate  = frame */

    jack_transport_reposition( m_jack_client, &pos );
#endif // USE_JACK_BBT_POSITION

    if(global_is_running)
        m_reposition = false;

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

    m_reset_tempo_list = true; // since we cleared it as we went along
    inner_stop();
}

bool
perform::get_tempo_reset()
{
    return m_reset_tempo_list;
}

void
perform::set_tempo_reset(bool a_reset)
{
    m_reset_tempo_list = a_reset;
}

bool
perform::get_tempo_load()
{
    return m_load_tempo_list;
}

void
perform::set_tempo_load(bool a_load)
{
    m_load_tempo_list = a_load;
}

void
perform::set_start_tempo(double a_bpm)
{
    tempo_mark marker;
    marker.bpm = a_bpm;
    marker.tick = STARTING_MARKER;

    if(!m_list_total_marker.size()) // normal file loading .s42 file size will be zero
    {
        m_list_total_marker.push_front(marker);
    }
    else                            // midi file import - user wants to change BPM so just load at start
    {
        (*m_list_total_marker.begin())= marker;
    }

    set_tempo_load(true);
}

double
perform::get_start_tempo()
{
    return m_list_total_marker.begin()->bpm;
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

void perform::inner_stop(bool a_midi_clock)
{
    global_is_running = false;
    reset_sequences();
    m_usemidiclock = a_midi_clock;
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

bool perform::get_playback_mode()
{
    return m_playback_mode;
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

/*
    This process callback is called by jack whether stopped or rolling.
    Assuming every jack cycle...
    "...client supplied function that is called by the engine anytime there is work to be done".
    There seems to be no definition of '...work to be done'.
    nframes = buffer_size -- is not used.
*/

int jack_process_callback(jack_nframes_t nframes, void* arg)
{
    perform *m_mainperf = (perform *) arg;

    /* For start or FF/RW/ key-p when not running */
    if(!global_is_running)
    {
        jack_position_t pos;
        jack_transport_state_t state = jack_transport_query( m_mainperf->m_jack_client, &pos );

        /* we are stopped, do we need to start? */
        if(state == JackTransportRolling || state == JackTransportStarting )
        {
            /* we need to start */
            //printf("JackTransportState [%d]\n",state);
            m_mainperf->m_jack_transport_state_last = JackTransportStarting;
            m_mainperf->inner_start( global_song_start_mode );
            //printf("JackTransportState [%d]\n",m_mainperf->m_jack_transport_state);
        }
        /* we don't need to start - just reposition transport marker */
        else
        {
            long tick = get_current_jack_position(pos.frame, (void *)m_mainperf);
            long diff = tick - m_mainperf->get_jack_stop_tick();

            if(diff != 0)
            {
                m_mainperf->set_reposition();
                m_mainperf->set_starting_tick(tick);
                m_mainperf->set_jack_stop_tick(tick);
            }
        }
    }
#ifdef USE_MODIFIABLE_JACK_TEMPO    // For jack in slave mode, allow tempo change
    else
    {
        /* this won't work with new tempo markers!!!!*/
        jack_position_t pos;
        jack_transport_query(m_mainperf->m_jack_client, &pos);
        if (! m_mainperf->m_jack_master)
        {
            if (pos.beats_per_minute > 1.0)     /* a sanity check   */
            {
                static double s_old_bpm = 0.0;
                if (pos.beats_per_minute != s_old_bpm)
                {
                    s_old_bpm = pos.beats_per_minute;
                    //printf("BPM = %f\n", pos.beats_per_minute);
                    //m_mainperf->set_bpm(pos.beats_per_minute);
                    m_mainperf->set_start_tempo(pos.beats_per_minute);
                }
            }
        }
    }
#endif // USE_MODIFIABLE_JACK_TEMPO
    return 0;
}

#ifdef USE_JACK_BBT_POSITION
/* former slow sync callback - no longer used - now using jack_process_callback() - ca. 7/10/16 */
int jack_sync_callback(jack_transport_state_t state,
                       jack_position_t *pos, void *arg)
{
    //printf( "jack_sync_callback() " );

    perform *p = (perform *) arg;

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
#endif // USE_JACK_BBT_POSITION

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
        {
            if(!m_reposition)                                  // allow to start if key-p set
                position_jack(true, m_left_tick);
            else
                m_reposition = false;
        }

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
            total_tick = m_starting_tick;
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
            double bpm = m_master_bus.get_bpm() * ( 4.0 / m_bw);

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

            //printf ( "delta_tick[%ld]: delta_tick_num[%lld]: bpm [%d]\n", delta_tick, delta_tick_num, bpm  );
#ifdef JACK_SUPPORT

            // no init until we get a good lock

            if ( m_jack_running )
            {
                init_clock = false;

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

                m_jack_transport_state = jack_transport_query( m_jack_client, &m_jack_pos );

                if ( m_jack_transport_state_last  ==  JackTransportStarting &&
                        m_jack_transport_state       == JackTransportRolling )
                {
                    m_jack_frame_current =  jack_get_current_transport_frame( m_jack_client );
                    m_jack_frame_last = m_jack_frame_current;

                    //printf ("[Start Playback]\n" );

                    dumping = true;

                    if(global_song_start_mode)      // song mode use tempo map
                    {
                        jack_ticks_converted = get_current_jack_position(m_jack_frame_current,(void*)this);
                    }
                    else                            // live mode use start bpm only
                    {
                        m_jack_pos.beats_per_bar = m_bp_measure;
                        m_jack_pos.beat_type = m_bw;
                        m_jack_pos.ticks_per_beat = c_ppqn * 10;
                        m_jack_pos.beats_per_minute =  m_master_bus.get_bpm();

                        m_jack_tick =
                            m_jack_frame_current *
                            m_jack_pos.ticks_per_beat *
                            m_jack_pos.beats_per_minute / (m_jack_frame_rate * 60.0);

                        /* convert ticks */
                        jack_ticks_converted =
                            m_jack_tick * ((double) c_ppqn /
                                           (m_jack_pos.ticks_per_beat *
                                            m_jack_pos.beat_type / 4.0  ));
                    }

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

                    // if we are moving ahead
                    if ( (m_jack_frame_current > m_jack_frame_last))
                    {
                        if(global_song_start_mode)                  // song mode - use tempo map
                        {
                            m_jack_frame_last = m_jack_frame_current;
                        }
                        else                                        // live mode - use start bpm only
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
                    }

                    /* convert ticks */
                    if(global_song_start_mode)                  // song mode - use tempo map
                    {
                        jack_ticks_converted = get_current_jack_position(m_jack_frame_current,(void*)this);
                    }
                    else                                        // live mode - use start bpm only
                    {
                        jack_ticks_converted =
                            m_jack_tick * ((double) c_ppqn /
                                           (m_jack_pos.ticks_per_beat *
                                            m_jack_pos.beat_type / 4.0  ));
                    }
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
                /* if we reposition key-p, FF, rewind, adjust delta_tick for change
                 * then reset to adjusted starting  */
                if ( m_playback_mode && !m_jack_running && !m_usemidiclock && m_reposition)
                {
                    current_tick = clock_tick;      // needed if looping unchecked while global_is_running
                    delta_tick = m_starting_tick - clock_tick;
                    init_clock=true;                // must set to send EVENT_MIDI_SONG_POS
                    m_starting_tick = m_left_tick;  // restart at left marker
                    m_reposition = false;
                    m_reset_tempo_list = true;      // since we cleared it as we went along

                }

                /* default if jack is not compiled in, or not running */
                /* add delta to current ticks */
                clock_tick     += delta_tick;
                current_tick   += delta_tick;
                total_tick     += delta_tick;
                dumping = true;


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
#ifdef JACK_SUPPORT
                    static bool jack_position_once = false;
#endif // JACK_SUPPORT
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

#ifdef JACK_SUPPORT     // don't play during JackTransportStarting to avoid xruns on FF or rewind
                        if(m_jack_running && m_jack_transport_state != JackTransportStarting)
                            play( get_right_tick() - 1 );
#endif // JACK_SUPPORT
                        if(!m_jack_running)
                            play( get_right_tick() - 1 );

                        reset_sequences();

                        set_orig_ticks( get_left_tick() );
                        current_tick = (double) get_left_tick() + leftover_tick;
                        
                        if(!m_jack_running)
                            m_reset_tempo_list = true; // since we cleared it as we went along
                    }
#ifdef JACK_SUPPORT
                    else
                        jack_position_once = false;
#endif // JACK_SUPPORT
                }
                /* play */
#ifdef JACK_SUPPORT // don't play during JackTransportStarting to avoid xruns on FF or rewind
                if(m_jack_running && m_jack_transport_state != JackTransportStarting)
                    play( (long) current_tick );
#endif // JACK_SUPPORT
                if(!m_jack_running)
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
        }   // end while(global_is_running)

        if (global_stats)
        {
            printf("\n\n-- trigger width --\n");
            for (int i=0; i<100; i++ )
            {
                printf( "[%3d][%8ld]\n", i * 100, stats_all[i] );
            }
            printf("\n\n-- clock width --\n" );
            double bpm  = m_master_bus.get_bpm();

            printf("optimal: [%f]us\n", ((c_ppqn / 24)* 60000000 / c_ppqn / bpm));

            for ( int i=0; i<100; i++ )
            {
                printf( "[%3d][%8ld]\n", i * 300, stats_clock[i] );
            }
        }

        /* m_tick is the progress play tick that displays the progress line */
#ifdef JACK_SUPPORT
        if(m_playback_mode && m_jack_master) // master in song mode
        {
            position_jack(m_playback_mode,m_left_tick);
        }
        if(!m_playback_mode && m_jack_running && m_jack_master) // master in live mode
        {
            position_jack(m_playback_mode,0);
        }
#endif // JACK_SUPPORT
        if(!m_usemidiclock) // will be true if stopped by midi event
        {
            if(m_playback_mode && !m_jack_running) // song mode default
                m_tick = m_left_tick;

            if(!m_playback_mode && !m_jack_running) // live mode default
                m_tick = 0;
        }

        /* this means we leave m_tick at stopped location if in slave mode or m_usemidiclock = true */

        m_master_bus.flush();
        m_master_bus.stop();

#ifdef JACK_SUPPORT
        if(m_jack_running)
        {
            m_jack_stop_tick = get_current_jack_position(jack_get_current_transport_frame( m_jack_client ),(void *)this);
        }
#endif // JACK_SUPPORT
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
                    // only used when starting from the beginning of the song = 0
                    if (ev.get_status() == EVENT_MIDI_START)
                    {
                        //printf("EVENT_MIDI_START\n");
                        //stop();
                        start(global_song_start_mode);
                        m_midiclockrunning = true;
                        m_usemidiclock = true;
                        m_midiclocktick = 0;    // start at beginning of song
                        m_midiclockpos = 0;     // start at beginning of song
                    }
                    // midi continue: start from midi song position
                    // this will be sent immediately after  EVENT_MIDI_SONG_POS
                    // and is used for start from other than beginning of the song,
                    // or to start from previous location at EVENT_MIDI_STOP
                    else if (ev.get_status() == EVENT_MIDI_CONTINUE)
                    {
                        //printf("EVENT_MIDI_CONTINUE\n");
                        m_midiclockrunning = true;
                        start(global_song_start_mode);
                    }
                    // should hold the stop position in case the next event is continue
                    else if (ev.get_status() == EVENT_MIDI_STOP)
                    {
                        //printf("EVENT_MIDI_STOP\n");
                        m_midiclockrunning = false;
                        all_notes_off();
                        inner_stop(true);        // true = m_usemidiclock = true, i.e. hold m_tick position(output_func)
                        m_midiclockpos = m_tick; // set position to last location on stop - for continue
                    }
                    else if (ev.get_status() == EVENT_MIDI_CLOCK)
                    {
                        //printf("EVENT_MIDI_CLOCK - m_tick [%ld] \n", m_tick);
                        if (m_midiclockrunning)
                            m_midiclocktick += 8;
                    }
                    else if (ev.get_status() == EVENT_MIDI_SONG_POS)
                    {
                        unsigned char a, b;
                        ev.get_data(&a, &b);

                        m_midiclockpos = combine_bytes(a,b);

                        /*
                            http://www.blitter.com/~russtopia/MIDI/~jglatt/tech/midispec/ssp.htm

                            Example: If a Song Position value of 8 is received,
                            then a sequencer (or drum box) should cue playback to the
                            third quarter note of the song.
                            (8 MIDI beats * 6 MIDI clocks per MIDI beat = 48 MIDI Clocks.
                            Since there are 24 MIDI Clocks in a quarter note,
                            the first quarter occurs on a time of 0 MIDI Clocks,
                            the second quarter note occurs upon the 24th MIDI Clock,
                            and the third quarter note occurs on the 48th MIDI Clock).
                         */

                        m_midiclockpos *= 48;   // 8 MIDI beats * 6 MIDI clocks per MIDI beat = 48 MIDI Clocks.
                        //printf("EVENT_MIDI_SONG_POS - m_midiclockpos[%ld]\n", m_midiclockpos);
                    }

                    /* filter system wide messages */
                    if (ev.get_status() <= EVENT_SYSEX)
                    {
                        if( global_showmidi)
                            ev.print();

                        /* is there at least one sequence set ? */
                        if (m_master_bus.is_dumping())
                        {
                            ev.set_timestamp(m_tick);

                            /* dump to it - possibly multiple sequences set */
                            m_master_bus.dump_midi_input(ev);
                        }
                        
                        /* To fix the FF/RW sysex on the YPT that only sends on - this is the off key */
                        if (global_use_sysex)
                        {
                            if(FF_RW_button_type != FF_RW_RELEASE)
                            {
                                if(ev.is_note_off())
                                {
                                    /* notes 91 G5 & 96 C6 on YPT are the FF/RW keys */
                                    if(ev.get_note() == 91 || ev.get_note() == 96)
                                        FF_RW_button_type = FF_RW_RELEASE;
                                }
                            }
                        }
                    }

                    if (ev.get_status() == EVENT_SYSEX)
                    {
                        if (global_use_sysex)
                            parse_sysex(ev);

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

/*
    http://www.blitter.com/~russtopia/MIDI/~jglatt/tech/midispec/wheel.htm
    Two data bytes follow the status. The two bytes should be combined together
    to form a 14-bit value. The first data byte's bits 0 to 6 are bits 0 to 6 of
    the 14-bit value. The second data byte's bits 0 to 6 are really bits 7 to 13
    of the 14-bit value. In other words, assuming that a C program has the first
    byte in the variable First and the second data byte in the variable Second,
    here's how to combine them into a 14-bit value (actually 16-bit since most
    computer CPUs deal with 16-bit, not 14-bit, integers):
*/
unsigned short perform::combine_bytes(unsigned char First, unsigned char Second)
{
   unsigned short _14bit;
   _14bit = (unsigned short)Second;
   _14bit <<= 7;
   _14bit |= (unsigned short)First;
   return(_14bit);
}

void perform::parse_sysex(event a_e)
{
/*  http://www.indiana.edu/~emusic/etext/MIDI/chapter3_MIDI9.shtml
 *  A System Exclusive code set begins with 11110000 (240 decimal or F0 hex),
 *  followed by the manufacturer ID#, then by an unspecified number of
 *  data bytes of any ranges from 0-127) and ends with 11110111
 *  (decimal 247 or F7 hex), meaning End of SysEx message. No other coded
 *  are to be transmitted during a SysEx message (except a system real time
 *  message). Normally, after the manufacturer ID, each maker will have its
 *  own instrument model subcode, so a Yamaha DX7 will ignore a Yamaha SY77's
 *  patch dump. In addition, most instruments have a SysEx ID # setting so
 *  more than one of the same instruments can be on a network but not
 *  necessarily respond to a patch dump not intended for it.
 */

    enum sysex_YPT
    {
        SYS_YPT300_START,
        SYS_YPT300_STOP,
        SYS_YPT300_TOP,             //  Beginning of song
        SYS_YPT300_FAST_FORWARD,
        SYS_YPT300_REWIND,
        SYS_YPT300_METRONOME        //  or anything else
    };

    /* layout of YPT-300 sysex messages */
    //  EVENT_SYSEX                                         // byte 0 0xF0
    const unsigned char c_yamaha_ID         = 0x43;         // byte 1
    const unsigned long c_YPT_model_subcode = 0x73015015;   // bytes 2 - 5
    // 0x00                                                 // byte 6
    // the message we are looking for - enum 0 to 5         // byte 7
    // 0x00                                                 // byte 8
    // end sysex 0xF7                                       // byte 9

    unsigned char *data = a_e.get_sysex();
    long data_size =  a_e.get_size();

    if(data_size < 10)               // sanity check
        return;

    /* Check the manufacturer ID */
    if(data[1] != c_yamaha_ID)                      // could use others here
        return;

    /* Check the model subcode */
    unsigned long subcode = 0;

    subcode += (data[2] << 24);
    subcode += (data[3] << 16);
    subcode += (data[4] << 8);
    subcode += (data[5]);

    if(subcode != c_YPT_model_subcode)
        return;

/*    for(int i = 0; i < data_size; i++)
    {
        printf( "%02X \n", data[i]);
    }
 */

    /* We are good to go */
    switch(data[7])
    {
    case SYS_YPT300_START:
        start_playing();
        break;

    case SYS_YPT300_STOP:
        stop_playing();
        break;

    case SYS_YPT300_TOP:                        // beginning of song
        if(global_song_start_mode)              // don't bother reposition in 'Live' mode
        {
            if(is_jack_running())
            {
                set_reposition();
                set_starting_tick(0);
                position_jack(true, 0);
            }
            else
            {
                set_reposition();
                set_starting_tick(0);
            }
        }
        break;

    case SYS_YPT300_FAST_FORWARD:
        FF_RW_button_type = FF_RW_FORWARD;
        gtk_timeout_add(120,FF_RW_timeout,this);
        break;

    case SYS_YPT300_REWIND:
        FF_RW_button_type = FF_RW_REWIND;
        gtk_timeout_add(120,FF_RW_timeout,this);
        break;

    default:
        break;
    }
}

#ifdef JACK_SUPPORT
#ifdef USE_JACK_BBT_POSITION
/*  was called by jack_timebase_callback() & position_jack()>(debug)  */
void perform::jack_BBT_position(jack_position_t &pos, double jack_tick)
{
    pos.valid = JackPositionBBT;
    pos.beats_per_bar = m_bp_measure;
    pos.beat_type = m_bw;
    /* these are set in the timebase callback since they are needed for jack_tick */
    //pos.ticks_per_beat = c_ppqn * 10; // 192 * 10 = 1920
    //pos.beats_per_minute =  m_master_bus.get_bpm();

    pos.frame_rate =  m_jack_frame_rate; // comes from init_jack()

    /* Compute BBT info from frame number.  This is relatively
     * simple here, but would become complex if we supported tempo
     * or time signature changes at specific locations in the
     * transport timeline. */

    long ptick = 0, pbeat = 0, pbar = 0;
    pbar  = (long) ((long) jack_tick / (pos.ticks_per_beat *  pos.beats_per_bar ));
    pbeat = (long) ((long) jack_tick % (long) (pos.ticks_per_beat *  pos.beats_per_bar ));
    pbeat = pbeat / (long) pos.ticks_per_beat;
    ptick = (long) jack_tick % (long) pos.ticks_per_beat;

    pos.bar = pbar + 1;
    pos.beat = pbeat + 1;
    pos.tick = ptick;
    pos.bar_start_tick = pos.bar * pos.beats_per_bar *
                          pos.ticks_per_beat;

    //printf( " bbb [%2d:%2d:%4d] jack_tick [%f]\n", pos.bar, pos.beat, pos.tick, jack_tick );
}
#endif // USE_JACK_BBT_POSITION


/*
    This callback is only called by jack when seq42 is Master and is used to supply jack
    with BBT information based on frame position and frame_rate. It is called once on
    startup, and afterwards, only when transport is rolling.
*/
void
jack_timebase_callback
(
    jack_transport_state_t state,           // currently unused !!!
    jack_nframes_t nframes,
    jack_position_t * pos,
    int new_pos,
    void * arg
)
{
    if (pos == nullptr)
    {
        printf("jack_timebase_callback(): null position pointer");
        return;
    }
    
    perform *p = (perform *) arg;

    if(global_song_start_mode)      // song mode - use tempo map
    {
        /* From non-timeline timebase callback */
        position_info pi = solve_tempomap( pos->frame, arg );

        pos->valid = JackPositionBBT;

        pos->beats_per_bar = pi.beats_per_bar;
        pos->beat_type = pi.beat_type;
        pos->beats_per_minute = pi.tempo;

        pos->bar = pi.bbt.bar + 1;
        pos->beat = pi.bbt.beat + 1;
        pos->tick = pi.bbt.tick;
        pos->ticks_per_beat = 1920;     // c_ppqn * 10

        long ticks_per_bar = long(pos->ticks_per_beat * pos->beats_per_bar);
        pos->bar_start_tick = int(pos->bar * ticks_per_bar);
        
        if(new_pos)
        {
            p->set_tempo_reset(true);
        }
    }
    else                            // live mode - only use start bpm
    {
        /* From sequencer64 timebase callback */
        perform *p = (perform *) arg;

        pos->beats_per_minute = p->get_bpm();
        pos->beats_per_bar = p->m_bp_measure;
        pos->beat_type = p->m_bw;
        pos->ticks_per_beat = c_ppqn * 10;

        long ticks_per_bar = long(pos->ticks_per_beat * pos->beats_per_bar);
        long ticks_per_minute = long(pos->beats_per_minute * pos->ticks_per_beat);
        double framerate = double(pos->frame_rate * 60.0);

        double minute = pos->frame / framerate;
        long abs_tick = long(minute * ticks_per_minute);
        long abs_beat = 0;

        /*
         *  Handle 0 values of pos->ticks_per_beat and pos->beats_per_bar that
         *  occur at startup as JACK Master.
         */

        if (pos->ticks_per_beat > 0)                    // 0 at startup!
            abs_beat = long(abs_tick / pos->ticks_per_beat);

        if (pos->beats_per_bar > 0)                     // 0 at startup!
            pos->bar = int(abs_beat / pos->beats_per_bar);
        else
            pos->bar = 0;

        pos->beat = int(abs_beat - (pos->bar * pos->beats_per_bar) + 1);
        pos->tick = int(abs_tick - (abs_beat * pos->ticks_per_beat));
        pos->bar_start_tick = int(pos->bar * ticks_per_bar);
        pos->bar++;                             /* adjust start to bar 1 */

        pos->valid = JackPositionBBT;
    }
}

long convert_jack_frame_to_s42_tick(jack_nframes_t a_frame, double a_bpm, void *arg)
{
    perform *p = (perform *) arg;
    double jack_tick;
    double ticks_per_beat = c_ppqn * 10; // 192 * 10 = 1920
    double beat_type = p->get_bw();

    jack_tick =
        (a_frame) *
        ticks_per_beat  *
        a_bpm / ( p->m_jack_frame_rate* 60.0);

    /* convert ticks */
    return jack_tick * ((double) c_ppqn /
                    (ticks_per_beat *
                     beat_type / 4.0  ));
}

/* returns conversion of jack frame position to seq42 tick */
long get_current_jack_position(jack_nframes_t a_frame, void *arg)
{
    perform *p = (perform *) arg;
    jack_nframes_t current_frame = a_frame;

    uint32_t hold_frame = 0;

    list<tempo_mark>::iterator i;
    tempo_mark last_tempo = (*--p->m_list_no_stop_markers.end());

    for ( i = ++p->m_list_no_stop_markers.begin(); i != p->m_list_no_stop_markers.end(); ++i )
    {
        if( current_frame >= (*i).start )
        {
            hold_frame = (*i).start;
        }
        else
        {
            last_tempo = (*--i);
            break;
        }
    }

    uint32_t end_frames = current_frame - hold_frame;
    uint32_t s42_tick = last_tempo.tick + convert_jack_frame_to_s42_tick(end_frames, last_tempo.bpm, arg);

    return s42_tick;

#if 0
    perform *p = (perform *) arg;
    jack_nframes_t current_frame = a_frame;
    double jack_tick;
    double ticks_per_beat = c_ppqn * 10; // 192 * 10 = 1920
    double beats_per_minute =  p->get_bpm();
    double beat_type = p->get_bw();

    jack_tick =
        (current_frame) *
        ticks_per_beat  *
        beats_per_minute / ( p->m_jack_frame_rate* 60.0);


    /* convert ticks */
    return jack_tick * ((double) c_ppqn /
                    (ticks_per_beat *
                     beat_type / 4.0  ));

#endif // 0
}

void jack_shutdown(void *arg)
{
    perform *p = (perform *) arg;
    p->m_jack_running = false;

    printf("JACK shut down.\nJACK sync Disabled.\n");
}


void print_jack_pos( jack_position_t* jack_pos )
{
//    return;
    printf( "print_jack_pos()\n" );
    printf( "    frame[%d]\n", jack_pos->frame);
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
perform::track_is_song_exportable(int a_track)
{
    if (is_active_track(a_track) && get_track(a_track)->get_track_trigger_count() > 0 &&
                !get_track(a_track)->get_song_mute()) // don't count tracks with NO triggers or muted
        {
            if(get_track(a_track)->get_number_of_sequences() > 0) // don't count tracks with NO sequences(even if they have a trigger)
                return true;
        }
    return false;
}


std::string
perform::current_date_time()
{
    std::string mystring;
    std::string strTemp;

    time_t     now = time(0);
    struct tm  tstruct;
    char       buf[80];
    tstruct = *localtime(&now);
    // Visit http://en.cppreference.com/w/cpp/chrono/c/strftime
    // for more information about date/time format
    strftime(buf, sizeof(buf), "%Y-%m-%d.%X", &tstruct);

    mystring = std::string(buf);

// Format: = "2012-05-06.01:03:59 PM" of mystring Does NOT use military time
    return mystring;
}

bool
perform::save( const Glib::ustring& a_filename )
{
    m_mutex.lock();

    ofstream file (a_filename.c_str (), ios::out | ios::binary | ios::trunc);

    if (!file.is_open ()) return false;

    /* file version 5 */
    file.write((const char *) &c_file_identification, sizeof(uint64_t)); // magic number file ID

    char p_version[global_VERSION_array_size];
    strncpy(p_version, VERSION, global_VERSION_array_size);
    file.write((const char *) p_version, sizeof(char)* global_VERSION_array_size);

    char time[global_time_array_size];
    std::string s_time = current_date_time();
    strncpy(time, s_time.c_str(), global_time_array_size);
    file.write(time, sizeof(char)* global_time_array_size);

    /* end file version 5 */

    file.write((const char *) &c_file_version, global_file_int_size);

    /* version 7 use tempo list */
    uint32_t list_size = m_list_total_marker.size();
    file.write((const char *) &list_size, sizeof(list_size));
    list<tempo_mark>::iterator i;
    for ( i = m_list_total_marker.begin(); i != m_list_total_marker.end(); i++ )
    {
        file.write((const char * ) &(*i).tick, sizeof((*i).tick));
        file.write((const char * ) &(*i).bpm, sizeof((*i).bpm));
        file.write((const char * ) &(*i).bw, sizeof((*i).bw));                  // not currently used - future use maybe
        file.write((const char * ) &(*i).bp_measure, sizeof((*i).bp_measure));  // ditto
        // we don't need to write the start frame since it will be recalculated when loaded.
    }

    int bp_measure = get_bp_measure(); // version 4
    file.write((const char *) &bp_measure, global_file_int_size);

    int bw = get_bw();                 // version 4
    file.write((const char *) &bw, global_file_int_size);

    int swing_amount8 = get_swing_amount8();
    file.write((const char *) &swing_amount8, global_file_int_size);
    int swing_amount16 = get_swing_amount16();
    file.write((const char *) &swing_amount16, global_file_int_size);

    int active_tracks = 0;
    for (int i=0; i< c_max_track; i++ )
    {
        if ( is_active_track(i) ) active_tracks++;
    }
    file.write((const char *) &active_tracks, global_file_int_size);

    for (int i=0; i< c_max_track; i++ )
    {
        if ( is_active_track(i) )
        {
            int trk_idx = i; // file version 3
            file.write((const char *) &trk_idx, global_file_int_size);

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

    bool ret = true;

    int64_t file_id = 0;
    file.read((char *) &file_id, sizeof(int64_t));

    if(file_id != c_file_identification) // if it does not match, maybe its old file so reset to beginning
    {
        file.clear();
        file.seekg(0, ios::beg);
        /* since we are checking for the < version 5 files now, then reset int sizes to original */
        global_file_int_size = sizeof(int);
        global_file_long_int_size = sizeof(long);
    }
    else    // we have version 5 or greater file
    {
        char program_version[global_VERSION_array_size +1];
        file.read(program_version,sizeof(char)* global_VERSION_array_size);
        program_version[global_VERSION_array_size] = '\0';

        char time[global_time_array_size +1];
        file.read(time,sizeof(char)* global_time_array_size);
        time[global_time_array_size] = '\0';

        printf("SEQ42 Release Version [%s]\n", program_version);
        printf("File Created [%s]\n", time);
    }

    int version;
    file.read((char *) &version, global_file_int_size);

    printf("File Version [%d]\n",version);

    if (version < 0 || version > c_file_version)
    {
        fprintf(stderr, "Invalid file version detected: %d\n", version);
        /*reset to defaults */
        global_file_int_size = sizeof(int32_t);
        global_file_long_int_size = sizeof(int32_t);
        file.close();
        return false;
    }

    if(version > 6) // version 7 uses tempo markers
    {
        uint32_t list_size;
        file.read((char *) &list_size, sizeof(list_size));

        tempo_mark marker;
        for(unsigned i = 0; i < list_size; ++i)
        {
            file.read((char *) &marker.tick, sizeof(marker.tick));
            file.read((char *) &marker.bpm, sizeof(marker.bpm));
            file.read((char *) &marker.bw, sizeof(marker.bw));
            file.read((char *) &marker.bp_measure, sizeof(marker.bp_measure));
            // we don't need start marker since set_tempo_load() will recalculate it

            m_list_total_marker.push_back(marker);
        }

        set_tempo_load(true);
    }
    else
    {
        if(version > 5)
        {
            double bpm; // file version 6 uses double
            file.read((char *) &bpm, sizeof(bpm));
            set_start_tempo(bpm);
        }
        else
        {
            int bpm;    // prior to version 6 uses int
            file.read((char *) &bpm, global_file_int_size);
            set_start_tempo(bpm);
        }
    }

    int bp_measure = 4;
    if(version > 3)
    {
        file.read((char *) &bp_measure, global_file_int_size);
    }

    set_bp_measure(bp_measure);

    int bw = 4;
    if(version > 3)
    {
        file.read((char *) &bw, global_file_int_size);
    }

    set_bw(bw);

    int swing_amount8 = 0;
    if(version > 1)
    {
        file.read((char *) &swing_amount8, global_file_int_size);
    }

    set_swing_amount8(swing_amount8);
    int swing_amount16 = 0;
    if(version > 1)
    {
        file.read((char *) &swing_amount16, global_file_int_size);
    }

    set_swing_amount16(swing_amount16);

    int active_tracks;
    file.read((char *) &active_tracks, global_file_int_size);

    int trk_index = 0;
    for (int i=0; i< active_tracks; i++ )
    {
        trk_index = i;
        if(version > 2)
        {
            file.read((char *) &trk_index, global_file_int_size);
        }

        new_track(trk_index);
        if(! get_track(trk_index)->load(&file, version))
        {
            ret = false;
        }
    }

    file.close();

    /* reset to default if ID does not match since
    it would have been changed for prior < 5 version check */
    if(file_id != c_file_identification)
    {
        global_file_int_size = sizeof(int32_t);
        global_file_long_int_size = sizeof(int32_t);
    }

    return ret;
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

/****************************************************/

void perform::set_setlist_mode(bool mode)
{
    m_setlist_mode = mode;
}

bool perform::get_setlist_mode()
{
    return m_setlist_mode;
}

void perform::set_setlist_file(const Glib::ustring& fn)
{   
    printf("Opening setlist %s\n",fn.c_str());
    
    if(m_setlist_file != "")                                // if we have a previous file, then reset everything
    {
        m_setlist_fileset.clear();
        m_setlist_nfiles = 0;
        m_setlist_current_idx = 0;
    }
    
    m_setlist_file = fn;                                    // set the file
    
    /*Now read the file*/
    std::ifstream openFile(m_setlist_file);

    if(openFile)
    {
        std::string strFileLine = "";
        while(getline(openFile,strFileLine))
        {
            m_setlist_fileset.push_back(strFileLine);       // load into vector
        }
        openFile.close();
        
        if(m_setlist_fileset.size())                        // if we got something
        {
            m_setlist_nfiles = m_setlist_fileset.size();
        }
        else                                                // if we did not get anything
        {
            error_message_gtk("No files listed in setlist!\n");
            set_setlist_mode(false);                        // abandon ship
        }
    }
    else
    {
        Glib::ustring message = "Unable to open setlist file\n";
        message += m_setlist_file; 
        error_message_gtk(message);
        set_setlist_mode(false);                            // abandon ship
    }
}

Glib::ustring perform::get_setlist_current_file()
{
    return m_setlist_fileset[m_setlist_current_idx];
}

int perform::get_setlist_index()
{
    return m_setlist_current_idx;
}

bool perform::set_setlist_index(int index)
{
    if(index < 0)
        return false;

    if(index >= m_setlist_nfiles)
        return false;

    m_setlist_current_idx = index;
    
    return true;
}
