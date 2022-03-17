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

#include "mainwnd.h"
#include "perform.h"
#include "seqtime.h"
#include "globals.h"

using namespace Gtk;

class mainwnd;

/* The 'L' and 'R' marker track for setting loops and edit markers */
class perftime: public Gtk::DrawingArea
{

private:

    Cairo::RefPtr<Cairo::ImageSurface> m_surface;
    Glib::RefPtr<Gdk::Pixbuf> m_pixbuf;

    perform      * const m_mainperf;
    mainwnd      * const m_mainwnd;

    Glib::RefPtr<Adjustment> const m_hadjust;

    int m_window_x, m_window_y;
    int m_perf_scale_x;

    int m_4bar_offset;

    int m_snap, m_measure_length;
    bool m_moving_left;
    bool m_moving_right;
    bool m_moving_paste;
    uint64_t m_paste_tick;
    bool m_draw_background;

    void on_realize();
    bool on_button_press_event(GdkEventButton* a_ev);
    bool on_button_release_event(GdkEventButton* a_ev);
    bool on_motion_notify_event(GdkEventMotion* a_ev);
    void on_size_allocate(Gtk::Allocation &a_r );

    void draw_background();
    void change_horz();

protected:
    bool on_draw(const Cairo::RefPtr<Cairo::Context>& cr);

public:

    perftime( perform *a_perf, mainwnd *a_main, Glib::RefPtr<Adjustment> a_hadjust );

    void set_horizontal_zoom (int a_zoom);
    void set_guides( int a_snap, int a_measure );
    void redraw();
    bool on_key_release_event(GdkEventKey* a_ev);
};

