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

#include <cctype>
#include <csignal>
#include <cerrno>
#include <cstring>

#include "mainwnd.h"
#include "perform.h"
#include "midifile.h"
#include "s42file.h"
#include "sequence.h"
#include "seqlist.h"

#include "pixmaps/seq42_32.xpm"
#include "pixmaps/seq42.xpm"
#include "pixmaps/seq42_playlist.xpm"
#include "pixmaps/play2.xpm"
#include "pixmaps/seqlist.xpm"
#include "pixmaps/stop.xpm"
#include "pixmaps/snap.xpm"
#include "pixmaps/expand.xpm"
#include "pixmaps/collapse.xpm"
#include "pixmaps/loop.xpm"
#include "pixmaps/copy.xpm"
#include "pixmaps/undo.xpm"
#include "pixmaps/redo.xpm"
#include "pixmaps/down.xpm"
#include "pixmaps/transportFollow.xpm"
#include "pixmaps/transpose.xpm"
#include "pixmaps/fastforward.xpm"
#include "pixmaps/rewind.xpm"

#ifdef JACK_SUPPORT
#include "pixmaps/jack.xpm"
#endif // JACK_SUPPORT

using namespace sigc;

short global_file_int_size = sizeof(int32_t);
short global_file_long_int_size = sizeof(int32_t);

bool global_is_running = false;
bool global_is_modified = false;
bool global_seqlist_need_update = false;
ff_rw_type_e FF_RW_button_type = FF_RW_RELEASE;

#define add_tooltip( obj, text ) obj->set_tooltip_text( text);

mainwnd::mainwnd(perform *a_p, Glib::RefPtr<Gtk::Application> app):
    m_mainperf(a_p),
    m_app(app),
    m_options(NULL),
    m_snap(c_ppqn / 4),
    m_bp_measure(4),
    m_bw(4),
    m_tick_time_as_bbt(false),
    m_toggle_time_type(false),
    m_closing_windows(false)
#ifdef NSM_SUPPORT
    ,m_nsm_visible(true)
    ,m_dirty_flag(false)
    ,m_nsm(NULL)
