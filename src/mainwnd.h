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
#include "sequence.h"
#include "event.h"
#include "options.h"
#include "maintime.h"
#include "globals.h"
#include "perfnames.h"
#include "perfroll.h"
#include "perftime.h"

#include <map>
#include <gtkmm.h>
#include <string>

using namespace Gtk;

using namespace Menu_Helpers;

class perfroll;
class perftime;

class mainwnd : public Gtk::Window
{
private:

    perform  *m_mainperf;
    static int m_sigpipe[2];

#if GTK_MINOR_VERSION < 12
    Tooltips *m_tooltips;
#endif
    MenuBar  *m_menubar;
    Menu     *m_menu_file;
    Menu     *m_menu_edit;
    Menu     *m_menu_help;

//    perform  *m_mainperf;

    maintime *m_main_time;

    options *m_options;

    Gdk::Cursor   m_main_cursor;

    Button      *m_button_stop;
    Button      *m_button_rewind;
    Button      *m_button_play;
    Button      *m_button_fastforward;

    SpinButton  *m_spinbutton_bpm;
    Adjustment  *m_adjust_bpm;
    Button      *m_button_tap;

    SpinButton  *m_spinbutton_swing_amount8;
    Adjustment  *m_adjust_swing_amount8;
    SpinButton  *m_spinbutton_swing_amount16;
    Adjustment  *m_adjust_swing_amount16;

    sigc::connection   m_timeout_connect;

    Table *m_table;

    VScrollbar *m_vscroll;
    HScrollbar *m_hscroll;

    Adjustment *m_vadjust;
    Adjustment *m_hadjust;

    perfnames *m_perfnames;
    perfroll *m_perfroll;
    perftime *m_perftime;

    Menu *m_menu_snap;
    Button *m_button_snap;
    Entry *m_entry_snap;

    Menu *m_menu_xpose;
    Button *m_button_xpose;
    Entry *m_entry_xpose;

    ToggleButton *m_button_loop;
    ToggleButton *m_button_mode;
    ToggleButton *m_button_jack;
    Button *m_button_seq;

    ToggleButton *m_button_follow;

    Button *m_button_expand;
    Button *m_button_collapse;
    Button *m_button_copy;

    Button *m_button_grow;
    Button *m_button_undo;
    Button *m_button_redo;

    Button      *m_button_bp_measure;
    Entry       *m_entry_bp_measure;

    Button      *m_button_bw;
    Entry       *m_entry_bw;

    SpinButton  *m_spinbutton_load_offset;
    Adjustment  *m_adjust_load_offset;

    HBox *m_hbox;
    HBox *m_hlbox;

    /* time signature, beats per measure, beat width */
    Menu       *m_menu_bp_measure;
    Menu       *m_menu_bw;

    /* set snap to in pulses */
    int m_snap;
    int m_bp_measure;
    int m_bw;
    /* End variables that used to be in perfedit */
    
    /* tap button - From sequencer64 */
    int m_current_beats; // value is displayed in the button.
    long m_base_time_ms; // Indicates the first time the tap button was ... tapped.
    long m_last_time_ms; // Indicates the last time the tap button was tapped.

    void file_import_dialog();
    void options_dialog();
    void about_dialog();

    //void adj_callback_ss( );
    void adj_callback_bpm( );
    void bw_button_callback(int a_beat_width);
    void bp_measure_button_callback(int a_beats_per_measeure);
    void xpose_button_callback( int a_xpose);
    void adj_callback_swing_amount8( );
    void adj_callback_swing_amount16( );
    bool timer_callback( );

    void start_playing();
    void stop_playing();
    void rewind(bool a_press);
    void fast_forward(bool a_press);
    void update_window_title();
    void toLower(basic_string<char>&);
    void new_open_error_dialog();
    void file_new();
    void file_open();
    void file_save();
    void file_save_as(file_type_e type, void *a_seq_or_track = nullptr);
    void export_midi(const Glib::ustring&, file_type_e type, void *a_seq_or_track = nullptr);

    void file_exit();
    void new_file();
    bool save_file();
    void choose_file();
    int query_save_changes();
    bool is_save();
    static void handle_signal(int sig);
    bool install_signal_handlers();
    bool signal_action(Glib::IOCondition condition);

    /* Begin method that used to be in perfedit */
    void set_bp_measure( int a_beats_per_measure );
    void set_bw( int a_beat_width );
    void set_snap (int a_snap);

    void set_guides();

    void apply_song_transpose ();

    void grow ();
    void delete_unused_seq ();
    void create_triggers();
    void open_seqlist ();
    void set_song_mute(mute_op op);

    void set_looped ();
    void toggle_looped ();

    void set_song_mode ();
    void toggle_song_mode ();

    void set_jack_mode();
    void toggle_jack();

    void set_follow_transport ();
    void toggle_follow_transport();

    void expand ();
    void collapse ();
    void copy ();
    void undo_type();
    void undo_trigger(int a_track);
    void undo_trigger();
    void undo_track(int a_track );
    void undo_perf();
    void redo_type();
    void redo_trigger (int a_track);
    void redo_trigger ();
    void redo_track(int a_track );
    void redo_perf();

    void popup_menu (Menu * a_menu);

    /* End method that used to be in perfedit */

    void set_xpose (int a_xpose);
    
    /* From  sequencer64 tap button */
    void tap ();
    void set_tap_button (int beats);
    double update_bpm ();

public:

    mainwnd(perform *a_p);
    ~mainwnd();

    static bool zoom_check (int z)
    {
        return z >= c_perf_max_zoom && z <= (4 * c_perf_scale_x);
    }

    void set_zoom (int z);
    void open_file(const Glib::ustring&);
    void export_sequence_midi(sequence *a_seq);
    void export_trigger_midi(track *a_track);
    bool on_delete_event(GdkEventAny *a_e);
    bool on_key_press_event(GdkEventKey* a_ev);
    bool on_key_release_event(GdkEventKey* a_ev);
    friend int FF_RW_timeout(void *arg);
};

int FF_RW_timeout(void *arg);
