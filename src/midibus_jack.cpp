//----------------------------------------------------------------------------
//
//  JACK MIDI backend for seq42 (Linux only)
//
//-----------------------------------------------------------------------------

#include "midibus_jack.h"

#ifdef JACK_MIDI_SUPPORT

#include <unistd.h>     // usleep
#include <cstring>      // memcpy
#include <cstdio>       // snprintf (non-RT only)
#include <algorithm>

static inline uint32_t u32min(uint32_t a, uint32_t b) { return a < b ? a : b; }

int midibus_jack::m_clock_mod = 16 * 4;

void midibus_jack::set_clock_mod(int a_clock_mod)
{
    if (a_clock_mod != 0)
        m_clock_mod = a_clock_mod;
}

int midibus_jack::get_clock_mod()
{
    return m_clock_mod;
}

/*
 * Packet format in out ringbuffer:
 *   u32 size
 *   u8  bytes[size]
 *
 * (No timestamps; we emit at time=0 in next process cycle.)
 */

bool midibus_jack::enqueue_bytes(const uint8_t *data, uint32_t sz)
{
    if (!m_out_rb || !data || sz == 0)
        return false;

    const uint32_t need = uint32_t(sizeof(uint32_t)) + sz;
    if (jack_ringbuffer_write_space(m_out_rb) < need)
        return false;

    jack_ringbuffer_write(m_out_rb, (const char*)&sz, sizeof(uint32_t));
    jack_ringbuffer_write(m_out_rb, (const char*)data, sz);
    return true;
}

bool midibus_jack::enqueue_short(uint8_t b0, uint8_t b1, uint8_t b2, uint32_t sz)
{
    uint8_t tmp[3] = { b0, b1, b2 };
    return enqueue_bytes(tmp, sz);
}

void midibus_jack::enqueue_realtime(uint8_t status)
{
    (void) enqueue_bytes(&status, 1);
}

midibus_jack::midibus_jack(jack_client_t *client, const string &port_name, int a_id) :
    m_id(a_id),
    m_name(port_name),
    m_clock_type(e_clock_off),
    m_inputing(false),
    m_client(client),
    m_port(nullptr),
    m_out_rb(nullptr),
    m_lasttick(-1)
{
    // Preallocate ringbuffer for outgoing messages (RT-safe access)
    m_out_rb = jack_ringbuffer_create(mastermidibus_jack::c_out_rb_bytes);
    if (m_out_rb)
        jack_ringbuffer_mlock(m_out_rb);
}

midibus_jack::~midibus_jack()
{
    if (m_out_rb)
    {
        jack_ringbuffer_free(m_out_rb);
        m_out_rb = nullptr;
    }
    // Port is owned/closed when client closes; no need to unregister here.
}

bool midibus_jack::init_out()
{
    if (!m_client)
        return false;

    // Register a JACK MIDI output port
    m_port = jack_port_register
    (
        m_client,
        m_name.c_str(),
        JACK_DEFAULT_MIDI_TYPE,
        JackPortIsOutput,
        0
    );

    return m_port != nullptr;
}

bool midibus_jack::init_out_sub()
{
    // Same as init_out in JACK (subscriptions handled externally via JACK connections)
    return init_out();
}

bool midibus_jack::init_in()
{
    // Input is handled as a single port in mastermidibus_jack
    return true;
}

bool midibus_jack::init_in_sub()
{
    return init_in();
}

bool midibus_jack::deinit_in()
{
    return true;
}

void midibus_jack::print()
{
    // non-RT
    printf("%s", m_name.c_str());
}

void midibus_jack::flush()
{
    // No-op: JACK MIDI buffers are produced/consumed in process callback
}

