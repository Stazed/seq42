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

#include "tempo_popup.h"
#include "tempo.h"

tempo_popup::~tempo_popup()
{
    
}

tempo_popup::tempo_popup(tempo *a_tempo) :
    m_tempo(a_tempo),
    m_escape(false)
{
    std::string title = "BPM";
    set_title(title);
    set_size_request(150, 50);
    
 //   manage (new Tooltips ());
    
    /* bpm spin button */
    //m_adjust_bpm = manage(new Adjustment(m_mainperf->get_bpm(), c_bpm_minimum, c_bpm_maximum, 1));
    m_adjust_bpm = manage(new Adjustment(120.0, c_bpm_minimum, c_bpm_maximum, 1)); // FIXME starting value
    m_spinbutton_bpm = manage( new SpinButton( *m_adjust_bpm ));
    m_spinbutton_bpm->set_editable( true );
    m_spinbutton_bpm->set_digits(2);                    // 2 = two decimal precision
    m_adjust_bpm->signal_value_changed().connect(
        mem_fun(*this, &tempo_popup::adj_callback_bpm ));   
    
    m_spinbutton_bpm->set_update_policy(Gtk::UPDATE_IF_VALID);
    
    HBox *hbox = manage(new HBox());
    hbox->set_border_width(6);
    
    hbox->pack_start(*m_spinbutton_bpm, true, false );
    
    add(*hbox);
    set_modal();
    //set_decorated(false);
    show_all();
    raise();
    
}

void
tempo_popup::adj_callback_bpm()
{
    if(!m_escape)
    {
        m_tempo->set_BPM(m_adjust_bpm->get_value());
        global_is_modified = true;
    }

    hide();
}


bool
tempo_popup::on_key_press_event( GdkEventKey* a_ev )
{
    if (a_ev->keyval == GDK_Escape)
    {
        m_escape = true;
        hide();
    }
    
    if (a_ev->keyval == GDK_Return)
    {
        adj_callback_bpm();
    }
 
    return Gtk::Window::on_key_press_event(a_ev);
}

bool
tempo_popup::on_delete_event(GdkEventAny *a_event)
{
    delete this;
    return false;
}
