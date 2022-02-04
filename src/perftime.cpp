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
#include "perftime.h"
#include "font.h"


perftime::perftime( perform *a_perf, mainwnd *a_main, Adjustment *a_hadjust ) :
    m_black(Gdk::Color("black")),
    m_white(Gdk::Color("white")),
    m_grey(Gdk::Color("green")),
    //m_grey(Gdk::Color("grey")),

    m_mainperf(a_perf),
    m_mainwnd(a_main),
    m_hadjust(a_hadjust),

    m_perf_scale_x(c_perf_scale_x),

    m_4bar_offset(0),

    m_snap(c_ppqn),
    m_measure_length(c_ppqn * 4)
{
    add_events( Gdk::BUTTON_PRESS_MASK |
                Gdk::BUTTON_RELEASE_MASK );

    // in the constructor you can only allocate colors,
    // get_window() returns 0 because we have not been realized
    Glib::RefPtr<Gdk::Colormap> colormap = get_default_colormap();
    colormap->alloc_color( m_black );
    colormap->alloc_color( m_white );
    colormap->alloc_color( m_grey );

    m_hadjust->signal_value_changed().connect( mem_fun( *this, &perftime::change_horz ));

    set_double_buffered( false );
}

void
perftime::increment_size()
{

}

void
perftime::update_sizes()
{

}

void
perftime::set_zoom (int a_zoom)
{
    if (m_mainwnd->zoom_check(a_zoom))
    {
        m_perf_scale_x = a_zoom;
        draw_background();
    }
}

void
perftime::on_realize()
{
    // we need to do the default realize
    Gtk::DrawingArea::on_realize();

    // Now we can allocate any additional resources we need
    m_window = get_window();
    m_window->clear();

    set_size_request( 10, c_timearea_y );
}

void
perftime::change_horz( )
{
    if ( m_4bar_offset != (int) m_hadjust->get_value() )
    {
        m_4bar_offset = (int) m_hadjust->get_value();
        queue_draw();
    }
}

void
perftime::set_guides( int a_snap, int a_measure )
{
    m_snap = a_snap;
    m_measure_length = a_measure;
    queue_draw();
}

int
perftime::idle_progress( )
{
    return true;
}

void
perftime::update_pixmap()
{

}

void
perftime::draw_pixmap_on_window()
{

}

bool
perftime::on_expose_event (GdkEventExpose * /* ev */ )
{
    draw_background();
    return true;
}

void
perftime::draw_background()
{
    /* clear background */
    cairo_t *cr = gdk_cairo_create (m_window->gobj());
    cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);            // white FIXME
    cairo_set_line_width(cr, 1.0);
    cairo_rectangle(cr, 0, 0, m_window_x, m_window_y);
    cairo_stroke_preserve(cr);
    cairo_fill(cr);

    cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);            // black  FIXME
    cairo_set_line_width(cr, 1.0);
    cairo_move_to(cr, 0, m_window_y - 1);
    cairo_line_to(cr, m_window_x, m_window_y - 1);
    cairo_stroke(cr);

    /* draw vertical lines */
    long tick_offset = (m_4bar_offset * 16 * c_ppqn);
    long first_measure = tick_offset / m_measure_length;

    float bar_draw = m_measure_length / (float) m_perf_scale_x;

    int bar_skip = 1;

    if(bar_draw < 24)
        bar_skip = 4;
    if(bar_draw < 12)
        bar_skip = 8;
    if(bar_draw < 6)
        bar_skip = 16;
    if(bar_draw < 3)
        bar_skip = 32;
    if(bar_draw < .75)
        bar_skip = 64;

#if 0

    0   1   2   3   4   5   6
    |   |   |   |   |   |   |
    |    |    |    |    |    |
    0    1    2    3    4    5

#endif
    cairo_set_source_rgb(cr, 0.6, 0.6, 0.6);            // Grey  FIXME
    cairo_set_line_width(cr, 1);

    for ( int i=first_measure;
            i<first_measure+(m_window_x * m_perf_scale_x / (m_measure_length)) + 1; i += bar_skip  )
    {
        int x_pos = ((i * m_measure_length) - tick_offset) / m_perf_scale_x;

        /* beat */
        cairo_move_to(cr, x_pos, 0);
        cairo_line_to(cr, x_pos, m_window_y);
        cairo_stroke(cr);

        /* bar numbers */
        char bar[16];
        snprintf( bar, sizeof(bar), "%d", i + 1 );

        cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);    // Black FIXME
        cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
        cairo_set_font_size(cr, 9.0);
        cairo_move_to(cr, x_pos + 2, 8.0);
        cairo_show_text( cr, bar);
    }

    /* The 'L' and 'R' markers */
    long left = m_mainperf->get_left_tick( );
    long right = m_mainperf->get_right_tick( );

    left -= (m_4bar_offset * 16 * c_ppqn);
    left /= m_perf_scale_x;
    right -= (m_4bar_offset * 16 * c_ppqn);
    right /= m_perf_scale_x;

    if ( left >=0 && left <= m_window_x )
    {
        // set background for tempo labels to black
        cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);    // Black FIXME

        // draw the black background for the labels
        cairo_rectangle(cr, left, m_window_y - 9, 7, 10);
        cairo_stroke_preserve(cr);
        cairo_fill(cr);

        // print the 'L' label in white
        cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);    // White FIXME
        cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
        cairo_set_font_size(cr, 9.0);
        cairo_move_to(cr, left + 1, 17.0);
        cairo_show_text( cr, "L");
    }

    if ( right >=0 && right <= m_window_x )
    {
        // set background for tempo labels to black
        cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);    // Black FIXME

        // draw the black background for the labels
        cairo_rectangle(cr, right - 6, m_window_y - 9, 7, 10);
        cairo_stroke_preserve(cr);
        cairo_fill(cr);

        // print the 'R' label in white
        cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);    // White FIXME
        cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
        cairo_set_font_size(cr, 9.0);
        cairo_move_to(cr, right - 6, 17.0);
        cairo_show_text( cr, "R");
    }

    cairo_destroy(cr);
}

bool
perftime::on_button_press_event(GdkEventButton* p0)
{
    long tick = (long) p0->x;
    tick *= m_perf_scale_x;
    //tick = tick - (tick % (c_ppqn * 4));
    tick += (m_4bar_offset * 16 * c_ppqn);

    tick = tick - (tick % m_snap);

    //if ( p0->button == 2 )
    //m_mainperf->set_start_tick( tick );
    if ( p0->button == 1 )
    {
        m_mainperf->set_left_tick( tick );
    }

    if ( p0->button == 3 )
    {
        m_mainperf->set_right_tick( tick + m_snap );
    }

    queue_draw();

    return true;
}

bool
perftime::on_button_release_event(GdkEventButton* p0)
{
    return false;
}

void
perftime::on_size_allocate(Gtk::Allocation &a_r )
{
    Gtk::DrawingArea::on_size_allocate( a_r );

    m_window_x = a_r.get_width();
    m_window_y = a_r.get_height();
}
