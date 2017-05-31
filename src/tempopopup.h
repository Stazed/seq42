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

#include <gtkmm.h>
#include <string>
#include "globals.h"

using namespace Gtk;

class tempo;


class tempopopup: public Gtk::Window
{
private:

    HBox *m_hbox;
    SpinButton  *m_spinbutton_bpm;
    Adjustment  *m_adjust_bpm;
    tempo * m_tempo;
    
    double m_BPM_value;
    bool m_escape;
    bool m_return;
    
    bool on_key_press_event(GdkEventKey* a_ev);
    void adj_callback_bpm();

public:

    tempopopup (tempo *a_tempo);
    
    void popup_tempo_win();
    
};


