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

/* forward declarations*/
#include "mastermidibus_iface.h"
class mastermidibus_alsa;
class midibus_alsa;

#ifndef HAVE_LIBASOUND
#include "midibus_portmidi.h"
#else
#include <alsa/asoundlib.h>
#include <alsa/seq_midi_event.h>

#include <string>

#include "event.h"
#include "sequence.h"
#include "mutex.h"
#include "globals.h"

const int c_midibus_output_size = 0x100000;
const int c_midibus_input_size =  0x100000;
const int c_midibus_sysex_chunk = 0x100;


class midibus_alsa
{

private:

    int m_id;

    clock_e m_clock_type;
    bool m_inputing;

    static int m_clock_mod;

    /* sequencer client handle */
#if HAVE_LIBASOUND
    snd_seq_t * const m_seq;

    /* address of client */
    const int m_dest_addr_client;
    const int m_dest_addr_port;

    const int m_local_addr_client;
    int m_local_addr_port;
#endif

    /* id of queue */
    int m_queue;

    /* name of bus */
    string m_name;

    /* last tick */
    long m_lasttick;

    /* locking */
    seq42::mutex m_mutex;

    /* mutex */
    void lock();
    void unlock();

public:

#if HAVE_LIBASOUND
    /* constructor, client#, port#, sequencer,
       name of client, name of port */
    midibus_alsa( int a_localclient,
             int a_destclient,
             int a_destport,
             snd_seq_t  *a_seq,
             const char *a_client_name,
             const char *a_port_name,
             int a_id,
             int a_queue );

    midibus_alsa( int a_localclient,
             snd_seq_t  *a_seq,
             int a_id,
             int a_queue );
#endif

#ifdef __WIN32__
    midibus( char a_id, int a_queue );
#endif

    ~midibus_alsa();

    bool init_out(  );
    bool init_in(  );
    bool deinit_in(  );
    bool init_out_sub(  );
    bool init_in_sub(  );

    void print();

    string get_name();
    int get_id();

    /* puts an event in the queue */
    void play( event *a_e24, unsigned char a_channel );
    void sysex( event *a_e24 );

    /* clock */
    void start();
    void stop();
    void clock(  long a_tick );
    void continue_from( long a_tick );
    void init_clock( long a_tick );
    void set_clock( clock_e a_clocking );
    clock_e get_clock( );

    void set_input( bool a_inputing );
    bool get_input( );

    void flush();
    //void remove_queued_on_events( int a_tag );

    /* master midi bus sets up the bus */
    friend class mastermidibus_alsa;

    /* address of client */
#if HAVE_LIBASOUND
    int get_client(void)
    {
        return m_dest_addr_client;
    };
    int get_port(void)
    {
        return m_dest_addr_port;
    };
#endif

    static void set_clock_mod( int a_clock_mod );
    static int get_clock_mod();
};

class mastermidibus_alsa : public mastermidibus_iface
{
private:

    /* sequencer client handle */
#if HAVE_LIBASOUND
    snd_seq_t *m_alsa_seq;
#endif

    int m_num_out_buses;
    int m_num_in_buses;

    midibus_alsa *m_buses_out[c_maxBuses];
    midibus_alsa *m_buses_in[c_maxBuses];
    midibus_alsa *m_bus_announce;

    bool m_buses_out_active[c_maxBuses];
    bool m_buses_in_active[c_maxBuses];

    bool m_buses_out_init[c_maxBuses];
    bool m_buses_in_init[c_maxBuses];

    clock_e m_init_clock[c_maxBuses];
    bool m_init_input[c_maxBuses];

    /* id of queue */
    int m_queue;

    int m_ppqn;
    double m_bpm;

    int  m_num_poll_descriptors;
    struct pollfd *m_poll_descriptors;

    /* for dumping midi input to sequence for recording */
    bool m_dumping_input;
    sequence *m_seq;
    vector < sequence *> m_vector_sequence;

    /* current note transpose value (for all transposable tracks) */
    int m_transpose;

    int m_swing_amount8;
    int m_swing_amount16;

    /* locking */
    seq42::mutex m_mutex;

    /* mutex */
    void lock();
    void unlock();

public:

    mastermidibus_alsa();
    ~mastermidibus_alsa() override;

    void init() override;

#if HAVE_LIBASOUND
    snd_seq_t* get_alsa_seq( )
    {
        return m_alsa_seq;
    };
#endif

    int get_num_out_buses() override;
    int get_num_in_buses() override;

    void set_bpm(double a_bpm) override;
    void set_ppqn(int a_ppqn) override;
    double get_bpm() override
    {
        return m_bpm;
    }
    int get_ppqn() override
    {
        return m_ppqn;
    }

    string get_midi_out_bus_name( int a_bus ) override;
    string get_midi_in_bus_name( int a_bus ) override;

    void set_transpose( int a_transpose ) override
    {
        m_transpose = a_transpose;
    }
    int get_transpose() override
    {
        return m_transpose;
    }

    void set_swing_amount8(int a_swing_amount) override
    {
        if ( a_swing_amount < 0 )  a_swing_amount = 0;
        if ( a_swing_amount > c_max_swing_amount ) a_swing_amount = c_max_swing_amount;
        m_swing_amount8 = a_swing_amount;
    }
    int get_swing_amount8( ) override
    {
        return  m_swing_amount8;
    }

    void set_swing_amount16(int a_swing_amount) override
    {
        if ( a_swing_amount < 0 )  a_swing_amount = 0;
        if ( a_swing_amount > c_max_swing_amount ) a_swing_amount = c_max_swing_amount;
        m_swing_amount16 = a_swing_amount;
    }
    int get_swing_amount16( ) override
    {
        return  m_swing_amount16;
    }

    void print();
    void flush() override;

    void start() override;
    void stop() override;

    void clock(  long a_tick ) override;
    void continue_from( long a_tick ) override;
    void init_clock( long a_tick ) override;

    int poll_for_midi( ) override;
    bool is_more_input( ) override;
    bool get_midi_event( event *a_in ) override;
    void set_sequence_input( bool a_state, sequence *a_seq ) override;
    void dump_midi_input(event a_in) override;

    bool is_dumping( ) override
    {
        return m_dumping_input;
    }

    sequence* get_sequence( )
    {
        return m_seq;
    }
    void sysex( event *a_event ) override;

    void port_start( int a_client, int a_port );
    void port_exit( int a_client, int a_port );

    void play( unsigned char a_bus, event *a_e24, unsigned char a_channel ) override;

    void set_clock( unsigned char a_bus, clock_e a_clock_type ) override;
    clock_e get_clock( unsigned char a_bus ) override;

    void set_input( unsigned char a_bus, bool a_inputing ) override;
    bool get_input( unsigned char a_bus ) override;
};

#endif