#endif
{
    using namespace Menu_Helpers;

    set_icon(Gdk::Pixbuf::create_from_xpm_data(seq42_32_xpm));

    /* main window */
    update_window_title();
    set_size_request(860, 322);

    m_accelgroup = Gtk::AccelGroup::create();
    add_accel_group(m_accelgroup);

    m_main_time = manage( new maintime( ));

    m_menubar = manage(new MenuBar());

    m_menu_file = manage(new Menu());

    m_menu_recent = nullptr;

    m_menu_edit = manage(new Menu());

    m_menu_help = manage(new Menu());

    m_file_menu_items.resize(8);
    m_edit_menu_items.resize(11);

    m_file_menu_items[0].set_label("_File");
    m_file_menu_items[0].set_use_underline(true);
    m_file_menu_items[0].set_submenu(*m_menu_file);

    m_menubar->append(m_file_menu_items[0]);

    m_edit_menu_items[0].set_label("_Edit");
    m_edit_menu_items[0].set_use_underline(true);
    m_edit_menu_items[0].set_submenu(*m_menu_edit);

    m_menubar->append(m_edit_menu_items[0]);

    m_help_submenu_item.set_label("_Help");
    m_help_submenu_item.set_use_underline(true);
    m_help_submenu_item.set_submenu(*m_menu_help);

    m_menubar->append(m_help_submenu_item);

    /* file menu items */
    m_file_menu_items[1].set_label("_New");
    m_file_menu_items[1].set_use_underline(true);
    m_file_menu_items[1].add_accelerator("activate", m_accelgroup, 'n', Gdk::CONTROL_MASK, Gtk::ACCEL_VISIBLE);
    m_file_menu_items[1].signal_activate().connect(mem_fun(*this, &mainwnd::file_new));
    m_menu_file->append(m_file_menu_items[1]);

    m_file_menu_items[2].set_label("_Open");
    m_file_menu_items[2].set_use_underline(true);
    m_file_menu_items[2].add_accelerator("activate", m_accelgroup, 'o', Gdk::CONTROL_MASK, Gtk::ACCEL_VISIBLE);
    m_file_menu_items[2].signal_activate().connect(mem_fun(*this, &mainwnd::file_open));
    m_menu_file->append(m_file_menu_items[2]);

    /* Add the recent files submenu */
    update_recent_files_menu();

    m_file_menu_items[3].set_label("Open _playlist...");
    m_file_menu_items[3].set_use_underline(true);
    m_file_menu_items[3].signal_activate().connect(mem_fun(*this, &mainwnd::file_open_playlist));
    m_menu_file->append(m_file_menu_items[3]);

    m_menu_file->append(m_menu_separator1);

    m_file_menu_items[4].set_label("_Save");
    m_file_menu_items[4].set_use_underline(true);
    m_file_menu_items[4].add_accelerator("activate", m_accelgroup, 's', Gdk::CONTROL_MASK, Gtk::ACCEL_VISIBLE);
    m_file_menu_items[4].signal_activate().connect(mem_fun(*this, &mainwnd::file_save));
    m_menu_file->append(m_file_menu_items[4]);

    m_file_menu_items[5].set_label("Save _as...");
    m_file_menu_items[5].set_use_underline(true);
    m_file_menu_items[5].add_accelerator("activate", m_accelgroup, 's', Gdk::CONTROL_MASK | Gdk::SHIFT_MASK, Gtk::ACCEL_VISIBLE);
    m_file_menu_items[5].signal_activate().connect(sigc::bind(mem_fun(*this, &mainwnd::file_save_as), E_SEQ42_NATIVE_FILE, nullptr));
    m_menu_file->append(m_file_menu_items[5]);

    m_menu_file->append(m_menu_separator2);

    m_file_menu_items[6].set_label("O_ptions...");
    m_file_menu_items[6].set_use_underline(true);
    m_file_menu_items[6].signal_activate().connect(mem_fun(*this, &mainwnd::options_dialog));
    m_menu_file->append(m_file_menu_items[6]);

    m_menu_file->append(m_menu_separator3);

    m_file_menu_items[7].set_label("E_xit");
    m_file_menu_items[7].set_use_underline(true);
    m_file_menu_items[7].add_accelerator("activate", m_accelgroup, 'q', Gdk::CONTROL_MASK, Gtk::ACCEL_VISIBLE);
    m_file_menu_items[7].signal_activate().connect(mem_fun(*this, &mainwnd::file_exit));
    m_menu_file->append(m_file_menu_items[7]);

    /* edit menu items */
    m_edit_menu_items[1].set_label("Sequence _list");
    m_edit_menu_items[1].set_use_underline(true);
    m_edit_menu_items[1].signal_activate().connect(mem_fun(*this, &mainwnd::open_seqlist));
    m_menu_edit->append(m_edit_menu_items[1]);

    m_edit_menu_items[2].set_label("_Apply song transpose");
    m_edit_menu_items[2].set_use_underline(true);
    m_edit_menu_items[2].signal_activate().connect(mem_fun(*this, &mainwnd::apply_song_transpose));
    m_menu_edit->append(m_edit_menu_items[2]);

    m_edit_menu_items[3].set_label("_Delete unused sequences");
    m_edit_menu_items[3].set_use_underline(true);
    m_edit_menu_items[3].signal_activate().connect(mem_fun(*this, &mainwnd::delete_unused_seq));
    m_menu_edit->append(m_edit_menu_items[3]);

    m_edit_menu_items[4].set_label("_Create triggers between L and R for 'playing' sequences");
    m_edit_menu_items[4].set_use_underline(true);
    m_edit_menu_items[4].signal_activate().connect(mem_fun(*this, &mainwnd::create_triggers));
    m_menu_edit->append(m_edit_menu_items[4]);

    m_menu_edit->append(m_menu_separator4);

    m_edit_menu_items[5].set_label("_Mute all tracks");
    m_edit_menu_items[5].set_use_underline(true);
    m_edit_menu_items[5].signal_activate().connect(sigc::bind(mem_fun(*this, &mainwnd::set_song_mute), MUTE_ON));
    m_menu_edit->append(m_edit_menu_items[5]);

    m_edit_menu_items[6].set_label("_Unmute all tracks");
    m_edit_menu_items[6].set_use_underline(true);
    m_edit_menu_items[6].signal_activate().connect(sigc::bind(mem_fun(*this, &mainwnd::set_song_mute), MUTE_OFF));
    m_menu_edit->append(m_edit_menu_items[6]);

    m_edit_menu_items[7].set_label("_Toggle mute all tracks");
    m_edit_menu_items[7].set_use_underline(true);
    m_edit_menu_items[7].signal_activate().connect(sigc::bind(mem_fun(*this, &mainwnd::set_song_mute), MUTE_TOGGLE));
    m_menu_edit->append(m_edit_menu_items[7]);

    m_menu_edit->append(m_menu_separator5);

    m_edit_menu_items[8].set_label("_Import midi");
    m_edit_menu_items[8].set_use_underline(true);
    m_edit_menu_items[8].signal_activate().connect(mem_fun(*this, &mainwnd::file_import_dialog));
    m_menu_edit->append(m_edit_menu_items[8]);

    m_edit_menu_items[9].set_label("Midi e_xport (Seq 24/32/64)");
    m_edit_menu_items[9].set_use_underline(true);
    m_edit_menu_items[9].signal_activate().connect(sigc::bind(mem_fun(*this, &mainwnd::file_save_as), E_MIDI_SEQ24_FORMAT, nullptr));
    m_menu_edit->append(m_edit_menu_items[9]);

    m_edit_menu_items[10].set_label("Midi export _song");
    m_edit_menu_items[10].set_use_underline(true);
    m_edit_menu_items[10].signal_activate().connect(sigc::bind(mem_fun(*this, &mainwnd::file_save_as), E_MIDI_SONG_FORMAT, nullptr));
    m_menu_edit->append(m_edit_menu_items[10]);

    /* help menu items */
    m_help_menu_item.set_label("_About...");
    m_help_menu_item.set_use_underline(true);
    m_help_menu_item.signal_activate().connect(mem_fun(*this, &mainwnd::about_dialog));
    m_menu_help->append(m_help_menu_item);

    /* top line items */
    hbox1 = manage( new HBox( false, 2 ) );
    hbox1->set_border_width( 2 );
    
    m_image_seq42 = manage(new Image( Gdk::Pixbuf::create_from_xpm_data(seq42_xpm)));
    
    m_button_continue = manage( new ToggleButton( "S" ) );
    m_button_continue->signal_toggled().connect(  mem_fun( *this, &mainwnd::set_continue_callback ));
    add_tooltip( m_button_continue, "Toggle to set 'Stop/Pause' button method.\n"
            "If set to 'S', stopping transport will reposition to 'L' mark.\n"
            "If set to 'P', stopping transport will not reposition to 'L' mark." );
    
    m_button_continue->set_active( false );

    m_button_stop = manage( new Button() );
    m_button_stop->add(*manage( new Image(Gdk::Pixbuf::create_from_xpm_data( stop_xpm ))));
    m_button_stop->signal_clicked().connect( mem_fun( *this, &mainwnd::stop_playing));
    add_tooltip( m_button_stop, "Stop/Pause playing." );

    m_button_rewind = manage( new Button() );
    m_button_rewind->add(*manage( new Image(Gdk::Pixbuf::create_from_xpm_data( rewind_xpm ))));
    m_button_rewind->signal_pressed().connect(sigc::bind (mem_fun( *this, &mainwnd::rewind), true));
    m_button_rewind->signal_released().connect(sigc::bind (mem_fun( *this, &mainwnd::rewind), false));
    add_tooltip( m_button_rewind, "Rewind." );

    m_button_play = manage( new Button() );
    m_button_play->add(*manage( new Image(Gdk::Pixbuf::create_from_xpm_data( play2_xpm ))));
    m_button_play->signal_clicked().connect(  mem_fun( *this, &mainwnd::start_playing));
    add_tooltip( m_button_play, "Begin/Continue playing." );

    m_button_fastforward = manage( new Button() );
    m_button_fastforward->add(*manage( new Image(Gdk::Pixbuf::create_from_xpm_data( fastforward_xpm ))));
    m_button_fastforward->signal_pressed().connect(sigc::bind (mem_fun( *this, &mainwnd::fast_forward), true));
    m_button_fastforward->signal_released().connect(sigc::bind (mem_fun( *this, &mainwnd::fast_forward), false));
    add_tooltip( m_button_fastforward, "Fast Forward." );

    m_button_loop = manage( new ToggleButton() );
    m_button_loop->add(*manage( new Image(Gdk::Pixbuf::create_from_xpm_data( loop_xpm ))));
    m_button_loop->signal_toggled().connect(  mem_fun( *this, &mainwnd::set_looped ));
    add_tooltip( m_button_loop, "Play looped between L and R." );

    m_button_mode = manage( new ToggleButton( "Song" ) );
    m_button_mode->signal_toggled().connect(  mem_fun( *this, &mainwnd::set_song_mode ));
    add_tooltip( m_button_mode, "Toggle song mode (or live/sequence mode)." );
    if(global_song_start_mode)
    {
        m_button_mode->set_active( true );
    }

#ifdef JACK_SUPPORT
    m_button_jack = manage( new ToggleButton() );
    m_button_jack->add(*manage( new Image(Gdk::Pixbuf::create_from_xpm_data( jack_xpm ))));
    m_button_jack->signal_toggled().connect(  mem_fun( *this, &mainwnd::set_jack_mode ));
    add_tooltip( m_button_jack, "Toggle Jack sync connection" );
    if(global_with_jack_transport)
    {
        m_button_jack->set_active( true );
    }
#endif

    m_button_seq = manage( new Button() );
    m_button_seq->add(*manage( new Image(Gdk::Pixbuf::create_from_xpm_data( seqlist_xpm ))));
    m_button_seq->signal_clicked().connect(  mem_fun( *this, &mainwnd::open_seqlist ));
    add_tooltip( m_button_seq, "Open/Close sequence list" );

    m_button_follow = manage( new ToggleButton() );
    m_button_follow->add(*manage( new Image(Gdk::Pixbuf::create_from_xpm_data( transportFollow_xpm ))));
    m_button_follow->signal_clicked().connect(  mem_fun( *this, &mainwnd::set_follow_transport ));
    add_tooltip( m_button_follow, "Follow transport" );
    m_button_follow->set_active(true);

    hbox1->pack_start( *m_image_seq42, false, false);
    hbox1->pack_start( *m_button_continue, false, false );
    hbox1->pack_start( *m_button_stop, false, false );
    hbox1->pack_start( *m_button_rewind, false, false );
    hbox1->pack_start( *m_button_play, false, false );
    hbox1->pack_start( *m_button_fastforward, false, false );
    hbox1->pack_start( *m_button_loop, false, false );
    hbox1->pack_start( *m_button_mode, false, false );
#ifdef JACK_SUPPORT
    hbox1->pack_start(*m_button_jack, false, false );
#endif
    hbox1->pack_start( *m_button_seq, false, false );
    hbox1->pack_start( *m_button_follow, false, false );

    // adjust placement...
    VBox *vbox_b = manage( new VBox() );
    HBox *hbox2 = manage( new HBox( false, 0 ) );
    vbox_b->pack_start( *hbox2, false, false );
    hbox1->pack_end( *vbox_b, false, false );
    hbox2->set_spacing( 10 );

    /* timeline */
    hbox2->pack_start( *m_main_time, false, false );
    
    m_tick_time = manage(new Gtk::Label(""));
    m_button_time_type = manage(new Gtk::Button("HMS"));
    Gtk::HBox * hbox4 = manage(new Gtk::HBox(false, 0));
    m_tick_time->set_justify(Gtk::JUSTIFY_LEFT);
    
    m_button_time_type->set_focus_on_click(false);
    
    m_button_time_type->signal_clicked().connect
    (
        mem_fun(*this, &mainwnd::toggle_time_format)
    );
    add_tooltip
    (
        m_button_time_type,
        "Toggles between B:B:T and H:M:S format, showing the selected format."
    );
 
    hbox1->pack_end(*m_button_time_type, false, false);

    Gtk::Label * timedummy = manage(new Gtk::Label("   "));
    hbox4->pack_start(*timedummy, false, false, 0);
    hbox4->pack_start(*m_tick_time, false, false, 0);
    vbox_b->pack_start(*hbox4, false, false, 0);
    
    
    /* perfedit widgets */
    m_vadjust = Adjustment::create(0,0,1,1,1,1 );
    m_hadjust = Adjustment::create(0,0,1,1,1,1 );

    m_vscroll   =  manage(new VScrollbar( m_vadjust ));
    m_hscroll   =  manage(new HScrollbar( m_hadjust ));

    m_perfnames = manage( new perfnames( m_mainperf, this, m_vadjust ));

    m_perfroll = manage( new perfroll
                         (
                             m_mainperf,
                             this,
                             m_hadjust,
                             m_vadjust
                         ));
    m_perftime = manage( new perftime( m_mainperf, this, m_hadjust ));
    m_tempo = manage( new tempo( m_mainperf, this, m_hadjust ));
    
    /* init table, viewports and scroll bars */
    m_table     = manage( new Table( 6, 3, false));
    m_table->set_border_width( 2 );

    m_hbox      = manage( new HBox( false, 2 ));
    m_hlbox     = manage( new HBox( false, 2 ));

    m_hlbox->set_border_width( 2 );
    
    m_button_grow = manage( new Button());
    m_button_grow->add( *manage( new Arrow( Gtk::ARROW_RIGHT, Gtk::SHADOW_OUT )));
    m_button_grow->signal_clicked().connect( mem_fun( *this, &mainwnd::grow));
    add_tooltip( m_button_grow, "Increase size of Grid.\n"
            "This does not scroll but adds to the end of\n"
            "the grid if you scroll all the way to the right.");

    /* fill table */
    m_table->attach( *m_hlbox,  0, 3, 0, 1,  Gtk::FILL, Gtk::SHRINK, 2, 0 ); // shrink was 0

    m_table->attach( *m_perfnames,    0, 1, 3, 4, Gtk::FILL, Gtk::FILL );
    m_table->attach( *m_tempo, 1, 2, 1, 2, Gtk::FILL, Gtk::SHRINK );

    /* bpm spin button */
    m_adjust_bpm = Adjustment::create(c_bpm, c_bpm_minimum, c_bpm_maximum, 1);
    m_spinbutton_bpm = manage( new Bpm_spinbutton( m_adjust_bpm ));
    m_spinbutton_bpm->set_editable( true );
    m_spinbutton_bpm->set_digits(2);                    // 2 = two decimal precision
    m_spinbutton_bpm->set_numeric();
    m_spinbutton_bpm->signal_value_changed().connect(mem_fun(*this, &mainwnd::adj_callback_bpm ));

    add_tooltip( m_spinbutton_bpm, "Adjust starting beats per minute (BPM)" );

    /* bpm tap tempo button - sequencer64 */
    m_button_tap = manage(new Button("0"));
    m_button_tap->signal_clicked().connect(mem_fun(*this, &mainwnd::tap));
    add_tooltip
    (
        m_button_tap,
        "Tap in time to set the beats per minute (BPM) value. "
        "After 5 seconds of no taps, the tap-counter will reset to 0. "
        "Also see the File / Options / Keyboard / Tap BPM key assignment."
    );
    
    Gtk::HBox *bpm_hbox = manage( new HBox( false, 2 ));
    
    bpm_hbox->pack_start(*m_spinbutton_bpm, Gtk::PACK_SHRINK);
    bpm_hbox->pack_start( *m_button_tap, false, false );
    
    m_table->attach( *bpm_hbox,0,1,1,3, Gtk::SHRINK, Gtk::SHRINK);

    m_table->attach( *m_perftime, 1, 2, 2, 3, Gtk::FILL, Gtk::SHRINK );
    m_table->attach( *m_perfroll, 1, 2, 3, 4,
                     Gtk::FILL | Gtk::SHRINK,
                     Gtk::FILL | Gtk::SHRINK );

    m_table->attach( *m_vscroll, 2, 3, 3, 4, Gtk::SHRINK, Gtk::FILL | Gtk::EXPAND  );

    m_table->attach( *m_hbox,  0, 1, 4, 5,  Gtk::FILL, Gtk::SHRINK, 0, 2 );
    m_table->attach( *m_hscroll, 1, 2, 4, 5, Gtk::FILL | Gtk::EXPAND, Gtk::SHRINK  );
    m_table->attach( *m_button_grow, 2, 3, 4, 5, Gtk::SHRINK, Gtk::SHRINK  );

    m_menu_xpose =   manage( new Menu());
    char num[11];
    for ( int i=-12; i<=12; ++i)
    {
        if (i)
        {
            snprintf(num, sizeof(num), "%+d [%s]", i, c_interval_text[abs(i)]);
        }
        else
        {
            snprintf(num, sizeof(num), "0 [normal]");
        }

        MenuItem * menu_item = new MenuItem(num);
        menu_item->signal_activate().connect(sigc::bind(mem_fun(*this,&mainwnd::xpose_button_callback), i ));
        m_menu_xpose->insert(*menu_item, 0);
    }

    m_button_xpose = manage( new Button());
    m_button_xpose->add( *manage( new Image(Gdk::Pixbuf::create_from_xpm_data( transpose_xpm ))));
    m_button_xpose->signal_clicked().connect(  sigc::bind<Menu *>( mem_fun( *this, &mainwnd::popup_menu), m_menu_xpose  ));
    add_tooltip( m_button_xpose, "Song transpose" );
    m_entry_xpose = manage( new Entry());
    m_entry_xpose->set_width_chars(3);
    m_entry_xpose->set_editable( false );

    m_menu_snap =   manage( new Menu());
    m_snap_menu_items.resize(25);

    m_snap_menu_items[0].set_label("1/1");
    m_snap_menu_items[0].signal_activate().connect(sigc::bind(mem_fun(*this,&mainwnd::set_snap), 1 ));
    m_menu_snap->append(m_snap_menu_items[0]);

    m_snap_menu_items[1].set_label("1/2");
    m_snap_menu_items[1].signal_activate().connect(sigc::bind(mem_fun(*this,&mainwnd::set_snap), 2 ));
    m_menu_snap->append(m_snap_menu_items[1]);

    m_snap_menu_items[2].set_label("1/4");
    m_snap_menu_items[2].signal_activate().connect(sigc::bind(mem_fun(*this,&mainwnd::set_snap), 4 ));
    m_menu_snap->append(m_snap_menu_items[2]);

    m_snap_menu_items[3].set_label("1/8");
    m_snap_menu_items[3].signal_activate().connect(sigc::bind(mem_fun(*this,&mainwnd::set_snap), 8 ));
    m_menu_snap->append(m_snap_menu_items[3]);

    m_snap_menu_items[4].set_label("1/16");
    m_snap_menu_items[4].signal_activate().connect(sigc::bind(mem_fun(*this,&mainwnd::set_snap), 16 ));
    m_menu_snap->append(m_snap_menu_items[4]);

    m_snap_menu_items[5].set_label("1/32");
    m_snap_menu_items[5].signal_activate().connect(sigc::bind(mem_fun(*this,&mainwnd::set_snap), 32 ));
    m_menu_snap->append(m_snap_menu_items[5]);

    m_menu_snap->append(m_menu_separator6);

    m_snap_menu_items[6].set_label("1/3");
    m_snap_menu_items[6].signal_activate().connect(sigc::bind(mem_fun(*this,&mainwnd::set_snap), 3 ));
    m_menu_snap->append(m_snap_menu_items[6]);

    m_snap_menu_items[7].set_label("1/6");
    m_snap_menu_items[7].signal_activate().connect(sigc::bind(mem_fun(*this,&mainwnd::set_snap), 6 ));
    m_menu_snap->append(m_snap_menu_items[7]);

    m_snap_menu_items[8].set_label("1/12");
    m_snap_menu_items[8].signal_activate().connect(sigc::bind(mem_fun(*this,&mainwnd::set_snap), 12 ));
    m_menu_snap->append(m_snap_menu_items[8]);

    m_snap_menu_items[9].set_label("1/24");
    m_snap_menu_items[9].signal_activate().connect(sigc::bind(mem_fun(*this,&mainwnd::set_snap), 24 ));
    m_menu_snap->append(m_snap_menu_items[9]);

    m_menu_snap->append(m_menu_separator7);

    m_snap_menu_items[10].set_label("1/5");
    m_snap_menu_items[10].signal_activate().connect(sigc::bind(mem_fun(*this,&mainwnd::set_snap), 5 ));
    m_menu_snap->append(m_snap_menu_items[10]);

    m_snap_menu_items[11].set_label("1/10");
    m_snap_menu_items[11].signal_activate().connect(sigc::bind(mem_fun(*this,&mainwnd::set_snap), 10 ));
    m_menu_snap->append(m_snap_menu_items[11]);

    m_snap_menu_items[12].set_label("1/20");
    m_snap_menu_items[12].signal_activate().connect(sigc::bind(mem_fun(*this,&mainwnd::set_snap), 20 ));
    m_menu_snap->append(m_snap_menu_items[12]);

    m_snap_menu_items[13].set_label("1/40");
    m_snap_menu_items[13].signal_activate().connect(sigc::bind(mem_fun(*this,&mainwnd::set_snap), 40 ));
    m_menu_snap->append(m_snap_menu_items[13]);

    m_menu_snap->append(m_menu_separator8);

    m_snap_menu_items[14].set_label("1/7");
    m_snap_menu_items[14].signal_activate().connect(sigc::bind(mem_fun(*this,&mainwnd::set_snap), 7 ));
    m_menu_snap->append(m_snap_menu_items[14]);

    m_snap_menu_items[15].set_label("1/9");
    m_snap_menu_items[15].signal_activate().connect(sigc::bind(mem_fun(*this,&mainwnd::set_snap), 9 ));
    m_menu_snap->append(m_snap_menu_items[15]);

    m_snap_menu_items[16].set_label("1/11");
    m_snap_menu_items[16].signal_activate().connect(sigc::bind(mem_fun(*this,&mainwnd::set_snap), 11 ));
    m_menu_snap->append(m_snap_menu_items[16]);

    m_snap_menu_items[17].set_label("1/13");
    m_snap_menu_items[17].signal_activate().connect(sigc::bind(mem_fun(*this,&mainwnd::set_snap), 13 ));
    m_menu_snap->append(m_snap_menu_items[17]);

    m_snap_menu_items[18].set_label("1/14");
    m_snap_menu_items[18].signal_activate().connect(sigc::bind(mem_fun(*this,&mainwnd::set_snap), 14 ));
    m_menu_snap->append(m_snap_menu_items[18]);

    m_snap_menu_items[19].set_label("1/15");
    m_snap_menu_items[19].signal_activate().connect(sigc::bind(mem_fun(*this,&mainwnd::set_snap), 15 ));
    m_menu_snap->append(m_snap_menu_items[19]);

    m_snap_menu_items[20].set_label("1/18");
    m_snap_menu_items[20].signal_activate().connect(sigc::bind(mem_fun(*this,&mainwnd::set_snap), 18 ));
    m_menu_snap->append(m_snap_menu_items[20]);

    m_snap_menu_items[21].set_label("1/22");
    m_snap_menu_items[21].signal_activate().connect(sigc::bind(mem_fun(*this,&mainwnd::set_snap), 22 ));
    m_menu_snap->append(m_snap_menu_items[21]);

    m_snap_menu_items[22].set_label("1/26");
    m_snap_menu_items[22].signal_activate().connect(sigc::bind(mem_fun(*this,&mainwnd::set_snap), 26 ));
    m_menu_snap->append(m_snap_menu_items[22]);

    m_snap_menu_items[23].set_label("1/28");
    m_snap_menu_items[23].signal_activate().connect(sigc::bind(mem_fun(*this,&mainwnd::set_snap), 28 ));
    m_menu_snap->append(m_snap_menu_items[23]);

    m_snap_menu_items[24].set_label("1/30");
    m_snap_menu_items[24].signal_activate().connect(sigc::bind(mem_fun(*this,&mainwnd::set_snap), 30 ));
    m_menu_snap->append(m_snap_menu_items[24]);

    /* snap */
    m_button_snap = manage( new Button());
    m_button_snap->add( *manage( new Image(Gdk::Pixbuf::create_from_xpm_data( snap_xpm ))));
    m_button_snap->signal_clicked().connect(  sigc::bind<Menu *>( mem_fun( *this, &mainwnd::popup_menu), m_menu_snap  ));
    add_tooltip( m_button_snap, "Grid snap. (Fraction of Measure Length)" );
    m_entry_snap = manage( new Entry());
    m_entry_snap->set_width_chars(4);
    m_entry_snap->set_editable( false );

    m_menu_bp_measure = manage( new Menu() );
    m_menu_bw = manage( new Menu() );

    /* bw */
    m_bw_menu_items.resize(5);

    m_bw_menu_items[0].set_label("1");
    m_bw_menu_items[0].signal_activate().connect(sigc::bind(mem_fun(*this, &mainwnd::bw_button_callback), 1 ));
    m_menu_bw->append(m_bw_menu_items[0]);

    m_bw_menu_items[1].set_label("2");
    m_bw_menu_items[1].signal_activate().connect(sigc::bind(mem_fun(*this, &mainwnd::bw_button_callback), 2 ));
    m_menu_bw->append(m_bw_menu_items[1]);

    m_bw_menu_items[2].set_label("4");
    m_bw_menu_items[2].signal_activate().connect(sigc::bind(mem_fun(*this, &mainwnd::bw_button_callback), 4 ));
    m_menu_bw->append(m_bw_menu_items[2]);

    m_bw_menu_items[3].set_label("8");
    m_bw_menu_items[3].signal_activate().connect(sigc::bind(mem_fun(*this, &mainwnd::bw_button_callback), 8 ));
    m_menu_bw->append(m_bw_menu_items[3]);

    m_bw_menu_items[4].set_label("16");
    m_bw_menu_items[4].signal_activate().connect(sigc::bind(mem_fun(*this, &mainwnd::bw_button_callback), 16 ));
    m_menu_bw->append(m_bw_menu_items[4]);

    char b[20];

    for( int i=0; i<16; i++ )
    {
        snprintf( b, sizeof(b), "%d", i+1 );

        /* length */
        MenuItem * menu_item = new MenuItem(b);
        menu_item->signal_activate().connect(sigc::bind(mem_fun(*this, &mainwnd::bp_measure_button_callback), i + 1 ));
        m_menu_bp_measure->append(*menu_item);
    }

    /* beats per measure */
    m_button_bp_measure = manage( new Button());
    m_button_bp_measure->add( *manage( new Image(Gdk::Pixbuf::create_from_xpm_data( down_xpm  ))));
    m_button_bp_measure->signal_clicked().connect(  sigc::bind<Menu *>( mem_fun( *this, &mainwnd::popup_menu), m_menu_bp_measure  ));
    add_tooltip( m_button_bp_measure, "Time Signature. Beats per Measure" );
    m_entry_bp_measure = manage( new Entry());
    m_entry_bp_measure->set_width_chars(2);
    m_entry_bp_measure->set_editable( false );

    /* beat width */
    m_button_bw = manage( new Button());
    m_button_bw->add( *manage( new Image(Gdk::Pixbuf::create_from_xpm_data( down_xpm  ))));
    m_button_bw->signal_clicked().connect(  sigc::bind<Menu *>( mem_fun( *this, &mainwnd::popup_menu), m_menu_bw  ));
    add_tooltip( m_button_bw, "Time Signature.  Length of Beat" );
    m_entry_bw = manage( new Entry());
    m_entry_bw->set_width_chars(2);
    m_entry_bw->set_editable( false );

    /* swing_amount spin buttons */
    m_adjust_swing_amount8 = Adjustment::create(0, 0, c_max_swing_amount, 1);
    m_spinbutton_swing_amount8 = manage( new SpinButton( m_adjust_swing_amount8 ));

    m_adjust_swing_amount16 = Adjustment::create(0, 0, c_max_swing_amount, 1);
    m_spinbutton_swing_amount16 = manage( new SpinButton( m_adjust_swing_amount16 ));

    m_spinbutton_swing_amount8->set_editable(true);
    m_spinbutton_swing_amount8->set_width_chars(2);
    m_spinbutton_swing_amount8->signal_value_changed().connect(
        mem_fun(*this, &mainwnd::adj_callback_swing_amount8 ));
    add_tooltip( m_spinbutton_swing_amount8, "Adjust 1/8 swing amount" );

    m_spinbutton_swing_amount16->set_editable(true);
    m_spinbutton_swing_amount16->set_width_chars(2);
    m_spinbutton_swing_amount16->signal_value_changed().connect(
        mem_fun(*this, &mainwnd::adj_callback_swing_amount16 ));
    add_tooltip( m_spinbutton_swing_amount16, "Adjust 1/16 swing amount" );

    /* undo */
    m_button_undo = manage( new Button());
    m_button_undo->add( *manage( new Image(Gdk::Pixbuf::create_from_xpm_data( undo_xpm  ))));
    m_button_undo->signal_clicked().connect(  mem_fun( *this, &mainwnd::undo_type));
    add_tooltip( m_button_undo, "Undo." );

    /* redo */
    m_button_redo = manage( new Button());
    m_button_redo->add( *manage( new Image(Gdk::Pixbuf::create_from_xpm_data( redo_xpm  ))));
    m_button_redo->signal_clicked().connect(  mem_fun( *this, &mainwnd::redo_type));
    add_tooltip( m_button_redo, "Redo." );

    /* expand */
    m_button_expand = manage( new Button());
    m_button_expand->add(*manage( new Image(Gdk::Pixbuf::create_from_xpm_data( expand_xpm ))));
    m_button_expand->signal_clicked().connect(  mem_fun( *this, &mainwnd::expand));
    add_tooltip( m_button_expand, "Expand between L and R markers." );

    /* collapse */
    m_button_collapse = manage( new Button());
    m_button_collapse->add(*manage( new Image(Gdk::Pixbuf::create_from_xpm_data( collapse_xpm ))));
    m_button_collapse->signal_clicked().connect(  mem_fun( *this, &mainwnd::collapse));
    add_tooltip( m_button_collapse, "Collapse between L and R markers." );

    /* copy */
    m_button_copy = manage( new Button());
    m_button_copy->add(*manage( new Image(Gdk::Pixbuf::create_from_xpm_data( copy_xpm ))));
    m_button_copy->signal_clicked().connect(  mem_fun( *this, &mainwnd::copy ));
    add_tooltip( m_button_copy, "Expand and copy between L and R markers." );

    m_hlbox->pack_end( *m_button_copy, false, false );
    m_hlbox->pack_end( *m_button_expand, false, false );
    m_hlbox->pack_end( *m_button_collapse, false, false );
    m_hlbox->pack_end( *m_button_redo, false, false );
    m_hlbox->pack_end( *m_button_undo, false, false );
     
    m_hlbox->pack_start( *m_button_bp_measure, false, false );
    m_hlbox->pack_start( *m_entry_bp_measure, false, false );

    m_hlbox->pack_start( *(manage(new Label( "/" ))), false, false, 4);

    m_hlbox->pack_start( *m_button_bw, false, false );
    m_hlbox->pack_start( *m_entry_bw, false, false );

    m_hlbox->pack_start( *(manage(new VSeparator())), false, false, 4);

    m_hlbox->pack_start( *m_button_snap, false, false );
    m_hlbox->pack_start( *m_entry_snap, false, false );

    m_hlbox->pack_start( *m_button_xpose, false, false );
    m_hlbox->pack_start( *m_entry_xpose, false, false );

    m_hlbox->pack_start( *(manage(new Label( "swing" ))), false, false, 4);
    m_hlbox->pack_start(*m_spinbutton_swing_amount8, false, false );
    m_hlbox->pack_start(*(manage( new Label( "1/8" ))), false, false, 4);
    m_hlbox->pack_start(*m_spinbutton_swing_amount16, false, false );
    m_hlbox->pack_start(*(manage( new Label( "1/16" ))), false, false, 4);

    /* set up a vbox, put the menu in it, and add it */
    VBox *vbox = new VBox();
    vbox->pack_start(*hbox1, false, false );
    vbox->pack_start(*m_table, true, true, 0 );

    VBox *ovbox = new VBox();

    ovbox->pack_start(*m_menubar, false, false );
    ovbox->pack_start( *vbox );

    m_perfroll->set_can_focus();
    m_perfroll->grab_focus();

    /* add box */
    this->add (*ovbox);

    /* m_tempo->set_start_tempo(c_bpm);
     * this sets the tempo marker list start.
     * we need to set this here or jack timebase callback
     * will call reset_tempo_play_marker_list(); upon
     * initialization due to newpos being set. The call
     * to reset uses the value of the start tempo from
     * m_list_total_markers - which is sometimes not
     * set do to timing of the tempo marker creation.
     * So, set the default here so we do not get junk.
       */
    m_tempo->set_start_BPM(c_bpm);
    
    set_bw( 4 ); // set this first
    set_snap( 4 );
    set_bp_measure( 4 );
    set_xpose( 0 );

    /* tap button  */
    m_current_beats = 0;
    m_base_time_ms  = 0;
    m_last_time_ms  = 0;

    m_perfroll->init_before_show();

    /* show everything */
    show_all();

    add_events( Gdk::KEY_PRESS_MASK | Gdk::KEY_RELEASE_MASK );

    m_timeout_connect = Glib::signal_timeout().connect(
                            mem_fun(*this, &mainwnd::timer_callback), c_redraw_ms);

}