void midibus_jack::play(event *a_e24, unsigned char a_channel)
{
    if (!a_e24)
        return;

    lock();

    // Encode to raw MIDI bytes similar to ALSA version
    uint8_t status = uint8_t(a_e24->get_status() + (a_channel & 0x0F));
    uint8_t d1 = 0, d2 = 0;
    a_e24->get_data(&d1, &d2);

    // Determine message size from status nibble
    const uint8_t high = status & 0xF0;

    if (status >= 0xF8)               // realtime single-byte
        enqueue_short(status, 0, 0, 1);
    else if (status == 0xF1 || status == 0xF3) // 2-byte system common
        enqueue_short(status, d1, 0, 2);
    else if (status == 0xF2)          // Song Position Pointer: 3 bytes
        enqueue_short(status, d1, d2, 3);
    else if (high == 0xC0 || high == 0xD0) // Program change / Channel pressure
        enqueue_short(status, d1, 0, 2);
    else
        enqueue_short(status, d1, d2, 3);

    unlock();
}

void midibus_jack::sysex(event *a_e24)
{
    if (!a_e24)
        return;

    lock();

    uint8_t *data = a_e24->get_sysex();
    long sz = a_e24->get_size();
    if (data && sz > 0)
    {
        // JACK MIDI can carry variable-length; will be written as a single event if it fits
        enqueue_bytes(data, (uint32_t)sz);
    }

    unlock();
}

void midibus_jack::start()
{
    m_lasttick = -1;
    if (m_clock_type != e_clock_off)
        enqueue_realtime(0xFA); // MIDI Start
}

void midibus_jack::stop()
{
    m_lasttick = -1;
    if (m_clock_type != e_clock_off)
        enqueue_realtime(0xFC); // MIDI Stop
}

void midibus_jack::continue_from(long a_tick)
{
    // MIDI SPP value is in MIDI beats (16th notes), i.e. a_tick / (ppqn/4)
    // We can’t access ppqn directly here; use global c_ppqn like ALSA code.
    long pp16 = (c_ppqn / 4);
    long leftover = (a_tick % pp16);
    long beats = (a_tick / pp16);
    long starting_tick = a_tick - leftover;
    if (leftover > 0)
        starting_tick += pp16;

    m_lasttick = starting_tick - 1;

    if (m_clock_type != e_clock_off)
    {
        // Song Position Pointer: 0xF2, 14-bit value LSB/MSB
        uint16_t spp = (uint16_t) (beats & 0x3FFF);
        uint8_t lsb = uint8_t(spp & 0x7F);
        uint8_t msb = uint8_t((spp >> 7) & 0x7F);

        enqueue_short(0xF2, lsb, msb, 3); // SPP
        enqueue_realtime(0xFB);           // Continue
    }
}

void midibus_jack::init_clock(long a_tick)
{
    if (m_clock_type == e_clock_pos && a_tick != 0)
    {
        continue_from(a_tick);
    }
    else if (m_clock_type == e_clock_mod || a_tick == 0)
    {
        start();

        long clock_mod_ticks = (c_ppqn / 4) * m_clock_mod;
        long leftover = (a_tick % clock_mod_ticks);
        long starting_tick = a_tick - leftover;

        if (leftover > 0)
            starting_tick += clock_mod_ticks;

        m_lasttick = starting_tick - 1;
    }
}

void midibus_jack::clock(long a_tick)
{
    lock();

    if (m_clock_type != e_clock_off)
    {
        bool done = false;
        long uptotick = a_tick;

        if (m_lasttick >= uptotick)
            done = true;

        while (!done)
        {
            m_lasttick++;

            if (m_lasttick >= uptotick)
                done = true;

            // 24 clocks per quarter note => one clock per (ppqn/24) ticks
            if (m_lasttick % (c_ppqn / 24) == 0)
                enqueue_realtime(0xF8); // MIDI Clock
        }
    }

    unlock();
}

void midibus_jack::set_input(bool a_inputing)
{
    m_inputing = a_inputing;
    // No per-bus subscription; user connects to master input port.
}

/* ------------------------------------------------------------------------ */

