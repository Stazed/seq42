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

#include "globals.h"
#include "perform.h"

using namespace Gtk;

class options : public Gtk::Dialog
{

private:

    perform *m_perf;

    Button  *m_button_ok;
    Label* interaction_method_label;
    Label* interaction_method_desc_label;

    Notebook *m_notebook;

    enum button
    {
        e_jack_transport,
        e_jack_master,
        e_jack_master_cond
    };

    void clock_callback_off( int a_bus, RadioButton *a_button );
    void clock_callback_on ( int a_bus, RadioButton *a_button );
    void clock_callback_mod( int a_bus, RadioButton *a_button );

    void clock_mod_callback( Glib::RefPtr<Gtk::Adjustment> adj );

    void input_callback( int a_bus, Button *a_button );

    void transport_callback( button a_type, Button *a_button );

    void mouse_seq42_callback(Gtk::RadioButton*);
    void mouse_fruity_callback(Gtk::RadioButton*);
    /*notebook pages*/
    void add_midi_clock_page();
    void add_midi_input_page();
    void add_keyboard_page();
    void add_mouse_page();
    void add_jack_sync_page();

public:

    options( Gtk::Window &parent, perform *a_p );
};

