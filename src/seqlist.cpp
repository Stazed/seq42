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

#include "seqlist.h"
#include "seqedit.h"

#include "pixmaps/stop.xpm"
#include "pixmaps/play2.xpm"
#include "pixmaps/create.xpm"
#include "pixmaps/fastforward.xpm"
#include "pixmaps/rewind.xpm"

#define add_tooltip( obj, text ) obj->set_tooltip_text( text);

seqlist::seqlist (perform *a_perf, mainwnd *a_main)
{
    m_perf = a_perf;
    m_main = a_main;

    /* main window */
    set_title("seq42 - sequence list");
    set_size_request(548, 200);
    set_border_width( 2 );

    m_vbox = manage( new VBox( false, 2 ));
    add(*m_vbox);

    m_hbox = manage( new HBox( false, 2 ));

    /* Stop, rewind, play, FF buttons */
    m_button_stop = manage( new Button( ));
    m_button_stop->add( *manage( new Image(Gdk::Pixbuf::create_from_xpm_data( stop_xpm ))));
    m_button_stop->signal_clicked().connect( mem_fun(*this,&seqlist::stop_playing));
    add_tooltip( m_button_stop, "Stop/Pause playing." );
    m_hbox->pack_start(*m_button_stop, false, false);
    
    m_button_rewind = manage( new Button() );
    m_button_rewind->add(*manage( new Image(Gdk::Pixbuf::create_from_xpm_data( rewind_xpm ))));
    m_button_rewind->signal_pressed().connect(sigc::bind (mem_fun( *this, &seqlist::rewind), true));
    m_button_rewind->signal_released().connect(sigc::bind (mem_fun( *this, &seqlist::rewind), false));
    add_tooltip( m_button_rewind, "Rewind." );
    m_hbox->pack_start(*m_button_rewind, false, false);
    
    m_button_play = manage( new Button() );
    m_button_play->add( *manage( new Image(Gdk::Pixbuf::create_from_xpm_data( play2_xpm  ))));
    m_button_play->signal_clicked().connect(  mem_fun( *this, &seqlist::start_playing));
    add_tooltip( m_button_play, "Begin/Continue playing." );
    m_hbox->pack_start(*m_button_play, false, false);
    
    m_button_fastforward = manage( new Button() );
    m_button_fastforward->add(*manage( new Image(Gdk::Pixbuf::create_from_xpm_data( fastforward_xpm ))));
    m_button_fastforward->signal_pressed().connect(sigc::bind (mem_fun( *this, &seqlist::fast_forward), true));
    m_button_fastforward->signal_released().connect(sigc::bind (mem_fun( *this, &seqlist::fast_forward), false));
    add_tooltip( m_button_fastforward, "Fast Forward." );
    m_hbox->pack_start(*m_button_fastforward, false, false);
    
    m_button_create_triggers = manage( new Button());
    m_button_create_triggers->add( *manage( new Image(Gdk::Pixbuf::create_from_xpm_data( create_xpm ))));
    m_button_create_triggers->signal_clicked().connect( mem_fun( *this, &seqlist::create_triggers));
    add_tooltip( m_button_create_triggers, "Create triggers for all playing sequences.\n"
            "Triggers will be placed between the L and R markers.\n"
            "The transport must be stopped to create the triggers.\n"
            "Any existing triggers from the same track within the\n"
            "L and R markers will be over-written by the selected sequence");
    m_hbox->pack_start(*m_button_create_triggers, false, false);

    m_button_all_off = manage( new Button("All Off") );
    m_button_all_off->signal_clicked().connect(  mem_fun( *this, &seqlist::off_sequences));
    m_hbox->pack_end(*m_button_all_off, false, false);

    m_vbox->pack_start(m_ScrolledWindow);
    m_vbox->pack_start(*m_hbox, Gtk::PACK_SHRINK);

    //Only show the scrollbars when they are necessary:
    m_ScrolledWindow.set_policy(Gtk::POLICY_AUTOMATIC, Gtk::POLICY_AUTOMATIC);
    m_ScrolledWindow.add(m_TreeView);

    //Create the Tree model:
    m_refTreeModel = Gtk::ListStore::create(m_Columns);
    m_TreeView.set_model(m_refTreeModel);
    m_TreeView.set_rules_hint(true);

    m_TreeView.append_column("Trk", m_Columns.m_trk_num);
    m_TreeView.append_column("Bus", m_Columns.m_midi_bus);
    m_TreeView.append_column("Chan", m_Columns.m_midi_chan);
    m_TreeView.append_column("Trk Name", m_Columns.m_trk_name);
    m_TreeView.append_column("Seq", m_Columns.m_seq_num);
    m_TreeView.append_column("Seq Name", m_Columns.m_seq_name);
    m_TreeView.append_column_editable("Playing", m_Columns.m_playing);
    m_TreeView.append_column("Triggers", m_Columns.m_triggers);

    // Allow sorting by clicking on columns
    Gtk::TreeView::Column* pColumn;
    for(guint i = 0; i < 8; i++)
    {
        pColumn = m_TreeView.get_column(i);
        pColumn->set_sort_column(i);
    }

    m_TreeView.signal_button_release_event().connect(sigc::mem_fun(*this, &seqlist::on_button_release_event), true );

    m_perf->set_seqlist_open(true);
    update_model(); // needed or reopen of window fails to show the list
    show_all();
}