mastermidibus_jack::mastermidibus_jack()
{
    for (int i = 0; i < c_maxBuses; ++i)
    {
        m_buses_out_active[i] = false;
        m_buses_out_init[i] = false;
        m_buses_out[i] = nullptr;
    }

    // Input ringbuffer for decoded raw MIDI packets
    m_in_rb = jack_ringbuffer_create(c_in_rb_bytes);
    if (m_in_rb)
        jack_ringbuffer_mlock(m_in_rb);

    // Open JACK client (no printf here in library code; keep it simple)
    m_client = jack_client_open(global_client_name.c_str(), JackNullOption, nullptr);
    if (!m_client)
    {
        // Leave uninitialized; init() will no-op safely
        return;
    }

    jack_on_shutdown(m_client, &mastermidibus_jack::s_shutdown, this);
    jack_set_process_callback(m_client, &mastermidibus_jack::s_process, this);

    m_sample_rate = jack_get_sample_rate(m_client);
}

mastermidibus_jack::~mastermidibus_jack()
{
    for (int i = 0; i < m_num_out_buses; ++i)
        delete m_buses_out[i];

    if (m_in_rb)
    {
        jack_ringbuffer_free(m_in_rb);
        m_in_rb = nullptr;
    }

    if (m_client)
    {
        jack_client_close(m_client);
        m_client = nullptr;
    }
}

void mastermidibus_jack::s_shutdown(void *arg)
{
    // JACK is going away. Avoid heavy work; just mark client null.
    auto *self = static_cast<mastermidibus_jack*>(arg);
    self->m_client = nullptr;
}

int mastermidibus_jack::s_process(jack_nframes_t nframes, void *arg)
{
    return static_cast<mastermidibus_jack*>(arg)->process(nframes);
}

long mastermidibus_jack::frame_to_tick(jack_nframes_t abs_frame) const
{
    const double bpm  = m_bpm.load();
    const int ppqn    = m_ppqn.load();
    if (bpm <= 0.0 || ppqn <= 0 || m_sample_rate == 0)
        return 0;

    // frames_per_tick = (sample_rate * 60) / (bpm * ppqn)
    const double frames_per_tick = (double(m_sample_rate) * 60.0) / (bpm * double(ppqn));
    const double tick = double(abs_frame) / frames_per_tick;
    return (long) (tick + 0.5); // nearest
}

void mastermidibus_jack::push_in_event(jack_nframes_t abs_frame, const uint8_t *data, uint32_t sz)
{
    if (!m_in_rb || !data || sz == 0)
        return;

    // Packet: u32 sz, u64 abs_frame, bytes[sz]
    const uint32_t need = uint32_t(sizeof(uint32_t) + sizeof(uint64_t) + sz);
    if (jack_ringbuffer_write_space(m_in_rb) < need)
        return; // drop if overflow; RT-safe

    const uint64_t t = (uint64_t)abs_frame;
    jack_ringbuffer_write(m_in_rb, (const char*)&sz, sizeof(uint32_t));
    jack_ringbuffer_write(m_in_rb, (const char*)&t, sizeof(uint64_t));
    jack_ringbuffer_write(m_in_rb, (const char*)data, sz);
}

bool mastermidibus_jack::pop_in_event(jack_nframes_t &abs_frame, uint8_t *dst, uint32_t &sz, uint32_t dst_cap)
{
    if (!m_in_rb)
        return false;

    if (jack_ringbuffer_read_space(m_in_rb) < (sizeof(uint32_t) + sizeof(uint64_t)))
        return false;

    uint32_t msgsz = 0;
    uint64_t t = 0;

    jack_ringbuffer_read(m_in_rb, (char*)&msgsz, sizeof(uint32_t));
    jack_ringbuffer_read(m_in_rb, (char*)&t, sizeof(uint64_t));

    if (msgsz == 0 || msgsz > dst_cap)
    {
        // If oversized, drop payload safely
        const size_t avail = jack_ringbuffer_read_space(m_in_rb);
        const size_t drop = std::min<size_t>(avail, msgsz);
        char tmp[256];
        size_t left = drop;
        while (left > 0)
        {
            size_t chunk = std::min<size_t>(left, sizeof(tmp));
            jack_ringbuffer_read(m_in_rb, tmp, chunk);
            left -= chunk;
        }
        return false;
    }

    if (jack_ringbuffer_read_space(m_in_rb) < msgsz)
        return false; // malformed

    jack_ringbuffer_read(m_in_rb, (char*)dst, msgsz);

    abs_frame = (jack_nframes_t)t;
    sz = msgsz;
    return true;
}

