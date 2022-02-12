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

#   define add_tooltip( obj, text ) obj->set_tooltip_text( text);

tempopopup::tempopopup(tempo *a_tempo) :
    m_tempo(a_tempo),
    m_escape(false),
    m_return(false),
    m_is_typing(false),
    m_current_beats(0),
    m_base_time_ms(0),
    m_last_time_ms(0)
{
 //   std::string title = "BPM";
 //   set_title(title);
    set_size_request(150, 50);

    /* bpm spin button */
#ifdef GTKMM_3_SUPPORT

#else
    m_adjust_bpm = manage(new Adjustment(m_tempo->m_mainperf->get_bpm(), c_bpm_minimum -1, c_bpm_maximum, 1));
    m_spinbutton_bpm = manage( new SpinButton( *m_adjust_bpm ));
    m_spinbutton_bpm->set_editable( true );
    m_spinbutton_bpm->set_digits(2);                             // 2 = two decimal precision
    m_adjust_bpm->signal_value_changed().connect(
        mem_fun(*this, &tempopopup::adj_callback_bpm ));
 
    m_spinbutton_bpm->set_numeric();
#endif
    
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
    
    /* bpm tap tempo button - sequencer64 */
    m_button_tap = manage(new Button("0"));
    m_button_tap->signal_clicked().connect(sigc::mem_fun(*this, &tempopopup::tap));
    add_tooltip
    (
        m_button_tap,
        "Tap in time to set the beats per minute (BPM) value. "
        "After 5 seconds of no taps, the tap-counter will reset to 0. "
        "Also see the File / Options / Keyboard / Tap BPM key assignment."
    );
    m_button_tap->set_can_focus(false);  // to force all keys to the spin button
    
    HBox *hbox = manage(new HBox());
    hbox->set_border_width(6);
    
    hbox->pack_start(*bpmlabel, Gtk::PACK_SHRINK);
    hbox->pack_start(*m_spinbutton_bpm, Gtk::PACK_SHRINK );
    hbox->pack_start(*m_button_tap, Gtk::PACK_SHRINK, 5 );
    
    add(*hbox);
    set_modal();                            // keep focus until done
    set_transient_for(*m_tempo->m_mainwnd); // always on top
    set_decorated(false);                   // don't show title bar
    
    add_events( Gdk::KEY_PRESS_MASK | Gdk::KEY_RELEASE_MASK );
    
    m_timeout_connect = Glib::signal_timeout().connect(
                        mem_fun(*this, &tempopopup::timer_callback), 25);
}

bool
tempopopup::timer_callback(  )
{
    /* Tap button - sequencer64 */
    if (m_current_beats > 0)
    {
        if (m_last_time_ms > 0)
        {
            struct timespec spec;
            clock_gettime(CLOCK_REALTIME, &spec);
            long ms = long(spec.tv_sec) * 1000;     /* seconds to ms        */
            ms += round(spec.tv_nsec * 1.0e-6);     /* nanoseconds to ms    */
            long difference = ms - m_last_time_ms;
            if (difference > 5000L)                 /* 5 second wait        */
            {
                m_current_beats = m_base_time_ms = m_last_time_ms = 0;
                set_tap_button(0);
            }
        }
    }
    return true;
}

void
tempopopup::adj_callback_bpm()
{
    if(!m_escape)
    {
        if(m_return)
        {
            if(!m_is_typing)    // first typed is old value so we discard
            {
                m_tempo->set_BPM(m_adjust_bpm->get_value());
            }
            hide();
        }
    }

    m_is_typing = false;        // using spinner & to keep typed second value
}

bool
tempopopup::on_key_press_event( GdkEventKey* a_ev )
{
    if ( a_ev->keyval == m_tempo->m_mainperf->m_key_bpm_dn )
    {
        m_adjust_bpm->set_value(m_adjust_bpm->get_value() - 1);
        return true;
    }
    if ( a_ev->keyval ==  m_tempo->m_mainperf->m_key_bpm_up )
    {
        m_adjust_bpm->set_value(m_adjust_bpm->get_value() + 1);
        return true;
    }
    
    if (a_ev->keyval  == m_tempo->m_mainperf->m_key_tap_bpm )
    {
        tap();
        return true;
    }
    
    if (a_ev->keyval == GDK_KEY_Escape)
    {
        m_escape = true;
        hide();
        return true;
    }
    
    if (a_ev->keyval == GDK_KEY_Return || a_ev->keyval == GDK_KEY_KP_Enter)
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
         NOT updated amount to m_tempo->set_BPM. The m_is_typing bool is set any
         time a keypress is use to eliminate the dupicate.  */
        m_return = true;
        bool is_typing = m_is_typing;   // must check before callback since cb will set to false
        
        adj_callback_bpm();
        
        if(is_typing)                   // must set return = true or we get nothing if typed...??
            return true;
    }
    m_is_typing = true;
    
    return Gtk::Window::on_key_press_event(a_ev);
}

void 
tempopopup::popup_tempo_win()
{
    m_return = false;
    m_escape = false;
    m_is_typing = false;
    m_spinbutton_bpm->set_value(m_tempo->m_mainperf->get_bpm());    // set to default starting bpm
    m_spinbutton_bpm->select_region(0,-1);                          // select all for easy typing replacement
    m_spinbutton_bpm->grab_focus();
    show_all();
    raise();
}


void
tempopopup::tap ()
{
    m_is_typing = false;
    double bpm = update_bpm();
    set_tap_button(m_current_beats);
    if (m_current_beats > 1)                    /* first one is useless */
        m_adjust_bpm->set_value(double(bpm));
}

void
tempopopup::set_tap_button (int beats)
{
    Gtk::Label * tapptr(dynamic_cast<Gtk::Label *>(m_button_tap->get_child()));
    if (tapptr != nullptr)
    {
        char temp[8];
        snprintf(temp, sizeof(temp), "%d", beats);
        tapptr->set_text(temp);
    }
}

double
tempopopup::update_bpm ()
{
    double bpm = 0.0;
    struct timespec spec;
    clock_gettime(CLOCK_REALTIME, &spec);
    long ms = long(spec.tv_sec) * 1000;     /* seconds to milliseconds      */
    ms += round(spec.tv_nsec * 1.0e-6);     /* nanoseconds to milliseconds  */
    if (m_current_beats == 0)
    {
        m_base_time_ms = ms;
        m_last_time_ms = 0;
    }
    else if (m_current_beats >= 1)
    {
        int diffms = ms - m_base_time_ms;
        bpm = m_current_beats * 60000.0 / diffms;
        m_last_time_ms = ms;
    }
    ++m_current_beats;
    return bpm;
}