mainwnd::~mainwnd()
{
    delete m_options;
}

/*
 * move through the playlist (jmp is 0 on start and 1 if right arrow, -1 for left arrow)
 */
bool mainwnd::playlist_jump(int jmp, bool a_verify)
{
    if(global_is_running)                       // don't allow jump if running
        return false;
    
    bool result = false;
    if(a_verify)                                // we will run through all the files
    {
        m_mainperf->set_playlist_index(0);       // start at zero
        jmp = 0;                                // to get the first one
    }

    while(1)
    {
        if(m_mainperf->set_playlist_index(m_mainperf->get_playlist_index() + jmp))
        {
            if(Glib::file_test(m_mainperf->get_playlist_current_file(), Glib::FILE_TEST_EXISTS))
            {
                if(open_file(m_mainperf->get_playlist_current_file()))
                {
                    if(a_verify)    // verify whole playlist
                    {
                        jmp = 1;    // after the first one set to 1 for jump
                        continue;   // keep going till the end of list
                    }
                    result = true;
                    break;
                }
                else
                {
                    Glib::ustring message = "Playlist file open error\n";
                    message += m_mainperf->get_playlist_current_file();
                    m_mainperf->error_message_gtk(message);
                    m_mainperf->set_playlist_mode(false);    // abandon ship
                    result = false;
                    break;  
                }
            }
            else
            {
                Glib::ustring message = "Playlist file does not exist\n";
                message += m_mainperf->get_playlist_current_file();
                m_mainperf->error_message_gtk(message);
                m_mainperf->set_playlist_mode(false);        // abandon ship
                result = false;
                break;  
            }
        }
        else                                                // end of file list
        {
            result = true;  // means we got to the end or beginning, without error
            break;
        }
    }
    
    if(!result)                                             // if errors occured above
    {
        update_window_title();
        update_window_xpm();
    }
    
    return result;
}

