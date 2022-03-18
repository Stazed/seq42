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

#include "seqedit.h"
#include "sequence.h"
#include "midibus.h"
#include "controllers.h"
#include "event.h"
#include "options.h"

#include "pixmaps/play.xpm"
#include "pixmaps/q_rec.xpm"
#include "pixmaps/rec.xpm"
#include "pixmaps/thru.xpm"
#include "pixmaps/midi.xpm"
#include "pixmaps/snap.xpm"
#include "pixmaps/zoom.xpm"
#include "pixmaps/length.xpm"
#include "pixmaps/scale.xpm"
#include "pixmaps/key.xpm"
#include "pixmaps/down.xpm"
#include "pixmaps/note_length.xpm"
#include "pixmaps/undo.xpm"
#include "pixmaps/redo.xpm"
#include "pixmaps/quanize.xpm"
#include "pixmaps/sequences.xpm"
#include "pixmaps/tools.xpm"
#include "pixmaps/seq-editor.xpm"
#include "pixmaps/chord.xpm"
#include "mainwnd.h"


# define add_tooltip( obj, text ) obj->set_tooltip_text( text);

int seqedit::m_initial_zoom = 2;
int seqedit::m_initial_snap = c_ppqn / 4;
int seqedit::m_initial_note_length = c_ppqn / 4;
int seqedit::m_initial_scale = 0;
int seqedit::m_initial_chord = 0;
int seqedit::m_initial_key = 0;
int seqedit::m_initial_bg_seq = -1;
int seqedit::m_initial_bg_trk = -1;

// Actions
const int select_all_notes      = 1;
const int select_all_events     = 2;
const int select_inverse_notes  = 3;
const int select_inverse_events = 4;

const int quantize_notes    = 5;
const int quantize_events   = 6;

const int randomize_events   = 7;

const int tighten_events   =  8;
const int tighten_notes    =  9;

const int transpose     = 10;
const int transpose_h   = 12;

const int expand_pattern   = 13;
const int compress_pattern = 14;

const int select_even_notes = 15;
const int select_odd_notes  = 16;

const int reverse_pattern = 17;

/* connects to a menu item, tells the performance
   to launch the timer thread */
void
seqedit::menu_action_quantise()
{
}

seqedit::seqedit( sequence *a_seq,
                  perform *a_perf, mainwnd *a_main) :

    /* set the performance */
    m_seq(a_seq),
    m_mainperf(a_perf),
    m_mainwnd(a_main),

    m_zoom(m_initial_zoom),
    m_key_y(c_key_y),
    m_keyarea_y(c_keyarea_y),
    m_rollarea_y(c_rollarea_y),
    m_vertical_zoom(c_default_vertical_zoom),
    m_snap(m_initial_snap),
    m_note_length(m_initial_note_length),
    m_scale(m_initial_scale),
    m_chord(m_initial_chord),
    m_key(m_initial_key),
    m_bg_seq(m_initial_bg_seq),
    m_bg_trk(m_initial_bg_trk)
{
    set_icon(Gdk::Pixbuf::create_from_xpm_data(seq_editor_xpm));

    /* main window */
    std::string title = "seq42 - sequence - ";
    title.append(m_seq->get_name());
    set_title(title);
    set_size_request(860, 500);

    m_seq->set_editing( true );

    /* scroll bars */
    m_vadjust = Adjustment::create(55, 0, c_num_keys, 1, 1, 1);
    m_hadjust = Adjustment::create(0, 0, 1, 1, 1, 1);
    m_vscroll_new   =  manage(new VScrollbar( m_vadjust ));
    m_hscroll_new   =  manage(new HScrollbar( m_hadjust ));

    /* get some new objects */
    m_seqkeys_wid  = manage( new seqkeys(  m_seq,
                                           m_vadjust ));

    m_seqtime_wid  = manage( new seqtime(  m_seq,
                                           m_zoom,
                                           m_hadjust ));

    m_seqdata_wid  = manage( new seqdata(  m_seq,
                                           m_zoom,
                                           m_hadjust));

    m_seqevent_wid = manage( new seqevent( m_seq,
                                           m_zoom,
                                           m_snap,
                                           m_seqdata_wid,
                                           m_hadjust));

    m_toggle_play = manage( new ToggleButton() );

    m_seqroll_wid  = manage( new seqroll(  m_mainperf,
                                           m_seq,
                                           m_zoom,
                                           m_snap,
                                           m_seqdata_wid,
                                           m_seqevent_wid,
                                           m_seqkeys_wid,
                                           m_hadjust,
                                           m_vadjust,
                                           m_toggle_play));

    m_lfo_wnd = new lfownd(m_seq, m_seqdata_wid); // not managed child window - must be deleted in on_delete_event

    /* menus */
    m_menubar   =  manage( new MenuBar());
    m_menu_tools = manage( new Menu() );
    m_menu_zoom =  manage( new Menu());
    m_menu_snap =   manage( new Menu());
    m_menu_note_length = manage( new Menu());
    m_menu_length = manage( new Menu());
    m_menu_bp_measure = manage( new Menu() );
    m_menu_bw = manage( new Menu() );
    m_menu_swing_mode = manage( new Menu());
    m_menu_rec_vol = manage( new Menu() );
    m_menu_rec_type = NULL;

    m_menu_sequences = NULL;

    m_menu_key = manage( new Menu());
    m_menu_scale = manage( new Menu());
    m_menu_chords = manage( new Menu());
    m_menu_data = NULL;

    create_menus();

    /* init table, viewports and scroll bars */
    m_table     = manage( new Table( 7, 4, false));
    m_vbox      = manage( new VBox( false, 2 ));
    m_hbox      = manage( new HBox( false, 2 ));
    m_hbox2     = manage( new HBox( false, 2 ));
    m_hbox3     = manage( new HBox( false, 2 ));
    HBox *dhbox = manage( new HBox( false, 2 ));

    m_vbox->set_border_width( 2 );

    /* fill table */
    m_table->attach( *m_seqkeys_wid,    0, 1, 1, 2, Gtk::SHRINK, Gtk::FILL );

    m_table->attach( *m_seqtime_wid, 1, 2, 0, 1, Gtk::FILL, Gtk::SHRINK );
    m_table->attach( *m_seqroll_wid, 1, 2, 1, 2,
                     Gtk::FILL |  Gtk::SHRINK,
                     Gtk::FILL |  Gtk::SHRINK );

    m_table->attach( *m_seqevent_wid, 1, 2, 2, 3, Gtk::FILL, Gtk::SHRINK );
    m_table->attach( *m_seqdata_wid, 1, 2, 3, 4, Gtk::FILL, Gtk::SHRINK );
    m_table->attach( *dhbox, 1, 2, 4, 5, Gtk::FILL | Gtk::EXPAND, Gtk::SHRINK, 0, 2 );

    m_table->attach( *m_vscroll_new, 2, 3, 1, 2, Gtk::SHRINK, Gtk::FILL | Gtk::EXPAND  );
    m_table->attach( *m_hscroll_new, 1, 2, 5, 6, Gtk::FILL | Gtk::EXPAND, Gtk::SHRINK  );

    /* no expand, just fit the widgets */
    /* m_vbox->pack_start(*m_menubar, false, false, 0); */
    m_vbox->pack_start(*m_hbox,  false, false, 0);
    m_vbox->pack_start(*m_hbox2, false, false, 0);
    m_vbox->pack_start(*m_hbox3, false, false, 0);

    /* exapand, cause rollview expands */
    m_vbox->pack_start(*m_table, true, true, 0);

    /* data button */
    m_button_data = manage( new Button( "Event" ));
    m_button_data->signal_clicked().connect(
        mem_fun( *this, &seqedit::popup_event_menu));

    m_entry_data = manage( new Entry( ));
    m_entry_data->set_size_request(40,-1);
    m_entry_data->set_editable( false );

    dhbox->pack_start( *m_button_data, false, false );
    dhbox->pack_start( *m_entry_data, true, true );

    /* lfo button */
    m_button_lfo = manage (new Button("LFO") );
    dhbox->pack_start(*m_button_lfo, false, false);
    m_button_lfo->signal_clicked().connect ( mem_fun(m_lfo_wnd, &lfownd::toggle_visible));

    /* background sequence */
    m_button_sequence = manage( new Button());
    m_button_sequence->add( *manage( new Image(Gdk::Pixbuf::create_from_xpm_data( sequences_xpm  ))));
    m_button_sequence->signal_clicked().connect(
        mem_fun( *this, &seqedit::popup_sequence_menu));
    add_tooltip( m_button_sequence, "Background Sequence" );
    m_entry_sequence = manage( new Entry());
    m_entry_sequence->set_size_request(10, -1);
    m_entry_sequence->set_editable( false );

    dhbox->pack_start( *m_button_sequence, false, false );
    dhbox->pack_start( *m_entry_sequence, true, true );

    /* play, rec, thru */
    m_toggle_play->add(  *manage( new Image(Gdk::Pixbuf::create_from_xpm_data( play_xpm ))));
    m_toggle_play->signal_clicked().connect(
        mem_fun( *this, &seqedit::play_change_callback));
    add_tooltip( m_toggle_play, "Sequence dumps data to midi bus." );

    m_toggle_record = manage( new ToggleButton(  ));
    m_toggle_record->add( *manage( new Image(Gdk::Pixbuf::create_from_xpm_data( rec_xpm ))));
    m_toggle_record->signal_clicked().connect(
        mem_fun( *this, &seqedit::record_change_callback));
    add_tooltip( m_toggle_record, "Records incoming midi data." );
    
    m_toggle_q_rec = manage( new ToggleButton(  ));
    m_toggle_q_rec->add( *manage( new Image(Gdk::Pixbuf::create_from_xpm_data( q_rec_xpm ))));
    m_toggle_q_rec->signal_clicked().connect(
        mem_fun( *this, &seqedit::q_rec_change_callback));
    add_tooltip( m_toggle_q_rec, "Quantized Record." );

    /* Record button */
    m_button_rec_type = manage( new Button( "Merge" ));
    m_button_rec_type->signal_clicked().connect(
          mem_fun( *this, &seqedit::popup_record_menu));
    add_tooltip( m_button_rec_type,
        "Select recording type for patterns: merge events; overwrite events; "
        "expand the pattern size while recording; or expand and replace while recording." );

    m_button_rec_vol = manage( new Button());
    m_button_rec_vol->add( *manage( new Label("Free")));
    m_button_rec_vol->signal_clicked().connect(
        sigc::bind<Menu *>( mem_fun( *this, &seqedit::popup_menu), m_menu_rec_vol  ));
    add_tooltip( m_button_rec_vol, "Select recording/generation volume." );

    m_toggle_thru = manage( new ToggleButton(  ));
    m_toggle_thru->add( *manage( new Image(Gdk::Pixbuf::create_from_xpm_data( thru_xpm ))));
    m_toggle_thru->signal_clicked().connect(
        mem_fun( *this, &seqedit::thru_change_callback));
    add_tooltip( m_toggle_thru, "Incoming midi data passes "
                 "thru to sequences midi bus and channel." );

    m_toggle_play->set_active( m_seq->get_playing());
    m_toggle_record->set_active( m_seq->get_recording());
    m_toggle_thru->set_active( m_seq->get_thru());

    dhbox->pack_end( *m_button_rec_vol, false, false);
    dhbox->pack_end( *m_button_rec_type, false, false);    
    dhbox->pack_end( *m_toggle_q_rec, false, false);
    dhbox->pack_end( *m_toggle_record, false, false);
    dhbox->pack_end( *m_toggle_thru, false, false);
    dhbox->pack_end( *m_toggle_play, false, false);
    dhbox->pack_end( *(manage(new VSeparator( ))), false, false, 4);

    fill_top_bar();

    if (m_seq->get_name() != std::string("Untitled"))
    {
        m_seqroll_wid->set_can_focus();
        m_seqroll_wid->grab_focus();
    }

    /* add table */
    this->add( *m_vbox );
    /* show everything */
    show_all();

    /* sets scroll bar to the middle */
    //gfloat middle = m_vscroll->get_adjustment()->get_upper() / 3;
    //m_vscroll->get_adjustment()->set_value(middle);
    m_seqroll_wid->set_ignore_redraw(true);

    set_zoom( m_zoom );
    set_snap( m_snap );
    set_note_length( m_note_length );
    set_background_sequence( m_bg_trk, m_bg_seq );

    set_bp_measure( m_seq->get_bp_measure() );
    set_bw( m_seq->get_bw() );
    m_seq->set_unit_measure(); /* need to set this before set_measures() */
    set_measures( get_measures(), false );

    set_data_type( EVENT_NOTE_ON );

    set_scale( m_scale );
    set_chord( m_chord );
    set_key( m_key );

    set_swing_mode(m_seq->get_swing_mode());

    m_seqroll_wid->set_ignore_redraw(false);
    add_events(Gdk::SCROLL_MASK);

    set_track_info();
    m_seqroll_wid->redraw();
    
    m_mainperf->set_sequence_editing_list(true);
}