int mastermidibus_jack::process(jack_nframes_t nframes)
{
    if (!m_client)
        return 0;

    // 1) Output: write per-bus MIDI events into each port buffer
    for (int i = 0; i < m_num_out_buses; ++i)
    {
        midibus_jack *bus = m_buses_out[i];
        if (!bus || !m_buses_out_active[i])
            continue;

        void *obuf = jack_port_get_buffer(bus->port(), nframes);
        jack_midi_clear_buffer(obuf);

        jack_ringbuffer_t *rb = bus->out_rb();
        if (!rb)
            continue;

        // Drain packets: [u32 sz][bytes...]
        while (jack_ringbuffer_read_space(rb) >= sizeof(uint32_t))
        {
            uint32_t sz = 0;

            // Peek size safely: read then possibly rollback is not supported,
            // so we read size and verify enough remains.
            jack_ringbuffer_read(rb, (char*)&sz, sizeof(uint32_t));
            if (sz == 0)
                continue;

            if (jack_ringbuffer_read_space(rb) < sz)
            {
                // Not enough payload (shouldn't happen in correct writer),
                // drop remainder defensively by clearing buffer.
                break;
            }

            uint8_t tmp[1024];
            if (sz > sizeof(tmp))
            {
                // Drop too-large events rather than malloc in RT
                // Consume payload
                size_t left = sz;
                char sink[256];
                while (left > 0)
                {
                    size_t chunk = std::min<size_t>(left, sizeof(sink));
                    jack_ringbuffer_read(rb, sink, chunk);
                    left -= chunk;
                }
                continue;
            }

            jack_ringbuffer_read(rb, (char*)tmp, sz);

            // Write at time=0 in current process cycle.
            // If the buffer is full, jack_midi_event_write returns non-zero; drop.
            (void) jack_midi_event_write(obuf, 0, tmp, sz);
        }
    }

    // 2) Input: copy incoming events from JACK input port into input ringbuffer
    if (m_in_port && m_in_active)
    {
        void *ibuf = jack_port_get_buffer(m_in_port, nframes);
        const uint32_t ecnt = jack_midi_get_event_count(ibuf);
        const jack_nframes_t base = jack_last_frame_time(m_client);

        for (uint32_t i = 0; i < ecnt; ++i)
        {
            jack_midi_event_t ev {};
            if (jack_midi_event_get(&ev, ibuf, i) == 0 && ev.buffer && ev.size > 0)
            {
                // Absolute frame timestamp for this event
                const jack_nframes_t abs_frame = base + ev.time;
                push_in_event(abs_frame, ev.buffer, (uint32_t)ev.size);
            }
        }
    }

    return 0;
}

void mastermidibus_jack::init()
{
    if (!m_client)
        return;

    // Register one JACK MIDI input port
    m_in_port = jack_port_register
    (
        m_client,
        "midi_in",
        JACK_DEFAULT_MIDI_TYPE,
        JackPortIsInput,
        0
    );

    // Create output buses (fixed set like "manual" mode)
    const int num_buses = 16;
    m_num_out_buses = num_buses;
    m_num_in_buses  = 1;

    for (int i = 0; i < num_buses; ++i)
    {
        char pname[64];

        // Use alias if defined (like ALSA code does)
        if (global_user_midi_bus_definitions[i].alias.length() > 0)
        {
            std::snprintf(pname, sizeof(pname), "out_%02d_%s", i + 1,
                          global_user_midi_bus_definitions[i].alias.c_str());
        }
        else
        {
            std::snprintf(pname, sizeof(pname), "out_%02d", i + 1);
        }

        m_buses_out[i] = new midibus_jack(m_client, pname, i);
        m_buses_out_init[i] = true;

        if (m_buses_out[i]->init_out_sub())
        {
            m_buses_out_active[i] = true;
        }
        else
        {
            m_buses_out_active[i] = false;
        }
    }

    // Activate JACK client (starts process callback)
    jack_activate(m_client);

    // Defaults
    set_bpm(c_bpm);
    set_ppqn(c_ppqn);
    set_swing_amount8(0);
    set_swing_amount16(0);

    set_sequence_input(false, nullptr);
}

