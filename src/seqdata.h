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
#include "globals.h"

using namespace Gtk;

/* Note or CC event velocity adjustment area */
class seqdata : public Gtk::DrawingArea
{

private:

    Cairo::RefPtr<Cairo::ImageSurface> m_surface;

    sequence     * const m_seq;

    /* one pixel == m_zoom ticks */
    int          m_zoom;

    Glib::RefPtr<Adjustment> const m_hadjust;

    int m_window_x, m_window_y;

    int m_drop_x, m_drop_y;
    int m_current_x, m_current_y;

    int m_scroll_offset_ticks;
    int m_scroll_offset_x;

    /* what is the data window currently editing ? */
    unsigned char m_status;
    unsigned char m_cc;

    bool m_dragging;
    bool m_drag_handle;

    void on_realize();

    bool on_button_press_event(GdkEventButton* a_ev);
    bool on_button_release_event(GdkEventButton* a_ev);
    bool on_motion_notify_event(GdkEventMotion* a_p0);
    bool on_leave_notify_event(GdkEventCrossing* p0);
    bool on_scroll_event( GdkEventScroll* a_ev ) ;
    void on_size_allocate(Gtk::Allocation& );

    void update_sizes();
    void convert_x( int a_x, long *a_tick );

    void xy_to_rect( int a_x1,  int a_y1,
                     int a_x2,  int a_y2,
                     int *a_x,  int *a_y,
                     int *a_w,  int *a_h );

    void draw_events_on_window();

    void change_horz();

protected:
    bool on_draw(const Cairo::RefPtr<Cairo::Context>& cr);

public:

    seqdata(sequence *a_seq, int a_zoom, Glib::RefPtr<Adjustment> a_hadjust);

    void reset();
    void redraw();
    void set_zoom( int a_zoom );
    void set_data_type( unsigned char a_status, unsigned char a_control  );

    void queue_draw_background() {queue_draw();}

    friend class seqroll;
    friend class seqevent;
    friend class lfownd;
};