void
seqedit::create_menus()
{
    using namespace Menu_Helpers;

    char b[20];

    /* zoom */
    for (int i = c_min_zoom; i <= c_max_zoom; i*=2)
    {
        snprintf(b, sizeof(b), "1:%d", i);
        MenuItem * menu_item = new MenuItem(b);
        menu_item->signal_activate().connect(sigc::bind(mem_fun(*this, &seqedit::set_zoom), i ) );
        m_menu_zoom->append(*menu_item);
    }

    /* note snap */
    m_snap_menu_items.resize(15);

    m_snap_menu_items[0].set_label("1");
    m_snap_menu_items[0].signal_activate().connect(sigc::bind(mem_fun(*this, &seqedit::set_snap), c_ppqn * 4) );
    m_menu_snap->append(m_snap_menu_items[0]);
    
    m_snap_menu_items[1].set_label("1/2");
    m_snap_menu_items[1].signal_activate().connect(sigc::bind(mem_fun(*this, &seqedit::set_snap), c_ppqn * 2) );
    m_menu_snap->append(m_snap_menu_items[1]);

    m_snap_menu_items[2].set_label("1/4");
    m_snap_menu_items[2].signal_activate().connect(sigc::bind(mem_fun(*this, &seqedit::set_snap), c_ppqn * 1) );
    m_menu_snap->append(m_snap_menu_items[2]);

    m_snap_menu_items[3].set_label("1/8");
    m_snap_menu_items[3].signal_activate().connect(sigc::bind(mem_fun(*this, &seqedit::set_snap), c_ppqn / 2) );
    m_menu_snap->append(m_snap_menu_items[3]);

    m_snap_menu_items[4].set_label("1/16");
    m_snap_menu_items[4].signal_activate().connect(sigc::bind(mem_fun(*this, &seqedit::set_snap), c_ppqn / 4) );
    m_menu_snap->append(m_snap_menu_items[4]);

    m_snap_menu_items[5].set_label("1/32");
    m_snap_menu_items[5].signal_activate().connect(sigc::bind(mem_fun(*this, &seqedit::set_snap), c_ppqn / 8) );
    m_menu_snap->append(m_snap_menu_items[5]);

    m_snap_menu_items[6].set_label("1/64");
    m_snap_menu_items[6].signal_activate().connect(sigc::bind(mem_fun(*this, &seqedit::set_snap), c_ppqn / 16) );
    m_menu_snap->append(m_snap_menu_items[6]);

    m_snap_menu_items[7].set_label("1/128");
    m_snap_menu_items[7].signal_activate().connect(sigc::bind(mem_fun(*this, &seqedit::set_snap), c_ppqn / 32) );
    m_menu_snap->append(m_snap_menu_items[7]);

    m_menu_snap->append(m_menu_separator0);

    m_snap_menu_items[8].set_label("1/3");
    m_snap_menu_items[8].signal_activate().connect(sigc::bind(mem_fun(*this, &seqedit::set_snap), c_ppqn * 4 / 3) );
    m_menu_snap->append(m_snap_menu_items[8]);

    m_snap_menu_items[9].set_label("1/6");
    m_snap_menu_items[9].signal_activate().connect(sigc::bind(mem_fun(*this, &seqedit::set_snap), c_ppqn * 2 / 3) );
    m_menu_snap->append(m_snap_menu_items[9]);

    m_snap_menu_items[10].set_label("1/12");
    m_snap_menu_items[10].signal_activate().connect(sigc::bind(mem_fun(*this, &seqedit::set_snap), c_ppqn * 1 / 3) );
    m_menu_snap->append(m_snap_menu_items[10]);

    m_snap_menu_items[11].set_label("1/24");
    m_snap_menu_items[11].signal_activate().connect(sigc::bind(mem_fun(*this, &seqedit::set_snap), c_ppqn / 2 / 3) );
    m_menu_snap->append(m_snap_menu_items[11]);

    m_snap_menu_items[12].set_label("1/48");
    m_snap_menu_items[12].signal_activate().connect(sigc::bind(mem_fun(*this, &seqedit::set_snap), c_ppqn / 4 / 3) );
    m_menu_snap->append(m_snap_menu_items[12]);

    m_snap_menu_items[13].set_label("1/96");
    m_snap_menu_items[13].signal_activate().connect(sigc::bind(mem_fun(*this, &seqedit::set_snap), c_ppqn / 8 / 3) );
    m_menu_snap->append(m_snap_menu_items[13]);

    m_snap_menu_items[14].set_label("1/192");
    m_snap_menu_items[14].signal_activate().connect(sigc::bind(mem_fun(*this, &seqedit::set_snap), c_ppqn / 16 / 3) );
    m_menu_snap->append(m_snap_menu_items[14]);

    /* note note length */
    m_note_length_menu_items.resize(15);

    m_note_length_menu_items[0].set_label("1");
    m_note_length_menu_items[0].signal_activate().connect(sigc::bind(mem_fun(*this, &seqedit::set_note_length), c_ppqn * 4) );
    m_menu_note_length->append(m_note_length_menu_items[0]);

    m_note_length_menu_items[1].set_label("1/2");
    m_note_length_menu_items[1].signal_activate().connect(sigc::bind(mem_fun(*this, &seqedit::set_note_length), c_ppqn * 2) );
    m_menu_note_length->append(m_note_length_menu_items[1]);

    m_note_length_menu_items[2].set_label("1/4");
    m_note_length_menu_items[2].signal_activate().connect(sigc::bind(mem_fun(*this, &seqedit::set_note_length), c_ppqn * 1) );
    m_menu_note_length->append(m_note_length_menu_items[2]);

    m_note_length_menu_items[3].set_label("1/8");
    m_note_length_menu_items[3].signal_activate().connect(sigc::bind(mem_fun(*this, &seqedit::set_note_length), c_ppqn / 2) );
    m_menu_note_length->append(m_note_length_menu_items[3]);

    m_note_length_menu_items[4].set_label("1/16");
    m_note_length_menu_items[4].signal_activate().connect(sigc::bind(mem_fun(*this, &seqedit::set_note_length), c_ppqn / 4) );
    m_menu_note_length->append(m_note_length_menu_items[4]);

    m_note_length_menu_items[5].set_label("1/32");
    m_note_length_menu_items[5].signal_activate().connect(sigc::bind(mem_fun(*this, &seqedit::set_note_length), c_ppqn / 8) );
    m_menu_note_length->append(m_note_length_menu_items[5]);

    m_note_length_menu_items[6].set_label("1/64");
    m_note_length_menu_items[6].signal_activate().connect(sigc::bind(mem_fun(*this, &seqedit::set_note_length), c_ppqn / 16) );
    m_menu_note_length->append(m_note_length_menu_items[6]);

    m_note_length_menu_items[7].set_label("1/128");
    m_note_length_menu_items[7].signal_activate().connect(sigc::bind(mem_fun(*this, &seqedit::set_note_length), c_ppqn / 32) );
    m_menu_note_length->append(m_note_length_menu_items[7]);

    m_menu_note_length->append(m_menu_separator1);

    m_note_length_menu_items[8].set_label("1/3");
    m_note_length_menu_items[8].signal_activate().connect(sigc::bind(mem_fun(*this, &seqedit::set_note_length), c_ppqn * 4 / 3) );
    m_menu_note_length->append(m_note_length_menu_items[8]);

    m_note_length_menu_items[9].set_label("1/6");
    m_note_length_menu_items[9].signal_activate().connect(sigc::bind(mem_fun(*this, &seqedit::set_note_length), c_ppqn * 2 / 3) );
    m_menu_note_length->append(m_note_length_menu_items[9]);

    m_note_length_menu_items[10].set_label("1/12");
    m_note_length_menu_items[10].signal_activate().connect(sigc::bind(mem_fun(*this, &seqedit::set_note_length), c_ppqn * 1 / 3) );
    m_menu_note_length->append(m_note_length_menu_items[10]);

    m_note_length_menu_items[11].set_label("1/24");
    m_note_length_menu_items[11].signal_activate().connect(sigc::bind(mem_fun(*this, &seqedit::set_note_length), c_ppqn / 2 / 3) );
    m_menu_note_length->append(m_note_length_menu_items[11]);

    m_note_length_menu_items[12].set_label("1/48");
    m_note_length_menu_items[12].signal_activate().connect(sigc::bind(mem_fun(*this, &seqedit::set_note_length), c_ppqn / 4 / 3) );
    m_menu_note_length->append(m_note_length_menu_items[12]);

    m_note_length_menu_items[13].set_label("1/96");
    m_note_length_menu_items[13].signal_activate().connect(sigc::bind(mem_fun(*this, &seqedit::set_note_length), c_ppqn / 8 / 3) );
    m_menu_note_length->append(m_note_length_menu_items[13]);

    m_note_length_menu_items[14].set_label("1/192");
    m_note_length_menu_items[14].signal_activate().connect(sigc::bind(mem_fun(*this, &seqedit::set_note_length), c_ppqn / 16 / 3) );
    m_menu_note_length->append(m_note_length_menu_items[14]);

    /* Key */
    for (int i = 0; i < 12; i++)
    {
        MenuItem * menu_item = new MenuItem(c_key_text[i]);
        menu_item->signal_activate().connect(sigc::bind(mem_fun(*this, &seqedit::set_key), i) );
        m_menu_key->append(*menu_item);
    }

    /* bw */
    m_bw_menu_items.resize(5);

    m_bw_menu_items[0].set_label("1");
    m_bw_menu_items[0].signal_activate().connect(sigc::bind(mem_fun(*this, &seqedit::set_bw), 1) );
    m_menu_bw->append(m_bw_menu_items[0]);

    m_bw_menu_items[1].set_label("2");
    m_bw_menu_items[1].signal_activate().connect(sigc::bind(mem_fun(*this, &seqedit::set_bw), 2) );
    m_menu_bw->append(m_bw_menu_items[1]);

    m_bw_menu_items[2].set_label("4");
    m_bw_menu_items[2].signal_activate().connect(sigc::bind(mem_fun(*this, &seqedit::set_bw), 4) );
    m_menu_bw->append(m_bw_menu_items[2]);

    m_bw_menu_items[3].set_label("8");
    m_bw_menu_items[3].signal_activate().connect(sigc::bind(mem_fun(*this, &seqedit::set_bw), 8) );
    m_menu_bw->append(m_bw_menu_items[3]);

    m_bw_menu_items[4].set_label("16");
    m_bw_menu_items[4].signal_activate().connect(sigc::bind(mem_fun(*this, &seqedit::set_bw), 16) );
    m_menu_bw->append(m_bw_menu_items[4]);

    /* note volume */
    m_record_volume_menu_items.resize(9);
    
    m_record_volume_menu_items[0].set_label("Free");
    m_record_volume_menu_items[0].signal_activate().connect(sigc::bind(mem_fun(*this, &seqedit::set_rec_vol), 0) );
    m_menu_rec_vol->append(m_record_volume_menu_items[0]);

    m_record_volume_menu_items[1].set_label("Fixed 8 (127)");
    m_record_volume_menu_items[1].signal_activate().connect(sigc::bind(mem_fun(*this, &seqedit::set_rec_vol), 127) );
    m_menu_rec_vol->append(m_record_volume_menu_items[1]);

    m_record_volume_menu_items[2].set_label("Fixed 7 (112)");
    m_record_volume_menu_items[2].signal_activate().connect(sigc::bind(mem_fun(*this, &seqedit::set_rec_vol), 112) );
    m_menu_rec_vol->append(m_record_volume_menu_items[2]);

    m_record_volume_menu_items[3].set_label("Fixed 6 (96)");
    m_record_volume_menu_items[3].signal_activate().connect(sigc::bind(mem_fun(*this, &seqedit::set_rec_vol), 96) );
    m_menu_rec_vol->append(m_record_volume_menu_items[3]);

    m_record_volume_menu_items[4].set_label("Fixed 5 (80)");
    m_record_volume_menu_items[4].signal_activate().connect(sigc::bind(mem_fun(*this, &seqedit::set_rec_vol), 80) );
    m_menu_rec_vol->append(m_record_volume_menu_items[4]);

    m_record_volume_menu_items[5].set_label("Fixed 4 (64)");
    m_record_volume_menu_items[5].signal_activate().connect(sigc::bind(mem_fun(*this, &seqedit::set_rec_vol), 64) );
    m_menu_rec_vol->append(m_record_volume_menu_items[5]);

    m_record_volume_menu_items[6].set_label("Fixed 3 (48)");
    m_record_volume_menu_items[6].signal_activate().connect(sigc::bind(mem_fun(*this, &seqedit::set_rec_vol), 48) );
    m_menu_rec_vol->append(m_record_volume_menu_items[6]);

    m_record_volume_menu_items[7].set_label("Fixed 2 (32)");
    m_record_volume_menu_items[7].signal_activate().connect(sigc::bind(mem_fun(*this, &seqedit::set_rec_vol), 32) );
    m_menu_rec_vol->append(m_record_volume_menu_items[7]);

    m_record_volume_menu_items[8].set_label("Fixed 1 (16)");
    m_record_volume_menu_items[8].signal_activate().connect(sigc::bind(mem_fun(*this, &seqedit::set_rec_vol), 16) );
    m_menu_rec_vol->append(m_record_volume_menu_items[8]);

    for (int i = int(c_scale_off); i < int(c_scale_size); i++)
    {
        /* music scale      */
        MenuItem * menu_item = new MenuItem(c_scales_text[i]);
        menu_item->signal_activate().connect(sigc::bind(mem_fun(*this, &seqedit::set_scale), i) );
        m_menu_scale->append(*menu_item);
    }

    for (int i = int(0); i < int(c_chord_number); i++)
    {
        /* Chords     */
        MenuItem * menu_item = new MenuItem(c_chord_table_text[i]);
        menu_item->signal_activate().connect(sigc::bind(mem_fun(*this, &seqedit::set_chord), i) );
        m_menu_chords->append(*menu_item);
    }

    for( int i = 0; i < 16; i++ )
    {
        snprintf(b, sizeof(b), "%d", i + 1);

        MenuItem * menu_item1 = new MenuItem(b);
        menu_item1->signal_activate().connect(sigc::bind(mem_fun(*this, &seqedit::measures_button_callback), i + 1) );
        m_menu_length->append(*menu_item1);

        MenuItem * menu_item2 = new MenuItem(b);
        menu_item2->signal_activate().connect(sigc::bind(mem_fun(*this, &seqedit::set_bp_measure), i + 1) );
        m_menu_bp_measure->append(*menu_item2);
    }

    MenuItem * menu_item32 = new MenuItem("32");
    menu_item32->signal_activate().connect(sigc::bind(mem_fun(*this, &seqedit::measures_button_callback), 32) );
    m_menu_length->append(*menu_item32);

    MenuItem * menu_item64 = new MenuItem("64");
    menu_item64->signal_activate().connect(sigc::bind(mem_fun(*this, &seqedit::measures_button_callback), 64) );
    m_menu_length->append(*menu_item64);

    /* Swing */
    MenuItem * menu_item_off = new MenuItem("Off");
    menu_item_off->signal_activate().connect(sigc::bind(mem_fun(*this, &seqedit::swing_button_callback), c_no_swing) );
    m_menu_swing_mode->append(*menu_item_off);

    MenuItem * menu_item18 = new MenuItem("1/8");
    menu_item18->signal_activate().connect(sigc::bind(mem_fun(*this, &seqedit::swing_button_callback), c_swing_eighths) );
    m_menu_swing_mode->append(*menu_item18);

    MenuItem * menu_item16 = new MenuItem("1/16");
    menu_item16->signal_activate().connect(sigc::bind(mem_fun(*this, &seqedit::swing_button_callback), c_swing_sixteenths) );
    m_menu_swing_mode->append(*menu_item16);
}

