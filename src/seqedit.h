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

#ifndef SEQ42_SEQEDIT
#define SEQ42_SEQEDIT

#include "sequence.h"
#include "seqkeys.h"
#include "seqroll.h"
#include "seqdata.h"
#include "seqtime.h"
#include "seqevent.h"
#include "perform.h"
#include "trackedit.h"

#include <gtkmm.h>
#include <sigc++/bind.h>

#include <list>
#include <string>

#include "globals.h"
#include "lfownd.h"

using namespace Gtk;


/* has a seqroll and paino roll */
class seqedit : public Gtk::Window
{

 private:
 
    static const int c_min_zoom = 1;
    static const int c_max_zoom = 32;
 	
    MenuBar    *m_menubar;

    Menu       *m_menu_tools;
    Menu       *m_menu_zoom;
    Menu       *m_menu_snap;
    Menu       *m_menu_note_length;

    /* length in measures */
    Menu       *m_menu_length;
    Menu       *m_menu_data;
    Menu       *m_menu_key;
    Menu       *m_menu_scale;
    Menu       *m_menu_sequences;

    /* time signature, beats per measure, beat width */
    Menu       *m_menu_bpm;
    Menu       *m_menu_bw;

    Menu       *m_menu_swing_mode;

    SpinButton  *m_spinbutton_vel;
    Adjustment  *m_adjust_vel;


    sequence   *m_seq;
    perform    *m_mainperf;

    int         m_pos;

    seqroll    *m_seqroll_wid;
    seqkeys    *m_seqkeys_wid;
    seqdata    *m_seqdata_wid;
    seqtime    *m_seqtime_wid;
    seqevent   *m_seqevent_wid;

    Table      *m_table;
    VBox       *m_vbox;
    HBox       *m_hbox;
    HBox       *m_hbox2;
    HBox       *m_hbox3;

    Adjustment *m_vadjust;
    Adjustment *m_hadjust;

    VScrollbar *m_vscroll_new;
    HScrollbar *m_hscroll_new;
    
    Button      *m_button_undo;
    Button      *m_button_redo;
    Button      *m_button_quanize;
    
    Button      *m_button_tools;

    Button      *m_button_sequence;
    Entry       *m_entry_sequence;
    
    Button      *m_button_track;
    Entry       *m_entry_track;
    Label       *m_label_bus;
    Entry       *m_entry_bus;
    Label       *m_label_channel;
    Entry       *m_entry_channel;
    
    Button      *m_button_snap;
    Entry       *m_entry_snap;
    
    Button      *m_button_note_length;
    Entry       *m_entry_note_length;
    
    Button      *m_button_zoom;
    Entry       *m_entry_zoom;
    
    Button      *m_button_length;
    Entry       *m_entry_length;
    
    Button      *m_button_swing_mode;
    Entry       *m_entry_swing_mode;
    
    Button      *m_button_key;
    Entry       *m_entry_key;
    
    Button      *m_button_scale;
    Entry       *m_entry_scale;

    Tooltips    *m_tooltips;

    Button       *m_button_data;
    Entry        *m_entry_data;

    Button      *m_button_bpm;
    Entry       *m_entry_bpm;

    Button      *m_button_bw;
    Entry       *m_entry_bw;

    Button      *m_button_stop;
    Button      *m_button_play;
    Button	*m_button_lfo;
    lfownd     *m_lfo_wnd;
    
    ToggleButton *m_toggle_play;
    ToggleButton *m_toggle_record;
    ToggleButton *m_toggle_q_rec;
    ToggleButton *m_toggle_thru;

    RadioButton *m_radio_select;
    RadioButton *m_radio_grow;
    RadioButton *m_radio_draw;
    
    Entry       *m_entry_name;

    /* the zoom 0  1  2  3  4  
                 1, 2, 4, 8, 16 */
    int         m_zoom;
    static int  m_initial_zoom;

    /* set snap to in pulses, off = 1 */
    int         m_snap;
    static int  m_initial_snap;

    int         m_note_length;
    static int  m_initial_note_length;

    /* music scale and key */
    int         m_scale;
    static int  m_initial_scale;

    int         m_key;
    static int  m_initial_key;

    int        m_bg_seq;
    static int m_initial_bg_seq;
    int        m_bg_trk;
    static int m_initial_bg_trk;

    long        m_measures;

    /* what is the data window currently editing ? */
    unsigned char m_editing_status;
    unsigned char m_editing_cc;

    void set_zoom( int a_zoom );
    void set_snap( int a_snap );
    void set_note_length( int a_note_length );

    void set_bpm( int a_beats_per_measure );
    void set_bw( int a_beat_width );
    void set_measures( int a_length_measures, bool a_adjust_triggers  );
    void apply_length( int a_bpm, int a_bw, int a_measures, bool a_adjust_triggers );
    long get_measures( void );

    void set_swing_mode( int a_mode  );
    int get_swing_mode( void );

    void set_scale( int a_scale );
    void set_key( int a_note );

    void set_background_sequence( int a_trk, int a_seq );
    
    void name_change_callback( void );
    void play_change_callback( void );
    void record_change_callback( void );
    void q_rec_change_callback( void );
    void thru_change_callback( void );
    void undo_callback( void );
    void redo_callback( void );

    void adj_callback_vel( );

    void set_data_type( unsigned char a_status, 
			unsigned char a_control = 0 );

    void update_all_windows( );

    void fill_top_bar( void );
    void create_menus( void );

    void menu_action_quantise( void );
    
    void popup_menu( Menu *a_menu );
    void popup_event_menu( void );
    void popup_sequence_menu( void );
    void popup_tool_menu( void );

    Gtk::Image* create_menu_image( bool a_state = false );
    
    void on_realize();

    bool timeout( void );

    void do_action( int a_action, int a_var );

    void mouse_action( mouse_action_e a_action );

    void start_playing();

    void stop_playing();

    void set_track_info( );

    void trk_edit();

 public:

    seqedit(sequence *a_seq, 
	    perform *a_perf);

    ~seqedit();

 
    bool on_delete_event(GdkEventAny *a_event);
    bool on_scroll_event(GdkEventScroll* a_ev);
    bool on_key_press_event(GdkEventKey* a_ev);

};











#endif