bool
mainwnd::verify_playlist_dialog()
{
    Gtk::MessageDialog warning("Do you wish the verify the playlist?\n",
                       false,
                       Gtk::MESSAGE_WARNING, Gtk::BUTTONS_YES_NO, true);

    auto result = warning.run();

    if (result == Gtk::RESPONSE_NO || result == Gtk::RESPONSE_DELETE_EVENT)
    {
        return false;
    }
    
    return true;
}

void
mainwnd::playlist_verify()
{
    bool result = false;
    
    result = playlist_jump(PLAYLIST_ZERO,true);             // true is verify mode
    
    if(result)                                  // everything loaded ok
    {
        m_mainperf->set_playlist_index(PLAYLIST_ZERO);      // set to start
        playlist_jump(PLAYLIST_ZERO);                       // load the first file
        printf("Playlist verification was successful!\n");
    }
    else                                        // verify failed somewhere
    {
        new_file();                             // clear and start clean
    }
}

// This is the GTK timer callback, used to draw our current time and bpm
// and_events, the main window
bool
mainwnd::timer_callback(  )
{
    m_perfroll->redraw_dirty_tracks();
    m_perfroll->draw_progress();
    m_perfnames->redraw_dirty_tracks();

    long ticks = m_mainperf->get_tick();

    m_main_time->idle_progress( ticks );

    /* used on initial file load and during play with tempo changes from markers */
    if ( m_adjust_bpm->get_value() != m_mainperf->get_bpm())
    {
        m_adjust_bpm->set_value( m_mainperf->get_bpm());
    }

    if (m_button_mode->get_active() != global_song_start_mode)        // for seqroll keybinding
    {
        m_button_mode->set_active(global_song_start_mode);
    }

#ifdef JACK_SUPPORT
    if (m_button_jack->get_active() != m_mainperf->get_toggle_jack()) // for seqroll keybinding
    {
        toggle_jack();
    }

    if(global_is_running && m_button_jack->get_sensitive())
    {
        m_button_jack->set_sensitive(false);
    }
    else if(!global_is_running && !m_button_jack->get_sensitive())
    {
        m_button_jack->set_sensitive(true);
    }
#endif // JACK_SUPPORT
    
#ifdef NSM_SUPPORT
    if(m_nsm)
    {
        nsm_check_nowait( m_nsm );
        if (m_nsm_optional_gui && m_nsm_visible != global_nsm_gui)
        {
            m_nsm_visible = global_nsm_gui;
            if (m_nsm_visible)
            {
                show();
                nsm_send_is_shown( m_nsm );
            }
            else
            {
                m_app->hold();
                close_all_windows();
                hide();
                nsm_send_is_hidden( m_nsm );
            }
        }
        if (m_dirty_flag != global_is_modified)
        {
            m_dirty_flag = global_is_modified;
            if (m_dirty_flag)
            {
                nsm_send_is_dirty ( m_nsm );
            }
            else
            {
                nsm_send_is_clean ( m_nsm );
            }
        }
    }
#endif

    if(global_is_running && m_button_mode->get_sensitive())
    {
        m_button_mode->set_sensitive(false);
    }
    else if(!global_is_running && !m_button_mode->get_sensitive())
    {
        m_button_mode->set_sensitive(true);
    }

    if (m_button_follow->get_active() != m_mainperf->get_follow_transport())
    {
        m_button_follow->set_active(m_mainperf->get_follow_transport());
    }

    if(m_mainperf->m_have_undo && !m_button_undo->get_sensitive())
    {
        m_button_undo->set_sensitive(true);
    }
    else if(!m_mainperf->m_have_undo && m_button_undo->get_sensitive())
    {
        m_button_undo->set_sensitive(false);
    }

    if(m_mainperf->m_have_redo && !m_button_redo->get_sensitive())
    {
        m_button_redo->set_sensitive(true);
    }
    else if(!m_mainperf->m_have_redo && m_button_redo->get_sensitive())
    {
        m_button_redo->set_sensitive(false);
    }

    /* Tap button - sequencer64 */
    if (m_current_beats > 0)
    {
        if (m_last_time_ms > 0)
        {
            struct timespec spec;
            clock_gettime(CLOCK_REALTIME, &spec);
            long ms = long(spec.tv_sec) * 1000;     /* seconds to ms        */
            ms += round(spec.tv_nsec * 1.0e-6);     /* nanoseconds to ms    */
            long difference = ms - m_last_time_ms;
            if (difference > 5000L)                 /* 5 second wait        */
            {
                m_current_beats = m_base_time_ms = m_last_time_ms = 0;
                set_tap_button(0);
            }
        }
    }
    
    /* Undo and redo */
    if(m_mainperf->get_tempo_load())
    {
        m_mainperf->set_tempo_load(false);
        m_tempo->load_tempo_list();
    }
    
    if(m_spinbutton_bpm->get_have_leave() && !m_spinbutton_bpm->get_have_typing())
    {
        if(m_spinbutton_bpm->get_hold_bpm() != m_adjust_bpm->get_value() &&
                m_spinbutton_bpm->get_hold_bpm() != 0.0)
        {
            m_tempo->push_undo(true);                   // use the hold marker
            m_spinbutton_bpm->set_have_leave(false);
        }
    }
    /* when in playlist mode, tempo stop markers trigger play file increment.
     We have to let the transport completely stop before doing the 
     file loading or strange things happen*/
    if(m_mainperf->m_playlist_stop_mark && !global_is_running)
    {
        m_mainperf->m_playlist_stop_mark = false;
        playlist_jump(PLAYLIST_NEXT);    // next file
    }
    
    if(m_mainperf->m_playlist_midi_control_set && !global_is_running)
    {
        m_mainperf->m_playlist_midi_control_set = false;
        playlist_jump(m_mainperf->m_playlist_midi_jump_value);
        m_mainperf->m_playlist_midi_jump_value = PLAYLIST_ZERO;
    }
    
    /* Calculate the current time/BBT, and display it. */
    if (global_is_running || m_mainperf->get_reposition() || m_toggle_time_type)
    {
        m_toggle_time_type = false;
        if (m_tick_time_as_bbt)
        {
            std::string t = tick_to_measurestring(ticks);
            m_tick_time->set_text(t);
        }
        else
        {
            std::string t = tick_to_timestring(ticks); 
            m_tick_time->set_text(t);
        }
    }
    
    /* Shut off the reposition flag after the reposition */
    if (!global_is_running)
    {
        if((m_mainperf->get_starting_tick() == m_mainperf->get_tick()) && m_mainperf->get_reposition())
        {
            m_mainperf->set_reposition(false);
        }
    }

    return true;
}