void
seqedit::popup_tool_menu()
{
    using namespace Menu_Helpers;

    m_menu_tools = manage( new Menu());

    /* tools */
    Menu *holder;
    Menu *holder2;

    holder = manage( new Menu());

    m_tools_menu_items.clear();
    m_tools_menu_items.resize(23);
    
    m_tools_menu_items[0].set_label("All Notes");
    m_tools_menu_items[0].signal_activate().connect(sigc::bind(mem_fun(*this, &seqedit::do_action), select_all_notes, 0) );
    holder->append(m_tools_menu_items[0]);

    m_tools_menu_items[1].set_label("Inverse Notes");
    m_tools_menu_items[1].signal_activate().connect(sigc::bind(mem_fun(*this, &seqedit::do_action), select_inverse_notes, 0) );
    holder->append(m_tools_menu_items[1]);

    m_tools_menu_items[2].set_label("Even 1/4 Note Beats");
    m_tools_menu_items[2].signal_activate().connect(sigc::bind(mem_fun(*this, &seqedit::do_action), select_even_notes, c_ppqn) );
    holder->append(m_tools_menu_items[2]);

    m_tools_menu_items[3].set_label("Odd 1/4 Note Beats");
    m_tools_menu_items[3].signal_activate().connect(sigc::bind(mem_fun(*this, &seqedit::do_action), select_odd_notes, c_ppqn) );
    holder->append(m_tools_menu_items[3]);

    m_tools_menu_items[4].set_label("Even 1/8 Note Beats");
    m_tools_menu_items[4].signal_activate().connect(sigc::bind(mem_fun(*this, &seqedit::do_action), select_even_notes, c_ppen) );
    holder->append(m_tools_menu_items[4]);

    m_tools_menu_items[5].set_label("Odd 1/8 Note Beats");
    m_tools_menu_items[5].signal_activate().connect(sigc::bind(mem_fun(*this, &seqedit::do_action), select_odd_notes, c_ppen) );
    holder->append(m_tools_menu_items[5]);

    m_tools_menu_items[6].set_label("Even 1/16 Note Beats");
    m_tools_menu_items[6].signal_activate().connect(sigc::bind(mem_fun(*this, &seqedit::do_action), select_even_notes, c_ppsn) );
    holder->append(m_tools_menu_items[6]);

    m_tools_menu_items[7].set_label("Odd 1/16 Note Beats");
    m_tools_menu_items[7].signal_activate().connect(sigc::bind(mem_fun(*this, &seqedit::do_action), select_odd_notes, c_ppsn) );
    holder->append(m_tools_menu_items[7]);

    if ( m_editing_status !=  EVENT_NOTE_ON &&
            m_editing_status !=  EVENT_NOTE_OFF )
    {
        SeparatorMenuItem * menu_separator = new SeparatorMenuItem;
        holder->append(*menu_separator);

        m_tools_menu_items[8].set_label("All Events");
        m_tools_menu_items[8].signal_activate().connect(sigc::bind(mem_fun(*this, &seqedit::do_action), select_all_events, 0) );
        holder->append(m_tools_menu_items[8]);

        m_tools_menu_items[9].set_label("Inverse Events");
        m_tools_menu_items[9].signal_activate().connect(sigc::bind(mem_fun(*this, &seqedit::do_action), select_inverse_events, 0) );
        holder->append(m_tools_menu_items[9]);
    }

    m_tools_menu_items[10].set_label("Select");
    m_tools_menu_items[10].set_submenu(*holder);
    m_menu_tools->append(m_tools_menu_items[10]);

    holder = manage( new Menu());

    m_tools_menu_items[11].set_label("Quantize Selected Notes");
    m_tools_menu_items[11].signal_activate().connect(sigc::bind(mem_fun(*this, &seqedit::do_action), quantize_notes, 0 ) );
    holder->append(m_tools_menu_items[11]);

    m_tools_menu_items[12].set_label("Tighten Selected Notes");
    m_tools_menu_items[12].signal_activate().connect(sigc::bind(mem_fun(*this, &seqedit::do_action), tighten_notes, 0 ) );
    holder->append(m_tools_menu_items[12]);

    if ( m_editing_status !=  EVENT_NOTE_ON &&
            m_editing_status !=  EVENT_NOTE_OFF )
    {
        SeparatorMenuItem * menu_separator = new SeparatorMenuItem;
        holder->append(*menu_separator);

        m_tools_menu_items[13].set_label("Quantize Selected Events");
        m_tools_menu_items[13].signal_activate().connect(sigc::bind(mem_fun(*this, &seqedit::do_action), quantize_events, 0 ) );
        holder->append(m_tools_menu_items[13]);

        m_tools_menu_items[14].set_label("Tighten Selected Events");
        m_tools_menu_items[14].signal_activate().connect(sigc::bind(mem_fun(*this, &seqedit::do_action), tighten_events, 0 ) );
        holder->append(m_tools_menu_items[14]);
    }

    SeparatorMenuItem * menu_separator = new SeparatorMenuItem;
    holder->append(*menu_separator);

    m_tools_menu_items[15].set_label("Expand Pattern (double)");
    m_tools_menu_items[15].signal_activate().connect(sigc::bind(mem_fun(*this, &seqedit::do_action), expand_pattern, 0 ) );
    holder->append(m_tools_menu_items[15]);

    m_tools_menu_items[16].set_label("Compress Pattern (halve)");
    m_tools_menu_items[16].signal_activate().connect(sigc::bind(mem_fun(*this, &seqedit::do_action), compress_pattern, 0 ) );
    holder->append(m_tools_menu_items[16]);

    m_tools_menu_items[17].set_label("Modify Time");
    m_tools_menu_items[17].set_submenu(*holder);
    m_menu_tools->append(m_tools_menu_items[17]);

    holder = manage( new Menu());

    char num[11];

    holder2 = manage( new Menu());
    for ( int i = -12; i <= 12; ++i)
    {
        if (i != 0)
        {
            snprintf(num, sizeof(num), "%+d [%s]", i, c_interval_text[abs(i)]);

            MenuItem * menu_item = new MenuItem(num);
            menu_item->signal_activate().connect(sigc::bind(mem_fun(*this,&seqedit::do_action), transpose, i) );
            holder2->insert(*menu_item, 0);
        }
    }

    m_tools_menu_items[18].set_label("Transpose Selected");
    m_tools_menu_items[18].set_submenu(*holder2);
    holder->append(m_tools_menu_items[18]);

    holder2 = manage( new Menu());
    for ( int i = -7; i <= 7; ++i)
    {
        if (i != 0)
        {
            snprintf(num, sizeof(num), "%+d [%s]", (i<0) ? i-1 : i+1, c_chord_text[abs(i)]);

            MenuItem * menu_item = new MenuItem(num);
            menu_item->signal_activate().connect(sigc::bind(mem_fun(*this,&seqedit::do_action), transpose_h, i) );
            holder2->insert(*menu_item, 0);
        }
    }

    if ( m_scale != 0 )
    {
        m_tools_menu_items[19].set_label("Harmonic Transpose Selected");
        m_tools_menu_items[19].set_submenu(*holder2);
        holder->append(m_tools_menu_items[19]);
    }

    m_tools_menu_items[20].set_label("Modify Pitch");
    m_tools_menu_items[20].set_submenu(*holder);
    m_menu_tools->append(m_tools_menu_items[20]);

    holder = manage( new Menu());
    for ( int i = 1; i < 17; ++i)
    {
        snprintf(num, sizeof(num), "+/- %d", i);

        MenuItem * menu_item = new MenuItem(num);
        menu_item->signal_activate().connect(sigc::bind(mem_fun(*this,&seqedit::do_action), randomize_events, i) );
        holder->append(*menu_item);
    }

    m_tools_menu_items[21].set_label("Randomize Event Values");
    m_tools_menu_items[21].set_submenu(*holder);
    m_menu_tools->append(m_tools_menu_items[21]);
    
    m_tools_menu_items[22].set_label("Reverse pattern");
    m_tools_menu_items[22].signal_activate().connect(sigc::bind(mem_fun(*this, &seqedit::do_action), reverse_pattern, 0) );
    m_menu_tools->append(m_tools_menu_items[22]);

    m_menu_tools->show_all();
    m_menu_tools->popup_at_pointer(NULL);
}


