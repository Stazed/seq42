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


perftime::perftime( perform *a_perf, mainwnd *a_main, Glib::RefPtr<Adjustment> a_hadjust ) :
    m_mainperf(a_perf),
    m_mainwnd(a_main),
    m_hadjust(a_hadjust),

    m_perf_scale_x(c_perf_scale_x),

    m_4bar_offset(0),

    m_snap(c_ppqn),
    m_measure_length(c_ppqn * 4),
    m_draw_background(false)
{
    Gtk::Allocation allocation = get_allocation();
    m_surface = Cairo::ImageSurface::create(
        Cairo::Format::FORMAT_ARGB32,
        allocation.get_width(),
        allocation.get_height()
    );

    add_events( Gdk::BUTTON_PRESS_MASK |
                Gdk::BUTTON_RELEASE_MASK );

    m_hadjust->signal_value_changed().connect( mem_fun( *this, &perftime::change_horz ));

    set_double_buffered( false );
}

void
perftime::set_zoom (int a_zoom)
{
    if (m_mainwnd->zoom_check(a_zoom))
    {
        m_perf_scale_x = a_zoom;
        m_draw_background = true;
        queue_draw();
    }
}

void
perftime::on_realize()
{
    // we need to do the default realize
    Gtk::DrawingArea::on_realize();

    set_size_request( 10, c_timearea_y );
}

void
perftime::change_horz( )
{
    if ( m_4bar_offset != (int) m_hadjust->get_value() )
    {
        m_4bar_offset = (int) m_hadjust->get_value();
        m_draw_background = true;
        queue_draw();
    }
}

void
perftime::set_guides( int a_snap, int a_measure )
{
    m_snap = a_snap;
    m_measure_length = a_measure;
    m_draw_background = true;
    queue_draw();
}

void
perftime::draw_background()
{
    Cairo::RefPtr<Cairo::Context> cr = Cairo::Context::create(m_surface);

    Pango::FontDescription font;
    int text_width;
    int text_height;

    font.set_family(c_font);
    font.set_size((c_key_fontsize - 2) * Pango::SCALE);
    font.set_weight(Pango::WEIGHT_NORMAL);

    Gtk::Allocation allocation = get_allocation();
    const int width = allocation.get_width();
    const int height = allocation.get_height();

    cr->set_operator(Cairo::OPERATOR_CLEAR);
    cr->rectangle(-1, -1, width + 2, height + 2);
    cr->paint_with_alpha(0.0);
    cr->set_operator(Cairo::OPERATOR_OVER);

    /* clear background */
    cr->set_source_rgb( 1.0, 1.0, 1.0);            // white FIXME
    cr->set_line_width( 1.0);
    cr->rectangle( 0, 0, m_window_x, m_window_y);
    cr->stroke_preserve();
    cr->fill();

    cr->set_source_rgb( 0.0, 0.0, 0.0);            // black  FIXME
    cr->set_line_width( 1.0);
    cr->move_to( 0, m_window_y - 1);
    cr->line_to( m_window_x, m_window_y - 1);
    cr->stroke();

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
    cr->set_source_rgb( 0.6, 0.6, 0.6);            // Grey  FIXME
    cr->set_line_width( 1.0);

    for ( int i=first_measure;
            i<first_measure+(m_window_x * m_perf_scale_x / (m_measure_length)) + 1; i += bar_skip  )
    {
        int x_pos = ((i * m_measure_length) - tick_offset) / m_perf_scale_x;

        /* beat */
        cr->move_to( x_pos, 0);
        cr->line_to( x_pos, m_window_y);
        cr->stroke();

        /* bar numbers */
        char bar[16];
        snprintf( bar, sizeof(bar), "%d", i + 1 );

        auto t = create_pango_layout(bar);
        t->set_font_description(font);

        cr->set_source_rgb( 0.0, 0.0, 0.0);    // Black FIXME
        cr->move_to( x_pos + 2, 0);

        t->show_in_cairo_context(cr);
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
        cr->set_source_rgb( 0.0, 0.0, 0.0);    // Black FIXME

        auto t = create_pango_layout("L");
        font.set_weight(Pango::WEIGHT_BOLD);
        t->set_font_description(font);
        t->get_pixel_size(text_width, text_height);

        // draw the black background for the labels
        cr->rectangle( left, m_window_y - 9, text_width  + 2, text_height + 2);
        cr->stroke_preserve();
        cr->fill();

        // print the 'L' label in white
        cr->set_source_rgb( 1.0, 1.0, 1.0);    // White FIXME
        cr->move_to( left + 1,  (m_window_y *.5) - (text_height * .5) + 4 );

        t->show_in_cairo_context(cr);
    }

    if ( right >=0 && right <= m_window_x )
    {
        // set background for tempo labels to black
        cr->set_source_rgb( 0.0, 0.0, 0.0);    // Black FIXME

        auto t = create_pango_layout("R");
        font.set_weight(Pango::WEIGHT_BOLD);
        t->set_font_description(font);
        t->get_pixel_size(text_width, text_height);

        // draw the black background for the labels
        cr->rectangle( right - 7, m_window_y - 9, text_width  + 2, text_height + 2);
        cr->stroke_preserve();
        cr->fill();

        // print the 'R' label in white
        cr->set_source_rgb( 1.0, 1.0, 1.0);    // White FIXME
        cr->move_to( right - 6, (m_window_y *.5) - (text_height * .5) + 4 );

        t->show_in_cairo_context(cr);
    }
}

bool
perftime::on_draw(const Cairo::RefPtr<Cairo::Context>& cr)
{
    Gtk::Allocation allocation = get_allocation();
    const int width = allocation.get_width();
    const int height = allocation.get_height();

    // resize handler
    if (width != m_surface->get_width() || height != m_surface->get_height())
    {
        m_surface = Cairo::ImageSurface::create(
            Cairo::Format::FORMAT_ARGB32,
            allocation.get_width(),
            allocation.get_height()
        );
        m_draw_background = true;
    }

    if(m_draw_background)
    {
        m_draw_background = false;
        draw_background();
    }

    /* Clear previous background */
    cr->set_source_rgb(1.0, 1.0, 1.0);  // White FIXME
    cr->rectangle (0.0, 0.0, width, height);
    cr->stroke_preserve();
    cr->fill();

    /* Draw the new background */
    cr->set_source(m_surface, 0.0, 0.0);
    cr->paint();

    return true;
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

    m_draw_background = true;
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
