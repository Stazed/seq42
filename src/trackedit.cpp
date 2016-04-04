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

#include "trackedit.h"
#include "pixmaps/bus.xpm"
#include "pixmaps/midi.xpm"

// tooltip helper, for old vs new gtk...
#if GTK_MINOR_VERSION >= 12
#   define add_tooltip( obj, text ) obj->set_tooltip_text( text);
#else
#   define add_tooltip( obj, text ) tooltips->set_tip( *obj, text );
#endif

trackedit::trackedit (track *a_track)
{
    // FIXME create an icon?
    // set_icon(Gdk::Pixbuf::create_from_xpm_data(track_editor_xpm));

    m_track = a_track;

    /* main window */
    std::string title = "seq42 - track - ";
    title.append(m_track->get_name());
    set_title(title);
    set_size_request(400, 120);

    m_track->set_editing(true);

    m_tooltips = manage( new Tooltips( ) );
    m_vbox = manage( new VBox( false, 2 ));
    m_vbox->set_border_width( 2 );

    m_hbox = manage (new HBox ());
    m_hbox->set_border_width (6);
    m_vbox->pack_start (*m_hbox, false, false);

    m_label_name =  manage (new Label( "Name: " ));
    m_hbox->pack_start( *m_label_name , false, false );
    m_entry_name = manage( new Entry(  ));
    m_entry_name->set_max_length(c_max_track_name);
    m_entry_name->set_width_chars(c_max_track_name);
    m_entry_name->set_text( m_track->get_name());
    m_entry_name->select_region(0,0);
    m_entry_name->set_position(0);
    m_entry_name->signal_changed().connect(
    mem_fun( *this, &trackedit::name_change_callback));
    m_hbox->pack_start( *m_entry_name, true, true );

    m_hbox2 = manage (new HBox ());
    m_hbox2->set_border_width (6);
    m_vbox->pack_start (*m_hbox2, false, false);

    /* midi bus */
    m_button_bus = manage( new Button());
    m_button_bus->add( *manage( new Image(Gdk::Pixbuf::create_from_xpm_data( bus_xpm  ))));
    m_button_bus->signal_clicked().connect( mem_fun( *this, &trackedit::popup_midibus_menu));
    m_tooltips->set_tip( *m_button_bus, "Select Output Bus." );

    m_entry_bus = manage( new Entry());
    m_entry_bus->set_max_length(60);
    m_entry_bus->set_width_chars(60);
    m_entry_bus->set_editable( false );

    m_hbox2->pack_start( *m_button_bus , false, false );
    m_hbox2->pack_start( *m_entry_bus , true, true );

    /* midi channel */
    m_button_channel = manage( new Button());
    m_button_channel->add( *manage( new Image(Gdk::Pixbuf::create_from_xpm_data( midi_xpm  ))));
    m_button_channel->signal_clicked().connect(  mem_fun( *this, &trackedit::popup_midich_menu ));
    m_tooltips->set_tip( *m_button_channel, "Select Midi channel." );
    m_entry_channel = manage( new Entry());
    m_entry_channel->set_width_chars(2);
    m_entry_channel->set_editable( false );

    m_hbox2->pack_start( *m_button_channel , false, false );
    m_hbox2->pack_start( *m_entry_channel , false, false );


    m_hbox3 = manage (new HBox ());
    m_hbox3->set_border_width (6);
    m_vbox->pack_start (*m_hbox3, false, false);

    m_check_transposable = manage( new CheckButton( "Transposable" ));
    m_check_transposable->set_active(m_track->get_transposable());
    m_check_transposable->signal_toggled ().
        connect (bind
                (mem_fun (*this, &trackedit::transposable_change_callback),
                 m_check_transposable));
    m_hbox3->pack_start( *m_check_transposable, false, false );

    this->add( *m_vbox );
    show_all();

    set_midi_channel( m_track->get_midi_channel() );
    set_midi_bus( m_track->get_midi_bus() );

}

trackedit::~trackedit()
{
    //m_track->set_editing( false );
}


bool
trackedit::on_delete_event(GdkEventAny *a_event)
{
    //printf( "trackedit::on_delete_event()\n" );
    m_track->set_editing( false );
    delete this;
    m_track = NULL;
    return false;
}

void
trackedit::on_realize()
{
    // we need to do the default realize
    Gtk::Window::on_realize();
    Glib::signal_timeout().connect(mem_fun(*this, &trackedit::timeout),
            c_redraw_ms);
}


bool
trackedit::timeout()
{

    if (m_track->get_raise())
    {
        m_track->set_raise(false);
        raise();
    }

    return true;
}

void
trackedit::name_change_callback ()
{
    m_track->set_name( m_entry_name->get_text());
    global_is_modified = true;
}

void
trackedit::popup_midibus_menu()
{
    using namespace Menu_Helpers;

    m_menu_midibus = manage( new Menu());

    /* midi buses */
    mastermidibus *masterbus = m_track->get_master_midi_bus();
    for ( int i=0; i< masterbus->get_num_out_buses(); i++ ){
        m_menu_midibus->items().push_back(MenuElem(masterbus->get_midi_out_bus_name(i),
                                                   sigc::bind(mem_fun(*this,&trackedit::midi_bus_button_callback), i)));
    }

    m_menu_midibus->popup(0,0);
}

void
trackedit::popup_midich_menu()
{
    using namespace Menu_Helpers;

    m_menu_midich = manage( new Menu());

    int midi_bus = m_track->get_midi_bus();

    char b[16];

    /* midi channel menu */
    for( int i=0; i<16; i++ ){

        sprintf( b, "%d", i+1 );
        std::string name = string(b);
        int instrument = global_user_midi_bus_definitions[midi_bus].instrument[i];
        if ( instrument >= 0 && instrument < c_maxBuses )
        {
            name = name + (string(" (") +
                           global_user_instrument_definitions[instrument].instrument +
                           string(")") );
        }
        m_menu_midich->items().push_back(MenuElem(name,
                                                  sigc::bind(mem_fun(*this,&trackedit::midi_channel_button_callback),
                                                       i )));
    }

    m_menu_midich->popup(0,0);
}

void
trackedit::midi_channel_button_callback( int a_midichannel )
{
    if(m_track->get_midi_channel() != a_midichannel)
    {
        set_midi_channel(a_midichannel);
        global_is_modified = true;
    }
}

void
trackedit::set_midi_channel( int a_midichannel  )
{
    char b[10];
    sprintf( b, "%d", a_midichannel+1 );
    m_entry_channel->set_text(b);
    m_track->set_midi_channel( a_midichannel );
}

void
trackedit::midi_bus_button_callback( int a_midibus )
{
    if(m_track->get_midi_bus() != a_midibus)
    {
        set_midi_bus(a_midibus);
        global_is_modified = true;
    }
}

void
trackedit::set_midi_bus( int a_midibus )
{
    m_track->set_midi_bus( a_midibus );
    mastermidibus *mmb =  m_track->get_master_midi_bus();
    m_entry_bus->set_text( mmb->get_midi_out_bus_name( a_midibus ));
}

void
trackedit::transposable_change_callback(CheckButton *a_button)
{
    m_track->set_transposable(a_button->get_active());
    global_is_modified = true;
}
