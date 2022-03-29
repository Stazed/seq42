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

#include <iostream>
#include <fstream>
#include <string>

#include "optionsfile.h"

extern Glib::ustring last_used_dir;
extern Glib::ustring last_midi_dir;

optionsfile::optionsfile(const Glib::ustring& a_name) :
    configfile( a_name )
{
}

bool
optionsfile::parse( perform *a_perf )
{
    /* open binary file */
    ifstream file ( m_name.c_str(), ios::in | ios::ate );

    if( ! file.is_open() )
        return false;

    /* run to start */
    file.seekg( 0, ios::beg );

#ifdef MIDI_CONTROL_SUPPORT
    line_after( &file, "[midi-control]" );

    unsigned int controls = 0;
    sscanf( m_line, "%u", &controls );
    
    /* Sanity check. */
    if (controls > c_midi_controls)
        controls = c_midi_controls;
    
    next_data_line( &file );

    for (unsigned int i = 0; i < controls; ++i)
    {
        int control = 0;
        int tog[6], on[6], off[6];
        sscanf(m_line,
               "%d [ %d %d %d %d %d %d ]"
                 " [ %d %d %d %d %d %d ]"
                 " [ %d %d %d %d %d %d ]",
               
                &control,
                &tog[0], &tog[1], &tog[2], &tog[3], &tog[4], &tog[5],
                &on[0], &on[1], &on[2], &on[3], &on[4], &on[5],
                &off[0], &off[1], &off[2], &off[3], &off[4], &off[5]
            );
        
        a_perf->get_midi_control_toggle(i)->m_active            =  tog[0];
        a_perf->get_midi_control_toggle(i)->m_inverse_active    =  tog[1];
        a_perf->get_midi_control_toggle(i)->m_status            =  tog[2];
        a_perf->get_midi_control_toggle(i)->m_data              =  tog[3];
        a_perf->get_midi_control_toggle(i)->m_min_value         =  tog[4];
        a_perf->get_midi_control_toggle(i)->m_max_value         =  tog[5];
        
        a_perf->get_midi_control_on(i)->m_active                =  on[0];
        a_perf->get_midi_control_on(i)->m_inverse_active        =  on[1];
        a_perf->get_midi_control_on(i)->m_status                =  on[2];
        a_perf->get_midi_control_on(i)->m_data                  =  on[3];
        a_perf->get_midi_control_on(i)->m_min_value             =  on[4];
        a_perf->get_midi_control_on(i)->m_max_value             =  on[5];
        
        a_perf->get_midi_control_off(i)->m_active               =  off[0];
        a_perf->get_midi_control_off(i)->m_inverse_active       =  off[1];
        a_perf->get_midi_control_off(i)->m_status               =  off[2];
        a_perf->get_midi_control_off(i)->m_data                 =  off[3];
        a_perf->get_midi_control_off(i)->m_min_value            =  off[4];
        a_perf->get_midi_control_off(i)->m_max_value            =  off[5];
        
        next_data_line(&file);
    }
#endif // MIDI_CONTROL_SUPPORT
    
    line_after( &file, "[midi-clock]" );
    long buses = 0;
    sscanf( m_line, "%ld", &buses );
    next_data_line( &file );

    for ( int i=0; i<buses; ++i )
    {
        long bus_on, bus;
        sscanf( m_line, "%ld %ld", &bus, &bus_on );
        a_perf->get_master_midi_bus( )->set_clock( bus, (clock_e) bus_on );
        next_data_line( &file );
    }

    line_after( &file, "[keyboard-control]" );

    sscanf( m_line, "%u", &a_perf->m_key_start );
    next_data_line( &file );

    sscanf( m_line, "%u", &a_perf->m_key_stop );
    next_data_line( &file );

    sscanf( m_line, "%u", &a_perf->m_key_loop );
    next_data_line( &file );

    sscanf( m_line, "%u", &a_perf->m_key_bpm_dn );
    next_data_line( &file );

    sscanf( m_line, "%u", &a_perf->m_key_bpm_up );

    line_after( &file, "[New-keys]" );
    sscanf( m_line, "%u", &a_perf->m_key_song );
    next_data_line( &file );

    sscanf( m_line, "%u", &a_perf->m_key_seqlist );
    next_data_line( &file );

    sscanf( m_line, "%u", &a_perf->m_key_follow_trans );
    next_data_line( &file );

    sscanf( m_line, "%u", &a_perf->m_key_forward );
    next_data_line( &file );

    sscanf( m_line, "%u", &a_perf->m_key_rewind );
    next_data_line( &file );

    sscanf( m_line, "%u", &a_perf->m_key_pointer );

#ifdef JACK_SUPPORT
    next_data_line( &file );
    sscanf( m_line, "%u", &a_perf->m_key_jack );
#endif // JACK_SUPPORT
    
    next_data_line( &file );
    sscanf( m_line, "%u", &a_perf->m_key_tap_bpm );
    
    next_data_line( &file );
    sscanf( m_line, "%u", &a_perf->m_key_playlist_next );
    
    next_data_line( &file );
    sscanf( m_line, "%u", &a_perf->m_key_playlist_prev );
    

    line_after( &file, "[jack-transport]" );
    long flag = 0;

    sscanf( m_line, "%ld", &flag );
    global_with_jack_transport = (bool) flag;

    next_data_line( &file );
    sscanf( m_line, "%ld", &flag );
    global_with_jack_master = (bool) flag;

    next_data_line( &file );
    sscanf( m_line, "%ld", &flag );
    global_with_jack_master_cond = (bool) flag;

    line_after( &file, "[midi-input]" );
    buses = 0;
    sscanf( m_line, "%ld", &buses );
    next_data_line( &file );

    for ( int i=0; i<buses; ++i )
    {
        long bus_on, bus;
        sscanf( m_line, "%ld %ld", &bus, &bus_on );
        a_perf->get_master_midi_bus( )->set_input( bus, (bool) bus_on );
        next_data_line( &file );
    }

    /* midi clock mod */
    long ticks = 64;
    line_after( &file, "[midi-clock-mod-ticks]" );
    sscanf( m_line, "%ld", &ticks );
    midibus::set_clock_mod(ticks);

    /* manual alsa ports */
    line_after( &file, "[manual-alsa-ports]" );
    sscanf( m_line, "%ld", &flag );
    global_manual_alsa_ports = (bool) flag;

    /* last used dir */
    line_after( &file, "[last-used-dir]" );
    //FIXME: check for a valid path is missing
    if (m_line[0] == '/')
        last_used_dir.assign(m_line);

    /* last midi dir */
    line_after( &file, "[last-midi-dir]" );
    //FIXME: check for a valid path is missing
    if (m_line[0] == '/')
        last_midi_dir.assign(m_line);
    
    /* recent files */
    line_after( &file, "[recent-files]" );
    {
        int count;
        sscanf(m_line, "%d", &count);
        for (int i = 0; i < count; ++i)
        {
            next_data_line( &file );
            if (strlen(m_line) > 0)
                a_perf->add_recent_file(std::string(m_line), true);
        }
    }
    
    /* interaction method  */
    long method = 0;
    line_after( &file, "[interaction-method]" );
    sscanf( m_line, "%ld", &method );
    global_interactionmethod = (interaction_method_e)method;
    
    line_after( &file, "[vertical-zoom-sequence]" );
    if ( !file.eof() )
    {
        sscanf( m_line, "%ld", &flag );

        if( (int) flag < 1 || (int)flag > 17 )
            flag = 1.0;

        global_sequence_editor_vertical_zoom = (int) flag;
    }

    line_after( &file, "[vertical-zoom-song]" );
    if ( !file.eof() )
    {
        sscanf( m_line, "%ld", &flag );

        if( (int) flag < 1 || (int)flag > 50 )
            flag = 10.0;

        global_song_editor_vertical_zoom = (int) flag;
    }
    
    line_after( &file, "[horizontal-zoom-song]" );
    if ( !file.eof() )
    {
        sscanf( m_line, "%ld", &flag );

        if( (int) flag < 0 || (int)flag > 60 )
            flag = 12.0;

        global_song_editor_horizontal_zoom = (int) flag;
    }

    file.close();

    return true;
}