void
mainwnd::undo_type()
{
    if(m_tempo->get_hold_undo())
        return;
    
    char type = '\0';
    if(m_mainperf->undo_vect.size() > 0)
        type = m_mainperf->undo_vect[m_mainperf->undo_vect.size() -1].type;
    else
        return;

    switch (type)
    {
    case c_undo_trigger:
        undo_trigger(m_mainperf->undo_vect[m_mainperf->undo_vect.size() -1].track);
        break;
    case c_undo_track:
        undo_track(m_mainperf->undo_vect[m_mainperf->undo_vect.size() -1].track);
        break;
    case c_undo_perf:
        undo_perf();
        break;
    case c_undo_collapse_expand:
        undo_trigger();
        break;
    case c_undo_bpm:
        undo_bpm();
        break;
    case c_undo_import:
        undo_perf(true);
        break;
    default:
        break;
    }
    m_mainperf->set_have_undo();
}

void
mainwnd::undo_trigger(int a_track)
{
    m_mainperf->pop_trigger_undo(a_track);
}

void
mainwnd::undo_trigger() // collapse and expand
{
    m_mainperf->pop_trigger_undo();
}

void
mainwnd::undo_track( int a_track )
{
    m_mainperf->pop_track_undo(a_track);
}

void
mainwnd::undo_perf(bool a_import)
{
    m_mainperf->pop_perf_undo(a_import);
}

void
mainwnd::undo_bpm()
{
    m_tempo->pop_undo();
}

void
mainwnd::redo_type()
{
    if(m_tempo->get_hold_undo())
        return;
    
    char type = '\0';
    if(m_mainperf->redo_vect.size() > 0)
        type = m_mainperf->redo_vect[m_mainperf->redo_vect.size() -1].type;
    else
        return;

    switch (type)
    {
    case c_undo_trigger:
        redo_trigger(m_mainperf->redo_vect[m_mainperf->redo_vect.size() - 1].track);
        break;
    case c_undo_track:
        redo_track(m_mainperf->redo_vect[m_mainperf->redo_vect.size() - 1].track);
        break;
    case c_undo_perf:
        redo_perf();
        break;
    case c_undo_collapse_expand:
        redo_trigger();
        break;
    case c_undo_bpm:
        redo_bpm();
        break;
    case c_undo_import:
        redo_perf(true);
        break;
    default:
        break;
    }
    m_mainperf->set_have_redo();
}

void
mainwnd::redo_trigger(int a_track) // single track
{
    m_mainperf->pop_trigger_redo(a_track);
}

void
mainwnd::redo_trigger()
{
    m_mainperf->pop_trigger_redo();
}

void
mainwnd::redo_track( int a_track )
{
    m_mainperf->pop_track_redo(a_track);
}

void
mainwnd::redo_perf(bool a_import)
{
    m_mainperf->pop_perf_redo(a_import);
}

void
mainwnd::redo_bpm()
{
    m_tempo->pop_redo();
}

void
mainwnd::start_playing()
{
    m_mainperf->start_playing();
}

void
mainwnd::set_continue_callback()
{
    m_mainperf->set_continue(m_button_continue->get_active());
    bool is_active = m_button_continue->get_active();
    
    std::string label = is_active ? "P" : "S";
    Gtk::Label * lblptr(dynamic_cast<Gtk::Label *>
    (
         m_button_continue->get_child())
    );
    if (lblptr != NULL)
        lblptr->set_text(label);
}

void
mainwnd::stop_playing()
{
    m_mainperf->stop_playing();
}

void
mainwnd::rewind(bool a_press)
{
    if(a_press)
    {
        if(FF_RW_button_type == FF_RW_REWIND) // for key repeat, just ignore repeat
            return;
        
        FF_RW_button_type = FF_RW_REWIND;
    }
    else
        FF_RW_button_type = FF_RW_RELEASE;

    g_timeout_add(120,FF_RW_timeout,m_mainperf);
}

void
mainwnd::fast_forward(bool a_press)
{
    if(a_press)
    {
        if(FF_RW_button_type == FF_RW_FORWARD) // for key repeat, just ignore repeat
            return;
        
        FF_RW_button_type = FF_RW_FORWARD;
    }
    else
        FF_RW_button_type = FF_RW_RELEASE;

    g_timeout_add(120,FF_RW_timeout,m_mainperf);
}

void
mainwnd::collapse() // all tracks
{
    m_mainperf->push_trigger_undo();
    m_mainperf->move_triggers( false );
    m_perfroll->redraw_all_tracks();
}

void
mainwnd::copy() // all tracks
{
    m_mainperf->push_trigger_undo();
    m_mainperf->copy_triggers(  );
    m_perfroll->redraw_all_tracks();
}

void
mainwnd::expand() // all tracks
{
    m_mainperf->push_trigger_undo();
    m_mainperf->move_triggers( true );
    m_perfroll->redraw_all_tracks();
}

void
mainwnd::set_looped()
{
    m_mainperf->set_looping( m_button_loop->get_active());
}

void
mainwnd::toggle_looped() // for key mapping
{
    // Note that this will trigger the button signal callback.
    m_button_loop->set_active( ! m_button_loop->get_active() );
}

void
mainwnd::set_song_mode()
{
    global_song_start_mode = m_button_mode->get_active();
    
    bool is_active = m_button_mode->get_active();
    
    /*
     * spaces with 'Live' are to keep button width close
     * to the same when changed for cosmetic purposes.
     */
    
    std::string label = is_active ? "Song" : " Live ";
    Gtk::Label * lblptr(dynamic_cast<Gtk::Label *>
    (
         m_button_mode->get_child())
    );
    if (lblptr != NULL)
        lblptr->set_text(label);
}

void
mainwnd::toggle_song_mode()
{
    // Note that this will trigger the button signal callback.
    if(!global_is_running)
    {
        m_button_mode->set_active( ! m_button_mode->get_active() );
    }
}

void
mainwnd::set_jack_mode ()
{
    if(m_button_jack->get_active() && !global_is_running)
        m_mainperf->init_jack ();

    if(!m_button_jack->get_active() && !global_is_running)
        m_mainperf->deinit_jack ();

    if(m_mainperf->is_jack_running())
        m_button_jack->set_active(true);
    else
        m_button_jack->set_active(false);

    m_mainperf->set_jack_mode(m_mainperf->is_jack_running()); // for seqroll keybinding

    // for setting the transport tick to display in the correct location
    // FIXME currently does not work for slave from disconnected - need jack position
    if(global_song_start_mode)
    {
        m_mainperf->set_starting_tick(m_mainperf->get_left_tick());
        
        if(m_mainperf->is_jack_running() && m_mainperf->is_jack_master() )
            m_mainperf->position_jack(true, m_mainperf->get_left_tick());
        
        m_mainperf->set_reposition();
    }
    else
        m_mainperf->set_starting_tick(m_mainperf->get_tick());
}

void
mainwnd::toggle_jack()
{
    // Note that this will trigger the button signal callback.
    m_button_jack->set_active( ! m_button_jack->get_active() );
}

void
mainwnd::set_follow_transport()
{
    m_mainperf->set_follow_transport(m_button_follow->get_active());
}

void
mainwnd::toggle_follow_transport()
{
    // Note that this will trigger the button signal callback.
    m_button_follow->set_active( ! m_button_follow->get_active() );
}

void
mainwnd::popup_menu(Menu *a_menu)
{
    a_menu->show_all();
    a_menu->popup_at_pointer(NULL);
}

void
mainwnd::set_guides()
{
    long measure_ticks = (c_ppqn * 4) * m_bp_measure / m_bw;
    long snap_ticks =  measure_ticks / m_snap;
    long beat_ticks = (c_ppqn * 4) / m_bw;
    m_perfroll->set_guides( snap_ticks, measure_ticks, beat_ticks );
    m_perftime->set_guides( snap_ticks, measure_ticks );
    m_tempo->set_guides( snap_ticks, measure_ticks );
}

void
mainwnd::set_snap( int a_snap  )
{
    char b[10];
    snprintf( b, sizeof(b), "1/%d", a_snap );
    m_entry_snap->set_text(b);

    m_snap = a_snap;
    set_guides();
}

void mainwnd::bp_measure_button_callback(int a_beats_per_measure)
{
    if(m_bp_measure != a_beats_per_measure )
    {
        set_bp_measure(a_beats_per_measure);
        global_is_modified = true;
    }
}

void mainwnd::set_bp_measure( int a_beats_per_measure )
{
    if(a_beats_per_measure < 1 || a_beats_per_measure > 16)
        a_beats_per_measure = 4;

    m_mainperf->set_bp_measure(a_beats_per_measure);

    if(a_beats_per_measure <= 7)
        set_snap(a_beats_per_measure *2);
    else
        set_snap(a_beats_per_measure);

    char b[10];
    snprintf(b, sizeof(b), "%d", a_beats_per_measure );
    m_entry_bp_measure->set_text(b);

    m_bp_measure = a_beats_per_measure;
    set_guides();
}

void mainwnd::bw_button_callback(int a_beat_width)
{
    if(m_bw != a_beat_width )
    {
        set_bw(a_beat_width);
        global_is_modified = true;
    }
}

void mainwnd::set_bw( int a_beat_width )
{
    if(a_beat_width < 1 || a_beat_width > 16)
        a_beat_width = 4;

    m_mainperf->set_bw(a_beat_width);
    char b[10];
    snprintf(b, sizeof(b), "%d", a_beat_width );
    m_entry_bw->set_text(b);

    m_bw = a_beat_width;
    set_guides();
}

int mainwnd::get_bw()
{
    return m_bw;
}

void
mainwnd::set_zoom (int z)
{
    m_perfroll->set_zoom(z);
    m_perftime->set_zoom(z);
    m_tempo->set_zoom(z);
}

void
mainwnd::xpose_button_callback( int a_xpose)
{
    if(m_mainperf->get_master_midi_bus()->get_transpose() != a_xpose)
    {
        set_xpose(a_xpose);
    }
}

void
mainwnd::set_xpose( int a_xpose  )
{
    char b[11];
    snprintf( b, sizeof(b), "%+d", a_xpose );
    m_entry_xpose->set_text(b);

    m_mainperf->all_notes_off();
    m_mainperf->get_master_midi_bus()->set_transpose(a_xpose);
}

void
mainwnd::tap ()
{
    if(!m_tempo->get_hold_undo())
        m_tempo->set_hold_undo(true);
    
    double bpm = update_bpm();
    set_tap_button(m_current_beats);
    if (m_current_beats > 1)                    /* first one is useless */
        m_adjust_bpm->set_value(double(bpm));
}

