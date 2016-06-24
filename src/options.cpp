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

#include "options.h"
#include "keybindentry.h"
#include <sstream>

// tooltip helper, for old vs new gtk...
#if GTK_MINOR_VERSION >= 12
#   define add_tooltip( obj, text ) obj->set_tooltip_text( text);
#else
#   define add_tooltip( obj, text ) m_tooltips->set_tip( *obj, text );
#endif

bool global_interaction_method_change = false;

options::options (Gtk::Window & parent, perform * a_p):
    Gtk::Dialog ("Options", parent, true, true),
    m_perf(a_p)
{
#if GTK_MINOR_VERSION < 12
    m_tooltips = manage(new Tooltips());
#endif

    HBox *hbox = manage (new HBox ());
    get_vbox ()->pack_start (*hbox, false, false);

    get_action_area ()->set_border_width (2);
    hbox->set_border_width (6);

    m_button_ok = manage (new Button ("Ok"));
    get_action_area ()->pack_end (*m_button_ok, false, false);
    m_button_ok->signal_clicked ().connect (mem_fun (*this, &options::hide));


    m_notebook = manage (new Notebook ());
    hbox->pack_start (*m_notebook);
    add_midi_clock_page();
    add_midi_input_page();
    add_keyboard_page();
    add_mouse_page();
    add_jack_sync_page();
}

/*MIDI Clock page*/
void
options::add_midi_clock_page()
{
    // Clock  Buses
    int buses = m_perf->get_master_midi_bus ()->get_num_out_buses ();

    VBox *vbox = manage(new VBox());
    vbox->set_border_width(6);
    m_notebook->append_page(*vbox, "MIDI _Clock", true);

    manage (new Tooltips ());

    for (int i = 0; i < buses; i++)
    {
        HBox *hbox2 = manage (new HBox ());
        Label *label = manage( new Label(m_perf->get_master_midi_bus ()->
                                         get_midi_out_bus_name (i), 0));

        hbox2->pack_start (*label, false, false);

        Gtk::RadioButton * rb_off = manage (new RadioButton ("Off"));
        add_tooltip( rb_off, "Midi Clock will be disabled.");

        Gtk::RadioButton * rb_on = manage (new RadioButton ("On (Pos)"));
        add_tooltip( rb_on,
                     "Midi Clock will be sent. Midi Song Position and Midi Continue will be sent if starting greater than tick 0 in song mode, otherwise Midi Start is sent.");

        Gtk::RadioButton * rb_mod = manage (new RadioButton ("On (Mod)"));
        add_tooltip( rb_mod, "Midi Clock will be sent.  Midi Start will be sent and clocking will begin once the song position has reached the modulo of the specified Size. (Used for gear that doesn't respond to Song Position)");

        Gtk::RadioButton::Group group = rb_off->get_group ();
        rb_on->set_group (group);
        rb_mod->set_group (group);

        rb_off->signal_toggled().connect (sigc::bind(mem_fun (*this,
                                          &options::clock_callback_off), i, rb_off ));
        rb_on->signal_toggled ().connect (sigc::bind(mem_fun (*this,
                                          &options::clock_callback_on),  i, rb_on  ));
        rb_mod->signal_toggled().connect (sigc::bind(mem_fun (*this,
                                          &options::clock_callback_mod), i, rb_mod ));

        hbox2->pack_end (*rb_mod, false, false );
        hbox2->pack_end (*rb_on, false, false);
        hbox2->pack_end (*rb_off, false, false);

        vbox->pack_start( *hbox2, false, false );

        switch ( m_perf->get_master_midi_bus ()->get_clock (i))
        {
        case e_clock_off:
            rb_off->set_active(1);
            break;
        case e_clock_pos:
            rb_on->set_active(1);
            break;
        case e_clock_mod:
            rb_mod->set_active(1);
            break;
        }
    }

    Adjustment *clock_mod_adj = new Adjustment(midibus::get_clock_mod(),
            1, 16 << 10, 1 );
    SpinButton *clock_mod_spin = new SpinButton( *clock_mod_adj );

    HBox *hbox2 = manage (new HBox ());

    hbox2->pack_start(*(manage(new Label(
                                   "Clock Start Modulo (1/16 Notes)"))), false, false, 4);
    hbox2->pack_start(*clock_mod_spin, false, false );

    vbox->pack_start( *hbox2, false, false );

    clock_mod_adj->signal_value_changed().connect(sigc::bind(mem_fun(*this,
            &options::clock_mod_callback), clock_mod_adj));
}

/*MIDI Input page*/
void
options::add_midi_input_page()
{
    // Input Buses
    int buses = m_perf->get_master_midi_bus ()->get_num_in_buses ();

    VBox *vbox = manage(new VBox ());
    vbox->set_border_width(6);
    m_notebook->append_page(*vbox, "MIDI _Input", true);

    for (int i = 0; i < buses; i++)
    {
        CheckButton *check = manage(new CheckButton(
                                        m_perf->get_master_midi_bus()->get_midi_in_bus_name(i), 0));
        check->signal_toggled().connect(bind(mem_fun(*this,
                                             &options::input_callback), i, check));
        check->set_active(m_perf->get_master_midi_bus()->get_input(i));

        vbox->pack_start(*check, false, false);
    }
}

