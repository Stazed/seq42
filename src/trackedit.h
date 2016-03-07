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

#include <gtkmm/button.h>
#include <gtkmm/window.h>
#include <gtkmm/accelgroup.h>
#include <gtkmm/box.h>
#include <gtkmm/main.h>
#include <gtkmm/window.h>
#include <gtkmm/dialog.h>
#include <gtkmm/table.h>
#include <gtkmm/label.h>
#include <gtkmm/entry.h>
#include <gtkmm/tooltips.h>
#include <gtkmm/checkbutton.h>

#include <sigc++/bind.h>

#include "globals.h"
#include "track.h"

using namespace Gtk;



class trackedit : public Gtk::Window
{

 private:

    track *m_track;

    Tooltips *m_tooltips;
    VBox *m_vbox;
    HBox *m_hbox;
    HBox *m_hbox2;
    HBox *m_hbox3;
    Label *m_label_name;
    Entry *m_entry_name;
    Menu *m_menu_midich;
    Menu *m_menu_midibus;
    Button *m_button_bus;
    Entry  *m_entry_bus;
    Button *m_button_channel;
    Entry  *m_entry_channel;
    CheckButton *m_check_transposable;

    void name_change_callback();
    void set_midi_channel( int a_midichannel );
    void set_midi_bus( int a_midibus );
    void popup_midibus_menu();
    void popup_midich_menu();

    void transposable_change_callback(CheckButton *a_button);

    void on_realize();
    bool timeout();

public:

    trackedit( track *a_track );
    ~trackedit();

    bool on_delete_event(GdkEventAny *a_event);

};
