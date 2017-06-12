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

/* 
 * File:   tempo.h
 * Author: sspresto
 *
 * Created on May 28, 2017, 11:54 AM
 */

#pragma once

#include "mainwnd.h"
#include "perform.h"
#include "seqtime.h"

#include <list>
#include <stack>
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
#include "tempopopup.h"
#include "mutex.h"

using namespace Gtk;
using std::list;

class mainwnd;

/* piano time*/
class tempo: public Gtk::DrawingArea
{

private:

    Glib::RefPtr<Gdk::GC> m_gc;
    Glib::RefPtr<Gdk::Window>   m_window;
    Gdk::Color    m_black, m_white, m_grey;

    Glib::RefPtr<Gdk::Pixbuf> m_pixbuf;

    perform      * const m_mainperf;
    mainwnd      * const m_mainwnd;
    Adjustment   * const m_hadjust;
    
    /* holds the markers */
    list < tempo_mark > m_list_marker;
    list < tempo_mark > m_list_no_stop_markers;
    tempo_mark m_current_mark;
    
    list < tempo_mark > m_list_undo_hold;

    stack < list < tempo_mark > >m_list_undo;
    stack < list < tempo_mark > >m_list_redo;
    
    tempopopup  *m_popup_tempo_wnd;
    
    int m_window_x, m_window_y;
    int m_perf_scale_x;

    int m_4bar_offset;

    int m_snap, m_measure_length;
    
    /* locking */
    seq42_mutex m_mutex;
    void lock ();
    void unlock ();

    void on_realize();
    bool on_expose_event(GdkEventExpose* a_ev);
    bool on_button_press_event(GdkEventButton* a_ev);
    bool on_button_release_event(GdkEventButton* a_ev);
    bool on_motion_notify_event(GdkEventMotion* a_ev);
    void on_size_allocate(Gtk::Allocation &a_r );
    
    void draw_background();
    void update_sizes();
    void draw_pixmap_on_window();
    void draw_progress_on_window();
    void update_pixmap();

    int idle_progress();

    void change_horz();
    void set_tempo_marker(long a_tick);

public:

    tempo( perform *a_perf, mainwnd *a_main, Adjustment *a_hadjust );
    ~tempo();

    void set_zoom (int a_zoom);

    void reset();
    void set_scale( int a_scale );
    void set_guides( int a_snap, int a_measure );

    void increment_size();
    void set_BPM(double a_bpm);
    static bool sort_tempo_mark(const tempo_mark &a, const tempo_mark &b);
    static bool reverse_sort_tempo_mark(const tempo_mark &a, const tempo_mark &b);
    void add_marker(tempo_mark a_mark);
    void set_start_BPM(double a_bpm);
    void reset_tempo_list(bool play_list_only = false);
    void load_tempo_list();
    void calculate_marker_start();
    
    void push_undo(bool a_hold = false);
    void pop_undo();
    void pop_redo();
    void set_hold_undo (bool a_hold);
    int get_hold_undo ();
    
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

    Bpm_spinbutton(Adjustment& adjustment, double climb_rate =  0.0, guint digits =  0);
    
    void set_have_enter(bool a_enter);
    bool get_have_enter();
    void set_have_leave(bool a_leave);
    bool get_have_leave();
    void set_have_typing(bool a_type);
    bool get_have_typing();
    
    void set_hold_bpm(double a_bpm = 0.0);
    double get_hold_bpm();
};