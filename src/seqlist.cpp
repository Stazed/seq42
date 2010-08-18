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

seqlist::seqlist (perform *a_perf)
{
    m_perf = a_perf;

    /* main window */
    set_title("seq42 - sequence list");
    set_size_request(458, 200);
    set_border_width( 2 );

    add(m_ScrolledWindow);
    //Only show the scrollbars when they are necessary:
    m_ScrolledWindow.set_policy(Gtk::POLICY_AUTOMATIC, Gtk::POLICY_AUTOMATIC);
    m_ScrolledWindow.add(m_TreeView);

    //Create the Tree model:
    m_refTreeModel = Gtk::ListStore::create(m_Columns);
    m_TreeView.set_model(m_refTreeModel);
    m_TreeView.set_rules_hint(true);

    m_TreeView.append_column("Trk#", m_Columns.m_trk_num);
    m_TreeView.append_column("Trk Name", m_Columns.m_trk_name);
    m_TreeView.append_column("Seq#", m_Columns.m_seq_num);
    m_TreeView.append_column("Seq Name", m_Columns.m_seq_name);
    m_TreeView.append_column_editable("Playing", m_Columns.m_playing);
    m_TreeView.append_column("Triggers", m_Columns.m_triggers);

    // Allow sorting by clicking on columns 
    Gtk::TreeView::Column* pColumn; 
    for(guint i = 0; i < 6; i++) { 
        pColumn = m_TreeView.get_column(i); 
        pColumn->set_sort_column(i); 
    }

    m_TreeView.signal_button_release_event().connect(sigc::mem_fun(*this, &seqlist::on_button_release_event), true );

    m_perf->set_seqlist_open(true);
    update_model();
    show_all();
}

seqlist::~seqlist()
{
}


bool
seqlist::on_delete_event(GdkEventAny *a_event)
{
    //printf( "seqlist::on_delete_event()\n" );
    m_perf->set_seqlist_open(false);
    delete this;
    return false;
}

bool
seqlist::get_selected_playing_state()
{
    if(m_TreeView.get_selection()->count_selected_rows()!=1)
    {
        return NULL;
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
        if ( a_e->button == 1 ){
            a_seq->set_playing(get_selected_playing_state());
        }
        else if ( a_e->button == 3 ){
            using namespace Menu_Helpers;
            track *a_track = a_seq->get_track();
            Menu *menu = manage( new Menu());
            menu->items().push_back(MenuElem("Edit", sigc::bind(mem_fun(*this,&seqlist::edit_seq), a_seq )));
            menu->items().push_back(MenuElem("Delete", sigc::bind(mem_fun(*this,&seqlist::del_seq), a_track, a_track->get_sequence_index(a_seq) )));
            menu->popup(0,0);
        }
    }
    return true;
}

void
seqlist::update_model()
{
    m_refTreeModel->clear();
    Gtk::TreeModel::Row row;
    for(int i=0; i< c_max_track; i++ ){
        if ( m_perf->is_active_track(i) ) {
            track *a_track =  m_perf->get_track( i );
            for(int s=0; s< a_track->get_number_of_sequences(); s++ ){
                 sequence *a_seq =  a_track->get_sequence( s );
                 row = *(m_refTreeModel->append());
                 row[m_Columns.m_trk_num] = i+1;
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
seqlist::timeout( void )
{
    if (m_perf->get_seqlist_raise())
    {
        m_perf->set_seqlist_raise(false);
        raise();
    }

    // FIXME: instead of polling for dirt... move to a model where the mainwnd
    // sends us notification events.
    for(int i=0; i< c_max_track; i++ ){
        if ( m_perf->is_active_track(i) ) {
            track *a_track =  m_perf->get_track( i );
            if(a_track->is_dirty_seqlist())
            {
                update_model();
                break;
            }
        }
    }
    return true;
}


void
seqlist::edit_seq( sequence *a_seq )
{
    if(a_seq->get_editing()) {
        a_seq->set_raise(true);
    } else {
        new seqedit( a_seq, m_perf );
    }
}

void
seqlist::del_seq( track *a_track, int a_seq )
{
    // FIXME: if sequence is triggered, ask for confirmation
    if( a_track->get_trigger_count_for_seqidx(a_seq))
    {
    }
    a_track->delete_sequence( a_seq );
}