/*Keyboard page*/
/*Keybinding setup (editor for .seq24rc keybindings).*/
void
options::add_keyboard_page()
{
    VBox *mainbox = manage(new VBox());
    mainbox->set_spacing(6);
    m_notebook->append_page(*mainbox, "_Keyboard", true);

    Label* label;
    KeyBindEntry* entry;

    /*Frame for sequence toggle keys*/
    Frame* controlframe = manage(new Frame("Control keys"));
    controlframe->set_border_width(4);
    mainbox->pack_start(*controlframe, Gtk::PACK_SHRINK);

    Table* controltable = manage(new Table(4, 8, false));
    controltable->set_border_width(4);
    controltable->set_spacings(4);
    controlframe->add(*controltable);

    label = manage(new Label("Start", Gtk::ALIGN_RIGHT));
    entry = manage(new KeyBindEntry(&m_perf->m_key_start));
    controltable->attach(*label, 0, 1, 0, 1);
    controltable->attach(*entry, 1, 2, 0, 1);

    label = manage(new Label("Stop", Gtk::ALIGN_RIGHT));
    entry = manage(new KeyBindEntry(&m_perf->m_key_stop));
    controltable->attach(*label, 0, 1, 1, 2);
    controltable->attach(*entry, 1, 2, 1, 2);

    label = manage(new Label("Rewind", Gtk::ALIGN_RIGHT));
    entry = manage(new KeyBindEntry(&m_perf->m_key_rewind));
    controltable->attach(*label, 0, 1, 2, 3);
    controltable->attach(*entry, 1, 2, 2, 3);

    label = manage(new Label("Forward", Gtk::ALIGN_RIGHT));
    entry = manage(new KeyBindEntry(&m_perf->m_key_forward));
    controltable->attach(*label, 0, 1, 3, 4);
    controltable->attach(*entry, 1, 2, 3, 4);

    label = manage(new Label("Pointer key", Gtk::ALIGN_RIGHT));
    entry = manage(new KeyBindEntry(&m_perf->m_key_pointer));
    controltable->attach(*label, 0, 1, 4, 5);
    controltable->attach(*entry, 1, 2, 4, 5);

    label = manage(new Label("looping", Gtk::ALIGN_RIGHT));
    entry = manage(new KeyBindEntry(&m_perf->m_key_loop));
    controltable->attach(*label, 0, 1, 5, 6);
    controltable->attach(*entry, 1, 2, 5, 6);

    label = manage(new Label("Song", Gtk::ALIGN_RIGHT));
    entry = manage(new KeyBindEntry(&m_perf->m_key_song));
    controltable->attach(*label, 2, 3, 0, 1);
    controltable->attach(*entry, 3, 4, 0, 1);

    label = manage(new Label("seqlist", Gtk::ALIGN_RIGHT));
    entry = manage(new KeyBindEntry(&m_perf->m_key_seqlist));
    controltable->attach(*label, 2, 3, 1, 2);
    controltable->attach(*entry, 3, 4, 1, 2);

    label = manage(new Label("transport", Gtk::ALIGN_RIGHT));
    entry = manage(new KeyBindEntry(&m_perf->m_key_follow_trans));
    controltable->attach(*label, 2, 3, 2, 3);
    controltable->attach(*entry, 3, 4, 2, 3);

    label = manage(new Label("bpm down", Gtk::ALIGN_RIGHT));
    entry = manage(new KeyBindEntry(&m_perf->m_key_bpm_dn));
    controltable->attach(*label, 2, 3, 3, 4);
    controltable->attach(*entry, 3, 4, 3, 4);

    label = manage(new Label("bpm up", Gtk::ALIGN_RIGHT));
    entry = manage(new KeyBindEntry(&m_perf->m_key_bpm_up));
    controltable->attach(*label, 2, 3, 4, 5);
    controltable->attach(*entry, 3, 4, 4, 5);

#ifdef JACK_SUPPORT
    label = manage(new Label("Jack", Gtk::ALIGN_RIGHT));
    entry = manage(new KeyBindEntry(&m_perf->m_key_jack));
    controltable->attach(*label, 2, 3, 5, 6);
    controltable->attach(*entry, 3, 4, 5, 6);
#endif // JACK_SUPPORT


}

