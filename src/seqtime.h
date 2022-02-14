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

#include "sequence.h"
#include "seqtime.h"
#include "globals.h"

#ifdef GTKMM_3_SUPPORT
#include <gtkmm.h>
#else
#include <gtkmm/button.h>
#include <gtkmm/window.h>
#include <gtkmm/accelgroup.h>
#include <gtkmm/box.h>
#include <gtkmm/main.h>
#include <gtkmm/menu.h>
#include <gtkmm/menubar.h>
#include <gtkmm/eventbox.h>
#include <gtkmm/window.h>
#include <gtkmm/table.h>
#include <gtkmm/drawingarea.h>
#include <gtkmm/widget.h>
#include <gtkmm/adjustment.h>
#endif

using namespace Gtk;

/* The bar and measures above the sequence editor grid */
class seqtime: public Gtk::DrawingArea
{

private:

    Glib::RefPtr<Gdk::Window>   m_window;
    Cairo::RefPtr<Cairo::ImageSurface> m_surface;
    Cairo::RefPtr<Cairo::Context>  m_surface_window;
#ifdef GTKMM_3_SUPPORT
    Glib::RefPtr<Adjustment> m_hadjust;
#else
    Gtk::Adjustment   *m_hadjust;
#endif

    int m_scroll_offset_ticks;
    int m_scroll_offset_x;

    sequence     *m_seq;

    /* one pixel == m_zoom ticks */
    int          m_zoom;

    int m_window_x, m_window_y;
    bool m_redraw_window;

    void on_realize();
    bool on_expose_event(GdkEventExpose* a_ev);
    bool on_button_press_event(GdkEventButton* a_ev);
    bool on_button_release_event(GdkEventButton* a_ev);
    void draw_surface_on_window();
    void draw_progress_on_window();
    void update_surface();

    bool idle_progress();

    void on_size_allocate(Gtk::Allocation& );

    void change_horz();

    void update_sizes();

public:
#ifdef GTKMM_3_SUPPORT
    seqtime( sequence *a_seq, int a_zoom, Glib::RefPtr<Adjustment> a_hadjust );
#else
    seqtime( sequence *a_seq, int a_zoom, Gtk::Adjustment *a_hadjust );
#endif

    void reset();
    void redraw();
    void set_zoom( int a_zoom );
};
