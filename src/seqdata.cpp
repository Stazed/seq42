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
#include <cairomm-1.0/cairomm/context.h>

#include "event.h"
#include "seqdata.h"
#include "font.h"

seqdata::seqdata(sequence *a_seq, int a_zoom, Gtk::Adjustment *a_hadjust):
    m_seq(a_seq),

    m_zoom(a_zoom),

    m_hadjust(a_hadjust),

    m_scroll_offset_ticks(0),
    m_scroll_offset_x(0),
    m_status(0x00),

    m_dragging(false),
    m_drag_handle(false),
    m_background_draw(false)
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
                Gdk::LEAVE_NOTIFY_MASK |
                Gdk::SCROLL_MASK );

    set_can_focus();
    set_double_buffered( false );

    set_size_request( 10,  c_dataarea_y );
}

void
seqdata::update_sizes()
{
    if( get_realized() )
    {
        m_surface_window = m_window->create_cairo_context();
        m_background_draw = true;
    }
}

void
seqdata::reset()
{
    m_scroll_offset_ticks = (int) m_hadjust->get_value();
    m_scroll_offset_x = m_scroll_offset_ticks / m_zoom;

    update_sizes();
}

void
seqdata::redraw()
{
    m_background_draw = true;
}

void
seqdata::on_realize()
{
    // we need to do the default realize
    Gtk::DrawingArea::on_realize();

    // Now we can allocate any additional resources we need
    m_window = get_window();
    m_window->clear();
    m_surface_window = m_window->create_cairo_context();

    m_hadjust->signal_value_changed().connect( mem_fun( *this, &seqdata::change_horz ));

    update_sizes();
}

void
seqdata::set_zoom( int a_zoom )
{
    if ( m_zoom != a_zoom )
    {
        m_zoom = a_zoom;
        reset();
    }
}

void
seqdata::set_data_type( unsigned char a_status, unsigned char a_control = 0  )
{
    m_status = a_status;
    m_cc = a_control;

    this->redraw();
}


void
seqdata::draw_events_on(  Glib::RefPtr<Gdk::Drawable> a_draw  )
{
    m_background_draw = false;
    Cairo::RefPtr<Cairo::Context> cr = Cairo::Context::create(m_surface);

    Gtk::Allocation allocation = get_allocation();
    const int width = allocation.get_width();
    const int height = allocation.get_height();

    cr->set_operator(Cairo::OPERATOR_CLEAR);
    cr->rectangle(-1, -1, width + 2, height + 2);
    cr->paint_with_alpha(1.0);
    cr->set_operator(Cairo::OPERATOR_OVER);

    Pango::FontDescription font;
    int text_width;
    int text_height;

    font.set_family(c_font);
    font.set_size(c_key_fontsize * Pango::SCALE);
    font.set_weight(Pango::WEIGHT_NORMAL);

    long tick;

    unsigned char d0,d1;

    bool selected;

    int event_x;
    int event_height;

    /*  For note ON there can be multiple events on the same vertical in which
        the selected item can be covered.  For note ON's the selected item needs
        to be drawn last so it can be seen.  So, for other events the below var
        num_selected_events will be -1 for ALL_EVENTS. For note ON's only, the
        var will be the number of selected events. If 0 then only one pass is
        needed. If > 0 then two passes are needed, one for unselected (first), and one for
        selected (last).
    */
    int num_selected_events = ALL_EVENTS;
    int selection_type = num_selected_events;

    if ( m_status == EVENT_NOTE_ON)
    {
        num_selected_events = m_seq->get_num_selected_events(m_status, m_cc);

        /* For first pass - if any selected,  selection_type = UNSELECTED_EVENTS.
           For second pass will be set to num_selected_events*/
        if(num_selected_events > 0)
            selection_type = UNSELECTED_EVENTS;
    }

    int start_tick = m_scroll_offset_ticks ;
    int end_tick = (m_window_x * m_zoom) + m_scroll_offset_ticks;

    SECOND_PASS_NOTE_ON: // yes this is a goto... yikes!!!!

    m_seq->reset_draw_marker();

    while ( m_seq->get_next_event( m_status,
                                   m_cc,
                                   &tick, &d0, &d1,
                                   &selected, selection_type ) == true )
    {
        if ( tick >= start_tick && tick <= end_tick )
        {
            /* turn into screen corrids */
            event_x = tick / m_zoom;

            /* Clear background of event velocity labels - we need
             * to do this before the handles or they get cropped */
            cr->set_source_rgb(1.0, 1.0, 1.0);  // White FIXME
            cr->rectangle(event_x + 3 - m_scroll_offset_x,
                                  c_dataarea_y - 40,
                                  8, 40);
            cr->fill();

            if(selected)
            {
                cr->set_source_rgb(1.0, 0.27, 0.0);    // Red FIXME);
            }
            else
            {
                cr->set_source_rgb(0.0, 0.0, 1.0);    // blue FIXME
            }

            /* generate the value */
            event_height = d1;

            if ( m_status == EVENT_PROGRAM_CHANGE ||
                    m_status == EVENT_CHANNEL_PRESSURE  )
            {
                event_height = d0;
            }

            /* draw vert lines */
            cr->move_to(event_x -  m_scroll_offset_x +1, c_dataarea_y - event_height);
            cr->line_to(event_x -  m_scroll_offset_x +1, c_dataarea_y);
            cr->stroke();

            /* draw handle */
            cr->arc(event_x -  m_scroll_offset_x + 1,
                                c_dataarea_y - event_height + 2,
                                c_data_handle_radius,
                                0, 2 * M_PI);
            cr->stroke_preserve();
            cr->fill();

            /* event numbers */
            auto t = create_pango_layout(to_string(event_height));
            t->set_font_description(font);
            t->set_justify(Pango::ALIGN_CENTER);
            t->set_width(0);
            t->set_wrap(Pango::WRAP_CHAR);
            t->get_pixel_size(text_width, text_height);

            /* Volume label level */
            int height_offset = 13;
            
            if (event_height > 9)
            {
                height_offset = 26;
                if (event_height > 99)
                    height_offset = 40;
            }

            cr->set_source_rgb(0.0, 0.0, 0.0);  // Black FIXME
            cr->move_to(event_x + 3 - m_scroll_offset_x,
                                  c_dataarea_y - height_offset);
            t->show_in_cairo_context(cr);
        }
    }

    if(selection_type == UNSELECTED_EVENTS)
    {
        selection_type = num_selected_events;
        goto SECOND_PASS_NOTE_ON;
    }

    /* Clear previous background */
    m_surface_window->set_source_rgb(1.0, 1.0, 1.0);  // White FIXME
    m_surface_window->rectangle (0.0, 0.0, width, height);
    m_surface_window->stroke_preserve();
    m_surface_window->fill();

    /* Draw the new background */
    m_surface_window->set_source(m_surface, 0.0, 0.0);
    m_surface_window->paint();
}

