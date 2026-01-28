#pragma once

#include <string>
#include "event.h"
#include "sequence.h"

enum clock_e
{
    e_clock_off,
    e_clock_pos,
    e_clock_mod
};

class mastermidibus_iface
{
public:
    virtual ~mastermidibus_iface() = default;

    virtual void init() = 0;

    virtual int get_num_out_buses() = 0;
    virtual int get_num_in_buses() = 0;

    virtual void set_bpm(double) = 0;
    virtual void set_ppqn(int) = 0;
    virtual double get_bpm() = 0;
    virtual int get_ppqn() = 0;

    virtual std::string get_midi_out_bus_name(int) = 0;
    virtual std::string get_midi_in_bus_name(int) = 0;

    virtual void set_transpose(int a_transpose) = 0;
    virtual int  get_transpose() = 0;

    virtual void set_swing_amount8(int a_swing_amount) = 0;
    virtual int  get_swing_amount8() = 0;

    virtual void set_swing_amount16(int a_swing_amount) = 0;
    virtual int  get_swing_amount16() = 0;

    virtual void flush() = 0;

    virtual void start() = 0;
    virtual void stop() = 0;
    virtual void clock(long) = 0;
    virtual void continue_from(long) = 0;
    virtual void init_clock(long) = 0;

    virtual int  poll_for_midi() = 0;
    virtual bool is_more_input() = 0;
    virtual bool get_midi_event(event *a_in) = 0;

    virtual void set_sequence_input(bool, sequence*) = 0;
    virtual void dump_midi_input(event a_in) = 0;
    virtual bool is_dumping( ) = 0;

    virtual void sysex(event *a_event) = 0;
    virtual void play(unsigned char bus, event *a_e24, unsigned char channel) = 0;

    virtual void set_clock(unsigned char bus, clock_e) = 0;
    virtual clock_e get_clock(unsigned char bus) = 0;

    virtual void set_input(unsigned char bus, bool) = 0;
    virtual bool get_input(unsigned char bus) = 0;
};