void
seqedit::do_action( int a_action, int a_var )
{
    switch (a_action)
    {
    case select_all_notes:
        m_seq->select_events(EVENT_NOTE_ON, 0);
        m_seq->select_events(EVENT_NOTE_OFF, 0);
        break;

    case select_all_events:
        m_seq->select_events(m_editing_status, m_editing_cc);
        break;

    case select_inverse_notes:
        m_seq->select_events(EVENT_NOTE_ON, 0, true);
        m_seq->select_events(EVENT_NOTE_OFF, 0, true);
        break;

    case select_inverse_events:
        m_seq->select_events(m_editing_status, m_editing_cc, true);
        break;

    case select_even_notes:
        m_seq->select_even_or_odd_notes(a_var, true);
        break;

    case select_odd_notes:
        m_seq->select_even_or_odd_notes(a_var, false);
        break;

    case randomize_events:
        m_seq->push_undo();
        m_seq->randomize_selected(m_editing_status, m_editing_cc, a_var);
        break;

    case quantize_notes:
        m_seq->quanize_events(EVENT_NOTE_ON, 0, m_snap, 1, true);
        break;

    case quantize_events:
        m_seq->quanize_events(m_editing_status, m_editing_cc, m_snap, 1);
        break;

    case tighten_notes:
        m_seq->quanize_events(EVENT_NOTE_ON, 0, m_snap, 2, true);
        break;

    case tighten_events:
        m_seq->quanize_events(m_editing_status, m_editing_cc, m_snap, 2);
        break;

    case transpose:
        m_seq->transpose_notes(a_var, 0);
        m_seq->set_dirty();     // update mainwnd
        break;

    case transpose_h:
        m_seq->transpose_notes(a_var, m_scale);
        m_seq->set_dirty();     // update mainwnd
        break;

    case expand_pattern:
        m_seq->push_undo();
        m_seq->multiply_pattern(2.0);
        set_measures(get_measures(), true);
        break;

    case compress_pattern:
        m_seq->push_undo();
        m_seq->multiply_pattern(0.5);
        set_measures(get_measures(), true);
        break;

    case reverse_pattern:
        m_seq->push_undo();
        m_seq->reverse_pattern();
        break;

    default:
        break;
    }

    m_seqroll_wid->redraw();
    m_seqtime_wid->redraw();
    m_seqdata_wid->redraw();
    m_seqevent_wid->redraw();
}


