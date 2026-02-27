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
//----------------------------------------------------------------------------
//
//  JACK MIDI backend for seq42 (Linux only)
//
//  This file is intended to mirror the ALSA midibus/mastermidibus interface,
//  but implemented using JACK MIDI ports + a JACK process callback.
//
//-----------------------------------------------------------------------------

#pragma once

#ifdef JACK_MIDI_SUPPORT

#include <jack/jack.h>
#include <jack/midiport.h>
#include <jack/ringbuffer.h>

#include <atomic>
#include <cstdint>
#include <string>
#include <vector>

#include "mastermidibus_iface.h"
#include "event.h"
#include "sequence.h"
#include "mutex.h"
#include "globals.h"

using std::string;
using std::vector;


/* forward declarations*/
class mastermidibus_jack;
class midibus_jack;

/*
 * A JACK "bus" in this context is one JACK MIDI output port (and optionally
 * a named destination concept purely for UI). Users connect ports in JACK.
 */
class midibus_jack
{
private:
    int     m_id;
    string  m_name;

    clock_e m_clock_type;
    bool    m_inputing;

    static int m_clock_mod;

    // Owned by mastermidibus_jack (lifetime > midibus_jack)
    jack_client_t * m_client;

    // Port for output or input (we use output buses + one shared input bus in master)
    jack_port_t * m_port;

    // Per-bus outgoing ringbuffer (bytes encoded as small packets)
    jack_ringbuffer_t * m_out_rb;

    // For clock generation
    long m_lasttick;

    // Lock for non-RT access to state; never used in RT callback.
    seq42::mutex m_mutex;

    void lock()   { m_mutex.lock(); }
    void unlock() { m_mutex.unlock(); }

    bool enqueue_bytes(const uint8_t *data, uint32_t sz);
    bool enqueue_short(uint8_t b0, uint8_t b1, uint8_t b2, uint32_t sz);
    bool enqueue_realtime(uint8_t status);

public:
    midibus_jack
    (
        jack_client_t *client,
        const string &port_name,
        int a_id
    );

    ~midibus_jack();

    // Mirrors ALSA API shape (some are no-ops in JACK backend)
    bool init_out();
    bool init_in();        // generally unused per-output-bus; input handled in master
    bool deinit_in();
    bool init_out_sub();
    bool init_in_sub();

    void print();
    string get_name() const { return m_name; }
    int get_id() const { return m_id; }

    // Output
    void play(event *a_e24, unsigned char a_channel);
    void sysex(event *a_e24);

    // Clock/control
    void start();
    void stop();
    void clock(long a_tick);
    void continue_from(long a_tick);
    void init_clock(long a_tick);
    void set_clock(clock_e a_clocking) { m_clock_type = a_clocking; }
    clock_e get_clock() const { return m_clock_type; }

    void set_input(bool a_inputing);
    bool get_input() const { return m_inputing; }

    void flush(); // no-op for JACK (buffer flushed in process callback)

    jack_port_t * port() const { return m_port; }
    jack_ringbuffer_t * out_rb() const { return m_out_rb; }

    static void set_clock_mod(int a_clock_mod);
    static int get_clock_mod();
};

/*
 * JACK master: owns the JACK client, registers ports, runs process callback,
 * provides polling-style input API to the rest of seq42.
 */
class mastermidibus_jack : public mastermidibus_iface
{
private:
    jack_client_t * m_client {};
    jack_nframes_t  m_sample_rate { 48000 };

    int m_num_out_buses {0};
    int m_num_in_buses  {0};

    midibus_jack * m_buses_out[c_maxBuses] {};
    bool           m_buses_out_active[c_maxBuses] {};
    bool           m_buses_out_init[c_maxBuses] {};
    clock_e        m_init_clock[c_maxBuses];
    bool           m_init_input;

    // Single JACK MIDI input port (typical for sequencers)
    jack_port_t * m_in_port {};
    bool          m_in_active {true};

    // Input ringbuffer: packets of (u32 size, u64 frame_time, bytes[size])
    jack_ringbuffer_t * m_in_rb {};
    static constexpr size_t c_in_rb_bytes  = 1 << 20;   // 1 MiB
public:
    static constexpr size_t c_out_rb_bytes = 1 << 18;   // 256 KiB per bus
private:

    // Timing
    std::atomic<int>    m_ppqn { c_ppqn };
    std::atomic<double> m_bpm  { double(c_bpm) };

    // For dumping MIDI input into sequences for recording
    bool m_dumping_input {false};
    sequence *m_seq {nullptr};
    vector<sequence *> m_vector_sequence;

    int m_transpose {0};
    int m_swing_amount8 {0};
    int m_swing_amount16 {0};

    seq42::mutex m_mutex;
    void lock()   { m_mutex.lock(); }
    void unlock() { m_mutex.unlock(); }

    // JACK callbacks
    static int  s_process(jack_nframes_t nframes, void *arg);
    static void s_shutdown(void *arg);

    int process(jack_nframes_t nframes);

    // Helpers
    long frame_to_tick(jack_nframes_t abs_frame) const;
    void push_in_event(jack_nframes_t abs_frame, const uint8_t *data, uint32_t sz);
    bool pop_in_event(jack_nframes_t &abs_frame, uint8_t *dst, uint32_t &sz, uint32_t dst_cap);

public:
    mastermidibus_jack();
    ~mastermidibus_jack() override;

    void init() override;

    int get_num_out_buses() override { return m_num_out_buses; }
    int get_num_in_buses() override { return m_num_in_buses; }

    void set_bpm(double a_bpm) override;
    void set_ppqn(int a_ppqn) override;

    double get_bpm() override { return m_bpm.load(); }
    int get_ppqn() override { return m_ppqn.load(); }

    string get_midi_out_bus_name(int a_bus) override;
    string get_midi_in_bus_name(int /*a_bus*/) override { return string("[0] JACK MIDI in"); }

    void set_transpose(int a_transpose) override { m_transpose = a_transpose; }
    int  get_transpose() override { return m_transpose; }

    void set_swing_amount8(int a_swing_amount) override;
    int  get_swing_amount8() override { return m_swing_amount8; }

    void set_swing_amount16(int a_swing_amount) override;
    int  get_swing_amount16() override { return m_swing_amount16; }

    void print();
    void flush() override; // no-op

    // Transport-ish
    void start() override;
    void stop() override;
    void clock(long a_tick) override;
    void continue_from(long a_tick) override;
    void init_clock(long a_tick) override;

    // Input polling-style API
    int  poll_for_midi() override;
    bool is_more_input() override;
    bool get_midi_event(event *a_in) override;

    void set_sequence_input(bool a_state, sequence *a_seq) override;
    void dump_midi_input(event a_in) override;
    bool is_dumping() override { return m_dumping_input; }
    sequence* get_sequence() const { return m_seq; }

    void sysex(event *a_event) override;

    // Output
    void play(unsigned char a_bus, event *a_e24, unsigned char a_channel) override;

    void set_clock(unsigned char a_bus, clock_e a_clock_type) override;
    clock_e get_clock(unsigned char a_bus) override;

    void set_input(unsigned char /*a_bus*/, bool a_inputing) override {m_init_input = a_inputing;} // JACK input is single port
    bool get_input(unsigned char /*a_bus*/) override { return m_init_input; }

    jack_client_t * client() const { return m_client; }
};

#endif // JACK_MIDI_SUPPORT