bool
optionsfile::write( perform *a_perf  )
{
    /* open binary file */

    ofstream file ( m_name.c_str(), ios::out | ios::trunc  );
    char outs[1024];

    if( ! file.is_open() )
        return false;

    /* midi control */

    file << "#\n";
    file << "# Seq 42 Init File\n";
    file << "#\n\n\n";

#ifdef MIDI_CONTROL_SUPPORT
    file << "[midi-control]\n";
    file <<  c_midi_controls << "\n";

    for (int i=0; i< c_midi_controls; i++ )
    {
        switch( i )
        {

        case c_midi_control_play               :
            file << "# start/stop playing (toggle, start, stop)\n";
            break;
        case c_midi_control_record  :
            file << "# record\n";
            break;
        case c_midi_control_FF       :
            file << "# fast forward (forward, forward, stop)\n";
            break;
        case c_midi_control_rewind        :
            file << "# rewind (rewind, rewind, stop)\n";
            break;
        case c_midi_control_top        :
            file << "# beginning of song \n";
            break;
        case c_midi_control_playlist :
            file << "# playlist (value, next, previous)\n";
            break;
        case c_midi_control_reserved1   :
            file << "# reserved for expansion\n";
            break;
        case c_midi_control_reserved2   :
            file << "# reserved for expansion\n";
            break;
 
        default:
            break;
        }

        snprintf( outs, sizeof(outs), "%d [%1d %1d %3ld %3ld %3ld %3ld]"
                  " [%1d %1d %3ld %3ld %3ld %3ld]"
                  " [%1d %1d %3ld %3ld %3ld %3ld]",
                  i,
                  a_perf->get_midi_control_toggle(i)->m_active,
                  a_perf->get_midi_control_toggle(i)->m_inverse_active,
                  a_perf->get_midi_control_toggle(i)->m_status,
                  a_perf->get_midi_control_toggle(i)->m_data,
                  a_perf->get_midi_control_toggle(i)->m_min_value,
                  a_perf->get_midi_control_toggle(i)->m_max_value,

                  a_perf->get_midi_control_on(i)->m_active,
                  a_perf->get_midi_control_on(i)->m_inverse_active,
                  a_perf->get_midi_control_on(i)->m_status,
                  a_perf->get_midi_control_on(i)->m_data,
                  a_perf->get_midi_control_on(i)->m_min_value,
                  a_perf->get_midi_control_on(i)->m_max_value,

                  a_perf->get_midi_control_off(i)->m_active,
                  a_perf->get_midi_control_off(i)->m_inverse_active,
                  a_perf->get_midi_control_off(i)->m_status,
                  a_perf->get_midi_control_off(i)->m_data,
                  a_perf->get_midi_control_off(i)->m_min_value,
                  a_perf->get_midi_control_off(i)->m_max_value );

        file << string(outs) << "\n";
    }
#endif // MIDI_CONTROL_SUPPORT

    int buses = a_perf->get_master_midi_bus( )->get_num_out_buses();

    file << "\n\n\n[midi-clock]\n";
    file << buses << "\n";

    for (int i=0; i< buses; i++ )
    {
        file << "# " << a_perf->get_master_midi_bus( )->get_midi_out_bus_name(i) << "\n";
        snprintf(outs, sizeof(outs), "%d %d", i,
                 (char) a_perf->get_master_midi_bus( )->get_clock(i));
        file << outs << "\n";
    }

    /* midi clock mod  */
    file << "\n\n[midi-clock-mod-ticks]\n";
    file << midibus::get_clock_mod() << "\n";

    /* bus input data */
    buses = a_perf->get_master_midi_bus( )->get_num_in_buses();

    file << "\n\n\n[midi-input]\n";
    file << buses << "\n";

    for (int i=0; i< buses; i++ )
    {
        file << "# " << a_perf->get_master_midi_bus( )->get_midi_in_bus_name(i) << "\n";
        snprintf(outs, sizeof(outs), "%d %d", i,
                 (char) a_perf->get_master_midi_bus( )->get_input(i));
        file << outs << "\n";
    }

    /* manual alsa ports */
    file << "\n\n\n[manual-alsa-ports]\n";
    file << "# set to 1 if you want seq42 to create its own alsa ports and\n";
    file << "# not connect to other clients\n";
    file << global_manual_alsa_ports << "\n";

    /* interaction-method */
    int x = 0;
    file << "\n\n\n[interaction-method]\n";
    while (c_interaction_method_names[x] && c_interaction_method_descs[x])
    {
        file << "# " << x << " - '" << c_interaction_method_names[x]
             << "' (" << c_interaction_method_descs[x] << ")\n";
        ++x;
    }
    file << global_interactionmethod << "\n";

    /* vertical zoom */
    file << "\n\n\n[vertical-zoom-sequence]\n";
    file << "# Sequence Editor - value 1 to 17 - default 1\n";
    file << global_sequence_editor_vertical_zoom << "\n";

    file << "\n\n\n[vertical-zoom-song]\n";
    file << "# Song Editor - value 1 to 50 - default 9\n";
    file << global_song_editor_vertical_zoom << "\n";
    
    file << "\n\n\n[horizontal-zoom-song]\n";
    file << "# Song Editor - value 0 to 60 - default 12\n";
    file << global_song_editor_horizontal_zoom << "\n";
 
    /* Key board control */    
    file << "\n\n\n[keyboard-control]\n";

    file << a_perf->m_key_start << "        # "
         << gdk_keyval_name( a_perf->m_key_start )
         << " start sequencer\n";
    file << a_perf->m_key_stop << "        # "
         << gdk_keyval_name( a_perf->m_key_stop )
         << " stop sequencer\n";
    file << a_perf->m_key_loop << "        # "
         << gdk_keyval_name( a_perf->m_key_loop )
         << " toggle looping\n";
    file << a_perf->m_key_bpm_dn << "        # "
         << gdk_keyval_name( a_perf->m_key_bpm_dn )
         << " BPM down\n";
    file << a_perf->m_key_bpm_up << "        # "
         << gdk_keyval_name( a_perf->m_key_bpm_up )
         << " BPM up\n";

    file << "\n\n\n[New-keys]\n";

    file << a_perf->m_key_song << "        # "
         << gdk_keyval_name( a_perf->m_key_song )
         << " song mode\n";

    file << a_perf->m_key_seqlist << "        # "
         << gdk_keyval_name( a_perf->m_key_seqlist )
         << " sequence list\n";

    file << a_perf->m_key_follow_trans << "        # "
         << gdk_keyval_name( a_perf->m_key_follow_trans )
         << " follow transport\n";

    file << a_perf->m_key_forward << "        # "
         << gdk_keyval_name( a_perf->m_key_forward )
         << " fast forward\n";

    file << a_perf->m_key_rewind << "        # "
         << gdk_keyval_name( a_perf->m_key_rewind )
         << " rewind\n";

    file << a_perf->m_key_pointer << "        # "
         << gdk_keyval_name( a_perf->m_key_pointer )
         << " pointer key\n";

#ifdef JACK_SUPPORT
    file << a_perf->m_key_jack << "        # "
         << gdk_keyval_name( a_perf->m_key_jack )
         << " jack sync\n";
#endif // JACK_SUPPORT

    file << a_perf->m_key_tap_bpm << "        # "
         << gdk_keyval_name( a_perf->m_key_tap_bpm )
         << " tap BPM key\n";
    
    file << a_perf->m_key_playlist_next << "        # "
         << gdk_keyval_name( a_perf->m_key_playlist_next )
         << " Playlist next key\n";
    
    file << a_perf->m_key_playlist_prev << "        # "
         << gdk_keyval_name( a_perf->m_key_playlist_prev )
         << " Playlist previous key\n";

    file << "\n\n\n[jack-transport]\n\n"

         << "# jack_transport - Enable sync with JACK Transport.\n"
         << global_with_jack_transport << "\n\n"

         << "# jack_master - Seq42 will attempt to serve as JACK Master.\n"
         << global_with_jack_master << "\n\n"

         << "# jack_master_cond -  Seq42 will fail to be master if there is already a master set.\n"
         << global_with_jack_master_cond  << "\n\n";;

    file << "\n\n\n[last-used-dir]\n\n"
         << "# Last used directory.\n"
         << last_used_dir << "\n\n";

    file << "\n\n\n[last-midi-dir]\n\n"
         << "# Last midi directory.\n"
         << last_midi_dir << "\n\n";

    /*
     *  New feature from sequencer64 via Kepler34
     */

    int count = a_perf->recent_file_count();
    file << "\n"
        "[recent-files]\n\n"
        "# Holds a list of the last few recently-loaded .s42 files.\n\n"
        << count << "\n\n" ;

    if (count > 0)
    {
        for (int i = 0; i < count; ++i)
            file << a_perf->recent_file(i, false) << "\n";

        file << "\n";
    }
    
    file.close();
    return true;
}
