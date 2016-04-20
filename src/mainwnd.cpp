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
#include <gtk/gtkversion.h>

#include "mainwnd.h"
#include "perform.h"
#include "midifile.h"
#include "sequence.h"
#include "font.h"
#include "seqlist.h"

#include "pixmaps/seq42_32.xpm"
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
#include "pixmaps/jack.xpm"
#include "pixmaps/transportFollow.xpm"
#include "pixmaps/transpose.xpm"

using namespace sigc;

bool global_is_running = false;
bool global_is_modified = false;

// tooltip helper, for old vs new gtk...
#if GTK_MINOR_VERSION >= 12
#   define add_tooltip( obj, text ) obj->set_tooltip_text( text);
#else
#   define add_tooltip( obj, text ) m_tooltips->set_tip( *obj, text );
#endif

mainwnd::mainwnd(perform *a_p):
    m_mainperf(a_p),
    m_options(NULL),
    m_snap(c_ppqn / 4)
{
    using namespace Menu_Helpers;

    set_icon(Gdk::Pixbuf::create_from_xpm_data(seq42_32_xpm));

    /* main window */
    update_window_title();
    set_size_request(850, 322);

#if GTK_MINOR_VERSION < 12
    m_tooltips = manage( new Tooltips() );
#endif
    m_main_time = manage( new maintime( ));

    m_menubar = manage(new MenuBar());

    m_menu_file = manage(new Menu());
    m_menubar->items().push_front(MenuElem("_File", *m_menu_file));

    m_menu_edit = manage(new Menu());
    m_menubar->items().push_back(MenuElem("_Edit", *m_menu_edit));

    m_menu_help = manage( new Menu());
    m_menubar->items().push_back(MenuElem("_Help", *m_menu_help));

    /* file menu items */
    m_menu_file->items().push_back(MenuElem("_New",
                                            Gtk::AccelKey("<control>N"),
                                            mem_fun(*this, &mainwnd::file_new)));
    m_menu_file->items().push_back(MenuElem("_Open...",
                                            Gtk::AccelKey("<control>O"),
                                            mem_fun(*this, &mainwnd::file_open)));
    m_menu_file->items().push_back(MenuElem("_Save",
                                            Gtk::AccelKey("<control>S"),
                                            mem_fun(*this, &mainwnd::file_save)));
    m_menu_file->items().push_back(MenuElem("Save _as...",
                                            sigc::bind(mem_fun(*this, &mainwnd::file_save_as), 0)));

    m_menu_file->items().push_back(SeparatorElem());

    m_menu_file->items().push_back(MenuElem("O_ptions...",
                                            mem_fun(*this,&mainwnd::options_dialog)));
    m_menu_file->items().push_back(SeparatorElem());
    m_menu_file->items().push_back(MenuElem("E_xit",
                                            Gtk::AccelKey("<control>Q"),
                                            mem_fun(*this, &mainwnd::file_exit)));

    /* edit menu items */
    m_menu_edit->items().push_back(MenuElem("Sequence _list",
                                            mem_fun(*this, &mainwnd::open_seqlist)));

    m_menu_edit->items().push_back(MenuElem("_Apply song transpose",
                                            mem_fun(*this, &mainwnd::apply_song_transpose)));

    m_menu_edit->items().push_back(MenuElem("Increase _grid size",
                                            mem_fun(*this, &mainwnd::grow)));

    m_menu_edit->items().push_back(SeparatorElem());
    m_menu_edit->items().push_back(MenuElem("_Mute all tracks",
                                            sigc::bind(mem_fun(*this, &mainwnd::set_song_mute), MUTE_ON)));

    m_menu_edit->items().push_back(MenuElem("_Unmute all tracks",
                                            sigc::bind(mem_fun(*this, &mainwnd::set_song_mute), MUTE_OFF)));

    m_menu_edit->items().push_back(MenuElem("_Toggle mute for all tracks",
                                            sigc::bind(mem_fun(*this, &mainwnd::set_song_mute), MUTE_TOGGLE)));

    m_menu_edit->items().push_back(SeparatorElem());
    m_menu_edit->items().push_back(MenuElem("_Import midi...",
                                            mem_fun(*this, &mainwnd::file_import_dialog)));

    m_menu_edit->items().push_back(MenuElem("Midi e_xport (Seq 24)",
                                            sigc::bind(mem_fun(*this, &mainwnd::file_save_as), 1)));

    m_menu_edit->items().push_back(MenuElem("Midi export _song",
                                            sigc::bind(mem_fun(*this, &mainwnd::file_save_as), 2)));

    /* help menu items */
    m_menu_help->items().push_back(MenuElem("_About...",
                                            mem_fun(*this, &mainwnd::about_dialog)));

    /* top line items */
    HBox *hbox1 = manage( new HBox( false, 2 ) );
    hbox1->set_border_width( 2 );

    m_button_loop = manage( new ToggleButton() );
    m_button_loop->add(*manage( new Image(Gdk::Pixbuf::create_from_xpm_data( loop_xpm ))));
    m_button_loop->signal_toggled().connect(  mem_fun( *this, &mainwnd::set_looped ));
    add_tooltip( m_button_loop, "Play looped between L and R." );

    m_button_stop = manage( new Button() );
    m_button_stop->add(*manage( new Image(Gdk::Pixbuf::create_from_xpm_data( stop_xpm ))));
    m_button_stop->signal_clicked().connect( mem_fun( *this, &mainwnd::stop_playing));
    add_tooltip( m_button_stop, "Stop playing." );

    m_button_play = manage( new Button() );
    m_button_play->add(*manage( new Image(Gdk::Pixbuf::create_from_xpm_data( play2_xpm ))));
    m_button_play->signal_clicked().connect(  mem_fun( *this, &mainwnd::start_playing));
    add_tooltip( m_button_play, "Begin playing at L marker." );

    m_button_mode = manage( new ToggleButton( "song mode" ) );
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
    add_tooltip( m_button_seq, "Open sequence list" );

    m_button_follow = manage( new ToggleButton() );
    m_button_follow->add(*manage( new Image(Gdk::Pixbuf::create_from_xpm_data( transportFollow_xpm ))));
    m_button_follow->signal_clicked().connect(  mem_fun( *this, &mainwnd::set_follow_transport ));
    add_tooltip( m_button_follow, "Follow transport" );
    m_button_follow->set_active(true);

    hbox1->pack_start( *m_button_stop, false, false );
    hbox1->pack_start( *m_button_play, false, false );
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

    /* perfedit widgets */
    m_vadjust = manage( new Adjustment(0,0,1,1,1,1 ));
    m_hadjust = manage( new Adjustment(0,0,1,1,1,1 ));

    m_vscroll   =  manage(new VScrollbar( *m_vadjust ));
    m_hscroll   =  manage(new HScrollbar( *m_hadjust ));

    m_perfnames = manage( new perfnames( m_mainperf, m_vadjust ));

    m_perfroll = manage( new perfroll
                         (
                             m_mainperf,
                             this,
                             m_hadjust,
                             m_vadjust
                         ));
    m_perftime = manage( new perftime( m_mainperf, this, m_hadjust ));

    /* init table, viewports and scroll bars */
    m_table     = manage( new Table( 6, 3, false));
    m_table->set_border_width( 2 );

    m_hbox      = manage( new HBox( false, 2 ));
    m_hlbox     = manage( new HBox( false, 2 ));

    m_hlbox->set_border_width( 2 );

    /* fill table */
    m_table->attach( *m_hlbox,  0, 3, 0, 1,  Gtk::FILL, Gtk::SHRINK, 2, 0 ); // shrink was 0

    m_table->attach( *m_perfnames,    0, 1, 2, 3, Gtk::SHRINK, Gtk::FILL );

    m_table->attach( *m_perftime, 1, 2, 1, 2, Gtk::FILL, Gtk::SHRINK );
    m_table->attach( *m_perfroll, 1, 2, 2, 3,
                     Gtk::FILL | Gtk::SHRINK,
                     Gtk::FILL | Gtk::SHRINK );

    m_table->attach( *m_vscroll, 2, 3, 2, 3, Gtk::SHRINK, Gtk::FILL | Gtk::EXPAND  );

    m_table->attach( *m_hbox,  0, 1, 3, 4,  Gtk::FILL, Gtk::SHRINK, 0, 2 );
    m_table->attach( *m_hscroll, 1, 2, 3, 4, Gtk::FILL | Gtk::EXPAND, Gtk::SHRINK  );

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
        m_menu_xpose->items().push_front( MenuElem( num,
                                          sigc::bind(mem_fun(*this,&mainwnd::xpose_button_callback),
                                                  i )));
    }

    m_button_xpose = manage( new Button());
    m_button_xpose->add( *manage( new Image(Gdk::Pixbuf::create_from_xpm_data( transpose_xpm ))));
    m_button_xpose->signal_clicked().connect(  sigc::bind<Menu *>( mem_fun( *this, &mainwnd::popup_menu), m_menu_xpose  ));
    add_tooltip( m_button_xpose, "Song transpose" );
    m_entry_xpose = manage( new Entry());
    m_entry_xpose->set_size_request( 40, -1 );
    m_entry_xpose->set_editable( false );

    m_menu_snap =   manage( new Menu());
    m_menu_snap->items().push_back(MenuElem("1/1",   sigc::bind(mem_fun(*this,&mainwnd::set_snap), 1  )));
    m_menu_snap->items().push_back(MenuElem("1/2",   sigc::bind(mem_fun(*this,&mainwnd::set_snap), 2  )));
    m_menu_snap->items().push_back(MenuElem("1/4",   sigc::bind(mem_fun(*this,&mainwnd::set_snap), 4  )));
    m_menu_snap->items().push_back(MenuElem("1/8",   sigc::bind(mem_fun(*this,&mainwnd::set_snap), 8  )));
    m_menu_snap->items().push_back(MenuElem("1/16",   sigc::bind(mem_fun(*this,&mainwnd::set_snap), 16  )));
    m_menu_snap->items().push_back(MenuElem("1/32",   sigc::bind(mem_fun(*this,&mainwnd::set_snap), 32  )));
    m_menu_snap->items().push_back(SeparatorElem());
    m_menu_snap->items().push_back(MenuElem("1/3",   sigc::bind(mem_fun(*this,&mainwnd::set_snap), 3  )));
    m_menu_snap->items().push_back(MenuElem("1/6",   sigc::bind(mem_fun(*this,&mainwnd::set_snap), 6  )));
    m_menu_snap->items().push_back(MenuElem("1/12",   sigc::bind(mem_fun(*this,&mainwnd::set_snap), 12  )));
    m_menu_snap->items().push_back(MenuElem("1/24",   sigc::bind(mem_fun(*this,&mainwnd::set_snap), 24  )));
    m_menu_snap->items().push_back(SeparatorElem());
    m_menu_snap->items().push_back(MenuElem("1/5",   sigc::bind(mem_fun(*this,&mainwnd::set_snap), 5  )));
    m_menu_snap->items().push_back(MenuElem("1/10",   sigc::bind(mem_fun(*this,&mainwnd::set_snap), 10  )));
    m_menu_snap->items().push_back(MenuElem("1/20",   sigc::bind(mem_fun(*this,&mainwnd::set_snap), 20  )));
    m_menu_snap->items().push_back(MenuElem("1/40",   sigc::bind(mem_fun(*this,&mainwnd::set_snap), 40  )));
    m_menu_snap->items().push_back(SeparatorElem());
    m_menu_snap->items().push_back(MenuElem("1/7",   sigc::bind(mem_fun(*this,&mainwnd::set_snap), 7  )));
    m_menu_snap->items().push_back(MenuElem("1/9",   sigc::bind(mem_fun(*this,&mainwnd::set_snap), 9  )));
    m_menu_snap->items().push_back(MenuElem("1/11",   sigc::bind(mem_fun(*this,&mainwnd::set_snap), 11  )));
    m_menu_snap->items().push_back(MenuElem("1/13",   sigc::bind(mem_fun(*this,&mainwnd::set_snap), 13  )));
    m_menu_snap->items().push_back(MenuElem("1/14",   sigc::bind(mem_fun(*this,&mainwnd::set_snap), 14  )));
    m_menu_snap->items().push_back(MenuElem("1/15",   sigc::bind(mem_fun(*this,&mainwnd::set_snap), 15  )));

    /* snap */
    m_button_snap = manage( new Button());
    m_button_snap->add( *manage( new Image(Gdk::Pixbuf::create_from_xpm_data( snap_xpm ))));
    m_button_snap->signal_clicked().connect(  sigc::bind<Menu *>( mem_fun( *this, &mainwnd::popup_menu), m_menu_snap  ));
    add_tooltip( m_button_snap, "Grid snap. (Fraction of Measure Length)" );
    m_entry_snap = manage( new Entry());
    m_entry_snap->set_size_request( 40, -1 );
    m_entry_snap->set_editable( false );

    m_menu_bp_measure = manage( new Menu() );
    m_menu_bw = manage( new Menu() );

    /* bw */
    m_menu_bw->items().push_back(MenuElem("1", sigc::bind(mem_fun(*this,&mainwnd::bw_button_callback), 1  )));
    m_menu_bw->items().push_back(MenuElem("2", sigc::bind(mem_fun(*this,&mainwnd::bw_button_callback), 2  )));
    m_menu_bw->items().push_back(MenuElem("4", sigc::bind(mem_fun(*this,&mainwnd::bw_button_callback), 4  )));
    m_menu_bw->items().push_back(MenuElem("8", sigc::bind(mem_fun(*this,&mainwnd::bw_button_callback), 8  )));
    m_menu_bw->items().push_back(MenuElem("16", sigc::bind(mem_fun(*this,&mainwnd::bw_button_callback), 16 )));

    char b[20];

    for( int i=0; i<16; i++ )
    {
        snprintf( b, sizeof(b), "%d", i+1 );

        /* length */
        m_menu_bp_measure->items().push_back(MenuElem(b,
                                             sigc::bind(mem_fun(*this,&mainwnd::bp_measure_button_callback),i+1 )));
    }

    /* bpm spin button */
    m_adjust_bpm = manage(new Adjustment(m_mainperf->get_bpm(), 20, 500, 1));
    m_spinbutton_bpm = manage( new SpinButton( *m_adjust_bpm ));
    m_spinbutton_bpm->set_editable( false );
    m_adjust_bpm->signal_value_changed().connect(
        mem_fun(*this, &mainwnd::adj_callback_bpm ));
    add_tooltip( m_spinbutton_bpm, "Adjust beats per minute (BPM) value" );

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
    m_adjust_swing_amount8 = manage(new Adjustment(m_mainperf->get_swing_amount8(), 0, c_max_swing_amount, 1));
    m_spinbutton_swing_amount8 = manage( new SpinButton( *m_adjust_swing_amount8 ));
    m_spinbutton_swing_amount8->set_editable( false );
    m_adjust_swing_amount8->signal_value_changed().connect(
        mem_fun(*this, &mainwnd::adj_callback_swing_amount8 ));
    add_tooltip( m_spinbutton_swing_amount8, "Adjust 1/8 swing amount" );

    m_adjust_swing_amount16 = manage(new Adjustment(m_mainperf->get_swing_amount16(), 0, c_max_swing_amount, 1));
    m_spinbutton_swing_amount16 = manage( new SpinButton( *m_adjust_swing_amount16 ));
    m_spinbutton_swing_amount16->set_editable( false );
    m_adjust_swing_amount16->signal_value_changed().connect(
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

    m_hlbox->pack_start(*m_spinbutton_bpm, false, false );
    m_hlbox->pack_start(*(manage( new Label( "bpm" ))), false, false, 4);

    m_hlbox->pack_start( *m_button_bp_measure, false, false );
    m_hlbox->pack_start( *m_entry_bp_measure, false, false );

    m_hlbox->pack_start( *(manage(new Label( "/" ))), false, false, 4);

    m_hlbox->pack_start( *m_button_bw, false, false );
    m_hlbox->pack_start( *m_entry_bw, false, false );

    m_hlbox->pack_start( *(manage(new Label( "x" ))), false, false, 4);

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

    set_bw( 4 ); // set this first
    set_snap( 4 );
    set_bp_measure( 4 );
    set_xpose( 0 );

    m_perfroll->init_before_show();

    /* show everything */
    show_all();
    p_font_renderer->init( this->get_window() );

    add_events( Gdk::KEY_PRESS_MASK | Gdk::KEY_RELEASE_MASK );

    m_timeout_connect = Glib::signal_timeout().connect(
                            mem_fun(*this, &mainwnd::timer_callback), 25);

    m_sigpipe[0] = -1;
    m_sigpipe[1] = -1;
    install_signal_handlers();
}

