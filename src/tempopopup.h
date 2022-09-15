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


/* 
 * File:   tempopopup.h
 * Author: sspresto
 *
 * Created on May 29, 2017, 1:36 PM
 */

#pragma once

#include <string>
#include "globals.h"

using namespace Gtk;

class tempo;


class tempopopup: public Gtk::Window
{
private:

    HBox *m_hbox;
    SpinButton  *m_spinbutton_bpm;

    Glib::RefPtr<Adjustment> m_adjust_bpm;

    Button      *m_button_tap;
    tempo       *m_tempo;

    sigc::connection   m_timeout_connect;
    
    bool m_escape;
    bool m_return;
    bool m_is_typing;
    
    /* tap button - From sequencer64 */
    int m_current_beats; // value is displayed in the button.
    long m_base_time_ms; // Indicates the first time the tap button was ... tapped.
    long m_last_time_ms; // Indicates the last time the tap button was tapped.
    
    bool on_key_press_event(GdkEventKey* a_ev);
    void adj_callback_bpm();
    bool timer_callback( );

public:

    explicit tempopopup (tempo *a_tempo);
    
    void popup_tempo_win(int x, int y);
    
    /* From  sequencer64 tap button */
    void tap ();
    void set_tap_button (int beats);
    double update_bpm ();
    
};


