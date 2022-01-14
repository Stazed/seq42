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

// GTK text edit widget for getting keyboard button values (for binding keys)
// put cursor in text box, hit a key, something like  'a' (42)  appears...
// each keypress replaces the previous text.
// also supports keyevent and keygroup maps in the perform class

#pragma once

_Pragma("GCC diagnostic push")
_Pragma("GCC diagnostic ignored \"-Wdeprecated-declarations\"")
#include <gtkmm/entry.h>
_Pragma("GCC diagnostic pop")

/*forward declaration*/
class perform;

class KeyBindEntry : public Gtk::Entry
{
public:
    KeyBindEntry( unsigned int* location_to_write = NULL,
                  perform* p = NULL, long s = 0 ) ;

    void set( unsigned int val );
    virtual bool on_key_press_event(GdkEventKey* event);

private:
    unsigned int* m_key;
    perform* m_perf;
};