mainwnd::~mainwnd()
{
    delete m_options;

    if (m_sigpipe[0] != -1)
        close(m_sigpipe[0]);

    if (m_sigpipe[1] != -1)
        close(m_sigpipe[1]);
}

// This is the GTK timer callback, used to draw our current time and bpm
// ondd_events( the main window
bool
mainwnd::timer_callback(  )
{
    m_perfroll->redraw_dirty_tracks();
    m_perfroll->draw_progress();
    m_perfnames->redraw_dirty_tracks();

    long ticks = m_mainperf->get_tick();

    m_main_time->idle_progress( ticks );

    if ( m_adjust_bpm->get_value() != m_mainperf->get_bpm())
    {
        m_adjust_bpm->set_value( m_mainperf->get_bpm());
    }

    if ( m_bp_measure != m_mainperf->get_bp_measure())
    {
        set_bp_measure( m_mainperf->get_bp_measure());
    }

    if ( m_bw != m_mainperf->get_bw())
    {
        set_bw( m_mainperf->get_bw());
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

    if ( m_adjust_swing_amount8->get_value() != m_mainperf->get_swing_amount8())
    {
        m_adjust_swing_amount8->set_value( m_mainperf->get_swing_amount8());
    }

    if ( m_adjust_swing_amount16->get_value() != m_mainperf->get_swing_amount16())
    {
        m_adjust_swing_amount16->set_value( m_mainperf->get_swing_amount16());
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

    return true;
}

void
mainwnd::undo_type()
{
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
    default:
        break;
    }
    m_mainperf->set_have_undo();
}

void
mainwnd::undo_trigger(int a_track)
{
    m_mainperf->pop_trigger_undo(a_track);
    m_perfroll->queue_draw();
}

void
mainwnd::undo_trigger() // collapse and expand
{
    m_mainperf->pop_trigger_undo();
    m_perfroll->queue_draw();
}

void
mainwnd::undo_track( int a_track )
{
    m_mainperf->pop_track_undo(a_track);
    m_perfroll->queue_draw();
}

void
mainwnd::undo_perf()
{
    m_mainperf->pop_perf_undo();
    m_perfroll->queue_draw();
}

void
mainwnd::redo_type()
{
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
    default:
        break;
    }
    m_mainperf->set_have_redo();
}