seqlist::~seqlist()
{
}

bool
seqlist::on_delete_event(GdkEventAny * /* a_event */)
{
    //printf( "seqlist::on_delete_event()\n" );
#ifdef NSM_SUPPORT
    m_main->remove_window_pointer(this);
#endif
    m_perf->set_seqlist_open(false);
    delete this;
    return false;
}

bool
seqlist::get_selected_playing_state()
{
    if(m_TreeView.get_selection()->count_selected_rows()!=1)
    {
        return false;
    }
    Gtk::TreeModel::Children::iterator iter_selected = m_TreeView.get_selection()->get_selected();
    Gtk::TreeModel::Row current_row = *iter_selected;
    return current_row[m_Columns.m_playing];
}

sequence *
seqlist::get_selected_sequence()
{
    if(m_TreeView.get_selection()->count_selected_rows()!=1)
    {
        return NULL;
    }
    Gtk::TreeModel::Children::iterator iter_selected = m_TreeView.get_selection()->get_selected();
    Gtk::TreeModel::Row current_row = *iter_selected;
    int a_trk = current_row[m_Columns.m_trk_num] - 1;
    int a_seq = current_row[m_Columns.m_seq_num] - 1;
    return m_perf->get_sequence(a_trk, a_seq);
}

bool
seqlist::on_button_release_event(GdkEventButton* a_e)
{
    sequence *a_seq = get_selected_sequence();
    if(a_seq != NULL)
    {
        if ( a_e->button == 1 )
        {
            a_seq->set_playing(get_selected_playing_state(), false);
        }
        else if ( a_e->button == 3 )
        {
            using namespace Menu_Helpers;
            track *a_track = a_seq->get_track();
            Menu *menu = manage( new Menu());

            m_menu_items.clear();
            m_menu_items.resize(4);

            m_menu_items[0].set_label("Edit");
            m_menu_items[0].signal_activate().connect(sigc::bind(mem_fun(*this,&seqlist::edit_seq), a_seq  ));
            menu->append(m_menu_items[0]);

            m_menu_items[1].set_label("Copy");
            m_menu_items[1].signal_activate().connect(sigc::bind(mem_fun(*this,&seqlist::copy_seq), a_seq  ));
            menu->append(m_menu_items[1]);

            m_menu_items[2].set_label("Export");
            m_menu_items[2].signal_activate().connect(sigc::bind(mem_fun(*this,&seqlist::export_seq), a_seq  ));
            menu->append(m_menu_items[2]);

            m_menu_items[3].set_label("Delete");
            m_menu_items[3].signal_activate().connect(sigc::bind(mem_fun(*this,&seqlist::del_seq), a_track, a_track->get_sequence_index(a_seq)));
            menu->append(m_menu_items[3]);

            menu->show_all();
            menu->popup_at_pointer(NULL);
        }
    }
    return true;
}