/*Mouse page*/
void
options::add_mouse_page()
{
    VBox *vbox = manage(new VBox());
    m_notebook->append_page(*vbox, "_Mouse", true);

    /*Frame for transport options*/
    Frame* interactionframe = manage(new Frame("Interaction method"));
    interactionframe->set_border_width(4);
    vbox->pack_start(*interactionframe, Gtk::PACK_SHRINK);

    VBox *interactionbox = manage(new VBox());
    interactionbox->set_border_width(4);
    interactionframe->add(*interactionbox);

    Gtk::RadioButton *rb_seq42 = manage(new RadioButton(
                                            "se_q42 (original style)", true));
    interactionbox->pack_start(*rb_seq42, Gtk::PACK_SHRINK);

    Gtk::RadioButton * rb_fruity = manage(new RadioButton(
            "_fruity (similar to a certain well known sequencer)", true));
    interactionbox->pack_start(*rb_fruity, Gtk::PACK_SHRINK);

    Gtk::RadioButton::Group group = rb_seq42->get_group();
    rb_fruity->set_group(group);

    switch(global_interactionmethod)
    {
    case e_fruity_interaction:
        rb_fruity->set_active();
        break;

    case e_seq42_interaction:
    default:
        rb_seq42->set_active();
        break;
    }

    rb_seq42->signal_toggled().connect(sigc::bind(mem_fun(*this,
                                       &options::mouse_seq42_callback), rb_seq42));

    rb_fruity->signal_toggled().connect(sigc::bind(mem_fun(*this,
                                        &options::mouse_fruity_callback), rb_fruity));
}

/*Jack Sync page */
void
options::add_jack_sync_page()
{
#ifdef JACK_SUPPORT
    VBox *vbox = manage(new VBox());
    vbox->set_border_width(4);
    m_notebook->append_page(*vbox, "_Jack Sync", true);

    /*Frame for transport options*/
    Frame* transportframe = manage(new Frame("Transport"));
    transportframe->set_border_width(4);
    vbox->pack_start(*transportframe, Gtk::PACK_SHRINK);

    VBox *transportbox = manage(new VBox());
    transportbox->set_border_width(4);
    transportframe->add(*transportbox);

    CheckButton *check = manage(new CheckButton("Jack _Transport", true));
    check->set_active (global_with_jack_transport);
    add_tooltip( check, "Enable sync with JACK Transport.");
    check->signal_toggled().connect(bind(mem_fun(*this,
                                         &options::transport_callback), e_jack_transport, check));

    if(global_is_running)
        check->set_sensitive(false);

    transportbox->pack_start(*check, false, false);

    check = manage(new CheckButton("Trans_port Master", true));
    check->set_active (global_with_jack_master);
    add_tooltip( check, "Seq24 will attempt to serve as JACK Master.");
    check->signal_toggled().connect(bind(mem_fun(*this,
                                         &options::transport_callback), e_jack_master, check));

    if(global_is_running)
        check->set_sensitive(false);

    transportbox->pack_start(*check, false, false);

    check = manage (new CheckButton ("Master C_onditional", true));
    check->set_active (global_with_jack_master_cond);
    add_tooltip( check,
                 "Seq24 will fail to be master if there is already a master set.");
    check->signal_toggled().connect(bind(mem_fun(*this,
                                         &options::transport_callback), e_jack_master_cond, check));

    if(global_is_running)
        check->set_sensitive(false);

    transportbox->pack_start(*check, false, false);

#endif
}

void
options::clock_callback_off (int a_bus, RadioButton *a_button)
{
    if (a_button->get_active ())
        m_perf->get_master_midi_bus ()->set_clock(a_bus, e_clock_off );
}

void
options::clock_callback_on (int a_bus, RadioButton *a_button)
{
    if (a_button->get_active ())
        m_perf->get_master_midi_bus ()->set_clock(a_bus, e_clock_pos );
}

void
options::clock_callback_mod (int a_bus, RadioButton *a_button)
{
    if (a_button->get_active ())
        m_perf->get_master_midi_bus ()->set_clock(a_bus, e_clock_mod );
}

void
options::clock_mod_callback( Adjustment *adj )
{
    midibus::set_clock_mod((int)adj->get_value());
}

void
options::input_callback (int a_bus, Button * i_button)
{
    CheckButton *a_button = (CheckButton *) i_button;
    bool input = a_button->get_active ();
    m_perf->get_master_midi_bus ()->set_input (a_bus, input);
}

void
options::mouse_seq42_callback(Gtk::RadioButton *btn)
{
    if (btn->get_active())
    {
        global_interactionmethod = e_seq42_interaction;
        global_interaction_method_change = true;
    }
}

void
options::mouse_fruity_callback(Gtk::RadioButton *btn)
{
    if (btn->get_active())
        global_interactionmethod = e_fruity_interaction;
}

void
options::transport_callback (button a_type, Button * a_check)
{
    CheckButton *check = (CheckButton *) a_check;

    switch (a_type)
    {
    case e_jack_transport:
    {
        global_with_jack_transport = check->get_active ();
    }
    break;
    case e_jack_master:
    {
        global_with_jack_master = check->get_active ();
    }
    break;
    case e_jack_master_cond:
    {
        global_with_jack_master_cond = check->get_active ();
    }
    break;
    default:
        break;
    }
}