void
seqedit::fill_top_bar()
{
    /* name */
    m_entry_name = manage( new Entry( ));
    m_entry_name->set_name("Sequence Name");
    m_entry_name->set_max_length(c_max_seq_name);
    m_entry_name->set_width_chars(c_max_seq_name - 10);
    m_entry_name->set_text( m_seq->get_name());
    m_entry_name->select_region(0,0);
    m_entry_name->set_position(0);
    m_entry_name->signal_changed().connect(
        mem_fun( *this, &seqedit::name_change_callback));

    m_hbox->pack_start( *m_entry_name, true, true );
    m_hbox->pack_start( *(manage(new VSeparator( ))), false, false, 4);

    /* beats per measure */
    m_button_bp_measure = manage( new Button());
    m_button_bp_measure->add( *manage( new Image(Gdk::Pixbuf::create_from_xpm_data( down_xpm  ))));
    m_button_bp_measure->signal_clicked().connect(
        sigc::bind<Menu *>( mem_fun( *this, &seqedit::popup_menu),
                            m_menu_bp_measure  ));
    add_tooltip( m_button_bp_measure, "Time Signature. Beats per Measure" );
    m_entry_bp_measure = manage( new Entry());
    m_entry_bp_measure->set_width_chars(2);
    m_entry_bp_measure->set_editable( false );

    m_hbox->pack_start( *m_button_bp_measure, false, false );
    m_hbox->pack_start( *m_entry_bp_measure, false, false );

    m_hbox->pack_start( *(manage(new Label( "/" ))), false, false, 4);

    /* beat width */
    m_button_bw = manage( new Button());
    m_button_bw->add( *manage( new Image(Gdk::Pixbuf::create_from_xpm_data( down_xpm  ))));
    m_button_bw->signal_clicked().connect(
        sigc::bind<Menu *>( mem_fun( *this, &seqedit::popup_menu),
                            m_menu_bw  ));
    add_tooltip( m_button_bw, "Time Signature. Length of Beat" );
    m_entry_bw = manage( new Entry());
    m_entry_bw->set_width_chars(2);
    m_entry_bw->set_editable( false );

    m_hbox->pack_start( *m_button_bw, false, false );
    m_hbox->pack_start( *m_entry_bw, false, false );

    /* length */
    m_button_length = manage( new Button());
    m_button_length->add( *manage( new Image(Gdk::Pixbuf::create_from_xpm_data( length_xpm  ))));
    m_button_length->signal_clicked().connect(
        sigc::bind<Menu *>( mem_fun( *this, &seqedit::popup_menu),
                            m_menu_length  ));
    add_tooltip( m_button_length, "Sequence length in Bars." );
    m_entry_length = manage( new Entry());
    m_entry_length->set_width_chars(2);
    m_entry_length->set_editable( false );

    m_hbox->pack_start( *m_button_length, false, false );
    m_hbox->pack_start( *m_entry_length, false, false );

    /* swing mode */
    m_button_swing_mode = manage( new Button("swing"));
    //m_button_swing_mode->add( *manage( new Image(Gdk::Pixbuf::create_from_xpm_data( swing_mode_xpm  ))));
    m_button_swing_mode->signal_clicked().connect(
        sigc::bind<Menu *>( mem_fun( *this, &seqedit::popup_menu),
                            m_menu_swing_mode  ));
    add_tooltip( m_button_swing_mode, "Sequence swing mode." );
    m_entry_swing_mode = manage( new Entry());
    m_entry_swing_mode->set_width_chars(4);
    m_entry_swing_mode->set_editable( false );

    m_hbox->pack_start( *m_button_swing_mode, false, false );
    m_hbox->pack_start( *m_entry_swing_mode, false, false );

    m_hbox->pack_start( *(manage(new VSeparator( ))), false, false, 4);

    /* track info */
    m_button_track = manage( new Button());
    m_button_track->add( *manage( new Image(Gdk::Pixbuf::create_from_xpm_data( midi_xpm  ))));
    m_button_track->signal_clicked().connect(  mem_fun( *this, &seqedit::trk_edit));
    add_tooltip( m_button_track, "Edit track." );
    m_hbox->pack_start( *m_button_track, false, false );
    m_entry_track = manage( new Entry());
    m_entry_track->set_max_length(50);
    m_entry_track->set_width_chars(20);
    m_entry_track->set_editable( false );
    m_hbox->pack_start( *m_entry_track, true, true );
    m_label_bus = manage (new Label( "Bus" ));
    m_hbox->pack_start( *m_label_bus, false, false );
    m_entry_bus = manage( new Entry());
    m_entry_bus->set_max_length(2);
    m_entry_bus->set_width_chars(2);
    m_entry_bus->set_editable( false );
    m_hbox->pack_start( *m_entry_bus, false, false );
    m_label_channel = manage (new Label( "Ch" ));
    m_hbox->pack_start( *m_label_channel, false, false );
    m_entry_channel = manage( new Entry());
    m_entry_channel->set_max_length(2);
    m_entry_channel->set_width_chars(2);
    m_entry_channel->set_editable( false );
    m_hbox->pack_start( *m_entry_channel, false, false );

    /* undo */
    m_button_undo = manage( new Button());
    m_button_undo->add( *manage( new Image(Gdk::Pixbuf::create_from_xpm_data( undo_xpm  ))));
    m_button_undo->set_can_focus(false);
    m_button_undo->signal_clicked().connect(
        mem_fun( *this, &seqedit::undo_callback));
    add_tooltip( m_button_undo, "Undo." );

    m_hbox2->pack_start( *m_button_undo, false, false );

    /* redo */
    m_button_redo = manage( new Button());
    m_button_redo->add( *manage( new Image(Gdk::Pixbuf::create_from_xpm_data( redo_xpm  ))));
    m_button_redo->set_can_focus(false);
    m_button_redo->signal_clicked().connect(
        mem_fun( *this, &seqedit::redo_callback));
    add_tooltip( m_button_redo, "Redo." );

    m_hbox2->pack_start( *m_button_redo, false, false );

    /* quantize shortcut */
    m_button_quanize = manage( new Button());
    m_button_quanize->add( *manage( new Image(Gdk::Pixbuf::create_from_xpm_data( quanize_xpm  ))));
    m_button_quanize->signal_clicked().connect(
        sigc::bind(mem_fun(*this, &seqedit::do_action), quantize_notes, 0));
    add_tooltip( m_button_quanize, "Quantize Selection." );

    m_hbox2->pack_start( *m_button_quanize, false, false );

    m_hbox2->pack_start( *(manage(new VSeparator( ))), false, false, 4);

    /* tools button */
    m_button_tools = manage( new Button( ));
    m_button_tools->add( *manage( new Image(Gdk::Pixbuf::create_from_xpm_data( tools_xpm  ))));
    m_button_tools->signal_clicked().connect(
        mem_fun( *this, &seqedit::popup_tool_menu ));
    add_tooltip(m_button_tools, "Tools." );

    m_hbox2->pack_start( *m_button_tools, false, false );
    m_hbox2->pack_start( *(manage(new VSeparator( ))), false, false, 4);

    /* snap */
    m_button_snap = manage( new Button());
    m_button_snap->add( *manage( new Image(Gdk::Pixbuf::create_from_xpm_data( snap_xpm  ))));
    m_button_snap->signal_clicked().connect(
        sigc::bind<Menu *>( mem_fun( *this, &seqedit::popup_menu),
                            m_menu_snap  ));
    add_tooltip( m_button_snap, "Grid snap." );
    m_entry_snap = manage( new Entry());
    m_entry_snap->set_width_chars(5);
    m_entry_snap->set_editable( false );

    m_hbox2->pack_start( *m_button_snap, false, false );
    m_hbox2->pack_start( *m_entry_snap, false, false );

    /* note_length */
    m_button_note_length = manage( new Button());
    m_button_note_length->add( *manage( new Image(Gdk::Pixbuf::create_from_xpm_data( note_length_xpm  ))));
    m_button_note_length->signal_clicked().connect(
        sigc::bind<Menu *>( mem_fun( *this, &seqedit::popup_menu),
                            m_menu_note_length  ));
    add_tooltip( m_button_note_length, "Note Length." );
    m_entry_note_length = manage( new Entry());
    m_entry_note_length->set_width_chars(5);
    m_entry_note_length->set_editable( false );

    m_hbox2->pack_start( *m_button_note_length, false, false );
    m_hbox2->pack_start( *m_entry_note_length, false, false );

    /* zoom */
    m_button_zoom = manage( new Button());
    m_button_zoom->add( *manage( new Image(Gdk::Pixbuf::create_from_xpm_data( zoom_xpm  ))));
    m_button_zoom->signal_clicked().connect(
        sigc::bind<Menu *>( mem_fun( *this, &seqedit::popup_menu),
                            m_menu_zoom  ));
    add_tooltip( m_button_zoom, "Zoom. Pixels to Ticks" );
    m_entry_zoom = manage( new Entry());
    m_entry_zoom->set_width_chars(4);
    m_entry_zoom->set_editable( false );

    m_hbox2->pack_start( *m_button_zoom, false, false );
    m_hbox2->pack_start( *m_entry_zoom, false, false );

    m_hbox2->pack_start( *(manage(new VSeparator( ))), false, false, 4);

    /* key */
    m_button_key = manage( new Button());
    m_button_key->add( *manage( new Image(Gdk::Pixbuf::create_from_xpm_data( key_xpm  ))));
    m_button_key->signal_clicked().connect(
        sigc::bind<Menu *>( mem_fun( *this, &seqedit::popup_menu),
                            m_menu_key  ));
    add_tooltip( m_button_key, "Key of Sequence" );
    m_entry_key = manage( new Entry());
    m_entry_key->set_width_chars(5);
    m_entry_key->set_editable( false );

    m_hbox2->pack_start( *m_button_key, false, false );
    m_hbox2->pack_start( *m_entry_key, false, false );

    /* music scale */
    m_button_scale = manage( new Button());
    m_button_scale->add( *manage( new Image(Gdk::Pixbuf::create_from_xpm_data( scale_xpm  ))));
    m_button_scale->signal_clicked().connect(
        sigc::bind<Menu *>( mem_fun( *this, &seqedit::popup_menu),
                            m_menu_scale  ));
    add_tooltip( m_button_scale, "Musical Scale" );
    m_entry_scale = manage( new Entry());
    m_entry_scale->set_size_request(10, -1);
    m_entry_scale->set_editable( false );
    m_hbox2->pack_start( *m_button_scale, false, false );
    m_hbox2->pack_start( *m_entry_scale, true, true );

    /* music chord */
    m_button_chord = manage( new Button());
    m_button_chord->add( *manage( new Image(Gdk::Pixbuf::create_from_xpm_data( chord_xpm  ))));
    m_button_chord->signal_clicked().connect(
        sigc::bind<Menu *>( mem_fun( *this, &seqedit::popup_menu),
                            m_menu_chords  ));
    add_tooltip( m_button_chord, "Chord Selection" );
    m_entry_chord = manage( new Entry());
    m_entry_chord->set_size_request(10, -1);
    m_entry_chord->set_editable( false );

    m_hbox2->pack_start( *m_button_chord, false, false );
    m_hbox2->pack_start( *m_entry_chord, true, true );
}

