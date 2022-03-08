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
#include "pixmaps/mark_left.xpm"
#include "pixmaps/mark_right.xpm"


perftime::perftime( perform *a_perf, mainwnd *a_main, Glib::RefPtr<Adjustment> a_hadjust ) :
    m_mainperf(a_perf),
    m_mainwnd(a_main),
    m_hadjust(a_hadjust),

    m_perf_scale_x(c_perf_scale_x),

    m_4bar_offset(0),

    m_snap(c_ppqn),
    m_measure_length(c_ppqn * 4),
    m_moving_left(false),
    m_moving_right(false),
    m_moving_paste(false),
    m_paste_tick(0),
    m_draw_background(false)
{
    Gtk::Allocation allocation = get_allocation();
    m_surface = Cairo::ImageSurface::create(
        Cairo::Format::FORMAT_ARGB32,
        allocation.get_width(),
        allocation.get_height()
    );

    add_events( Gdk::BUTTON_PRESS_MASK |
                Gdk::BUTTON_RELEASE_MASK |
                Gdk::POINTER_MOTION_MASK |
                Gdk::KEY_PRESS_MASK | 
                Gdk::KEY_RELEASE_MASK);

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
    cr->set_source_rgb(c_back_light_grey.r, c_back_light_grey.g, c_back_light_grey.b);
    cr->set_line_width( 1.0);
    cr->rectangle( 0, 0, m_window_x, m_window_y);
    cr->stroke_preserve();
    cr->fill();

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

    cr->set_source_rgb(c_back_light_grey.r, c_back_light_grey.g, c_back_light_grey.b);
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

        cr->set_source_rgb(c_back_black.r, c_back_black.g, c_back_black.b);
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
        m_pixbuf = Gdk::Pixbuf::create_from_xpm_data(mark_left_xpm);
        Gdk::Cairo::set_source_pixbuf (cr, m_pixbuf, left - 2, m_window_y - 10);
        cr->paint();
    }

    if ( right >=0 && right <= m_window_x )
    {
        m_pixbuf = Gdk::Pixbuf::create_from_xpm_data(mark_right_xpm);
        Gdk::Cairo::set_source_pixbuf (cr, m_pixbuf, right - 10, m_window_y - 10);
        cr->paint();
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
        if ( p0->state & GDK_CONTROL_MASK )     // control key
        {
            m_moving_paste = true;
        }
        else    // 'L' marker
        {
            m_mainperf->set_left_tick( tick );
            m_mainwnd->set_perfroll_marker_change(true);
            m_moving_left = true;
            m_draw_background = true;
            queue_draw();
            return true;
        }
    }
    else if ( p0->button == 3 )
    {
        m_mainperf->set_right_tick( tick + m_snap );
        m_mainwnd->set_perfroll_marker_change(true);
        m_moving_right = true;
        m_draw_background = true;
        queue_draw();
        return true;
    }

    return false;
}

bool
perftime::on_button_release_event(GdkEventButton* p0)
{
    m_moving_left = false;
    m_moving_right = false;
    m_moving_paste = false;

    /* We only draw the marker lines when setting with the button pressed.
     * So we unset the line drawing here with false */
    m_mainwnd->set_perfroll_marker_change(false);
    m_mainwnd->set_marker_line_selection( 0 );

    return false;
}

bool
perftime::on_motion_notify_event(GdkEventMotion* a_ev)
{
    if ( !m_moving_left && !m_moving_right && !m_moving_paste)
        return false;

    long tick = (long) a_ev->x;
    tick *= m_perf_scale_x;

    tick += (m_4bar_offset * 16 * c_ppqn);
    tick = tick - (tick % m_snap);

    if ( tick < 0 )
        return false;

    if ( m_moving_paste )
    {
        m_mainwnd->set_marker_line_selection( tick );
        m_paste_tick = tick;
    }
    else if( m_moving_left )
    {
        /* Don't allow left tick to go beyond right */
        if (tick >= m_mainperf->get_right_tick())
        {
            long right_tick = tick + c_ppqn * 4;
            m_mainperf->set_right_tick(right_tick);
        }
        m_mainperf->set_left_tick( tick );
        m_mainwnd->set_perfroll_marker_change(true);
        m_draw_background = true;
        queue_draw();
    }
    else if( m_moving_right)
    {
        /* Don't allow right tick to go before left */
        if (tick <= m_mainperf->get_left_tick())
        {
            long left_tick = tick - c_ppqn * 4;
            if(left_tick < 0)
                left_tick = 0;

            m_mainperf->set_left_tick(left_tick);
        }
        m_mainperf->set_right_tick( tick + m_snap );
        m_mainwnd->set_perfroll_marker_change(true);
        m_draw_background = true;
        queue_draw();
    }

    return false;
}

bool 
perftime::on_key_release_event(GdkEventKey* a_ev)
{
    if ( m_moving_paste )
    {
        if ( a_ev->type == GDK_KEY_RELEASE )
        {
            if ( (a_ev->keyval ==  GDK_KEY_Control_L) || (a_ev->keyval ==  GDK_KEY_Control_R) )
            {
                m_moving_paste = false;
                m_mainwnd->set_marker_line_selection( 0 );
                m_mainwnd->paste_triggers((long) m_paste_tick);

                /* If we paste before the left marker then all the triggers get moved
                * right, so lets move the markers as well to keep them aligned */
                long left_tick = m_mainperf->get_left_tick();

                if (m_paste_tick < (uint64_t) left_tick)
                {
                    long right_tick = m_mainperf->get_right_tick();
                    long distance = right_tick - left_tick;
                    m_mainperf->set_right_tick(right_tick + distance);
                    m_mainperf->set_left_tick(left_tick + distance);
                    m_draw_background = true;
                    queue_draw();
                }

                return true;
            }
        }
    }

    return false;
}

void
perftime::on_size_allocate(Gtk::Allocation &a_r )
{
    Gtk::DrawingArea::on_size_allocate( a_r );

    m_window_x = a_r.get_width();
    m_window_y = a_r.get_height();
}
