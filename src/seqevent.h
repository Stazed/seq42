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
#include <gdkmm/cursor.h>
#include <gtkmm/adjustment.h>

#include "globals.h"
#include "seqdata.h"

using namespace Gtk;

// interaction methods
class seqevent;
struct FruitySeqEventInput
{
    FruitySeqEventInput() : m_justselected_one(false),
        m_is_drag_pasting_start(false),
        m_is_drag_pasting(false)
    {}
    bool m_justselected_one;
    bool m_is_drag_pasting_start;
    bool m_is_drag_pasting;
    bool on_button_press_event(GdkEventButton* a_ev, seqevent& ths);
    bool on_button_release_event(GdkEventButton* a_ev, seqevent& ths);
    bool on_motion_notify_event(GdkEventMotion* a_ev, seqevent& ths);
    void updateMousePtr(seqevent& ths);
};
struct Seq42SeqEventInput
{
    Seq42SeqEventInput() : m_adding( false ) {}
    bool on_button_press_event(GdkEventButton* a_ev, seqevent& ths);
    bool on_button_release_event(GdkEventButton* a_ev, seqevent& ths);
    bool on_motion_notify_event(GdkEventMotion* a_ev, seqevent& ths);
    void set_adding( bool a_adding, seqevent& ths );
    bool m_adding;
};

/* The small event grid below the note editor and above the velocity editor
 * shows the vertical event notes and CCs.
 */
class seqevent : public Gtk::DrawingArea
{

private:
    friend struct FruitySeqEventInput;
    FruitySeqEventInput m_fruity_interaction;

    friend struct Seq42SeqEventInput;
    Seq42SeqEventInput m_seq42_interaction;

    Cairo::RefPtr<Cairo::ImageSurface> m_surface;

    GdkRectangle m_old;
    GdkRectangle m_selected;

    Glib::RefPtr<Adjustment> m_hadjust;

    int m_scroll_offset_ticks;
    int m_scroll_offset_x;

    sequence     *m_seq;
    seqdata      *m_seqdata_wid;

    /* one pixel == m_zoom ticks */
    int          m_zoom;
    int          m_snap;

    int m_window_x, m_window_y;

    /* when highlighting a bunch of events */
    bool m_selecting;
    bool m_moving_init;
    bool m_moving;
    bool m_growing;
    bool m_painting;
    bool m_paste;

    /* where the dragging started */
    int m_drop_x;
    int m_drop_y;
    int m_current_x;
    int m_current_y;

    int m_move_snap_offset_x;

    /* what is the data window currently editing ? */
    unsigned char m_status;
    unsigned char m_cc;

    void on_realize();
    bool on_button_press_event(GdkEventButton* a_ev);
    bool on_button_release_event(GdkEventButton* a_ev);
    bool on_motion_notify_event(GdkEventMotion* a_ev);
    bool on_key_press_event(GdkEventKey* a_p0);
    bool on_focus_in_event(GdkEventFocus*);
    bool on_focus_out_event(GdkEventFocus*);
    void on_size_allocate(Gtk::Allocation& );

    void convert_x( int a_x, long *a_ticks );
    void convert_t( long a_ticks, int *a_x );

    void snap_y( int *a_y );
    void snap_x( int *a_x );

    void x_to_w( int a_x1, int a_x2,
                 int *a_x, int *a_w  );

    void drop_event( long a_tick );
    void draw_events_on ();

    void start_paste();

    void change_horz();
    void update_sizes();
    void draw_background();
    void update_surface();

protected:
    bool on_draw(const Cairo::RefPtr<Cairo::Context>& cr);

public:

    seqevent( sequence *a_seq,
              int a_zoom,
              int a_snap,
              seqdata *a_seqdata_wid,
              Glib::RefPtr<Adjustment> a_hadjust);

    void reset();
    void redraw();
    void set_zoom( int a_zoom );
    void set_snap( int a_snap );

    void set_data_type( unsigned char a_status, unsigned char a_control  );
};