void mastermidibus_jack::set_ppqn(int a_ppqn)
{
    if (a_ppqn > 0)
        m_ppqn.store(a_ppqn);
}

void mastermidibus_jack::set_bpm(double a_bpm)
{
    if (a_bpm > 0.0)
        m_bpm.store(a_bpm);
}

void mastermidibus_jack::set_swing_amount8(int a_swing_amount)
{
    if (a_swing_amount < 0) a_swing_amount = 0;
    if (a_swing_amount > c_max_swing_amount) a_swing_amount = c_max_swing_amount;
    m_swing_amount8 = a_swing_amount;
}

void mastermidibus_jack::set_swing_amount16(int a_swing_amount)
{
    if (a_swing_amount < 0) a_swing_amount = 0;
    if (a_swing_amount > c_max_swing_amount) a_swing_amount = c_max_swing_amount;
    m_swing_amount16 = a_swing_amount;
}

string mastermidibus_jack::get_midi_out_bus_name(int a_bus)
{
    if (a_bus < 0 || a_bus >= m_num_out_buses || !m_buses_out_init[a_bus])
        return string("[?] (unconnected)");

    if (m_buses_out_active[a_bus])
        return string("[") + std::to_string(a_bus) + "] " + m_buses_out[a_bus]->get_name();

    return string("[") + std::to_string(a_bus) + "] " + m_buses_out[a_bus]->get_name() + " (inactive)";
}

void mastermidibus_jack::print()
{
    printf("Available JACK MIDI Output Buses\n");
    for (int i = 0; i < m_num_out_buses; ++i)
        printf("%s\n", m_buses_out[i] ? m_buses_out[i]->get_name().c_str() : "(null)");
}

void mastermidibus_jack::flush()
{
    // No-op: JACK pushes each cycle
}

void mastermidibus_jack::start()
{
    lock();
    for (int i = 0; i < m_num_out_buses; ++i)
        if (m_buses_out[i] && m_buses_out_active[i])
            m_buses_out[i]->start();
    unlock();
}

void mastermidibus_jack::continue_from(long a_tick)
{
    lock();
    for (int i = 0; i < m_num_out_buses; ++i)
        if (m_buses_out[i] && m_buses_out_active[i])
            m_buses_out[i]->continue_from(a_tick);
    unlock();
}

void mastermidibus_jack::init_clock(long a_tick)
{
    lock();
    for (int i = 0; i < m_num_out_buses; ++i)
        if (m_buses_out[i] && m_buses_out_active[i])
            m_buses_out[i]->init_clock(a_tick);
    unlock();
}

void mastermidibus_jack::stop()
{
    lock();
    for (int i = 0; i < m_num_out_buses; ++i)
        if (m_buses_out[i] && m_buses_out_active[i])
            m_buses_out[i]->stop();
    unlock();
}

void mastermidibus_jack::clock(long a_tick)
{
    lock();
    for (int i = 0; i < m_num_out_buses; ++i)
        if (m_buses_out[i] && m_buses_out_active[i])
            m_buses_out[i]->clock(a_tick);
    unlock();
}

void mastermidibus_jack::sysex(event *a_ev)
{
    lock();
    for (int i = 0; i < m_num_out_buses; ++i)
        if (m_buses_out[i] && m_buses_out_active[i])
            m_buses_out[i]->sysex(a_ev);
    unlock();
}

void mastermidibus_jack::play(unsigned char a_bus, event *a_e24, unsigned char a_channel)
{
    if (a_bus >= (unsigned char)m_num_out_buses)
        return;

    lock();
    if (m_buses_out[a_bus] && m_buses_out_active[a_bus])
        m_buses_out[a_bus]->play(a_e24, a_channel);
    unlock();
}

