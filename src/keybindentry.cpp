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

#include "keybindentry.h"
#include "perform.h"



KeyBindEntry::KeyBindEntry( unsigned int* location_to_write,
                  perform* p, long s) :
        Entry(),
        m_key( location_to_write ),
        m_perf( p )
{
    if (m_key) set( *m_key );
}

void KeyBindEntry::set( unsigned int val )
{
    char buf[256] = "";
    char* special = gdk_keyval_name( val );
    char* p_buf = &buf[strlen(buf)];
    if (special)
        snprintf( p_buf, sizeof buf - (p_buf - buf), "%s", special );
    else
        snprintf( p_buf, sizeof buf - (p_buf - buf), "'%c'", (char)val );
    set_text( buf );
    int width = strlen(buf);
    set_width_chars( 1 <= width ? width : 1 );
}

bool KeyBindEntry::on_key_press_event(GdkEventKey* event)
{
    bool result = Entry::on_key_press_event( event );
    set( event->keyval );
    if (m_key)
    {
        *m_key = event->keyval;
        return true;
    }
    return result;
}
