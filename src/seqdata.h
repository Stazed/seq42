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
#include "seqkeys.h"

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

#include "globals.h"

/* Note or CC event velocity adjustment area */
class seqdata : public Gtk::DrawingArea
{

private:

    Glib::RefPtr<Gdk::Window> m_window;
    Cairo::RefPtr<Cairo::ImageSurface> m_surface;
    Cairo::RefPtr<Cairo::Context>  m_surface_window;
    
    sequence     * const m_seq;

    /* one pixel == m_zoom ticks */
    int          m_zoom;

    int m_window_x, m_window_y;

    int m_drop_x, m_drop_y;
    int m_current_x, m_current_y;

    Gtk::Adjustment   * const m_hadjust;

    int m_scroll_offset_ticks;
    int m_scroll_offset_x;

    /* what is the data window currently editing ? */
    unsigned char m_status;
    unsigned char m_cc;

    bool m_dragging;
    bool m_drag_handle;
    bool m_redraw_events;

    void on_realize();

    bool on_button_press_event(GdkEventButton* a_ev);
    bool on_button_release_event(GdkEventButton* a_ev);
    bool on_motion_notify_event(GdkEventMotion* a_p0);
    bool on_leave_notify_event(GdkEventCrossing* p0);
    bool on_scroll_event( GdkEventScroll* a_ev ) ;

    void update_sizes();
    void draw_line_on_window();

    void convert_x( int a_x, long *a_tick );

    void xy_to_rect( int a_x1,  int a_y1,
                     int a_x2,  int a_y2,
                     int *a_x,  int *a_y,
                     int *a_w,  int *a_h );

    void draw_events_on_window();

    void on_size_allocate(Gtk::Allocation& );
    void change_horz();

public:

    seqdata( sequence *a_seq, int a_zoom,  Gtk::Adjustment   *a_hadjust );

    void reset();
    void redraw();
    void set_zoom( int a_zoom );
    void set_data_type( unsigned char a_status, unsigned char a_control  );

    int idle_redraw();
    void queue_draw_background() {m_redraw_events = true;} ;

    friend class seqroll;
    friend class seqevent;
    friend class lfownd;
};