void
mainwnd::set_tap_button (int beats)
{
    if(beats == 0)
    {
        m_tempo->push_undo(true);
    }
    
    Gtk::Label * tapptr(dynamic_cast<Gtk::Label *>(m_button_tap->get_child()));
    if (tapptr != nullptr)
    {
        char temp[8];
        snprintf(temp, sizeof(temp), "%d", beats);
        tapptr->set_text(temp);
    }
}

double
mainwnd::update_bpm ()
{
    double bpm = 0.0;
    struct timespec spec;
    clock_gettime(CLOCK_REALTIME, &spec);
    long ms = long(spec.tv_sec) * 1000;     /* seconds to milliseconds      */
    ms += round(spec.tv_nsec * 1.0e-6);     /* nanoseconds to milliseconds  */
    if (m_current_beats == 0)
    {
        m_base_time_ms = ms;
        m_last_time_ms = 0;
    }
    else if (m_current_beats >= 1)
    {
        int diffms = ms - m_base_time_ms;
        bpm = m_current_beats * 60000.0 / diffms;
        m_last_time_ms = ms;
    }
    ++m_current_beats;
    return bpm;
}

void
mainwnd::grow()
{
    m_perfroll->increment_size();
}

void
mainwnd::delete_unused_seq()
{
    m_mainperf->delete_unused_sequences();
}

void
mainwnd::create_triggers()
{
    if(global_is_running)
        return;

    m_mainperf->create_triggers();
}

void
mainwnd::apply_song_transpose()
{
    if(global_is_running)
        return;

    if(m_mainperf->get_master_midi_bus()->get_transpose() != 0)
    {
        m_mainperf->apply_song_transpose();
        set_xpose(0);
    }
}

void
mainwnd::open_seqlist()
{
    if(m_mainperf->get_seqlist_open())
    {
        m_mainperf->set_seqlist_toggle(true);
    }
    else
    {
        seqlist * a_seqlist = new seqlist(m_mainperf, this);
        set_window_pointer(a_seqlist);
    }
}

void
mainwnd::set_song_mute(mute_op op)
{
    m_mainperf->set_song_mute(op);
    global_is_modified = true;
}

void
mainwnd::options_dialog()
{
    if ( m_options != NULL )
        delete m_options;
    m_options = new options( *this,  m_mainperf );
    m_options->show_all();
}

/* callback function */
void mainwnd::file_new()
{
    if (is_save())
        new_file();
}

void mainwnd::new_file()
{
    if(m_mainperf->clear_all())
    {
        m_tempo->clear_tempo_list();
        m_tempo->set_start_BPM(c_bpm);
        set_bp_measure(4);
        set_bw(4);
        set_xpose(0);
        set_swing_amount8(0);
        set_swing_amount16(0);
        m_mainperf->set_playlist_mode(false);
        
        global_filename = "";
        update_window_title();
        update_window_xpm();
        global_is_modified = false;
    }
    else
    {
        new_open_error_dialog();
        return;
    }
}

/* callback function */
void mainwnd::file_save()
{
    save_file();
}

/* callback function */
void mainwnd::file_save_as(file_type_e type, void *a_seq_or_track)
{
    Gtk::FileChooserDialog dialog("Save file as",
                                  Gtk::FILE_CHOOSER_ACTION_SAVE);
    
    switch(type)
    {
    case E_MIDI_SEQ24_FORMAT:
        dialog.set_title("Midi export (Seq 24/32/64)");
        break;
        
    case E_MIDI_SONG_FORMAT:
        dialog.set_title("Midi export song triggers");
        break;
        
    case E_MIDI_SOLO_SEQUENCE:
        dialog.set_title("Midi export sequence");
        break;
        
    case E_MIDI_SOLO_TRIGGER:
        dialog.set_title("Midi export solo trigger");
        break;
        
    case E_MIDI_SOLO_TRACK:
        dialog.set_title("Midi export solo track");
        break;
        
    default:            // Save file as -- native .s42 format
        break;
    }

    dialog.set_transient_for(*this);

    dialog.add_button(Gtk::Stock::CANCEL, Gtk::RESPONSE_CANCEL);
    dialog.add_button(Gtk::Stock::SAVE, Gtk::RESPONSE_OK);

    auto filter_midi = Gtk::FileFilter::create();

    if(type == E_SEQ42_NATIVE_FILE)
    {
        filter_midi->set_name("Seq42 files");
        filter_midi->add_pattern("*.s42");
    }
    else
    {
        filter_midi->set_name("MIDI files");
        filter_midi->add_pattern("*.midi");
        filter_midi->add_pattern("*.MIDI");
        filter_midi->add_pattern("*.mid");
        filter_midi->add_pattern("*.MID");
    }

    dialog.add_filter(filter_midi);

    auto filter_any = Gtk::FileFilter::create();
    filter_any->set_name("Any files");
    filter_any->add_pattern("*");
    dialog.add_filter(filter_any);

    if(type == E_SEQ42_NATIVE_FILE) // .s42
    {
        dialog.set_current_folder(last_used_dir);
    }
    else                            // midi
    {
        dialog.set_current_folder(last_midi_dir);
    }
    
    int result = dialog.run();

    switch (result)
    {
    case Gtk::RESPONSE_OK:
    {
        std::string fname = dialog.get_filename();
        auto current_filter = dialog.get_filter();

        if ((current_filter) &&
                (current_filter->get_name() == "Seq42 files"))
        {
            // check for Seq42 file extension; if missing, add .s42
            std::string suffix = fname.substr(
                                     fname.find_last_of(".") + 1, std::string::npos);
            toLower(suffix);
            if (suffix != "s42") fname = fname + ".s42";
        }

        if ((current_filter) &&
                (current_filter->get_name() == "MIDI files"))
        {
            // check for MIDI file extension; if missing, add .midi
            std::string suffix = fname.substr(
                                     fname.find_last_of(".") + 1, std::string::npos);
            toLower(suffix);
            if ((suffix != "midi") && (suffix != "mid"))
                fname = fname + ".midi";
        }

        if (Glib::file_test(fname, Glib::FILE_TEST_EXISTS))
        {
            Gtk::MessageDialog warning(*this,
                                       "File already exists!\n"
                                       "Do you want to overwrite it?",
                                       false,
                                       Gtk::MESSAGE_WARNING, Gtk::BUTTONS_YES_NO, true);
            auto result = warning.run();

            if (result == Gtk::RESPONSE_NO)
                return;
        }

        if(type == E_SEQ42_NATIVE_FILE)
        {
            global_filename = fname;
            update_window_title();
            update_window_xpm();
            save_file();
        }
        else
        {
            export_midi(fname, type, a_seq_or_track); //  a_seq_or_track will be nullptr if type = E_MIDI_SEQ24_FORMAT
        }

        break;
    }

    default:
        break;
    }
}

void mainwnd::export_midi(const Glib::ustring& fn, file_type_e type, void *a_seq_or_track)
{
    bool result = false;

    midifile f(fn);

    if(type == E_MIDI_SEQ24_FORMAT || type == E_MIDI_SOLO_SEQUENCE )
        result = f.write_sequences(m_mainperf, (sequence*)a_seq_or_track);  // seq24 format will be nullptr
    else
        result = f.write_song(m_mainperf, type,(track*)a_seq_or_track);     // song format will be nullptr
    

    if (!result)
    {
        Gtk::MessageDialog errdialog
        (
            *this,
            "Error writing file.",
            false,
            Gtk::MESSAGE_ERROR, Gtk::BUTTONS_OK,
            true
        );
        errdialog.run();
    }

    if(result)
        last_midi_dir = fn.substr(0, fn.rfind("/") + 1);
}

void mainwnd::new_open_error_dialog()
{
    Gtk::MessageDialog errdialog
    (
        *this,
        "All track edit and sequence edit\nwindows must be closed\nbefore opening a new file.",
        false,
        Gtk::MESSAGE_ERROR, Gtk::BUTTONS_OK,
        true
    );
    errdialog.run();
}

bool mainwnd::open_file(const Glib::ustring& fn)
{
    bool result;

    if(m_mainperf->clear_all())
    {
        m_tempo->clear_tempo_list();
        set_xpose(0);
        
        s42file f;
        result = f.load(fn, m_mainperf, this);

        global_is_modified = !result;

        if (!result)
        {
            Gtk::MessageDialog errdialog
            (
                *this,
                "Error reading file: " + fn,
                false,
                Gtk::MESSAGE_ERROR, Gtk::BUTTONS_OK,
                true
            );
            errdialog.run();
            global_filename = "";
            new_file();
            return false;
        }

        last_used_dir = fn.substr(0, fn.rfind("/") + 1);
        global_filename = fn;
        
        if(!m_mainperf->get_playlist_mode())            /* don't list files from playlist */
        {
            m_mainperf->add_recent_file(fn);           /* from Oli Kester's Kepler34/Sequencer 64       */
            update_recent_files_menu();
        }
        
        update_window_title();
        update_window_xpm();
    }
    else
    {
        new_open_error_dialog();
        return false;
    }
    return true;
}

void mainwnd::export_sequence_midi(sequence *a_seq)
{
    file_save_as(E_MIDI_SOLO_SEQUENCE, a_seq);
}

void mainwnd::export_trigger_midi(track *a_track)
{
    file_save_as(E_MIDI_SOLO_TRIGGER, a_track);
}

void mainwnd::export_track_midi(int a_track)
{
    file_save_as(E_MIDI_SOLO_TRACK, m_mainperf->get_track(a_track));
}

/*callback function*/
void mainwnd::file_open()
{
    if (is_save())
        choose_file();
}

/*callback function*/
void mainwnd::file_open_playlist()
{
    if (is_save())
    {
        choose_file(true);
    }
}

void mainwnd::choose_file(const bool playlist_mode)
{
    Gtk::FileChooserDialog dialog("Open Seq42 file",
                                  Gtk::FILE_CHOOSER_ACTION_OPEN);
    dialog.set_transient_for(*this);

    if(playlist_mode)
    	dialog.set_title("Open Playlist file");

    dialog.add_button(Gtk::Stock::CANCEL, Gtk::RESPONSE_CANCEL);
    dialog.add_button(Gtk::Stock::OPEN, Gtk::RESPONSE_OK);

    if(!playlist_mode)
    {
        auto filter_midi = Gtk::FileFilter::create();
        filter_midi->set_name("Seq42 files");
        filter_midi->add_pattern("*.s42");
        dialog.add_filter(filter_midi);
    }
    
    auto filter_any = Gtk::FileFilter::create();
    filter_any->set_name("Any files");
    filter_any->add_pattern("*");
    dialog.add_filter(filter_any);

    dialog.set_current_folder(last_used_dir);

    int result = dialog.run();

    switch(result)
    {
    case(Gtk::RESPONSE_OK):
        if(playlist_mode)
        {
            m_mainperf->set_playlist_mode(true);
            m_mainperf->set_playlist_file(dialog.get_filename());
            
            if(m_mainperf->get_playlist_mode())  // true means file load with no errors
            {
                if(verify_playlist_dialog())
                {
                    playlist_verify();
                }
                else
                {
                    playlist_jump(PLAYLIST_ZERO);
                }
            
                update_window_title();
                update_window_xpm();
            }
        }
        else
        {
            m_mainperf->set_playlist_mode(playlist_mode); // clear playlist flag if set.
            open_file(dialog.get_filename());
            update_window_title();
            update_window_xpm();
        }
    default:
        break;
    }
}