void
mainwnd::redo_trigger(int a_track) // single track
{
    m_mainperf->pop_trigger_redo(a_track);
    m_perfroll->queue_draw();
}

void
mainwnd::redo_trigger() // collapse and expand
{
    m_mainperf->pop_trigger_redo();
    m_perfroll->queue_draw();
}

void
mainwnd::redo_track( int a_track )
{
    m_mainperf->pop_track_redo(a_track);
    m_perfroll->queue_draw();
}

void
mainwnd::redo_perf()
{
    m_mainperf->pop_perf_redo();
    m_perfroll->queue_draw();
}

void
mainwnd::start_playing()
{
    m_mainperf->start_playing();
}

void
mainwnd::stop_playing()
{
    m_mainperf->stop_playing();
}

void
mainwnd::collapse() // all tracks
{
    m_mainperf->push_trigger_undo();
    m_mainperf->move_triggers( false );
    m_perfroll->queue_draw();
}

void
mainwnd::copy() // all tracks
{
    m_mainperf->push_trigger_undo();
    m_mainperf->copy_triggers(  );
    m_perfroll->queue_draw();
}

void
mainwnd::expand() // all tracks
{
    m_mainperf->push_trigger_undo();
    m_mainperf->move_triggers( true );
    m_perfroll->queue_draw();
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
    m_mainperf->set_left_frame();
}

