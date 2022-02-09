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
#include "event.h"
#include "seqtime.h"
#include "font.h"

seqtime::seqtime(sequence *a_seq, int a_zoom,
                 Gtk::Adjustment   *a_hadjust):
    m_hadjust(a_hadjust),
    m_scroll_offset_ticks(0),
    m_scroll_offset_x(0),
    m_seq(a_seq),
    m_zoom(a_zoom)
{
    add_events( Gdk::BUTTON_PRESS_MASK |
                Gdk::BUTTON_RELEASE_MASK );

    /* set default size */
    set_size_request( 10, c_timearea_y );

    set_double_buffered( false );
}

void
seqtime::update_sizes()
{
    /* set these for later */
    if( get_realized() )
    {
        m_pixmap = Gdk::Pixmap::create( m_window,
                                        m_window_x,
                                        m_window_y, -1 );
        update_pixmap();
        queue_draw();
    }
}

void
seqtime::on_realize()
{
    // we need to do the default realize
    Gtk::DrawingArea::on_realize();

    //Gtk::Main::idle.connect(mem_fun(this,&seqtime::idleProgress));
    Glib::signal_timeout().connect(mem_fun(*this,&seqtime::idle_progress), 50);

    // Now we can allocate any additional resources we need
    m_window = get_window();
    m_window->clear();

    m_hadjust->signal_value_changed().connect( mem_fun( *this, &seqtime::change_horz ));

    update_sizes();
}

void
seqtime::change_horz( )
{
    m_scroll_offset_ticks = (int) m_hadjust->get_value();
    m_scroll_offset_x = m_scroll_offset_ticks / m_zoom;

    update_pixmap();
    force_draw();
}

void
seqtime::on_size_allocate(Gtk::Allocation & a_r )
{
    Gtk::DrawingArea::on_size_allocate( a_r );

    m_window_x = a_r.get_width();
    m_window_y = a_r.get_height();

    update_sizes();
}

bool
seqtime::idle_progress( )
{
    return true;
}

void
seqtime::set_zoom( int a_zoom )
{
    if ( m_zoom != a_zoom )
    {
        m_zoom = a_zoom;
        reset();
    }
}

void
seqtime::reset()
{
    m_scroll_offset_ticks = (int) m_hadjust->get_value();
    m_scroll_offset_x = m_scroll_offset_ticks / m_zoom;

    update_sizes();
    update_pixmap();
    draw_pixmap_on_window();
}

void
seqtime::redraw()
{
    m_scroll_offset_ticks = (int) m_hadjust->get_value();
    m_scroll_offset_x = m_scroll_offset_ticks / m_zoom;

    update_pixmap();
    draw_pixmap_on_window();
}

void
seqtime::update_pixmap()
{
    /* clear background */
    cairo_t *cr = gdk_cairo_create (m_pixmap->gobj());
    cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);    // White FIXME
    cairo_set_line_width(cr, 1.0);
    cairo_rectangle(cr, 0.0, 0.0, m_window_x, m_window_y );
    cairo_stroke_preserve(cr);
    cairo_fill(cr);
    
    cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);    // Black FIXME
    cairo_move_to(cr, 0.0, m_window_y - 1);
    cairo_line_to(cr, m_window_x,  m_window_y - 1 );
    cairo_stroke(cr);

    // at 32, a bar every measure
    // at 16
    /*
        zoom   32         16         8        4        1


        ml
        c_ppqn
        *
        1      128
        2      64
        4      32        16         8
        8      16m       8          4          2       1
        16     8m        4          2          1       1
        32     4m        2          1          1       1
        64     2m        1          1          1       1
        128    1m        1          1          1       1
    */

    int measure_length_32nds =  m_seq->get_bp_measure() * 32 /
                                m_seq->get_bw();

    //printf ( "measure_length_32nds[%d]\n", measure_length_32nds );

    int measures_per_line = (128 / measure_length_32nds) / (32 / m_zoom);
    if ( measures_per_line <= 0 )
        measures_per_line = 1;

    //printf( "measures_per_line[%d]\n", measures_per_line );

    int ticks_per_measure =  m_seq->get_bp_measure() * (4 * c_ppqn) / m_seq->get_bw();
    int ticks_per_step =  ticks_per_measure * measures_per_line;
    int start_tick = m_scroll_offset_ticks - (m_scroll_offset_ticks % ticks_per_step );
    int end_tick = (m_window_x * m_zoom) + m_scroll_offset_ticks;

    //printf ( "ticks_per_step[%d] start_tick[%d] end_tick[%d]\n",
    //         ticks_per_step, start_tick, end_tick );

    /* draw vertical lines */
    for ( int i = start_tick; i < end_tick; i += ticks_per_step )
    {
        int base_line = i / m_zoom;

        /* beat */
        cairo_move_to(cr, base_line -  m_scroll_offset_x, 0);
        cairo_line_to(cr, base_line -  m_scroll_offset_x, m_window_y );
        cairo_stroke(cr);

        /* The bar numbers */
        char bar[16];
        snprintf(bar, sizeof(bar), "%d", (i/ ticks_per_measure ) + 1);

        cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
        cairo_set_font_size(cr, 9.0);
        cairo_move_to(cr, base_line + 2 -  m_scroll_offset_x, 7);
        cairo_show_text( cr, bar);
    }

    long end_x = m_seq->get_length() / m_zoom - m_scroll_offset_x;

    // set background for label to black
    cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);    // Black FIXME

    // draw the black background for the 'END' label
    cairo_rectangle(cr, end_x, 9, 22, 8 );
    cairo_stroke_preserve(cr);
    cairo_fill(cr);

    // print the 'END' label in white
    cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);    // White FIXME
    cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cr, 9.0);
    cairo_move_to(cr, end_x, 16);
    cairo_show_text( cr, "END");

    cairo_destroy(cr);
}

void
seqtime::draw_pixmap_on_window()
{
    cairo_t *cr = gdk_cairo_create (m_window->gobj());
    gdk_cairo_set_source_pixmap(cr, m_pixmap->gobj(), 0.0, 0.0);
    cairo_rectangle(cr, 0.0, 0.0, m_window_x, m_window_y);
    cairo_fill(cr);

    cairo_destroy(cr);
}

bool
seqtime::on_expose_event(GdkEventExpose* a_e)
{
    cairo_t *cr = gdk_cairo_create (m_window->gobj());
    gdk_cairo_set_source_pixmap(cr, m_pixmap->gobj(), a_e->area.x, a_e->area.y);
    cairo_rectangle(cr, a_e->area.x,
        a_e->area.y,
        a_e->area.width,
        a_e->area.height);
    cairo_fill(cr);

    cairo_destroy(cr);

    return true;
}

void
seqtime::force_draw()
{
    cairo_t *cr = gdk_cairo_create (m_window->gobj());
    gdk_cairo_set_source_pixmap(cr, m_pixmap->gobj(), 0.0, 0.0);
    cairo_rectangle(cr, 0.0, 0.0, m_window_x,  m_window_y);
    cairo_fill(cr);

    cairo_destroy(cr);
}

bool
seqtime::on_button_press_event(GdkEventButton* p0)
{
    return false;
}

bool
seqtime::on_button_release_event(GdkEventButton* p0)
{
    return false;
}
