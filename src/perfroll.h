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
#include <gtkmm/scrollbar.h>
#include <gtkmm/adjustment.h>
#include <gdkmm/cursor.h>

#include "globals.h"
#include "perform.h"
#include "mutex.h"
#include "track.h"
#include "trigger.h"


using namespace Gtk;

#include "perfroll_input.h"

const int c_perfroll_background_x = (c_ppqn * 4 * 16) / c_perf_scale_x;
const int c_perfroll_size_box_w = 3;
const int c_perfroll_size_box_click_w = c_perfroll_size_box_w+1 ;

/* performance roll */
class perfroll : public Gtk::DrawingArea
{
 private:
    friend class FruityPerfInput;
    FruityPerfInput m_fruity_interaction;

    friend class Seq42PerfInput;
    Seq42PerfInput m_seq42_interaction;

    Glib::RefPtr<Gdk::GC> m_gc;
    Glib::RefPtr<Gdk::Window> m_window;
    Gdk::Color    m_black, m_white, m_grey, m_lt_grey;

    Glib::RefPtr<Gdk::Pixmap> m_pixmap;
    Glib::RefPtr<Gdk::Pixmap> m_background;


    perform        *m_mainperf;

    int          m_snap;
    int          m_measure_length;
    int          m_beat_length;

    int          m_window_x, m_window_y;

    long         m_old_progress_ticks;

    int          m_4bar_offset;
    int          m_track_offset;

    int          m_roll_length_ticks;

    int          m_drop_x, m_drop_y;

    long         m_drop_tick;
    long         m_drop_tick_trigger_offset;
    int          m_drop_track;

    bool         m_track_active[c_max_track];

    Adjustment   *m_vadjust;
    Adjustment   *m_hadjust;

    bool m_moving;
    bool m_growing;
    bool m_grow_direction;

    void on_realize();
    bool on_expose_event(GdkEventExpose* a_ev);
    bool on_button_press_event(GdkEventButton* a_ev);
    bool on_button_release_event(GdkEventButton* a_ev);
    bool on_motion_notify_event(GdkEventMotion* a_ev);
    bool on_scroll_event( GdkEventScroll* a_ev ) ;

    void auto_scroll_horz(double progress);

    bool on_focus_in_event(GdkEventFocus*);
    bool on_focus_out_event(GdkEventFocus*);

    void on_size_request(GtkRequisition* );
    void on_size_allocate(Gtk::Allocation& );

    bool on_key_press_event(GdkEventKey* a_p0);

    void convert_xy( int a_x, int a_y, long *a_ticks, int *a_track);
    void convert_x( int a_x, long *a_ticks);
    void snap_x( int *a_x );

    void draw_track_on( Glib::RefPtr<Gdk::Drawable> a_draw, int a_track );
    void draw_background_on( Glib::RefPtr<Gdk::Drawable> a_draw, int a_track );

    void draw_drawable_row( Glib::RefPtr<Gdk::Drawable> a_dest, Glib::RefPtr<Gdk::Drawable> a_src,  long a_y );


    void change_horz();
    void change_vert();

    void trigger_menu_popup(GdkEventButton* a_ev, perfroll& ths);

    bool cross_track_paste;
    bool have_button_press;

 public:
    void set_guides( int a_snap, int a_measure, int a_beat );

    void update_sizes();
    void init_before_show( );
    void fill_background_pixmap();

    void increment_size();

    void draw_progress();

    void redraw_dirty_tracks();

    /* Trigger menu callbacks */
    void new_sequence(track *a_track, trigger *a_trigger);
    void edit_sequence(track *a_track, trigger *a_trigger);
    void set_trigger_sequence( track *a_track, trigger *a_trigger, int a_sequence );
    void del_trigger( track *a_track, long a_tick );
    void paste_trigger_sequence( track *p_track, sequence *a_sequence );
    void copy_sequence( track *a_track, trigger *a_trigger, sequence *a_seq );
    long get_default_trigger_length( perfroll& ths );

    perfroll( perform *a_perf,
	      Adjustment *a_hadjust,
	      Adjustment *a_vadjust );

    ~perfroll( );
};