void
seqedit::popup_menu(Menu *a_menu)
{
    a_menu->show_all();
    a_menu->popup_at_pointer(NULL);
}

void
seqedit::popup_sequence_menu()
{
    using namespace Menu_Helpers;

    m_menu_sequences = manage( new Menu());

    MenuItem * menu_item = new MenuItem("Off");
    menu_item->signal_activate().connect(sigc::bind(mem_fun(*this, &seqedit::set_background_sequence), -1, -1));
    m_menu_sequences->append(*menu_item);
    
    SeparatorMenuItem * menu_separator = new SeparatorMenuItem;
    m_menu_sequences->append(*menu_separator);

    char name[40];
    for ( int t=0; t<c_max_track; ++t )
    {
        if (! m_mainperf->is_active_track( t ))
        {
            continue;
        }
        track *a_track = m_mainperf->get_track(t);

        Menu *menu_t = NULL;
        bool inserted = false;

        for (unsigned s=0; s< a_track->get_number_of_sequences(); s++ )
        {
            if ( !inserted )
            {
                inserted = true;
                snprintf(name, sizeof(name), "[%d] %s", t+1, a_track->get_name());
                menu_t = manage( new Menu());

                MenuItem * menu_item = new MenuItem(name);
                menu_item->set_submenu(*menu_t);
                m_menu_sequences->append(*menu_item);
            }

            sequence *a_seq = a_track->get_sequence( s );
            snprintf(name, sizeof(name),"[%d] %s", s+1, a_seq->get_name());

            MenuItem * menu_item = new MenuItem(name);
            menu_item->signal_activate().connect(sigc::bind(mem_fun(*this, &seqedit::set_background_sequence), t, s));
            menu_t->append(*menu_item);
        }
    }

    m_menu_sequences->show_all();
    m_menu_sequences->popup_at_pointer(NULL);
}

void
seqedit::set_background_sequence( int a_trk, int a_seq )
{
    char name[60];

    m_initial_bg_trk = m_bg_trk = a_trk;
    m_initial_bg_seq = m_bg_seq = a_seq;

    sequence *seq = m_mainperf->get_sequence(a_trk, a_seq);

    if ( seq == NULL)
    {
        m_entry_sequence->set_text("Off");
        m_seqroll_wid->set_background_sequence( false, -1, -1 );
    }
    else
    {
        snprintf(name, sizeof(name),"[%d/%d] %s", a_trk+1, a_seq+1, seq->get_name());
        m_entry_sequence->set_text(name);
        m_seqroll_wid->set_background_sequence( true, a_trk, a_seq );
    }
}

void
seqedit::popup_event_menu()
{
    using namespace Menu_Helpers;

    /* temp */
    char b[20];

    bool note_on = false;
    bool note_off = false;
    bool aftertouch = false;
    bool program_change = false;
    bool channel_pressure = false;
    bool pitch_wheel = false;
    bool ccs[128];
    memset( ccs, false, sizeof(bool) * 128 );

    int midi_bus = m_seq->get_midi_bus();
    int midi_ch = m_seq->get_midi_channel();

    unsigned char status, cc;
    m_seq->reset_draw_marker();
    while ( m_seq->get_next_event( &status, &cc ) == true )
    {
        switch( status )
        {
        case EVENT_NOTE_OFF:
            note_off = true;
            break;
        case EVENT_NOTE_ON:
            note_on = true;
            break;
        case EVENT_AFTERTOUCH:
            aftertouch = true;
            break;
        case EVENT_CONTROL_CHANGE:
            ccs[cc] = true;
            break;
        case EVENT_PITCH_WHEEL:
            pitch_wheel = true;
            break;
        /* one data item */
        case EVENT_PROGRAM_CHANGE:
            program_change = true;
            break;
        case EVENT_CHANNEL_PRESSURE:
            channel_pressure = true;
            break;
        }
    }

    m_menu_data = manage( new Menu());

    m_data_menu_items.clear();
    m_data_menu_items.resize(6);

    m_data_menu_items[0].set_label("Note On Velocity");
    m_data_menu_items[0].set_active(note_on);
    m_data_menu_items[0].signal_activate().connect(sigc::bind(mem_fun(*this, &seqedit::set_data_type), (unsigned char) EVENT_NOTE_ON, 0 ));
    m_menu_data->append(m_data_menu_items[0]);

    MenuItem * menu_separator1 = new SeparatorMenuItem();
    m_menu_data->append(*menu_separator1);

    m_data_menu_items[1].set_label("Note Off Velocity");
    m_data_menu_items[1].set_active(note_off);
    m_data_menu_items[1].signal_activate().connect(sigc::bind(mem_fun(*this, &seqedit::set_data_type), (unsigned char) EVENT_NOTE_OFF, 0 ));
    m_menu_data->append(m_data_menu_items[1]);

    m_data_menu_items[2].set_label("AfterTouch");
    m_data_menu_items[2].set_active(aftertouch);
    m_data_menu_items[2].signal_activate().connect(sigc::bind(mem_fun(*this, &seqedit::set_data_type), (unsigned char) EVENT_AFTERTOUCH, 0 ));
    m_menu_data->append(m_data_menu_items[2]);

    m_data_menu_items[3].set_label("Program Change");
    m_data_menu_items[3].set_active(program_change);
    m_data_menu_items[3].signal_activate().connect(sigc::bind(mem_fun(*this, &seqedit::set_data_type), (unsigned char) EVENT_PROGRAM_CHANGE, 0 ));
    m_menu_data->append(m_data_menu_items[3]);

    m_data_menu_items[4].set_label("Channel Pressure");
    m_data_menu_items[4].set_active(channel_pressure);
    m_data_menu_items[4].signal_activate().connect(sigc::bind(mem_fun(*this, &seqedit::set_data_type), (unsigned char) EVENT_CHANNEL_PRESSURE, 0 ));
    m_menu_data->append(m_data_menu_items[4]);

    m_data_menu_items[5].set_label("Pitch Wheel");
    m_data_menu_items[5].set_active(pitch_wheel);
    m_data_menu_items[5].signal_activate().connect(sigc::bind(mem_fun(*this, &seqedit::set_data_type), (unsigned char) EVENT_PITCH_WHEEL, 0 ));
    m_menu_data->append(m_data_menu_items[5]);

    MenuItem * menu_separator2 = new SeparatorMenuItem();
    m_menu_data->append(*menu_separator2);

    /* create control change */
    for ( int i = 0; i < 8; i++ )
    {
        snprintf(b, sizeof(b), "Controls %d-%d", (i*16), (i*16) + 15);
        Menu *menu_cc = manage( new Menu() );

        for( int j = 0; j < 16; j++ )
        {
            string controller_name( c_controller_names[i*16+j] );
            int instrument = global_user_midi_bus_definitions[midi_bus].instrument[midi_ch];
            if ( instrument > -1 && instrument < c_max_instruments )
            {
                if ( global_user_instrument_definitions[instrument].controllers_active[i*16+j] )
                    controller_name = global_user_instrument_definitions[instrument].controllers[i*16+j];
            }

            CheckMenuItem * menu_item =  new CheckMenuItem();
            menu_item->set_label(controller_name);
            menu_item->set_active(ccs[i * 16 + j]);
            menu_item->signal_activate().connect(sigc::bind(mem_fun(this, &seqedit::set_data_type),
                                                   (unsigned char) EVENT_CONTROL_CHANGE, i * 16 + j));
            menu_cc->append(*menu_item);
        }

        MenuItem * menu_item = new MenuItem();
        menu_item->set_label(string(b));
        menu_item->set_submenu(*menu_cc);
        m_menu_data->append(*menu_item);
    }

    m_menu_data->show_all();
    m_menu_data->popup_at_pointer(NULL);
}

void
seqedit::popup_record_menu()
{
    using namespace Menu_Helpers;
    
    m_menu_rec_type = manage( new Menu());
    
    /* record type */
    m_record_type_menu_items.clear();
    m_record_type_menu_items.resize(4);

    m_record_type_menu_items[0].set_label("Legacy merge looped recording");
    m_record_type_menu_items[0].signal_activate().connect(sigc::bind(mem_fun(*this, &seqedit::set_rec_type), LOOP_RECORD_LEGACY ));
    m_menu_rec_type->append(m_record_type_menu_items[0]);

    m_record_type_menu_items[1].set_label("Overwrite looped recording");
    m_record_type_menu_items[1].signal_activate().connect(sigc::bind(mem_fun(*this, &seqedit::set_rec_type), LOOP_RECORD_OVERWRITE ));
    m_menu_rec_type->append(m_record_type_menu_items[1]);

    m_record_type_menu_items[2].set_label("Expand sequence length to fit recording");
    m_record_type_menu_items[2].signal_activate().connect(sigc::bind(mem_fun(*this, &seqedit::set_rec_type), LOOP_RECORD_EXPAND ));
    m_menu_rec_type->append(m_record_type_menu_items[2]);

    m_record_type_menu_items[3].set_label("Expand sequence length and overwrite");
    m_record_type_menu_items[3].signal_activate().connect(sigc::bind(mem_fun(*this, &seqedit::set_rec_type), LOOP_RECORD_EXP_OVR ));
    m_menu_rec_type->append(m_record_type_menu_items[3]);

    m_menu_rec_type->show_all();
    m_menu_rec_type->popup_at_pointer(NULL);
}

