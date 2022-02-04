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

#include "perform.h"
#include "track.h"
#include "trackmenu.h"

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

/* forward declaration */
class mainwnd;

using namespace Gtk;

#include "globals.h"

/* holds the left side track names */
class perfnames : public virtual Gtk::DrawingArea, public virtual trackmenu
{
private:

    Glib::RefPtr<Gdk::Window>   m_window;
    Gdk::Color    m_black, m_white, m_grey, m_orange, m_green;

    Glib::RefPtr<Gdk::Pixmap>   m_pixmap;

    perform      *m_mainperf;

    Adjustment   *m_vadjust;

    int m_window_x, m_window_y;

    int          m_track_offset;

    bool         m_track_active[c_max_track];
    
    bool         m_button_down;
    bool         m_moving;

    void on_realize();
    bool on_expose_event(GdkEventExpose* a_ev);
    bool on_button_press_event(GdkEventButton* a_ev);
    bool on_button_release_event(GdkEventButton* a_ev);
    bool on_motion_notify_event(GdkEventMotion* a_ev);
    void on_size_allocate(Gtk::Allocation& );
    bool on_scroll_event( GdkEventScroll* a_ev ) ;

    void draw_area();
    void update_pixmap();

    void convert_y( int a_y, int *a_note);

    void draw_track( int a_track );

    void change_vert();

    void redraw( int a_track );
    
    void check_global_solo_tracks();
    bool merge_tracks( track *a_merge_track );

public:

    void redraw_dirty_tracks();

    perfnames( perform *a_perf, mainwnd *a_main,
               Adjustment *a_vadjust   );
};