void mastermidibus_jack::set_clock(unsigned char a_bus, clock_e a_clock_type)
{
    if (a_bus >= (unsigned char)m_num_out_buses)
        return;

    lock();
    if (m_buses_out[a_bus] && m_buses_out_active[a_bus])
        m_buses_out[a_bus]->set_clock(a_clock_type);
    unlock();
}

clock_e mastermidibus_jack::get_clock(unsigned char a_bus)
{
    if (a_bus >= (unsigned char)m_num_out_buses)
        return e_clock_off;

    if (m_buses_out[a_bus] && m_buses_out_active[a_bus])
        return m_buses_out[a_bus]->get_clock();

    return e_clock_off;
}

int mastermidibus_jack::poll_for_midi()
{
    // JACK has no pollfd like ALSA sequencer here; use a bounded wait.
    // Return 1 if input available, 0 otherwise (mirrors "poll" style).
    const int max_ms = 1000;
    for (int i = 0; i < max_ms; ++i)
    {
        if (is_more_input())
            return 1;
        usleep(1000); // 1 ms
    }
    return 0;
}

bool mastermidibus_jack::is_more_input()
{
    if (!m_in_rb)
        return false;

    return jack_ringbuffer_read_space(m_in_rb) >= (sizeof(uint32_t) + sizeof(uint64_t));
}

bool mastermidibus_jack::get_midi_event(event *a_in)
{
    if (!a_in)
        return false;

    lock();

    uint8_t buffer[0x1000];
    uint32_t sz = 0;
    jack_nframes_t abs_frame = 0;

    if (!pop_in_event(abs_frame, buffer, sz, sizeof(buffer)))
    {
        unlock();
        return false;
    }

    // Convert timestamp to seq42 tick domain
    const long tick = frame_to_tick(abs_frame);
    a_in->set_timestamp(tick);
    a_in->set_size(sz);

    // Status handling similar to ALSA path
    const uint8_t status = buffer[0];

    if ((global_pass_sysex || global_showmidi) && status == EVENT_SYSEX)
    {
        a_in->start_sysex();
        (void)a_in->append_sysex(buffer, sz);
    }
    else
    {
        a_in->set_status(status, true); // true = do not clear channel bit

        uint8_t d1 = (sz > 1) ? buffer[1] : 0;
        uint8_t d2 = (sz > 2) ? buffer[2] : 0;
        a_in->set_data(d1, d2);

        // Note-on with vel 0 => note-off
        if (a_in->get_status() == EVENT_NOTE_ON && a_in->get_note_velocity() == 0x00)
            a_in->set_status(EVENT_NOTE_OFF, true);
    }

    unlock();
    return true;
}

void mastermidibus_jack::set_sequence_input(bool a_state, sequence *a_seq)
{
    lock();

    if (!a_state && a_seq == nullptr)
        m_vector_sequence.clear();

    if (!a_state && a_seq != nullptr)
    {
        for (unsigned i = 0; i < m_vector_sequence.size(); ++i)
        {
            if (m_vector_sequence[i] == a_seq)
            {
                m_vector_sequence.erase(m_vector_sequence.begin() + i);
                break;
            }
        }
    }

    if (a_state && a_seq != nullptr)
    {
        bool have = false;
        for (unsigned i = 0; i < m_vector_sequence.size(); ++i)
            if (m_vector_sequence[i] == a_seq)
                have = true;

        if (!have)
            m_vector_sequence.push_back(a_seq);
    }

    m_dumping_input = !m_vector_sequence.empty();

    unlock();
}

void mastermidibus_jack::dump_midi_input(event a_in)
{
    for (unsigned i = 0; i < m_vector_sequence.size(); ++i)
    {
        if ((m_vector_sequence[i] == nullptr) ||
            m_vector_sequence[i]->stream_event(&a_in))
        {
            break;
        }
    }
}

#endif // JACK_MIDI_SUPPORT