bool mainwnd::save_file()
{
    bool result = false;

    if (global_filename == "")
    {
        file_save_as(E_SEQ42_NATIVE_FILE, nullptr);
        return true;
    }

    s42file f;
    result = f.save(global_filename, m_mainperf);

    if (result && !m_mainperf->get_playlist_mode())            /* don't list files from playlist */
    {
        m_mainperf->add_recent_file(global_filename);
        update_recent_files_menu();
    }
    else if (!result)
    {
        Gtk::MessageDialog errdialog
        (
            *this,
            "Error writing file.",
            false,
            Gtk::MESSAGE_ERROR, Gtk::BUTTONS_OK,
            true
        );
        errdialog.run();
    }
    global_is_modified = !result;
    return result;
}

int mainwnd::query_save_changes()
{
    Glib::ustring query_str;

    if (global_filename == "")
        query_str = "Unnamed file was changed.\nSave changes?";
    else
        query_str = "File '" + global_filename + "' was changed.\n"
                    "Save changes?";

    Gtk::MessageDialog dialog
    (
        *this,
        query_str,
        false,
        Gtk::MESSAGE_QUESTION,
        Gtk::BUTTONS_NONE,
        true
    );

    dialog.add_button(Gtk::Stock::YES, Gtk::RESPONSE_YES);
    dialog.add_button(Gtk::Stock::NO, Gtk::RESPONSE_NO);
    dialog.add_button(Gtk::Stock::CANCEL, Gtk::RESPONSE_CANCEL);

    return dialog.run();
}

bool mainwnd::is_save()
{
    bool result = false;

    if (global_is_modified)
    {
        int choice = query_save_changes();
        switch (choice)
        {
        case Gtk::RESPONSE_YES:
            if (save_file())
                result = true;
            break;
        case Gtk::RESPONSE_NO:
            result = true;
            break;
        case Gtk::RESPONSE_CANCEL:
        default:
            break;
        }
    }
    else
        result = true;

    return result;
}

/**
 *  Sets up the recent .s42 files menu.  If the menu already exists, delete it.
 *  Then recreate the new menu named "&Recent .s42 files...".  Add all of the
 *  entries present in the m_mainperf->recent_files_count() list.  Hook each entry up
 *  to the open_file() function with each file-name as a parameter.  If there
 *  are none, just add a disabled "<none>" entry.
 */

#define SET_FILE    mem_fun(*this, &mainwnd::load_recent_file)

void
mainwnd::update_recent_files_menu ()
{
#ifdef NSM_SUPPORT
    if(m_nsm)
        return;
#endif

    if (m_menu_recent != nullptr)
    {
        // Nothing to do??
    }
    else
    {
        m_menu_recent = manage(new Gtk::Menu());
        MenuItem * menu_item = new MenuItem("Recent Files");
        menu_item->set_submenu(*m_menu_recent);
        m_menu_file->append(*menu_item);
    }

    if (m_mainperf->recent_file_count() > 0)
    {
        for (int i = 0; i < m_mainperf->recent_file_count(); ++i)
        {
            std::string filepath = m_mainperf->recent_file(i);     // shortened name
            MenuItem * menu_item = new MenuItem(filepath);
            menu_item->signal_activate().connect(sigc::bind(SET_FILE, i));
            m_menu_recent->append(*menu_item);
        }
    }
    else
    {
        MenuItem * menu_item = new MenuItem("<none>");
        menu_item->signal_activate().connect(sigc::bind(SET_FILE, (-1)));
        m_menu_recent->append(*menu_item);
    }
}

/**
 *  Looks up the desired recent .s42 file and opens it.  This function passes
 *  false as the shorten parameter of m_mainperf::recent_file().
 *
 * \param index
 *      Indicates which file in the list to open, ranging from 0 to the number
 *      of recent files minus 1.  If set to -1, then nothing is done.
 */

void
mainwnd::load_recent_file (int index)
{
    if (index >= 0 and index < m_mainperf->recent_file_count())
    {
        if (is_save())
        {
            std::string filepath = m_mainperf->recent_file(index, false);
            m_mainperf->set_playlist_mode(false);
            open_file(filepath);
        }
    }
}

/* convert string to lower case letters */
void
mainwnd::toLower(basic_string<char>& s)
{
    for (basic_string<char>::iterator p = s.begin();
            p != s.end(); p++)
    {
        *p = tolower(*p);
    }
}

void
mainwnd::file_import_dialog()
{
    Gtk::FileChooserDialog dialog("Import MIDI file",
                                  Gtk::FILE_CHOOSER_ACTION_OPEN);
    dialog.set_transient_for(*this);

    auto filter_midi = Gtk::FileFilter::create();
    filter_midi->set_name("MIDI files");
    filter_midi->add_pattern("*.midi");
    filter_midi->add_pattern("*.MIDI");
    filter_midi->add_pattern("*.mid");
    filter_midi->add_pattern("*.MID");
    dialog.add_filter(filter_midi);

    auto filter_any = Gtk::FileFilter::create();
    filter_any->set_name("Any files");
    filter_any->add_pattern("*");
    dialog.add_filter(filter_any);

    dialog.set_current_folder(last_midi_dir);

    ButtonBox *btnbox = dialog.get_action_area();
    HBox hbox( false, 2 );

    m_adjust_load_offset = Adjustment::create(-1, -1, SEQ24_SCREEN_SET_SIZE - 1, 1 );
    m_spinbutton_load_offset = manage( new SpinButton( m_adjust_load_offset ));
    m_spinbutton_load_offset->set_editable( true );
    m_spinbutton_load_offset->set_wrap( true );
    hbox.pack_end(*m_spinbutton_load_offset, false, false );

    hbox.pack_end(*(manage( new Label("Seq24 Screen Import"))), false, false, 4);

    btnbox->pack_start(hbox, false, false );

    dialog.add_button(Gtk::Stock::CANCEL, Gtk::RESPONSE_CANCEL);
    dialog.add_button(Gtk::Stock::OPEN, Gtk::RESPONSE_OK);

    dialog.show_all_children();

    int result = dialog.run();

    //Handle the response:
    switch(result)
    {
    case(Gtk::RESPONSE_OK):
    {
        try
        {
            midifile f( dialog.get_filename() );

            if(f.parse( m_mainperf, this, (int) m_adjust_load_offset->get_value() ))
            {
                last_midi_dir = dialog.get_filename().substr(0, dialog.get_filename().rfind("/") + 1);
            }
            else return;
        }
        catch(...)
        {
            Gtk::MessageDialog errdialog
            (
                *this,
                "Error reading file.",
                false,
                Gtk::MESSAGE_ERROR,
                Gtk::BUTTONS_OK,
                true
            );
            errdialog.run();
        }
        global_is_modified = true;

        m_adjust_bpm->set_value( m_mainperf->get_bpm() );

        break;
    }

    case(Gtk::RESPONSE_CANCEL):
        break;

    default:
        break;
    }
}

/*callback function*/
void mainwnd::file_exit()
{
    if (m_nsm && m_nsm_optional_gui)
    {
        global_nsm_gui = false;
    }
    else
    {
        if (is_save())
        {
            if (global_is_running)
                stop_playing();

            hide();
        }
    }
}

bool
mainwnd::on_delete_event(GdkEventAny *a_e)
{
    if (m_nsm && m_nsm_optional_gui)
    {
        // nsm : hide gui instead of closing
        global_nsm_gui = false;
        return true;
    }
    
    bool result = is_save();
    if (result && global_is_running)
        stop_playing();

    return !result;
}

void
mainwnd::about_dialog()
{
    Gtk::AboutDialog dialog;
    dialog.set_transient_for(*this);
    dialog.set_version(VERSION);
    dialog.set_comments("Interactive MIDI Sequencer\n");

    dialog.set_copyright(
        "(C) 2015 - present Stazed\n"
        "(C) 2010 - 2013 Sam Brauer\n"
        "(C) 2008 - 2009 Seq24team\n"
        "(C) 2002 - 2006 Rob C. Buse");
    dialog.set_website("https://github.com/Stazed/seq42");

    dialog.set_logo(Gdk::Pixbuf::create_from_xpm_data( seq42_xpm  ));

    Glib::ustring null_license;
    dialog.set_license(null_license);

    dialog.set_license_type(LICENSE_GPL_3_0);

    std::vector<Glib::ustring> list_authors;

    list_authors.push_back("Rob C. Buse <rcb@filter24.org>");
    list_authors.push_back("Ivan Hernandez <ihernandez@kiusys.com>");
    list_authors.push_back("Guido Scholz <guido.scholz@bayernline.de>");
    list_authors.push_back("Jaakko Sipari <jaakko.sipari@gmail.com>");
    list_authors.push_back("Peter Leigh <pete.leigh@gmail.com>");
    list_authors.push_back("Anthony Green <green@redhat.com>");
    list_authors.push_back("Daniel Ellis <mail@danellis.co.uk>");
    list_authors.push_back("Kevin Meinert <kevin@subatomicglue.com>");
    list_authors.push_back("Sam Brauer <sam.brauer@gmail.com>");
    list_authors.push_back("Stazed <stazed@mapson.com>");
    dialog.set_authors(list_authors);

    dialog.show();
    dialog.run();
}

void
mainwnd::adj_callback_bpm( )
{
    if(m_mainperf->get_bpm() !=  m_adjust_bpm->get_value())
    {
        if(m_spinbutton_bpm->get_have_typing())
        {
            m_tempo->push_undo();
            m_tempo->set_start_BPM(m_adjust_bpm->get_value());
            m_spinbutton_bpm->set_hold_bpm(0.0);
            return;
        }
        if(m_spinbutton_bpm->get_have_enter())      // for user using spinner
        {
            if(!m_tempo->get_hold_undo())
                m_tempo->set_hold_undo(true);

            m_spinbutton_bpm->set_have_enter(false);
        }
        /* call to set_start_BPM will call m_mainperf->set_bpm() */
        m_tempo->set_start_BPM(m_adjust_bpm->get_value());
    }
}

void
mainwnd::adj_callback_swing_amount8( )
{
    if(m_mainperf->get_swing_amount8() != (int) m_adjust_swing_amount8->get_value())
    {
        m_mainperf->set_swing_amount8( (int) m_adjust_swing_amount8->get_value());
        global_is_modified = true;
    }
}

void
mainwnd::adj_callback_swing_amount16( )
{
    if(m_mainperf->get_swing_amount16() != (int) m_adjust_swing_amount16->get_value())
    {
        m_mainperf->set_swing_amount16( (int) m_adjust_swing_amount16->get_value());
        global_is_modified = true;
    }
}

