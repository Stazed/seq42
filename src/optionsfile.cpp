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

#include "optionsfile.h"

extern Glib::ustring last_used_dir;

optionsfile::optionsfile(const Glib::ustring& a_name) :
    configfile( a_name )
{
}

optionsfile::~optionsfile()
{
}



bool
optionsfile::parse( perform *a_perf )
{

    /* file size */
    int file_size = 0;

    /* open binary file */
    ifstream file ( m_name.c_str(), ios::in | ios::ate );

    if( ! file.is_open() )
        return false;
    
    file_size = file.tellg();

    /* run to start */
    file.seekg( 0, ios::beg );

    line_after( &file, "[midi-clock]" );
    long buses = 0;
    sscanf( m_line, "%ld", &buses );
    next_data_line( &file );

    for ( int i=0; i<buses; ++i ){

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
    
    next_data_line( &file );
    sscanf( m_line, "%ld", &flag );
    global_jack_start_mode = (bool) flag;


    line_after( &file, "[midi-input]" );
    buses = 0;
    sscanf( m_line, "%ld", &buses );
    next_data_line( &file );

    for ( int i=0; i<buses; ++i ){

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

    /* interaction method  */
    long method = 0;
    line_after( &file, "[interaction-method]" );
    sscanf( m_line, "%ld", &method );
    global_interactionmethod = (interaction_method_e)method;
    
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
    

    int buses = a_perf->get_master_midi_bus( )->get_num_out_buses();

    file << "\n\n\n[midi-clock]\n";
    file << buses << "\n";
    
    for (int i=0; i< buses; i++ ){


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
    
    for (int i=0; i< buses; i++ ){


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

    file << "\n\n\n[jack-transport]\n\n"

         
         << "# jack_transport - Enable sync with JACK Transport.\n" 
         << global_with_jack_transport << "\n\n"

         << "# jack_master - Seq42 will attempt to serve as JACK Master.\n"
         << global_with_jack_master << "\n\n"

         << "# jack_master_cond -  Seq42 will fail to be master if there is already a master set.\n"
         << global_with_jack_master_cond  << "\n\n"

         << "# jack_start_mode\n"
         << "# 0 = Playback will be in live mode.  Use this to allow muting and unmuting of loops.\n" 
         << "# 1 = Playback will use the song editors data.\n"
         << global_jack_start_mode << "\n\n";

    
    file << "\n\n\n[last-used-dir]\n\n"
         << "# Last used directory.\n" 
         << last_used_dir << "\n\n";

    file.close();
    return true;
}
