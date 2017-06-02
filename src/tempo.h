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
    
    /* holds the events */
    list < tempo_mark > m_list_marker;
    tempo_mark m_current_mark;
    
    tempopopup  *m_popup_tempo_wnd;
    
    int m_window_x, m_window_y;
    int m_perf_scale_x;

    int m_4bar_offset;

    int m_snap, m_measure_length;

    void on_realize();
    bool on_expose_event(GdkEventExpose* a_ev);
    bool on_button_press_event(GdkEventButton* a_ev);
    bool on_button_release_event(GdkEventButton* a_ev);
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
    void reset_tempo_list();
    
    friend tempopopup;
};