void
seqedit::set_track_info( )
{
    char b[50];
    snprintf( b, sizeof(b), "[%d] %s",
              m_mainperf->get_track_index(m_seq->get_track()) + 1,
              m_seq->get_track()->get_name());
    m_entry_track->set_text(b);
    snprintf( b, sizeof(b), "%d",
              m_seq->get_track()->get_midi_bus() + 1);
    m_entry_bus->set_text(b);
    snprintf( b, sizeof(b), "%d",
              m_seq->get_track()->get_midi_channel() + 1);
    m_entry_channel->set_text(b);
}

void
seqedit::set_zoom( int a_zoom  )
{
    if ((a_zoom >= c_min_zoom) && (a_zoom <= c_max_zoom))
    {
        char b[10];

        snprintf(b, sizeof(b), "1:%d", a_zoom);
        m_entry_zoom->set_text(b);

        m_zoom = a_zoom;
        m_initial_zoom = a_zoom;
        m_seqroll_wid->set_zoom( m_zoom );
        m_seqtime_wid->set_zoom( m_zoom );
        m_seqdata_wid->set_zoom( m_zoom );
        m_seqevent_wid->set_zoom( m_zoom );
    }
}

void
seqedit::set_snap( int a_snap  )
{
    char b[10];

    snprintf(b, sizeof(b), "1/%d",   c_ppqn * 4 / a_snap);
    m_entry_snap->set_text(b);

    m_snap = a_snap;
    m_initial_snap = a_snap;
    m_seqroll_wid->set_snap( m_snap );
    m_seqevent_wid->set_snap( m_snap );
    m_seq->set_snap_tick(a_snap);
}

void
seqedit::set_note_length( int a_note_length  )
{
    char b[10];

    snprintf(b, sizeof(b), "1/%d",   c_ppqn * 4 / a_note_length);
    m_entry_note_length->set_text(b);

    m_note_length = a_note_length;
    m_initial_note_length = a_note_length;
    m_seqroll_wid->set_note_length( m_note_length );
}

void
seqedit::set_vertical_zoom( float a_zoom )
{    
    if ( a_zoom > c_max_seqroll_vertical_zoom || a_zoom < c_min_seqroll_vertical_zoom )
        return;
    
    m_vertical_zoom = a_zoom;
    
    m_key_y = c_key_y * m_vertical_zoom;
    m_keyarea_y = m_key_y * c_num_keys + 1;
    m_rollarea_y = m_keyarea_y;
    
    m_seqroll_wid->set_vertical_zoom(m_key_y, m_rollarea_y);
    m_seqkeys_wid->set_vertical_zoom(m_key_y, m_keyarea_y, m_rollarea_y, m_vertical_zoom);
    m_seqevent_wid->set_vertical_zoom(m_key_y);
}

void
seqedit::set_scale( int a_scale )
{
    m_entry_scale->set_text( c_scales_text[a_scale] );

    m_scale = m_initial_scale = a_scale;

    m_seqroll_wid->set_scale( m_scale );
    m_seqkeys_wid->set_scale( m_scale );
}

void
seqedit::set_chord( int a_chord )
{
    m_entry_chord->set_text( c_chord_table_text[a_chord] );

    m_chord = m_initial_chord = a_chord;

    m_seqroll_wid->set_chord( m_chord );
}


void
seqedit::set_key( int a_note )
{
    m_entry_key->set_text( c_key_text[a_note] );

    m_key = m_initial_key = a_note;

    m_seqroll_wid->set_key( m_key );
    m_seqkeys_wid->set_key( m_key );
}

void
seqedit::apply_length( int a_bp_measure, int a_bw, int a_measures, bool a_adjust_triggers )
{
    m_seq->set_length( a_measures * a_bp_measure * ((c_ppqn * 4) / a_bw), a_adjust_triggers );
    m_seq->set_unit_measure(); // for follow_progress , redraw, update progress

    m_seqroll_wid->reset();
    m_seqtime_wid->reset();
    m_seqdata_wid->reset();
    m_seqevent_wid->reset();
    m_seq->set_dirty();
}

long
seqedit::get_measures()
{
    long units = m_seq->get_unit_measure();

    long measures = (m_seq->get_length() / units);

    if (m_seq->get_length() % units != 0)
        measures++;

    return measures;
}

void
seqedit::measures_button_callback( int a_length_measures )
{
    if(m_measures != a_length_measures)
    {
        set_measures(a_length_measures,true);
        global_is_modified = true;
    }
}

void
seqedit::set_measures( int a_length_measures, bool a_adjust_triggers  )
{
    char b[10];

    snprintf(b, sizeof(b), "%d", a_length_measures);
    m_entry_length->set_text(b);

    m_measures = a_length_measures;
    apply_length( m_seq->get_bp_measure(), m_seq->get_bw(), a_length_measures, a_adjust_triggers );
}

void
seqedit::swing_button_callback( int a_mode)
{
    if(get_swing_mode() != a_mode)
    {
        set_swing_mode(a_mode);
        global_is_modified = true;
    }
}

int
seqedit::get_swing_mode( void  )
{
    return m_seq->get_swing_mode();
}

void
seqedit::set_swing_mode( int a_mode  )
{
    m_seq->set_swing_mode(a_mode);
    if(a_mode == c_swing_eighths)
    {
        m_entry_swing_mode->set_text("1/8");
    }
    else if(a_mode == c_swing_sixteenths)
    {
        m_entry_swing_mode->set_text("1/16");
    }
    else
    {
        m_entry_swing_mode->set_text("off");
    }
}

void
seqedit::set_bp_measure( int a_beats_per_measure )
{
    char b[4];

    snprintf(b, sizeof(b), "%d", a_beats_per_measure);
    m_entry_bp_measure->set_text(b);

    if ( a_beats_per_measure != m_seq->get_bp_measure() )
    {
        long length = get_measures();
        m_seq->set_bp_measure( a_beats_per_measure );
        apply_length( a_beats_per_measure, m_seq->get_bw(), length, true );
        global_is_modified = true;
    }
}

void
seqedit::set_bw( int a_beat_width  )
{
    char b[4];

    snprintf(b, sizeof(b), "%d", a_beat_width);
    m_entry_bw->set_text(b);

    if ( a_beat_width != m_seq->get_bw())
    {
        long length = get_measures();
        m_seq->set_bw( a_beat_width );
        apply_length( m_seq->get_bp_measure(), a_beat_width, length, true );
        global_is_modified = true;
    }
}

void
seqedit::name_change_callback()
{
    m_seq->set_name( m_entry_name->get_text());
    global_is_modified = true;
}

void
seqedit::play_change_callback()
{
    m_seq->set_playing( m_toggle_play->get_active() );
}

void
seqedit::record_change_callback()
{
    /*
        Both record_change_callback() and thru_change_callback() will call
        set_sequence_input() for the same sequence. We only need to call
        it if it is not already set, if setting. And, we should not unset
        it if the m_toggle_thru->get_active() is true.
    */

    if(!m_toggle_thru->get_active())    // only change if thru is not active
        m_mainperf->get_master_midi_bus()->set_sequence_input( m_toggle_record->get_active(), m_seq );

    m_seq->set_recording( m_toggle_record->get_active() );
}


void
seqedit::q_rec_change_callback()
{
    m_seq->set_quanized_rec( m_toggle_q_rec->get_active() );
    if(m_toggle_q_rec->get_active() && !m_toggle_record->get_active()) // If we set Q then also set record
        m_toggle_record->activate();                                   // but do not unset record if unset Q
}

void
seqedit::undo_callback()
{
    m_seq->pop_undo( );

    m_seqroll_wid->redraw();
    m_seqtime_wid->redraw();
    m_seqdata_wid->redraw();
    m_seqevent_wid->redraw();
    m_seq->set_dirty();
}

void
seqedit::redo_callback()
{
    m_seq->pop_redo( );

    m_seqroll_wid->redraw();
    m_seqtime_wid->redraw();
    m_seqdata_wid->redraw();
    m_seqevent_wid->redraw();
    m_seq->set_dirty();
}

void
seqedit::thru_change_callback()
{
    /*
        Both thru_change_callback() and record_change_callback() will call
        set_sequence_input() for the same sequence. We only need to call
        it if it is not already set, if setting. And, we should not unset
        it if the m_toggle_record->get_active() is true.
    */

    if(!m_toggle_record->get_active())  // only change if record is not active
        m_mainperf->get_master_midi_bus()->set_sequence_input( m_toggle_thru->get_active(), m_seq );

    m_seq->set_thru( m_toggle_thru->get_active() );
}