bool
mainwnd::on_key_press_event(GdkEventKey* a_ev)
{
    // control and modifier key combinations matching
    if ( a_ev->state & GDK_CONTROL_MASK )
    {
        /* Ctrl-Z: Undo */
        if ( a_ev->keyval == GDK_KEY_z || a_ev->keyval == GDK_KEY_Z )
        {
            undo_type();
            return true;
        }
        /* Ctrl-R: Redo */
        if ( a_ev->keyval == GDK_KEY_r || a_ev->keyval == GDK_KEY_R )
        {
            redo_type();
            return true;
        }
    }

    if ( (a_ev->type == GDK_KEY_PRESS) && !(a_ev->state & GDK_MOD1_MASK) && !( a_ev->state & GDK_CONTROL_MASK ))
    {
        if ( global_print_keys )
        {
            printf( "key_press[%d]\n", a_ev->keyval );
            fflush( stdout );
        }

        if ( a_ev->keyval == m_mainperf->m_key_bpm_dn )
        {
            if(!m_tempo->get_hold_undo())
                m_tempo->set_hold_undo(true);
            
            m_tempo->set_start_BPM(m_mainperf->get_bpm() - 1);
            m_adjust_bpm->set_value(  m_mainperf->get_bpm() );
            return true;
        }

        if ( a_ev->keyval ==  m_mainperf->m_key_bpm_up )
        {
            if(!m_tempo->get_hold_undo())
                m_tempo->set_hold_undo(true);
            
            m_tempo->set_start_BPM(m_mainperf->get_bpm() + 1);
            m_adjust_bpm->set_value(  m_mainperf->get_bpm() );
            return true;
        }

        if (a_ev->keyval  == m_mainperf->m_key_tap_bpm )
        {
            tap();
        }

        if ( a_ev->keyval ==  m_mainperf->m_key_seqlist )
        {
            open_seqlist();
            return true;
        }

        if ( a_ev->keyval ==  m_mainperf->m_key_loop )
        {
            toggle_looped();
            return true;
        }

        if ( a_ev->keyval ==  m_mainperf->m_key_song )
        {
            toggle_song_mode();
            return true;
        }

        if ( a_ev->keyval ==  m_mainperf->m_key_follow_trans )
        {
            toggle_follow_transport();
            return true;
        }

        if ( a_ev->keyval ==  m_mainperf->m_key_forward )
        {
            fast_forward(true);
            return true;
        }

        if ( a_ev->keyval ==  m_mainperf->m_key_rewind )
        {
            rewind(true);
            return true;
        }

#ifdef JACK_SUPPORT
        if ( a_ev->keyval ==  m_mainperf->m_key_jack )
        {
            toggle_jack();
            return true;
        }
#endif // JACK_SUPPORT
        // the start/end key may be the same key (i.e. SPACE)
        // allow toggling when the same key is mapped to both triggers (i.e. SPACEBAR)
        bool dont_toggle = m_mainperf->m_key_start != m_mainperf->m_key_stop;
        if ( a_ev->keyval == m_mainperf->m_key_start && (dont_toggle || !global_is_running) )
        {
            start_playing();
            if(a_ev->keyval == GDK_KEY_space)
                return true;
        }
        else if ( a_ev->keyval == m_mainperf->m_key_stop && (dont_toggle || global_is_running) )
        {
            stop_playing();
            if(a_ev->keyval == GDK_KEY_space)
                return true;
        }
        
        if(m_mainperf->get_playlist_mode())
        {
            if ( a_ev->keyval == m_mainperf->m_key_playlist_prev )
            {
            	playlist_jump(PLAYLIST_PREVIOUS);
                return true;
            }
            if ( a_ev->keyval == m_mainperf->m_key_playlist_next )
            {
            	playlist_jump(PLAYLIST_NEXT);
                return true;
            }
        }
        
        if (a_ev->keyval == GDK_KEY_F12)
        {
            m_mainperf->print();
            fflush( stdout );
            return true;
        }
    }

    return Gtk::Window::on_key_press_event(a_ev);
}

bool
mainwnd::on_key_release_event(GdkEventKey* a_ev)
{
    if ( a_ev->type == GDK_KEY_RELEASE )
    {
        if ( a_ev->keyval ==  m_mainperf->m_key_forward )
        {
            fast_forward(false);
            return true;
        }
        if ( a_ev->keyval ==  m_mainperf->m_key_rewind )
        {
            rewind(false);
            return true;
        }
        
        if ( a_ev->keyval == m_mainperf->m_key_bpm_dn )
        {
            m_tempo->push_undo(true);
            return true;
        }
        if ( a_ev->keyval ==  m_mainperf->m_key_bpm_up )
        {
            m_tempo->push_undo(true);
            return true;
        }
    }
    return false;
}
void
mainwnd::update_window_title()
{
    std::string title;

    if(m_mainperf->get_playlist_mode())
    {
    	char num[20];
    	sprintf(num,"%02d",m_mainperf->get_playlist_index() +1);
    	title =
            ( PACKAGE )
            + string(" - Playlist, Song ")
            + num
            + string(" - [")
            + Glib::filename_to_utf8(global_filename)
            + string( "]" );
    }
    else
    {
        if (global_filename == "")
            title = ( PACKAGE ) + string( " - [unnamed]" );
        else
            title =
                ( PACKAGE )
                + string( " - [" )
                + Glib::filename_to_utf8(global_filename)
                + string( "]" );
    }
    
    set_title ( title.c_str());
}

/* This changes the main window logo .xpm when in playlist mode and back */
void
mainwnd::update_window_xpm()
{
    hbox1->remove(*m_image_seq42);
    
    if(m_mainperf->get_playlist_mode())
    {
        m_image_seq42 = manage(new Image( Gdk::Pixbuf::create_from_xpm_data(seq42_xpm_playlist)));
    }
    else
    {
        m_image_seq42 = manage(new Image( Gdk::Pixbuf::create_from_xpm_data(seq42_xpm)));
    }

    if(m_image_seq42 != NULL)
    {
        hbox1->pack_start(*m_image_seq42, false, false);
        hbox1->reorder_child(*m_image_seq42, 0);
        m_image_seq42->show();
    }
}

/* update tempo class for file loading of tempo map */
void
mainwnd::load_tempo_list()
{
    m_tempo->load_tempo_list();
}

void
mainwnd::update_start_BPM(double bpm)
{
    m_tempo->set_start_BPM(bpm);
}

void
mainwnd::set_swing_amount8(int swing_amount8)
{
    m_mainperf->set_swing_amount8(swing_amount8);
    m_adjust_swing_amount8->set_value((double) swing_amount8);
}

void
mainwnd::set_swing_amount16(int swing_amount16)
{
    m_mainperf->set_swing_amount16(swing_amount16);
    m_adjust_swing_amount16->set_value((double) swing_amount16);
}

double
mainwnd::tempo_map_microseconds(unsigned long a_tick)
{
    /* live mode - we ignore tempo changes so use the first tempo only */
    if(!global_song_start_mode)
    {
        tempo_mark first_tempo = (* m_mainperf->m_list_no_stop_markers.begin());
        return ticks_to_delta_time_us (a_tick, first_tempo.bpm, c_ppqn);
    }
    
    /* song mode - cycle through tempo map */
    double hold_microseconds = 0;
    
    list<tempo_mark>::iterator i;
    tempo_mark last_tempo = (*--m_mainperf->m_list_no_stop_markers.end());
    
    for ( i = ++m_mainperf->m_list_no_stop_markers.begin(); i != m_mainperf->m_list_no_stop_markers.end(); ++i )
    {
        if( a_tick >= (*i).tick )
        {
            hold_microseconds = (*i).microseconds_start;
        }
        else
        {
            last_tempo = (*--i);
            break;
        }
    }
    
    uint64_t end_tick = a_tick - last_tempo.tick;
   
    return hold_microseconds + ticks_to_delta_time_us (end_tick, last_tempo.bpm, c_ppqn);
}

std::string
mainwnd::tick_to_timestring (long a_tick)
{
    unsigned long microseconds = tempo_map_microseconds(a_tick);
    int seconds = int(microseconds / 1000000UL);
    int minutes = seconds / 60;
    int hours = seconds / (60 * 60);
    minutes -= hours * 60;
    seconds -= (hours * 60 * 60) + (minutes * 60);
    microseconds -= (hours * 60 * 60 + minutes * 60 + seconds) * 1000000UL;

    char tmp[32];
    snprintf(tmp, sizeof tmp, "%03d:%d:%02d   ", hours, minutes, seconds);
    return std::string(tmp);
}

void
mainwnd::tick_to_midi_measures ( long a_tick, int &measures, int &beats, int &divisions )
{
    static const double s_epsilon = 0.000001;   /* HMMMMMMMMMMMMMMMMMMMMMMM */
    int W = m_mainperf->get_bw();
    int P = c_ppqn;
    int B = m_mainperf->get_bp_measure();
    bool result = (W > 0) && (P > 0) && (B > 0);
    if (result)
    {
        double m = a_tick * W / (4.0 * P * B);       /* measures, whole.frac     */
        double m_whole = floor(m);              /* holds integral measures  */
        m -= m_whole;                           /* get fractional measure   */
        double b = m * B;                       /* beats, whole.frac        */
        double b_whole = floor(b);              /* get integral beats       */
        b -= b_whole;                           /* get fractional beat      */
        double pulses_per_beat = 4 * P / W;     /* pulses/qn * qn/beat      */
        measures = (int(m_whole + s_epsilon) + 1);
        beats = (int(b_whole + s_epsilon) + 1);
        divisions = (int(b * pulses_per_beat + s_epsilon));
    }
}

std::string
mainwnd::tick_to_measurestring (long a_tick )
{
    int measures = 0;
    int beats = 0;
    int divisions = 0;

    char tmp[32];

    tick_to_midi_measures( a_tick, measures, beats, divisions );
    snprintf
    (
        tmp, sizeof tmp, "%03d:%d:%03d",
        measures, beats, divisions
    );
    return std::string(tmp);
}

void
mainwnd::toggle_time_format ()
{
    m_tick_time_as_bbt = ! m_tick_time_as_bbt;
    std::string label = m_tick_time_as_bbt ? "BBT" : "HMS" ;
    Gtk::Label * lbl(dynamic_cast<Gtk::Label *>(m_button_time_type->get_child()));
    if (lbl != NULL)
    {
        lbl->set_text(label);
        m_toggle_time_type = true;
    }
}

int
FF_RW_timeout(void *arg)
{
    perform *p = (perform *) arg;

    if(FF_RW_button_type != FF_RW_RELEASE)
    {
        p->FF_rewind();
        if(p->m_excell_FF_RW < 60.0f)
            p->m_excell_FF_RW *= 1.1f;
        return (TRUE);
    }

    p->m_excell_FF_RW = 1.0;
    return (FALSE);
}

#ifdef NSM_SUPPORT
void
mainwnd::close_all_windows()
{
    m_closing_windows = true;

    for(unsigned i = 0; i < m_vector_windows.size(); ++i)
    {
        m_vector_windows[i]->close();
    }

    m_vector_windows.clear();
    
    if ( m_options != NULL )
        m_options->hide();

    m_tempo->hide_tempo_popup();

    m_closing_windows = false;
}

void
mainwnd::set_window_pointer(Gtk::Window * a_win)
{
    m_vector_windows.push_back(a_win);
}

void
mainwnd::remove_window_pointer(Gtk::Window * a_win)
{
    if (m_closing_windows)
        return;
    
    for(unsigned i = 0; i < m_vector_windows.size(); ++i)
    {
        if (m_vector_windows[i] == a_win)
        {
            m_vector_windows.erase(m_vector_windows.begin() + i);
        }
    }
}

void
mainwnd::set_nsm_client(nsm_client_t *nsm, bool optional_gui)
{
    m_nsm = nsm;
    m_nsm_optional_gui = optional_gui;
    if(m_menu_file != nullptr)
    {
        m_file_menu_items[1].set_sensitive(false);  // New
        m_file_menu_items[2].set_sensitive(false);  // Open
        m_file_menu_items[3].set_sensitive(false);  // Open playlist
        m_file_menu_items[5].set_sensitive(false);  // Save As

        m_menu_recent->set_sensitive(false);        // Recent files submenu
        m_file_menu_items[7].set_label("Hide");     // Exit
    }
}
#endif  // NSM_SUPPORT