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
#include "tempo.h"

#ifdef NSM_SUPPORT
#include "nsm.h"
#endif

#include <map>
#include <string>

using namespace Gtk;

using namespace Menu_Helpers;

/* forward declarations */
class perfroll;
class perftime;
class tempo;
class perfnames;
class Bpm_spinbutton;

class mainwnd : public Gtk::Window
{
private:

    perform  *m_mainperf;
    Glib::RefPtr<Gtk::Application> m_app;
    
    /* Holds the various window pointers so they can be closed by NSM */
    vector< Gtk::Window *> m_vector_windows;

    MenuBar  *m_menubar;
    Menu     *m_menu_file;
    Menu     *m_menu_recent;          /**< File/Recent menu popup.    */
    Menu     *m_menu_edit;
    Menu     *m_menu_help;

    std::vector<MenuItem> m_file_menu_items;
    std::vector<MenuItem> m_edit_menu_items;
    std::vector<MenuItem> m_snap_menu_items;
    std::vector<MenuItem> m_bw_menu_items;
    MenuItem m_help_menu_item;
    MenuItem m_help_submenu_item;
    Glib::RefPtr<Gtk::AccelGroup> m_accelgroup;
    SeparatorMenuItem   m_menu_separator1;
    SeparatorMenuItem   m_menu_separator2;
    SeparatorMenuItem   m_menu_separator3;
    SeparatorMenuItem   m_menu_separator4;
    SeparatorMenuItem   m_menu_separator5;
    SeparatorMenuItem   m_menu_separator6;
    SeparatorMenuItem   m_menu_separator7;
    SeparatorMenuItem   m_menu_separator8;
    
    Gtk::HBox *hbox1;               /* top line mainwindow */
    Gtk::Image * m_image_seq42;     /* Image for Mainwindow logo.   */

    maintime *m_main_time;

    options *m_options;

    Button      *m_button_stop;
    Button      *m_button_rewind;
    Button      *m_button_play;
    Button      *m_button_fastforward;

    Bpm_spinbutton *m_spinbutton_bpm;

    Glib::RefPtr<Adjustment> m_adjust_bpm;
    Glib::RefPtr<Adjustment> m_adjust_swing_amount8;
    Glib::RefPtr<Adjustment> m_adjust_swing_amount16;

    Button      *m_button_tap;
    SpinButton  *m_spinbutton_swing_amount8;
    SpinButton  *m_spinbutton_swing_amount16;

    sigc::connection   m_timeout_connect;

    Table *m_table;

    VScrollbar *m_vscroll;
    HScrollbar *m_hscroll;

    Glib::RefPtr<Adjustment> m_vadjust;
    Glib::RefPtr<Adjustment> m_hadjust;

    perfnames *m_perfnames;
    perfroll *m_perfroll;
    perftime *m_perftime;
    tempo *m_tempo;

    Menu *m_menu_snap;
    Button *m_button_snap;
    Entry *m_entry_snap;

    Menu *m_menu_xpose;
    Button *m_button_xpose;
    Entry *m_entry_xpose;

    ToggleButton *m_button_continue;
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
    Glib::RefPtr<Adjustment> m_adjust_load_offset;

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
    
    /** From sequencer64
     *  Shows the current time into the song performance.
     */

    Gtk::Label * m_tick_time;
    /**
     *  This button will toggle the m_tick_time_as_bbt member.
     */

    Gtk::Button * m_button_time_type;

    /**
     *  Indicates whether to show the time as bar:beats:ticks or as
     *  hours:minutes:seconds.  The default is true:  bar:beats:ticks.
     */

    bool m_tick_time_as_bbt;
    
    /* Flag used when time type is toggled when stopped to update gui */
    bool m_toggle_time_type;

    /* Flag to indicate that all windows are being closed */
    bool m_closing_windows;

    void file_import_dialog();
    void options_dialog();
    void about_dialog();

    void adj_callback_bpm( );
    void bw_button_callback(int a_beat_width);
    void bp_measure_button_callback(int a_beats_per_measeure);
    void xpose_button_callback( int a_xpose);
    void toggle_time_format( );
    void adj_callback_swing_amount8( );
    void adj_callback_swing_amount16( );
    bool timer_callback( );

