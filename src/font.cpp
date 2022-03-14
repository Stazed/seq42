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

#include <string>

#include "font.h"
#include "string.h"

#include "pixmaps/font_white.xpm"
#include "pixmaps/font_black.xpm"


font::font( )
{
    init();
}

void
font::init( )
{
    m_black_pixbuf = Gdk::Pixbuf::create_from_xpm_data(font_black_xpm);
    m_white_pixbuf = Gdk::Pixbuf::create_from_xpm_data(font_white_xpm);
}

void
font::render_string_on_drawable(
    int x, int y,
    Cairo::RefPtr<Cairo::Context> cr,
    const char *str,
    font::Color col )
{
    int length = 0;

    if ( str != NULL )
        length = strlen(str);

    int font_w = 6;
    int font_h = 10;

    for( int i = 0; i < length; ++i )
    {
        unsigned char c = (unsigned char) str[i];

        // solid
        //int pixbuf_index_x = 2;
        //int pixbuf_index_y = 0;

        int pixbuf_index_x = c % 16;
        int pixbuf_index_y = c / 16;

        pixbuf_index_x *= 9;
        pixbuf_index_y *= 13;

        pixbuf_index_x += 2;
        pixbuf_index_y += 2;

        if ( col == font::BLACK )
            m_pixbuf = m_black_pixbuf;
        if ( col == font::WHITE )
            m_pixbuf = m_white_pixbuf;

        Glib::RefPtr<Gdk::Pixbuf> a_pixbuf = Gdk::Pixbuf::create_subpixbuf(m_pixbuf, pixbuf_index_x, pixbuf_index_y , font_w, font_h);
        
        Gdk::Cairo::set_source_pixbuf (cr, a_pixbuf, x + (i*font_w), y);
        
        cr->paint();
    }
}