void
mainwnd::toggle_song_mode()
{
    // Note that this will trigger the button signal callback.
    if(!global_is_running)
    {
        m_button_mode->set_active( ! m_button_mode->get_active() );
        m_mainperf->set_left_frame();
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
    a_menu->popup(0,0);
}

void
mainwnd::set_guides()
{
    long measure_ticks = (c_ppqn * 4) * m_bp_measure / m_bw;
    long snap_ticks =  measure_ticks / m_snap;
    long beat_ticks = (c_ppqn * 4) / m_bw;
    m_perfroll->set_guides( snap_ticks, measure_ticks, beat_ticks );
    m_perftime->set_guides( snap_ticks, measure_ticks );
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
    m_mainperf->set_bw(a_beat_width);
    char b[10];
    snprintf(b, sizeof(b), "%d", a_beat_width );
    m_entry_bw->set_text(b);

    m_bw = a_beat_width;
    set_guides();
}

void
mainwnd::set_zoom (int z)
{
    m_perfroll->set_zoom(z);
    m_perftime->set_zoom(z);
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
mainwnd::grow()
{
    m_perfroll->increment_size();
    m_perftime->increment_size();
}

void
mainwnd::apply_song_transpose()
{
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
        m_mainperf->set_seqlist_raise(true);
    }
    else
    {
        new seqlist(m_mainperf);
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
    m_mainperf->clear_all();
    set_bp_measure(4);
    set_bw(4);
    set_xpose(0);
    m_mainperf->set_bpm(120);
    m_mainperf->undo_vect.clear();
    m_mainperf->redo_vect.clear();
    m_mainperf->set_have_undo();
    m_mainperf->set_have_redo();

    global_filename = "";
    update_window_title();
    global_is_modified = false;
}

/* callback function */
void mainwnd::file_save()
{
    save_file();
}

/* callback function */
void mainwnd::file_save_as(int type)
{
    // default type 0 = .s42
    //         type 1 = .mid .midi etc for Seq24 export
    //         type 2 = .mid .midi etc for midi song export

    Gtk::FileChooserDialog dialog("Save file as",
                                  Gtk::FILE_CHOOSER_ACTION_SAVE);
    dialog.set_transient_for(*this);

    dialog.add_button(Gtk::Stock::CANCEL, Gtk::RESPONSE_CANCEL);
    dialog.add_button(Gtk::Stock::SAVE, Gtk::RESPONSE_OK);

    Gtk::FileFilter filter_midi;

    if(type == 0)
    {
        filter_midi.set_name("Seq42 files");
        filter_midi.add_pattern("*.s42");
    }

    if(type == 1 || type == 2)
    {
        filter_midi.set_name("MIDI files");
        filter_midi.add_pattern("*.midi");
        filter_midi.add_pattern("*.mid");
    }

    dialog.add_filter(filter_midi);

    Gtk::FileFilter filter_any;
    filter_any.set_name("Any files");
    filter_any.add_pattern("*");
    dialog.add_filter(filter_any);

    if(type == 0) // .s42
        dialog.set_current_folder(last_used_dir);

    if(type == 1 || type == 2) // .midi
        dialog.set_current_folder(last_midi_dir);

    int result = dialog.run();

    switch (result)
    {
    case Gtk::RESPONSE_OK:
    {
        std::string fname = dialog.get_filename();
        Gtk::FileFilter* current_filter = dialog.get_filter();

        if ((current_filter != NULL) &&
                (current_filter->get_name() == "Seq42 files"))
        {
            // check for Seq42 file extension; if missing, add .s42
            std::string suffix = fname.substr(
                                     fname.find_last_of(".") + 1, std::string::npos);
            toLower(suffix);
            if (suffix != "s42") fname = fname + ".s42";
        }

        if ((current_filter != NULL) &&
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

        if(type == 0)
        {
            global_filename = fname;
            update_window_title();
            save_file();
        }
        else
        {
            export_midi(fname, type);
        }

        break;
    }

    default:
        break;
    }
}

void mainwnd::export_midi(const Glib::ustring& fn, int type)
{
    bool result = false;

    midifile f(fn);

    if(type == 1)
        result = f.write_sequences(m_mainperf);
    else
        result = f.write_song(m_mainperf);

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

void mainwnd::open_file(const Glib::ustring& fn)
{
    bool result;

    m_mainperf->clear_all();

    set_xpose(0);
    m_mainperf->undo_vect.clear();
    m_mainperf->redo_vect.clear();
    m_mainperf->set_have_undo();
    m_mainperf->set_have_redo();

    result = m_mainperf->load(fn);

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
        return;
    }

    last_used_dir = fn.substr(0, fn.rfind("/") + 1);
    global_filename = fn;
    update_window_title();

    m_adjust_bpm->set_value( m_mainperf->get_bpm());

    m_adjust_swing_amount8->set_value( m_mainperf->get_swing_amount8());
    m_adjust_swing_amount16->set_value( m_mainperf->get_swing_amount16());
}

/*callback function*/
void mainwnd::file_open()
{
    if (is_save())
        choose_file();
}

void mainwnd::choose_file()
{
    Gtk::FileChooserDialog dialog("Open Seq42 file",
                                  Gtk::FILE_CHOOSER_ACTION_OPEN);
    dialog.set_transient_for(*this);

    dialog.add_button(Gtk::Stock::CANCEL, Gtk::RESPONSE_CANCEL);
    dialog.add_button(Gtk::Stock::OPEN, Gtk::RESPONSE_OK);

    Gtk::FileFilter filter_midi;
    filter_midi.set_name("Seq42 files");
    filter_midi.add_pattern("*.s42");
    dialog.add_filter(filter_midi);

    Gtk::FileFilter filter_any;
    filter_any.set_name("Any files");
    filter_any.add_pattern("*");
    dialog.add_filter(filter_any);

    dialog.set_current_folder(last_used_dir);

    int result = dialog.run();

    switch(result)
    {
    case(Gtk::RESPONSE_OK):
        open_file(dialog.get_filename());

    default:
        break;
    }
}

bool mainwnd::save_file()
{
    bool result = false;

    if (global_filename == "")
    {
        file_save_as(0);
        return true;
    }

    result = m_mainperf->save(global_filename);

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

    Gtk::FileFilter filter_midi;
    filter_midi.set_name("MIDI files");
    filter_midi.add_pattern("*.midi");
    filter_midi.add_pattern("*.mid");
    dialog.add_filter(filter_midi);

    Gtk::FileFilter filter_any;
    filter_any.set_name("Any files");
    filter_any.add_pattern("*");
    dialog.add_filter(filter_any);

    dialog.set_current_folder(last_midi_dir);

    ButtonBox *btnbox = dialog.get_action_area();
    HBox hbox( false, 2 );

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

            if(f.parse( m_mainperf ))
                last_midi_dir = dialog.get_filename().substr(0, dialog.get_filename().rfind("/") + 1);
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
    if (is_save())
    {
        if (global_is_running)
            stop_playing();
        hide();
    }
}

bool
mainwnd::on_delete_event(GdkEventAny *a_e)
{
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
    dialog.set_name(PACKAGE_NAME);
    dialog.set_version(VERSION);
    dialog.set_comments("Interactive MIDI Sequencer\n");

    dialog.set_copyright(
        "(C) 2015 -      Stazed\n"
        "(C) 2010 - 2013 Sam Brauer\n"
        "(C) 2008 - 2009 Seq24team\n"
        "(C) 2002 - 2006 Rob C. Buse");
    dialog.set_website("https://github.com/Stazed/seq42");

    std::list<Glib::ustring> list_authors;
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

    std::list<Glib::ustring> list_documenters;
    list_documenters.push_back("Dana Olson <seq24@ubuntustudio.com>");
    dialog.set_documenters(list_documenters);

    dialog.show_all_children();
    dialog.run();
}

void
mainwnd::adj_callback_bpm( )
{
    if(m_mainperf->get_bpm() != (int) m_adjust_bpm->get_value())
    {
        m_mainperf->set_bpm( (int) m_adjust_bpm->get_value());
        global_is_modified = true;
    }
}

void
mainwnd::adj_callback_swing_amount8( )
{
    m_mainperf->set_swing_amount8( (int) m_adjust_swing_amount8->get_value());
    global_is_modified = true;
}

void
mainwnd::adj_callback_swing_amount16( )
{
    m_mainperf->set_swing_amount16( (int) m_adjust_swing_amount16->get_value());
    global_is_modified = true;
}

bool
mainwnd::on_key_press_event(GdkEventKey* a_ev)
{
    // control and modifier key combinations matching
    if ( a_ev->state & GDK_CONTROL_MASK )
    {
        /* Ctrl-Z: Undo */
        if ( a_ev->keyval == GDK_z || a_ev->keyval == GDK_Z )
        {
            undo_type();
            return true;
        }
        /* Ctrl-R: Redo */
        if ( a_ev->keyval == GDK_r || a_ev->keyval == GDK_R )
        {
            redo_type();
            return true;
        }
    }

    if ( a_ev->type == GDK_KEY_PRESS )
    {
        if ( global_print_keys )
        {
            printf( "key_press[%d]\n", a_ev->keyval );
            fflush( stdout );
        }

        if ( a_ev->keyval == m_mainperf->m_key_bpm_dn )
        {
            m_mainperf->set_bpm( m_mainperf->get_bpm() - 1 );
            m_adjust_bpm->set_value(  m_mainperf->get_bpm() );
            return true;
        }

        if ( a_ev->keyval ==  m_mainperf->m_key_bpm_up )
        {
            m_mainperf->set_bpm( m_mainperf->get_bpm() + 1 );
            m_adjust_bpm->set_value(  m_mainperf->get_bpm() );
            return true;
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
            if(a_ev->keyval == GDK_space)
                return true;
        }
        else if ( a_ev->keyval == m_mainperf->m_key_stop && (dont_toggle || global_is_running) )
        {
            stop_playing();
            if(a_ev->keyval == GDK_space)
                return true;
        }
        else if (a_ev->keyval == GDK_F12)
        {
            m_mainperf->print();
            fflush( stdout );
            return true;
        }
    }

    return Gtk::Window::on_key_press_event(a_ev);
}


void
mainwnd::update_window_title()
{
    std::string title;

    if (global_filename == "")
        title = ( PACKAGE ) + string( " - song - unsaved" );
    else
        title =
            ( PACKAGE )
            + string( " - song - " )
            + Glib::filename_to_utf8(global_filename);

    set_title ( title.c_str());
}

int mainwnd::m_sigpipe[2];

/* Handler for system signals (SIGUSR1, SIGINT...)
 * Write a message to the pipe and leave as soon as possible
 */
void
mainwnd::handle_signal(int sig)
{
    if (write(m_sigpipe[1], &sig, sizeof(sig)) == -1)
    {
        printf("write() failed: %s\n", std::strerror(errno));
    }
}

bool
mainwnd::install_signal_handlers()
{
    /*install pipe to forward received system signals*/
    if (pipe(m_sigpipe) < 0)
    {
        printf("pipe() failed: %s\n", std::strerror(errno));
        return false;
    }

    /*install notifier to handle pipe messages*/
    Glib::signal_io().connect(sigc::mem_fun(*this, &mainwnd::signal_action),
                              m_sigpipe[0], Glib::IO_IN);

    /*install signal handlers*/
    struct sigaction action;
    memset(&action, 0, sizeof(action));
    action.sa_handler = handle_signal;

    if (sigaction(SIGUSR1, &action, NULL) == -1)
    {
        printf("sigaction() failed: %s\n", std::strerror(errno));
        return false;
    }

    if (sigaction(SIGINT, &action, NULL) == -1)
    {
        printf("sigaction() failed: %s\n", std::strerror(errno));
        return false;
    }

    return true;
}

bool
mainwnd::signal_action(Glib::IOCondition condition)
{
    int message;

    if ((condition & Glib::IO_IN) == 0)
    {
        printf("Error: unexpected IO condition\n");
        return false;
    }

    if (read(m_sigpipe[0], &message, sizeof(message)) == -1)
    {
        printf("read() failed: %s\n", std::strerror(errno));
        return false;
    }

    switch (message)
    {
    case SIGUSR1:
        save_file();
        break;
    case SIGINT:
        file_exit();
        break;
    default:
        printf("Unexpected signal received: %d\n", message);
        break;
    }
    return true;
}