    void start_playing();
    void set_continue_callback();
    void stop_playing();
public:
    void rewind(bool a_press);
    void fast_forward(bool a_press);
    void update_window_title();
private:
    void update_window_xpm();
    void toLower(basic_string<char>&);
    void new_open_error_dialog();
    void file_new();
    void file_open();
    void file_open_playlist();
    
#ifdef NSM_SUPPORT
    bool m_nsm_optional_gui;
    bool m_nsm_visible;
    /* Flag for holding the global_is_modified flag and checking for changes to notify NSM dirty, clean */
    bool m_dirty_flag;
    nsm_client_t *m_nsm;
public:
    void set_nsm_client(nsm_client_t *nsm, bool optional_gui);
private:
#endif

public:
    void file_save();
private:
    void file_save_as(file_type_e type, void *a_seq_or_track = nullptr);
    void export_midi(const Glib::ustring&, file_type_e type, void *a_seq_or_track = nullptr);

    void file_exit();
    void new_file();
    bool save_file();
    void choose_file(bool playlist_mode = false);
    int query_save_changes();
    bool is_save();
    void update_recent_files_menu ();
    void load_recent_file (int index);

    /* Begin method that used to be in perfedit */
    void set_snap (int a_snap);

    void set_guides();

    void apply_song_transpose ();

    void grow ();
    void delete_unused_seq ();
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
    void undo_perf(bool a_import = false);
    void undo_bpm();
    void redo_type();
    void redo_trigger (int a_track);
    void redo_trigger ();
    void redo_track(int a_track );
    void redo_perf(bool a_import = false);
    void redo_bpm();

    void popup_menu (Menu * a_menu);

    /* End method that used to be in perfedit */

    void set_xpose (int a_xpose);
    
    /* From  sequencer64 tap button */
    void tap ();
    void set_tap_button (int beats);
    double update_bpm ();
    
    double tempo_map_microseconds(unsigned long a_tick);
    
    inline double
    ticks_to_delta_time_us (long delta_ticks, double bpm, int ppqn)
    {
        return double(delta_ticks) * pulse_length_us(bpm, ppqn);
    }
    
    inline double
    pulse_length_us (double bpm, int ppqn)
    {
        return 60000000.0 / ppqn / bpm * ((double) m_bw / 4.0 );;
    }
    
    /* Sequencer64 */
    std::string tick_to_timestring(long a_tick);
    std::string tick_to_measurestring (long a_tick);
    void tick_to_midi_measures (long a_tick, int &measures, int &beats, int &divisions);
    void close_all_windows();
    void invalid_paste_triggers();
    
public:

    mainwnd(perform *a_p, Glib::RefPtr<Gtk::Application> app);
    ~mainwnd();

    static bool zoom_check (int z)
    {
        return z >= c_perf_max_zoom && z <= (4 * c_perf_scale_x);
    }

    int get_bw();
    void set_zoom (int z);
    bool open_file(const Glib::ustring& fn);
    void export_sequence_midi(sequence *a_seq);
    void export_trigger_midi(track *a_track);
    void export_track_midi(int a_track);
    bool on_delete_event(GdkEventAny *a_e);
    bool on_key_press_event(GdkEventKey* a_ev);
    bool on_key_release_event(GdkEventKey* a_ev);
    bool playlist_jump(int jmp, bool a_verify = false);
    bool verify_playlist_dialog();
    void playlist_verify();
    void load_tempo_list();
    void set_bp_measure(int bp_measure);
    void set_bw(int bw);
    void set_swing_amount8(int swing_amount8);
    void set_swing_amount16(int swing_amount16);
    void update_start_BPM(double bpm);
    void set_perfroll_marker_change(bool a_change);
    void set_marker_line_selection(uint64_t a_tick = 0);
    void set_window_pointer(Gtk::Window * a_win);
    void remove_window_pointer(Gtk::Window * a_win);
    void paste_triggers(long paste_tick, bool overwrite);
    void create_triggers();

    friend int FF_RW_timeout(void *arg);
};