void
seqlist::update_model()
{
    m_refTreeModel->clear();
    Gtk::TreeModel::Row row;
    for(int i=0; i< c_max_track; i++ )
    {
        if ( m_perf->is_active_track(i) )
        {
            track *a_track =  m_perf->get_track( i );
            for(unsigned s=0; s< a_track->get_number_of_sequences(); s++ )
            {
                sequence *a_seq =  a_track->get_sequence( s );
                row = *(m_refTreeModel->append());
                row[m_Columns.m_trk_num] = i+1;
                row[m_Columns.m_midi_bus] =  a_seq->get_midi_bus()+1;
                row[m_Columns.m_midi_chan] = a_seq->get_midi_channel()+1;
                row[m_Columns.m_trk_name] = a_track->get_name();
                row[m_Columns.m_seq_num] = s+1;
                row[m_Columns.m_seq_name] = a_seq->get_name();
                row[m_Columns.m_playing] = a_seq->get_playing();
                row[m_Columns.m_triggers] = a_track->get_trigger_count_for_seqidx(s);
            }
        }
    }
}

void
seqlist::on_realize()
{
    // we need to do the default realize
    Gtk::Window::on_realize();
    Glib::signal_timeout().connect(mem_fun(*this, &seqlist::timeout),
                                   c_redraw_ms);
}

bool
seqlist::timeout()
{
    if (m_perf->get_seqlist_toggle())
    {
        m_perf->set_seqlist_toggle(false);
        on_delete_event(0);
       // raise();
    }

    if(global_seqlist_need_update)
    {
        global_seqlist_need_update = false;
        update_model();
    }

    return true;
}

void
seqlist::edit_seq( sequence *a_seq )
{
    if(a_seq->get_editing())
    {
        a_seq->set_raise(true);
    }
    else
    {
        seqedit * a_seq_edit = new seqedit( a_seq, m_perf, m_main );
#ifdef NSM_SUPPORT
        m_main->set_window_pointer(a_seq_edit);
#endif
    }
}

void
seqlist::copy_seq( sequence *a_seq )
{
    m_perf->push_track_undo(m_perf->get_track_index(a_seq->get_track()));
    track *a_track = a_seq->get_track();
    int seq_idx = a_track->new_sequence();
    sequence *new_seq = a_track->get_sequence(seq_idx);
    *new_seq = *a_seq;
    char new_name[c_max_seq_name+1];
    snprintf(new_name, sizeof(new_name), "%s copy", new_seq->get_name());
    new_seq->set_name( new_name );
    seqedit * a_seq_edit = new seqedit( new_seq, m_perf, m_main );
#ifdef NSM_SUPPORT
    m_main->set_window_pointer(a_seq_edit);
#endif
}

void
seqlist::export_seq( sequence *a_seq )
{
    m_main->export_sequence_midi(a_seq);
}

void
seqlist::del_seq( track *a_track, int a_seq )
{
    if( a_track->get_trigger_count_for_seqidx(a_seq))
    {
        Gtk::MessageDialog warning(*this,
                                   "Sequence is triggered!\n"
                                   "Do you still want to delete it?",
                                   false,
                                   Gtk::MESSAGE_WARNING, Gtk::BUTTONS_YES_NO, true);

        auto result = warning.run();

        if (result == Gtk::RESPONSE_NO || result == Gtk::RESPONSE_DELETE_EVENT)
            return;
    }
    m_perf->push_track_undo(m_perf->get_track_index(a_track));
    a_track->delete_sequence( a_seq );
}

void
seqlist::create_triggers()
{
    m_main->create_triggers();
}

void
seqlist::start_playing()
{
    m_perf->start_playing();
}

void
seqlist::stop_playing()
{
    m_perf->stop_playing();
}

void
seqlist::rewind(bool a_press)
{
    m_main->rewind(a_press);
}

void
seqlist::fast_forward(bool a_press)
{
    m_main->fast_forward(a_press);
}

void
seqlist::off_sequences()
{
    m_perf->off_sequences();
}

/**
 *  So we can have use of any main window key binding.
 * 
 * @param a_p0
 *      The key event.
 * 
 * @return 
 *      True if used, false if not.
 */
bool
seqlist::on_key_press_event(GdkEventKey* a_p0)
{
    return m_main->on_key_press_event(a_p0);
}

/**
 * For rewind and fast forward key binding.
 * 
 * @param a_ev
 *      The event key released.
 * 
 * @return 
 *      True if used, false if not.
 */
bool
seqlist::on_key_release_event(GdkEventKey* a_ev)
{
    return m_main->on_key_release_event(a_ev);
}