int
seqdata::idle_redraw()
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
        m_background_draw = true;
    }

    /* no flicker, redraw */
    if ( m_background_draw )
    {
        draw_events_on( m_window );
    }

    draw_line_on_window();

    return true;
}

/* takes screen coordinates, give us notes and ticks */
void
seqdata::convert_x( int a_x, long *a_tick )
{
    *a_tick = a_x * m_zoom;
}

bool
seqdata::on_scroll_event( GdkEventScroll* a_ev )
{
    guint modifiers;    // Used to filter out caps/num lock etc.
    modifiers = gtk_accelerator_get_default_mod_mask ();

    /* This scroll event only handles basic scrolling without any
     * modifier keys such as GDK_CONTROL_MASK or GDK_SHIFT_MASK */
    if ((a_ev->state & modifiers) != 0)
        return false;

    if (  a_ev->direction == GDK_SCROLL_UP )
    {
        m_seq->increment_selected( m_status, m_cc );
    }

    if (  a_ev->direction == GDK_SCROLL_DOWN )
    {
        m_seq->decrement_selected( m_status, m_cc );
    }

    m_background_draw = true;

    return true;
}

bool
seqdata::on_button_press_event(GdkEventButton* a_p0)
{
    if ( a_p0->type == GDK_BUTTON_PRESS )
    {
        /* set values for line */
        m_drop_x = (int) a_p0->x + m_scroll_offset_x;
        m_drop_y = (int) a_p0->y;

        /* if they select the handle */
        long tick_s, tick_f;

        convert_x( m_drop_x - 3, &tick_s );
        convert_x( m_drop_x + 3, &tick_f );

        m_drag_handle = m_seq->select_event_handle(tick_s, tick_f,
                                              m_status, m_cc,
                                              c_dataarea_y - m_drop_y +3);

        if(m_drag_handle)
        {
            if(!m_seq->get_hold_undo()) // if they used line draw but did not leave...
                m_seq->push_undo();
        }
        else    // dragging - set starting point
        {
            m_current_x = (int) a_p0->x;
            m_current_y = (int) a_p0->y;
        }

        m_dragging = !m_drag_handle;
    }

    return true;
}

