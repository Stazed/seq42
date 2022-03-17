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

#include "globals.h"
#include "perform.h"
#include "mutex.h"
#include "track.h"
#include "trigger.h"
#include "mainwnd.h"

using namespace Gtk;

#include "perfroll_input.h"

const int c_perfroll_background_x = (c_ppqn * 4 * 16) / c_perf_max_zoom;
const int c_perfroll_size_box_w = 6;
const int c_perfroll_size_box_click_w = c_perfroll_size_box_w+1 ;

class mainwnd;

/* performance roll */
class perfroll : public Gtk::DrawingArea
{
private:

    friend class FruityPerfInput;
    FruityPerfInput m_fruity_interaction;

    friend class Seq42PerfInput;
    Seq42PerfInput m_seq42_interaction;

    Cairo::RefPtr<Cairo::Context>  m_surface_window;
    Cairo::RefPtr<Cairo::ImageSurface> m_surface_track;
    Cairo::RefPtr<Cairo::ImageSurface> m_surface_background;

    perform        * const m_mainperf;
    mainwnd        * const m_mainwnd;

    int          m_snap;
    int          m_measure_length;
    int          m_beat_length;

    int          m_perf_scale_x; // for zoom - replace c_perf_scale_x
    int          m_zoom;
    int          m_names_y;      // vertical zoom

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

    Glib::RefPtr<Adjustment> m_vadjust;
    Glib::RefPtr<Adjustment> m_hadjust;

    bool m_moving;
    bool m_growing;
    bool m_grow_direction;

    bool cross_track_paste;
    bool have_button_press;
    bool transport_follow;
    bool trans_button_press;
    bool m_redraw_tracks;
    bool m_have_realize;
    bool m_have_stop_reposition;
    bool m_marker_change;
    uint64_t m_line_location;

    void on_realize();
    bool on_button_press_event(GdkEventButton* a_ev);
    bool on_button_release_event(GdkEventButton* a_ev);
    bool on_motion_notify_event(GdkEventMotion* a_ev);
    bool on_scroll_event( GdkEventScroll* a_ev ) ;

    bool on_focus_in_event(GdkEventFocus*);
    bool on_focus_out_event(GdkEventFocus*);

    void on_size_allocate(Gtk::Allocation& );

    bool on_key_press_event(GdkEventKey* a_p0);

    void convert_xy( int a_x, int a_y, long *a_ticks, int *a_track);
    void convert_x( int a_x, long *a_ticks);
    void snap_x( int *a_x );

    void draw_track_on( int a_track );
    void draw_background_on( int a_track );
    void draw_track_on_window( int a_track );

    void change_horz();
    void change_vert();

    void trigger_menu_popup(perfroll& ths);

protected:
    bool on_draw(const Cairo::RefPtr<Cairo::Context>& cr);

public:

    void auto_scroll_horz();
    void set_zoom (int a_zoom);

    void set_guides( int a_snap, int a_measure, int a_beat );

    void update_sizes();
    void init_before_show( );
    void fill_background_surface();

    void increment_size();

    void draw_progress();

    void redraw_dirty_tracks();
    void redraw_all_tracks(){queue_draw();}
    void have_stopped_reposition(){m_have_stop_reposition = true;}
    void set_marker_changed(bool a_change){ m_marker_change = a_change;}
    void set_marker_line_selection(uint64_t a_tick){ m_line_location = a_tick;}

    /* Trigger menu callbacks */
    void new_sequence(track *a_track, trigger *a_trigger);
    void edit_sequence(track *a_track, trigger *a_trigger);
    void export_sequence( track *a_track, trigger *a_trigger);
    void export_trigger( track *a_track, trigger *a_trigger);
    void play_sequence( track *a_track, trigger *a_trigger);
    void set_trigger_sequence( track *a_track, trigger *a_trigger, int a_sequence );
    void del_trigger( track *a_track, long a_tick );
    void paste_trigger_mouse(long a_tick);
    void paste_trigger_sequence( track *p_track, sequence *a_sequence, long a_tick );
    void copy_sequence( track *a_track, trigger *a_trigger, sequence *a_seq );
    long get_default_trigger_length( perfroll& ths );

    perfroll( perform *a_perf,
              mainwnd *a_main,
              Glib::RefPtr<Adjustment> a_hadjust,
              Glib::RefPtr<Adjustment> a_vadjust );

    ~perfroll( );
};
