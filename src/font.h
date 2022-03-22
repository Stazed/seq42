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

#pragma once

#include "globals.h"

#include <gtkmm/image.h>
#include <gtkmm/widget.h>
#include <gtkmm/drawingarea.h>

#include <string>

using namespace Gtk;

const int c_font_width = 6;
const int c_font_height = 10;

class font
{

private:

    Glib::RefPtr<Gdk::Pixbuf>   m_pixbuf;
    Glib::RefPtr<Gdk::Pixbuf>   m_black_pixbuf;
    Glib::RefPtr<Gdk::Pixbuf>   m_white_pixbuf;

public:

    enum Color
    {
        BLACK = 0,
        WHITE = 1
    };

    font( );

    void init( );

    void render_string_on_drawable(
        int x,
        int y,
        Cairo::RefPtr<Cairo::Context> cr,
        const char *str,
        font::Color col,
        float scale_x = c_default_horizontal_zoom,
        float scale_y = c_default_vertical_zoom);

};

extern font *p_font_renderer;
