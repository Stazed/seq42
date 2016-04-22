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

#include <gtkmm.h>
#include <sigc++/bind.h>
#include "globals.h"
#include "sequence.h"
#include "seqdata.h"

using namespace Gtk;

class lfownd: public Gtk::Window
{
private:

    VScale *m_scale_value;
    VScale *m_scale_range;
    VScale *m_scale_speed;
    VScale *m_scale_phase;
    VScale *m_scale_wave;

    double m_value;
    double m_range;
    double m_speed;
    double m_phase;
    int m_wave;

    HBox *m_hbox;
    sequence *m_seq;
    seqdata *m_seqdata;

    void scale_lfo_change();
    bool on_focus_out_event(GdkEventFocus* p0 );

public:

    static double wave_func(double a_angle, int wave_type);
    lfownd (sequence *a_seq, seqdata *a_seqdata);
    void toggle_visible();
    virtual ~lfownd();
};
