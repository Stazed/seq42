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


#include "perform.h"
#include "sequence.h"
#include "event.h"
#include "options.h"
#include "maintime.h"


#ifndef SEQ42_MAINWINDOW
#define SEQ42_MAINWINDOW

#include <map>
#include <gtkmm.h>
#include <string>

#include "globals.h"
#include "perfnames.h"
#include "perfroll.h"
#include "perftime.h"

using namespace Gtk;

using namespace Menu_Helpers;


class mainwnd : public Gtk::Window
{
 private:

    bool      m_modified;
    static int m_sigpipe[2];

#if GTK_MINOR_VERSION < 12
    Tooltips *m_tooltips;
#endif
    MenuBar  *m_menubar;
    Menu     *m_menu_file;
    Menu     *m_menu_edit;
    Menu     *m_menu_help;

    perform  *m_mainperf;

    maintime *m_main_time;

    options *m_options;

    Gdk::Cursor   m_main_cursor;

    Button      *m_button_stop;
    Button      *m_button_play;

    SpinButton  *m_spinbutton_bpm;
    Adjustment  *m_adjust_bpm;

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

    Button *m_button_expand;
    Button *m_button_collapse;
    Button *m_button_copy;

    Button *m_button_grow;
    Button *m_button_undo;
    Button *m_button_redo;

    Button      *m_button_bpm;
    Entry       *m_entry_bpm;

    Button      *m_button_bw;
    Entry       *m_entry_bw;

    HBox *m_hbox;
    HBox *m_hlbox;

    /* time signature, beats per measure, beat width */
    Menu       *m_menu_bpm;
    Menu       *m_menu_bw;

    /* set snap to in pulses */
    int m_snap;
    int m_bpm;
    int m_bw;
    /* End variables that used to be in perfedit */

    void file_import_dialog( void );
    void options_dialog( void );
    void about_dialog( void );

    //void adj_callback_ss( );
    void adj_callback_bpm( );
    void adj_callback_swing_amount8( );
    void adj_callback_swing_amount16( );
    bool timer_callback( );

    void start_playing();
    void stop_playing();
    void update_window_title();
    void toLower(basic_string<char>&);
    bool is_modified();
    void file_new();
    void file_open();
    void file_save();
    void file_save_as(int type);
    void export_sequences(const Glib::ustring&);
    void export_song(const Glib::ustring& );
    void file_exit();
    void new_file();
//    void open_file(const Glib::ustring&);
    bool save_file();
    void choose_file();
    int query_save_changes();
    bool is_save();
    static void handle_signal(int sig);
    bool install_signal_handlers();
    bool signal_action(Glib::IOCondition condition);

    /* Begin method that used to be in perfedit */
    void set_bpm( int a_beats_per_measure );
    void set_bw( int a_beat_width );
    void set_snap (int a_snap);

    void set_guides( void );

    void apply_song_transpose (void);

    void grow (void);
    void open_seqlist (void);
    void set_song_mute(mute_op op);

    void set_looped (void);
    void toggle_looped (void);

    void set_song_mode (void);
    void toggle_song_mode (void);
    void set_jack_mode( void );

    void expand (void);
    void collapse (void);
    void copy (void);
    void undo_type( void );
    void undo_trigger(int a_track);
    void undo_trigger( void );
    void undo_track(int a_track );
    void undo_perf( void );
    void redo_type( void );
    void redo_trigger (int a_track);
    void redo_trigger ( void );
    void redo_track(int a_track );
    void redo_perf();

    void popup_menu (Menu * a_menu);

    /* End method that used to be in perfedit */

    void set_xpose (int a_xpose);

 public:

    mainwnd(perform *a_p);
    ~mainwnd();

    void open_file(const Glib::ustring&);
    bool on_delete_event(GdkEventAny *a_e);
    bool on_key_press_event(GdkEventKey* a_ev);

};


#endif