void
seqedit::set_data_type( unsigned char a_status, unsigned char a_control  )
{
    m_editing_status = a_status;
    m_editing_cc = a_control;

    m_seqevent_wid->set_data_type( a_status, a_control );
    m_seqdata_wid->set_data_type( a_status, a_control );
    m_seqroll_wid->set_data_type( a_status, a_control );

    char text[100];
    char hex[20];
    char type[80];

    snprintf(hex, sizeof(hex), "[0x%02X]", a_status);

    if ( a_status == EVENT_NOTE_OFF )
        snprintf(type, sizeof(type),"Note Off" );
    else if ( a_status == EVENT_NOTE_ON )
        snprintf(type, sizeof(type),"Note On" );
    else if ( a_status == EVENT_AFTERTOUCH )
        snprintf(type, sizeof(type), "Aftertouch" );
    else if ( a_status == EVENT_CONTROL_CHANGE )
    {
        int midi_bus = m_seq->get_midi_bus();
        int midi_ch = m_seq->get_midi_channel();

        string controller_name( c_controller_names[a_control] );
        int instrument = global_user_midi_bus_definitions[midi_bus].instrument[midi_ch];
        if ( instrument > -1 && instrument < c_max_instruments )
        {
            if ( global_user_instrument_definitions[instrument].controllers_active[a_control] )
                controller_name = global_user_instrument_definitions[instrument].controllers[a_control];
        }
        snprintf(type, sizeof(type), "Control Change - %s",
                 controller_name.c_str() );
    }

    else if ( a_status == EVENT_PROGRAM_CHANGE )
        snprintf(type, sizeof(type), "Program Change");
    else if ( a_status == EVENT_CHANNEL_PRESSURE )
        snprintf(type, sizeof(type), "Channel Pressure");
    else if ( a_status == EVENT_PITCH_WHEEL )
        snprintf(type, sizeof(type), "Pitch Wheel");
    else
        snprintf(type, sizeof(type), "Unknown MIDI Event");

    snprintf(text, sizeof(text), "%s %s", hex, type );

    m_entry_data->set_text( text );
}

void
seqedit::on_realize()
{
    // we need to do the default realize
    Gtk::Window::on_realize();
    Glib::signal_timeout().connect(mem_fun(*this, &seqedit::timeout),
                                   c_redraw_ms);
}

bool
seqedit::timeout()
{
    if (m_seq->get_raise())
    {
        m_seq->set_raise(false);
        raise();
    }

    m_seqroll_wid->draw_progress_on_window();
    
#ifdef MIDI_CONTROL_SUPPORT
    if(m_mainperf->get_sequence_record())
    {
        m_mainperf->set_sequence_record(false);
        m_toggle_record->set_active(!m_toggle_record->get_active());    // this triggers the button callback
    }
#endif // MIDI_CONTROL_SUPPORT

    if(m_seq->get_recording() &&  m_seqroll_wid->get_expanded_record() && 
            (m_seq->get_last_tick() >= ( m_seq->get_length() - ( m_seq->get_unit_measure()/4 ) )))
    {
        set_measures(get_measures() + 1,true);
        m_seqroll_wid->follow_progress();
    }

    if (m_seq->is_dirty_edit() )
    {
        m_seqroll_wid->redraw_events();
        m_seqevent_wid->redraw();
        m_seqdata_wid->redraw();
    }
    
    if(global_is_running && m_mainperf->get_follow_transport() &&
            !(m_seqroll_wid->get_expanded_record() && m_seq->get_recording()) )
    {
        m_seqroll_wid->follow_progress();
    }
    
    // FIXME: ick
    set_track_info();

    if(m_seq->m_have_undo && !m_button_undo->get_sensitive())
        m_button_undo->set_sensitive(true);
    else if(!m_seq->m_have_undo && m_button_undo->get_sensitive())
        m_button_undo->set_sensitive(false);

    if(m_seq->m_have_redo && !m_button_redo->get_sensitive())
        m_button_redo->set_sensitive(true);
    else if(!m_seq->m_have_redo && m_button_redo->get_sensitive())
        m_button_redo->set_sensitive(false);

    return true;
}

seqedit::~seqedit()
{
    //m_seq->set_editing( false );
}

bool
seqedit::on_delete_event(GdkEventAny * /* a_event */)
{
    //printf( "seqedit::on_delete_event()\n" );
#ifdef NSM_SUPPORT
    m_mainwnd->remove_window_pointer(this);
#endif

    m_seq->set_recording( false );
    m_mainperf->get_master_midi_bus()->set_sequence_input( false, m_seq );
    m_seq->set_editing( false );

    m_mainperf->set_sequence_editing_list(false);

    delete m_lfo_wnd;
    delete this;
    return false;
}


bool
seqedit::on_scroll_event( GdkEventScroll* a_ev )
{
    //printf("seqedit::on_scroll_event(x=%f,y=%f,state=%d)\n", a_ev->x, a_ev->y, a_ev->state);

    guint modifiers;    // Used to filter out caps/num lock etc.
    modifiers = gtk_accelerator_get_default_mod_mask ();

    if ((a_ev->state & modifiers) == GDK_CONTROL_MASK)
    {
        if (a_ev->direction == GDK_SCROLL_DOWN)
        {
            if (m_zoom*2 <= c_max_zoom)
                set_zoom(m_zoom*2);
        }
        else if (a_ev->direction == GDK_SCROLL_UP)
        {
            if (m_zoom/2 >= c_min_zoom)
                set_zoom(m_zoom/2);
        }
        return true;
    }

    if ((a_ev->state & modifiers) == GDK_MOD1_MASK)
    {
        if (a_ev->direction == GDK_SCROLL_DOWN)
        {
            set_vertical_zoom(m_vertical_zoom + c_vert_seqroll_zoom_step);
        }
        else if (a_ev->direction == GDK_SCROLL_UP)
        {
            set_vertical_zoom(m_vertical_zoom - c_vert_seqroll_zoom_step);
        }
        return true;
    }

    if ((a_ev->state & modifiers) == GDK_SHIFT_MASK)
    {
        double val = m_hadjust->get_value();
        double step = m_hadjust->get_step_increment();
        double upper = m_hadjust->get_upper();

        if (a_ev->direction == GDK_SCROLL_DOWN)
        {
            if (val + step < upper)
                m_hadjust->set_value(val + step);
            else
                m_hadjust->set_value(upper);
        }
        else if (a_ev->direction == GDK_SCROLL_UP)
        {
            m_hadjust->set_value(val - step);
        }
        return true;
    }

    return false;  // Not handled
}

bool
seqedit::on_key_press_event( GdkEventKey* a_ev )
{
    guint modifiers;    // Used to filter out caps/num lock etc.
    modifiers = gtk_accelerator_get_default_mod_mask ();

    if ((a_ev->state & modifiers) == GDK_CONTROL_MASK && a_ev->keyval == 'w')
        return on_delete_event((GdkEventAny*)a_ev);

    if(get_focus()->get_name() == "Sequence Name")    // if we are on the sequence name
        return Gtk::Window::on_key_press_event(a_ev); // return = don't do anything else

    if(! m_seqroll_wid->on_key_press_event(a_ev))     // seqroll has priority - no duplicates
    {
        bool result = false;
        if (a_ev->keyval == GDK_KEY_Z)              /* zoom in              */
        {
            set_zoom(m_zoom / 2);
            result = true;
        }
        else if (a_ev->keyval == GDK_KEY_0)         /* reset to normal zoom */
        {
            set_zoom(c_default_zoom);
            result = true;
        }
        else if (a_ev->keyval == GDK_KEY_z)         /* zoom out             */
        {
            set_zoom(m_zoom * 2);
            result = true;
        }

        /* Vertical zoom */
        if ( !(a_ev->state & GDK_CONTROL_MASK) )
        {
            if (a_ev->keyval == GDK_KEY_V)         /* zoom in              */
            {
                set_vertical_zoom(m_vertical_zoom + c_vert_seqroll_zoom_step);
                result = true;
            }
            else if (a_ev->keyval == GDK_KEY_9)    /* reset to normal zoom */
            {
                set_vertical_zoom(c_default_vertical_zoom);
                result = true;
            }
            else if (a_ev->keyval == GDK_KEY_v)    /* zoom out             */
            {
                set_vertical_zoom(m_vertical_zoom - c_vert_seqroll_zoom_step);
                result = true;
            }
        }

        if (! result)
            result = Gtk::Window::on_key_press_event(a_ev);

        return result;
    }

    return false;
}

void
seqedit::start_playing()
{
    if(!global_song_start_mode)
        m_seq->set_playing( m_toggle_play->get_active(),true );

    m_mainperf->start_playing();
}

void
seqedit::stop_playing()
{
    m_mainperf->stop_playing();
}

void
seqedit::trk_edit()
{
    track *a_track = m_seq->get_track();
    if(a_track->get_editing())
    {
        a_track->set_raise(true);
    }
    else
    {
        trackedit * a_trackedit = new trackedit(a_track, m_mainwnd);
#ifdef NSM_SUPPORT
        m_mainwnd->set_window_pointer(a_trackedit);
#endif
    }
}

void
seqedit::set_rec_vol( int a_rec_vol  )
{
    char selection[16];
    if (a_rec_vol == 0)
    {
        snprintf(selection, sizeof selection, "Free");
    }
    else
    {
        snprintf(selection, sizeof selection, "%d", a_rec_vol);
    }

    Gtk::Label * label(dynamic_cast<Gtk::Label *>(m_button_rec_vol->get_child()));
    
    if (label != nullptr)
    {
        label->set_text(selection);
    }
    
    m_seq->get_track()->set_default_velocity( a_rec_vol );
}

void
seqedit::set_rec_type( loop_record_t a_rec_type )
{
    std::string label = "Merge";
    
    switch (a_rec_type)
    {
    case LOOP_RECORD_LEGACY:

        m_seqroll_wid->set_expanded_recording(false);
        m_seq->set_overwrite_rec(false); 
        break;

    case LOOP_RECORD_OVERWRITE:
        
        m_seq->set_overwrite_rec(true);
        m_seqroll_wid->set_expanded_recording(false);
        label = "Replace";
        break;

    case LOOP_RECORD_EXPAND:

        m_seqroll_wid->set_expanded_recording(true);
        m_seq->set_overwrite_rec(false); 
        label = "Expand";
        break;
        
    case LOOP_RECORD_EXP_OVR:
        m_seqroll_wid->set_expanded_recording(true);
        m_seq->set_overwrite_rec(true); 
        label = "Exp/Rep";
        break;

    default:
        break;
    }

    Gtk::Label * ptr(dynamic_cast<Gtk::Label *>(m_button_rec_type->get_child()));
    
    if (ptr != nullptr)
    {
        char temp[8];
        snprintf(temp, sizeof(temp), "%s", label.c_str());
        ptr->set_text(temp);
    }
}
