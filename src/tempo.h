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

#include <list>
#include <stack>

#include <gtkmm.h>

#include "globals.h"
#include "tempopopup.h"
#include "mutex.h"

using namespace Gtk;
using std::list;

class mainwnd;

/* Tempo track */
class tempo: public Gtk::DrawingArea
{

private:

    Cairo::RefPtr<Cairo::ImageSurface> m_surface;

    Glib::RefPtr<Gdk::Pixbuf> m_pixbuf;

    perform      * const m_mainperf;
    mainwnd      * const m_mainwnd;

    Glib::RefPtr<Adjustment> const m_hadjust;

    /* holds the markers */
    list < tempo_mark > m_list_marker;
    list < tempo_mark > m_list_no_stop_markers;
    tempo_mark m_current_mark;
    
    list < tempo_mark > m_list_undo_hold;
    
    tempopopup  *m_popup_tempo_wnd;
    
    int m_window_x, m_window_y;
    int m_perf_scale_x;

    int m_4bar_offset;

    int m_snap, m_measure_length;
    
    bool m_init_move;
    bool m_moving;
    bool m_draw_background;
    tempo_mark m_move_marker;
    
    /* locking */
    seq42::mutex m_mutex;
    void lock ();
    void unlock ();

    void on_realize();
    bool on_button_press_event(GdkEventButton* a_ev);
    bool on_button_release_event(GdkEventButton* a_ev);
    bool on_motion_notify_event(GdkEventMotion* a_ev);
    bool on_leave_notify_event(GdkEventCrossing* a_ev);
    void on_size_allocate(Gtk::Allocation &a_r );
    
    void draw_background();

    void change_horz();
    void set_tempo_marker(long a_tick, int x, int y);
    
    inline double
    ticks_to_delta_time_us (long delta_ticks, double bpm, int ppqn)
    {
        return double(delta_ticks) * pulse_length_us(bpm, ppqn);
    }
    
    double pulse_length_us (double bpm, int ppqn);
    
    bool check_above_marker(uint64_t mouse_tick, bool a_delete, bool exact );

protected:
    bool on_draw(const Cairo::RefPtr<Cairo::Context>& cr);

public:

    tempo( perform *a_perf, mainwnd *a_main, Glib::RefPtr<Adjustment> a_hadjust );
    ~tempo();

    void set_zoom (int a_zoom);

    void set_guides( int a_snap, int a_measure );

    void set_BPM(double a_bpm);
    static bool sort_tempo_mark(const tempo_mark &a, const tempo_mark &b);
    static bool reverse_sort_tempo_mark(const tempo_mark &a, const tempo_mark &b);
    void add_marker(tempo_mark a_mark);
    void set_start_BPM(double a_bpm);
    void reset_tempo_list();
    void load_tempo_list();
    void calculate_marker_start();
    void clear_tempo_list();
    
    void push_undo(bool a_hold = false);
    void pop_undo();
    void pop_redo();
    void set_hold_undo (bool a_hold);
    int get_hold_undo ();
    void hide_tempo_popup() { m_popup_tempo_wnd->hide();}
    
    void print_marker_info(list<tempo_mark> a_list);
    
    friend tempopopup;
};


/* Modified spinbutton for using the mainwnd bpm spinner to allow for better undo support.
 * This allows user to spin and won't push to undo on every changed value, but will only
 * push undo when user leaves the widget. Modified to work with typed entries as well.
 */
class Bpm_spinbutton : public Gtk::SpinButton
{
private:
    
    bool m_have_enter;
    bool m_have_leave;
    bool m_is_typing;
    
    double m_hold_bpm;
    
    bool on_enter_notify_event(GdkEventCrossing* event);
    bool on_leave_notify_event(GdkEventCrossing* event);
    bool on_key_press_event(GdkEventKey* a_ev);
    
    public:

    Bpm_spinbutton(const Glib::RefPtr<Adjustment>& adjustment, double climb_rate =  0.0, guint digits =  0);
    
    void set_have_enter(bool a_enter);
    bool get_have_enter();
    void set_have_leave(bool a_leave);
    bool get_have_leave();
    void set_have_typing(bool a_type);
    bool get_have_typing();
    
    void set_hold_bpm(double a_bpm = 0.0);
    double get_hold_bpm();
};