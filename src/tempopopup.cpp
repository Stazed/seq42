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

#include "tempopopup.h"
#include "tempo.h"

// tooltip helper, for old vs new gtk...
#if GTK_MINOR_VERSION >= 12
#   define add_tooltip( obj, text ) obj->set_tooltip_text( text);
#else
#   define add_tooltip( obj, text ) m_tooltips->set_tip( *obj, text );
#endif


tempopopup::tempopopup(tempo *a_tempo) :
    m_tempo(a_tempo),
    m_BPM_value(-1),
    m_escape(false),
    m_return(false)
{
 //   std::string title = "BPM";
 //   set_title(title);
    set_size_request(120, 50);
    
    manage (new Tooltips ());
    
    /* bpm spin button */
    m_adjust_bpm = manage(new Adjustment(m_tempo->m_mainperf->get_bpm(), c_bpm_minimum -1, c_bpm_maximum, 1));
    m_spinbutton_bpm = manage( new SpinButton( *m_adjust_bpm ));
    m_spinbutton_bpm->set_editable( true );
    m_spinbutton_bpm->set_digits(2);                             // 2 = two decimal precision
    m_adjust_bpm->signal_value_changed().connect(
        mem_fun(*this, &tempopopup::adj_callback_bpm ));
 
    m_spinbutton_bpm->set_numeric();
    
    add_tooltip
    ( 
        m_spinbutton_bpm,
        "Adjust beats per minute (BPM) value.\n"
        "Values range from 0 to 600.00.\n"
        "Escape to leave without setting\n"
        "Entry of 0 indicates a STOP marker.\n"
    );
    
    m_spinbutton_bpm->set_update_policy(Gtk::UPDATE_IF_VALID);  // ignore bad entries
    
    Label* bpmlabel = manage(new Label("BPM"));
    
    HBox *hbox = manage(new HBox());
    hbox->set_border_width(6);
    
    hbox->pack_start(*bpmlabel, Gtk::PACK_SHRINK);
    hbox->pack_start(*m_spinbutton_bpm, Gtk::PACK_SHRINK );
    
    add(*hbox);
    set_modal();                            // keep focus until done
    set_transient_for(*m_tempo->m_mainwnd); // always on top
    set_decorated(false);                   // don't show title bar
}

void
tempopopup::adj_callback_bpm()
{
    if(!m_escape)
    {
        m_BPM_value = m_adjust_bpm->get_value();
        if(m_return)
        {
            m_tempo->set_BPM(m_adjust_bpm->get_value());
            global_is_modified = true;
            hide();
        }
    }
}


bool
tempopopup::on_key_press_event( GdkEventKey* a_ev )
{
    if (a_ev->keyval == GDK_Escape)
    {
        m_escape = true;
        hide();
        return true;
    }
    
    if (a_ev->keyval == GDK_Return || a_ev->keyval == GDK_KP_Enter)
    {
        // Typed value sends 2 callbacks - really ugly
        /* The desired behavior is to only set the marker after 'return' or 'enter'
         is pressed. Below works if the get_value() amount is updated, as in the case
         of accepting previous amount or when the user used the spin adjusters.
         In the case of a typed amount, the get_value() amount does not get updated
         until AFTER a return is pressed and received by the spinbutton. Also the 
         amount gets updated when hide() is called on the window. The hide() update
         then triggers the adj_callback_bpm() with the correct amount. But the below manual
         call to adj_callback_bpm() is called first and sends the incorrect old
         NOT updated amount to m_tempo->set_BPM. We fix this duplicate in add_marker()
         by eliminating previous markers set at the same tick location.
         I cannot find any way of preventing the duplicate send without breaking
         either accepting previous amounts, or using the spin adjusters. It's ugly
         but it seems to work... Hopefully someone else can find a more elegant
         solution.   */
        m_return = true;
        adj_callback_bpm();
        return true;
    }
    
    return Gtk::Window::on_key_press_event(a_ev);
}

void 
tempopopup::popup_tempo_win()
{
    m_return = false;
    m_escape = false;
    m_spinbutton_bpm->set_value(m_tempo->m_mainperf->get_bpm());    // set to default starting bpm
    m_spinbutton_bpm->select_region(0,-1);                          // select all for easy typing replacement
    show_all();
    raise();
}