bool
seqdata::on_button_release_event(GdkEventButton* a_p0)
{
    m_current_x = (int) a_p0->x + m_scroll_offset_x;
    m_current_y = (int) a_p0->y;

    if ( m_dragging )
    {
        long tick_s, tick_f;

        if ( m_current_x < m_drop_x )
        {
            swap( m_current_x, m_drop_x );
            swap( m_current_y, m_drop_y );
        }

        convert_x( m_drop_x, &tick_s );
        convert_x( m_current_x, &tick_f );

        m_seq->change_event_data_range( tick_s, tick_f,
                                        m_status,
                                        m_cc,
                                        c_dataarea_y - m_drop_y -1,
                                        c_dataarea_y - m_current_y-1 );

        /* convert x,y to ticks, then set events in range */
        m_dragging = false;
    }

    if(m_drag_handle)
    {
        m_drag_handle = false;
        m_seq->unselect();
        m_seq->set_dirty();
    }

    if(m_seq->get_hold_undo())
    {
        m_seq->push_undo(true);
        m_seq->set_hold_undo(false);
    }

    m_background_draw = true;
    return true;
}

// Takes two points, returns a Xwin rectangle
void
seqdata::xy_to_rect(  int a_x1,  int a_y1,
                      int a_x2,  int a_y2,
                      int *a_x,  int *a_y,
                      int *a_w,  int *a_h )
{
    /* checks mins / maxes..  the fills in x,y
       and width and height */

    if ( a_x1 < a_x2 )
    {
        *a_x = a_x1;
        *a_w = a_x2 - a_x1;
    }
    else
    {
        *a_x = a_x2;
        *a_w = a_x1 - a_x2;
    }

    if ( a_y1 < a_y2 )
    {
        *a_y = a_y1;
        *a_h = a_y2 - a_y1;
    }
    else
    {
        *a_y = a_y2;
        *a_h = a_y1 - a_y2;
    }
}

bool
seqdata::on_motion_notify_event(GdkEventMotion* a_p0)
{
    if(m_drag_handle)
    {
        m_current_y = (int) a_p0->y - 3;

        m_current_y = c_dataarea_y - m_current_y;

        if(m_current_y < 0 )
            m_current_y = 0;

        if(m_current_y > 127 )
            m_current_y = 127;

        m_seq->adjust_data_handle(m_status, m_current_y );

        m_background_draw = true;
    }

    if ( m_dragging )
    {
        m_current_x = (int) a_p0->x + m_scroll_offset_x;
        m_current_y = (int) a_p0->y;

        long tick_s, tick_f;

        int adj_x_min, adj_x_max,
            adj_y_min, adj_y_max;

        if ( m_current_x < m_drop_x )
        {
            adj_x_min = m_current_x;
            adj_y_min = m_current_y;
            adj_x_max = m_drop_x;
            adj_y_max = m_drop_y;
        }
        else
        {
            adj_x_max = m_current_x;
            adj_y_max = m_current_y;
            adj_x_min = m_drop_x;
            adj_y_min = m_drop_y;
        }

        convert_x( adj_x_min, &tick_s );
        convert_x( adj_x_max, &tick_f );

        m_seq->change_event_data_range( tick_s, tick_f,
                                        m_status,
                                        m_cc,
                                        c_dataarea_y - adj_y_min -1,
                                        c_dataarea_y - adj_y_max -1 );

        /* convert x,y to ticks, then set events in range */
        m_background_draw = true;
    }

    return true;
}

bool
seqdata::on_leave_notify_event(GdkEventCrossing* p0)
{
    m_background_draw = true;
    return true;
}

/**
 * The event adjustment line from mouse drag
 */
void
seqdata::draw_line_on_window()
{
    if (m_dragging)
    {
        m_surface_window->set_source_rgb(1.0, 0.27, 0.0);    // Red FIXME
        m_surface_window->move_to(m_current_x - m_scroll_offset_x, m_current_y);
        m_surface_window->line_to( m_drop_x - m_scroll_offset_x, m_drop_y);
        m_surface_window->stroke();
    }
}

void
seqdata::change_horz( )
{
    m_scroll_offset_ticks = (int) m_hadjust->get_value();
    m_scroll_offset_x = m_scroll_offset_ticks / m_zoom;

    m_background_draw = true;
}

void
seqdata::on_size_allocate(Gtk::Allocation& a_r )
{
    Gtk::DrawingArea::on_size_allocate( a_r );

    m_window_x = a_r.get_width();
    m_window_y = a_r.get_height();

    update_sizes();
}

