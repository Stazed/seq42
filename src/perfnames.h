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
#include "globals.h"

/* forward declaration */
class mainwnd;

using namespace Gtk;


/* holds the left side track names */
class perfnames : public virtual Gtk::DrawingArea, public virtual trackmenu
{
private:

    Cairo::RefPtr<Cairo::ImageSurface> m_surface;
    Glib::RefPtr<Gdk::Pixbuf> m_pixbuf;

    perform      *m_mainperf;

    Glib::RefPtr<Adjustment> m_vadjust;
    bool m_redraw_tracks;

    int m_window_x, m_window_y;

    int          m_track_offset;
    int          m_names_y;
    float        m_vertical_zoom;

    bool         m_track_active[c_max_track];
    
    bool         m_button_down;
    bool         m_moving;

    void on_realize() override;
    bool on_button_press_event(GdkEventButton* a_ev) override;
    bool on_button_release_event(GdkEventButton* a_ev) override;
    bool on_motion_notify_event(GdkEventMotion* a_ev) override;
    void on_size_allocate(Gtk::Allocation& ) override;
    bool on_scroll_event( GdkEventScroll* a_ev ) override;

    void convert_y( int a_y, int *a_note);

    void draw_track( int a_track );

    void change_vert();

    void redraw( int a_track ) override;
    
    void check_global_solo_tracks();
    bool merge_tracks( track *a_merge_track );

public:

    void redraw_dirty_tracks();
    void set_vertical_zoom (float a_zoom);

    perfnames( perform *a_perf, mainwnd *a_main,
               Glib::RefPtr<Adjustment> a_vadjust   );
protected:
    bool on_draw(const Cairo::RefPtr<Cairo::Context>& cr);

};

