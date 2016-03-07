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

#include <gtkmm.h>
#include <sigc++/bind.h>
#include "globals.h"
#include "track.h"
#include "perform.h"

using namespace Gtk;

class seqlist : public Gtk::Window
{

 private:

    perform *m_perf;

    class ModelColumns : public Gtk::TreeModel::ColumnRecord
    {
        public:
            ModelColumns()
            { add(m_trk_num); add(m_trk_name); add(m_seq_num); add(m_seq_name); add(m_playing); add(m_triggers); }

            Gtk::TreeModelColumn<int> m_trk_num;
            Gtk::TreeModelColumn<Glib::ustring> m_trk_name;
            Gtk::TreeModelColumn<int> m_seq_num;
            Gtk::TreeModelColumn<Glib::ustring> m_seq_name;
            Gtk::TreeModelColumn<bool> m_playing;
            Gtk::TreeModelColumn<int> m_triggers;
    };
    ModelColumns m_Columns;

    ScrolledWindow m_ScrolledWindow;
    TreeView m_TreeView;
    Glib::RefPtr<Gtk::ListStore> m_refTreeModel;

    VBox       *m_vbox;
    HBox       *m_hbox;
    Button     *m_button_stop;
    Button     *m_button_play;
    Button     *m_button_all_off;

    void update_model( );
    void edit_seq( sequence *a_seq );
    void copy_seq( sequence *a_seq );
    void del_seq( track *a_track, int a_seq );

    void start_playing();
    void stop_playing();
    void off_sequences();

    void on_realize();
    bool timeout();

    bool on_delete_event(GdkEventAny *a_event);
    bool on_button_release_event(GdkEventButton* a_e);
    bool on_key_press_event(GdkEventKey* a_p0);

    sequence * get_selected_sequence();
    bool get_selected_playing_state();


public:

    seqlist( perform *a_perf );
    ~seqlist();

};
