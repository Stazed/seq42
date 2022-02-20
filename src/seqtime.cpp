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

seqtime::seqtime(sequence *a_seq, int a_zoom, Glib::RefPtr<Adjustment> a_hadjust):
    m_hadjust(a_hadjust),
    m_scroll_offset_ticks(0),
    m_scroll_offset_x(0),
    m_seq(a_seq),
    m_zoom(a_zoom)
{
    Gtk::Allocation allocation = get_allocation();
    m_surface = Cairo::ImageSurface::create(
        Cairo::Format::FORMAT_ARGB32,
        allocation.get_width(),
        allocation.get_height()
    );

    add_events( Gdk::BUTTON_PRESS_MASK |
                Gdk::BUTTON_RELEASE_MASK );

    /* set default size */
    set_size_request( 10, c_timearea_y );

    set_double_buffered( false );
}

void
seqtime::update_sizes()
{
    if( get_realized() )
    {
        if (m_window_x != m_surface->get_width() || m_window_y != m_surface->get_height())
        {
            m_surface = Cairo::ImageSurface::create(
                Cairo::Format::FORMAT_ARGB32,
                m_window_x,
                m_window_y
            );
        }

        update_surface();
    }
}

void
seqtime::on_realize()
{
    // we need to do the default realize
    Gtk::DrawingArea::on_realize();

    m_hadjust->signal_value_changed().connect( mem_fun( *this, &seqtime::change_horz ));

    update_sizes();
}

void
seqtime::change_horz( )
{
    m_scroll_offset_ticks = (int) m_hadjust->get_value();
    m_scroll_offset_x = m_scroll_offset_ticks / m_zoom;

    update_surface();
}

void
seqtime::on_size_allocate(Gtk::Allocation & a_r )
{
    Gtk::DrawingArea::on_size_allocate( a_r );

    m_window_x = a_r.get_width();
    m_window_y = a_r.get_height();

    update_sizes();
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
}

void
seqtime::redraw()
{
    m_scroll_offset_ticks = (int) m_hadjust->get_value();
    m_scroll_offset_x = m_scroll_offset_ticks / m_zoom;

    update_surface();
}

void
seqtime::update_surface()
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
    cr->paint_with_alpha(1.0);
    cr->set_operator(Cairo::OPERATOR_OVER);

    cr->set_source_rgb(0.0, 0.0, 0.0);    // Black FIXME
    cr->move_to(0.0, m_window_y - 1);
    cr->line_to(m_window_x,  m_window_y - 1 );
    cr->stroke();

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
        cr->move_to(base_line -  m_scroll_offset_x, 0);
        cr->line_to(base_line -  m_scroll_offset_x, m_window_y );
        cr->stroke();

        /* The bar numbers */
        char bar[16];
        snprintf(bar, sizeof(bar), "%d", (i/ ticks_per_measure ) + 1);

        auto t = create_pango_layout(bar);
        t->set_font_description(font);
        t->get_pixel_size(text_width, text_height);

        cr->move_to(base_line + 2 -  m_scroll_offset_x, -1);

        t->show_in_cairo_context(cr);
    }

    long end_x = m_seq->get_length() / m_zoom - m_scroll_offset_x;
    
    auto t = create_pango_layout("END");
    font.set_size(c_key_fontsize * Pango::SCALE);
    font.set_weight(Pango::WEIGHT_BOLD);
    t->set_font_description(font);
    t->get_pixel_size(text_width, text_height);

    // set background for label to black
    cr->set_source_rgb(0.0, 0.0, 0.0);    // Black FIXME

    // draw the black background for the 'END' label
    cr->rectangle(end_x, (m_window_y *.5), text_width + 2, text_height );
    cr->stroke_preserve();
    cr->fill();
   
    // print the 'END' label in white
    cr->set_source_rgb(1.0, 1.0, 1.0);    // White FIXME
    cr->move_to(end_x + 1, (m_window_y *.5) - (text_height *.5) + 4);

    t->show_in_cairo_context(cr);

    queue_draw();
}

bool
seqtime::on_draw(const Cairo::RefPtr<Cairo::Context>& cr)
{
    cr->set_source_rgb(1.0, 1.0, 1.0);  // White FIXME
    cr->rectangle (0.0, 0.0, m_window_x, m_window_y);
    cr->stroke_preserve();
    cr->fill();

    cr->set_source(m_surface, 0.0, 0.0);
    cr->paint();

    return true;
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
