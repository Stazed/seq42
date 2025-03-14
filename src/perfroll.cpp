//----------------------------------------------------------------------------
//
//  This file is part of seq42.
//
//  seq42 is free software; you can redistribute it and/or modify
//  it under the terms of the GNU General)mm Public License as published by
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
#include "perfroll.h"
#include "font.h"
#include "seqedit.h"
#include "pixmaps/resize_black.xpm"
#include "pixmaps/resize_white.xpm"
#include "themes.h"


perfroll::perfroll( perform *a_perf,
                    mainwnd *a_main,
                    Glib::RefPtr<Adjustment> a_hadjust,
                    Glib::RefPtr<Adjustment> a_vadjust  ) :
    m_mainperf(a_perf),
    m_mainwnd(a_main),

    m_snap(0),
    m_measure_length(0),
    m_beat_length(0),

    m_perf_scale_x(c_perf_scale_x),       // 32 ticks per pixel
    m_horizontal_zoom(c_perf_scale_x),    // 32 ticks per pixel
    m_default_horizontal_zoom(c_perf_scale_x),
    m_x_zoom_ratio(c_default_horizontal_zoom),
    m_names_y(c_names_y),
    m_vertical_zoom(c_default_vertical_zoom),
    m_default_vertical_zoom(c_default_vertical_zoom),
    m_resize_handle_w(c_perfroll_resize_box_default),
    m_resize_handle_h(c_perfroll_resize_box_default),

    m_window_x(0),
    m_window_y(0),

    m_old_progress_ticks(0),

    m_4bar_offset(0),
    m_track_offset(0),
    m_roll_length_ticks(0),
    m_drop_x(0),
    m_drop_y(0),
    m_drop_tick(0),
    m_drop_tick_trigger_offset(0),
    m_drop_track(-1),   // set invalid so focus on track 0 will not be same as old focus

    m_vadjust(a_vadjust),
    m_hadjust(a_hadjust),

    m_moving(false),
    m_growing(false),

    cross_track_paste(false),
    have_button_press(false),
    transport_follow(true),
    trans_button_press(false),
    m_redraw_tracks(false),
    m_have_realize(false),
    m_have_stop_reposition(false),
    m_marker_change(false),
    m_line_location(0)
{
    Gtk::Allocation allocation = get_allocation();
    m_surface_track = Cairo::ImageSurface::create(
        Cairo::Format::FORMAT_ARGB32,
        allocation.get_width(),
        allocation.get_height()
    );

    m_surface_background = Cairo::ImageSurface::create(
        Cairo::Format::FORMAT_ARGB32,
        allocation.get_width(),
        allocation.get_height()
    );

    add_events( Gdk::BUTTON_PRESS_MASK |
                Gdk::BUTTON_RELEASE_MASK |
                Gdk::POINTER_MOTION_MASK |
                Gdk::KEY_PRESS_MASK |
                Gdk::KEY_RELEASE_MASK |
                Gdk::FOCUS_CHANGE_MASK |
                Gdk::SCROLL_MASK );

    set_size_request( 10, 10 );

    set_double_buffered( false );

    for( int i=0; i<c_max_track; ++i )
        m_track_active[i]=false;
}

perfroll::~perfroll( )
{

}

void
perfroll::change_horz( )
{
    if ( m_4bar_offset != (int) m_hadjust->get_value() )
    {
        m_4bar_offset = (int) m_hadjust->get_value();
        m_redraw_tracks = true;
    }
}

void
perfroll::change_vert( )
{
    if ( m_track_offset != (int) m_vadjust->get_value() )
    {
        m_track_offset = (int) m_vadjust->get_value();
        m_redraw_tracks = true;
    }
}

void
perfroll::on_realize()
{
    // we need to do the default realize
    Gtk::DrawingArea::on_realize();

    set_can_focus();
    update_sizes();

    m_hadjust->signal_value_changed().connect( mem_fun( *this, &perfroll::change_horz ));
    m_vadjust->signal_value_changed().connect( mem_fun( *this, &perfroll::change_vert ));

    // resize handler
    if (c_perfroll_background_x != m_surface_background->get_width() || m_names_y != m_surface_background->get_height())
    {
        m_surface_background = Cairo::ImageSurface::create(
            Cairo::Format::FORMAT_ARGB32,
            c_perfroll_background_x, m_names_y
        );
    }
    
    if (m_window_x != m_surface_track->get_width() || m_window_y != m_surface_track->get_height())
    {
        m_surface_track = Cairo::ImageSurface::create(
            Cairo::Format::FORMAT_ARGB32,
                m_window_x,  m_window_y
        );
    }

    /* and fill the background ( dotted lines n' such ) */
    fill_background_surface();
    m_redraw_tracks = true;
}

void
perfroll::init_before_show( )
{
    m_roll_length_ticks = m_mainperf->get_max_trigger();
    m_roll_length_ticks = m_roll_length_ticks -
                          ( m_roll_length_ticks % ( c_ppqn * 16 ));
    m_roll_length_ticks +=  c_ppqn * 4096;
}

void
perfroll::update_sizes()
{
    int h_bars         = m_roll_length_ticks / (c_ppqn * 16);
    int h_bars_visable = (m_window_x * m_perf_scale_x) / (c_ppqn * 16);

    m_hadjust->set_lower( 0 );
    m_hadjust->set_upper( h_bars );
    m_hadjust->set_page_size( h_bars_visable );
    m_hadjust->set_step_increment( 1 );
    m_hadjust->set_page_increment( 1 );
    int h_max_value = h_bars - h_bars_visable;

    if ( m_hadjust->get_value() > h_max_value )
    {
        m_hadjust->set_value( h_max_value );
    }

    m_vadjust->set_lower( 0 );
    m_vadjust->set_upper( c_max_track );
    m_vadjust->set_page_size( m_window_y / m_names_y );
    m_vadjust->set_step_increment( 1 );
    m_vadjust->set_page_increment( 1 );

    int v_max_value = c_max_track - (m_window_y / m_names_y);

    if ( m_vadjust->get_value() > v_max_value )
    {
        m_vadjust->set_value(v_max_value);
    }
    
    m_redraw_tracks = true;
}

void
perfroll::increment_size()
{
    m_roll_length_ticks += (c_ppqn * 512);
    update_sizes( );
}

/* updates background */
void
perfroll::fill_background_surface()
{
    Cairo::RefPtr<Cairo::Context> cr = Cairo::Context::create(m_surface_background);

    cr->set_operator(Cairo::OPERATOR_CLEAR);
    cr->rectangle(0, 0, c_perfroll_background_x,  m_names_y);
    cr->paint_with_alpha(1.0);
    cr->set_operator(Cairo::OPERATOR_OVER);

    /* clear background */
    cr->set_source_rgb(c_back_black.r, c_back_black.g, c_back_black.b);
    cr->set_line_width(1.0);
    cr->rectangle(0.0, 0.0, c_perfroll_background_x, m_names_y);
    cr->stroke_preserve();
    cr->fill();

    /* draw horizontal grey lines */
    cr->set_source_rgb(c_back_light_grey.r, c_back_light_grey.g, c_back_light_grey.b);
    static const std::vector<double> dashed = {1.0};
    static const std::vector<double> clear;
    cr->set_line_width(1.0);
    cr->set_dash(dashed, 1.0);
    cr->set_line_join(Cairo::LINE_JOIN_MITER);
    cr->move_to(0.0, 0.0);
    cr->line_to(c_perfroll_background_x, 0.0);
    cr->stroke();

    /* draw vertical lines */
    int beats = m_measure_length / m_beat_length;
    for ( int i = 0; i < beats ; )
    {
        if ( i == 0 )
        {
            cr->set_dash(clear, 0.0);        // Clear the dashes
            cr->set_line_width(2.0);
            cr->set_line_join(Cairo::LINE_JOIN_MITER);
        }
        else
        {
            cr->set_line_width(1.0);
            cr->set_dash(dashed, 1.0);
            cr->set_line_join(Cairo::LINE_JOIN_MITER);
        }

        /* solid line on every beat */
        cr->move_to(i * m_beat_length / m_perf_scale_x, 0.0);
        cr->line_to(i * m_beat_length / m_perf_scale_x, m_names_y);
        cr->stroke();

        // jump 2 if 16th notes
        if ( m_beat_length < c_ppqn/2 )
        {
            i += (c_ppqn / m_beat_length);
        }
        else
        {
            ++i;
        }
    }
}

/* simply sets the snap member */
void
perfroll::set_guides( int a_snap, int a_measure, int a_beat )
{
    m_snap = a_snap;
    m_measure_length = a_measure;
    m_beat_length = a_beat;

    if ( get_realized() )
    {
        fill_background_surface();
        m_redraw_tracks = true;
    }
}

void
perfroll::draw_progress()
{
    if (m_redraw_tracks)
    {
        if(!m_have_realize)     // We keep polling until we get it
            return;

        m_redraw_tracks = false;

        int y_s = 0;
        int y_f = m_window_y / m_names_y;

        for ( int y = y_s; y <= y_f; y++ )
        {
            int track = y + m_track_offset;

            draw_background_on(track );
            draw_track_on(track);
        }

        m_surface_window->set_source(m_surface_track, 0.0, 0.0);
        m_surface_window->paint();
        
        if ( m_marker_change )
        {
            /* Draw the 'L' and 'R' location lines */
            long L_tick = m_mainperf->get_left_tick();
            long R_tick = m_mainperf->get_right_tick();
            
            long tick_offset = m_4bar_offset * c_ppqn * 16;

            int L_mark = ( L_tick - tick_offset ) / m_perf_scale_x ;
            int R_mark = ( R_tick - tick_offset ) / m_perf_scale_x ;

            m_surface_window->set_source_rgb(c_marker_lines.r, c_marker_lines.g, c_marker_lines.b);
            m_surface_window->move_to(L_mark, 1.0);
            m_surface_window->line_to(L_mark, m_window_y);
            m_surface_window->stroke();

            m_surface_window->move_to(R_mark, 1.0);
            m_surface_window->line_to(R_mark, m_window_y);
            m_surface_window->stroke();
        }

        if ( m_line_location > 0 )
        {
            /* Draw the tempo marker location line */
            long tick_offset = m_4bar_offset * c_ppqn * 16;
            int tempo_tick = ( m_line_location - tick_offset ) / m_perf_scale_x ;

            m_surface_window->set_source_rgb(c_marker_lines.r, c_marker_lines.g, c_marker_lines.b);
            m_surface_window->move_to(tempo_tick, 1.0);
            m_surface_window->line_to(tempo_tick, m_window_y);
            m_surface_window->stroke();
        }

        m_have_stop_reposition = true;  // in case we are stopped, we need to draw the progress line
    }

    /* Here we draw the progress line */
    if (global_is_running || m_have_stop_reposition )
    {
        m_have_stop_reposition = false;

        /* draw progress line */
        long tick = m_mainperf->get_tick();
        long tick_offset = m_4bar_offset * c_ppqn * 16;

        int progress_x = ( tick - tick_offset ) / m_perf_scale_x ;

        /* Redraw the previous line to clear the previous progress */
        m_surface_window->set_source(m_surface_track, 0.0, 0.0);
        m_surface_window->rectangle(m_old_progress_ticks, 0, 1, m_window_y);
        m_surface_window->stroke_preserve();
        m_surface_window->fill();

        m_old_progress_ticks = progress_x;  // hold the position to clear for next line

        /* The new progress line */
        m_surface_window->set_source_rgb(c_progress_line.r, c_progress_line.g, c_progress_line.b);
        m_surface_window->set_line_width(2.0);
        m_surface_window->move_to(progress_x, 1.0);
        m_surface_window->line_to(progress_x, m_window_y);
        m_surface_window->stroke();

        /* We don't want to scroll when changing the marker cause it will reposition the marker... */
        if ( !m_marker_change && !m_line_location )
            auto_scroll_horz();
    }
}

bool 
perfroll::on_draw(const Cairo::RefPtr<Cairo::Context>& cr)
{
    m_surface_window = cr;

    m_have_realize = true;

    if (c_perfroll_background_x != m_surface_background->get_width() || m_names_y != m_surface_background->get_height())
    {
        m_surface_background = Cairo::ImageSurface::create(
            Cairo::Format::FORMAT_ARGB32,
            c_perfroll_background_x, m_names_y
        );

        fill_background_surface();
    }

    if (m_window_x != m_surface_track->get_width() || m_window_y != m_surface_track->get_height())
    {
        m_surface_track = Cairo::ImageSurface::create(
            Cairo::Format::FORMAT_ARGB32,
                m_window_x,  m_window_y
        );
    }
    
    m_redraw_tracks = true;
    
    draw_progress();

    return true;
}

void perfroll::draw_track_on( int a_track )
{
    long tick_on;
    long tick_off;
    long offset;
    bool selected;
    int seq_idx;
    sequence *seq;

    Cairo::RefPtr<Cairo::Context> cr = Cairo::Context::create(m_surface_track);
    cr->set_operator(Cairo::OPERATOR_DEST);
    cr->set_operator(Cairo::OPERATOR_OVER);

    cr->set_line_width(1.0);

    long tick_offset = m_4bar_offset * c_ppqn * 16;
    long x_offset = tick_offset / m_perf_scale_x;

    if ( a_track < c_max_track )
    {
        if ( m_mainperf->is_active_track( a_track ))
        {
            m_track_active[a_track] = true;

            track *trk =  m_mainperf->get_track( a_track );
            trk->reset_draw_trigger_marker();
            a_track -= m_track_offset;

            while ( trk->get_next_trigger( &tick_on, &tick_off, &selected, &offset, &seq_idx ))
            {
                if ( tick_off > 0 )
                {
                    long x_on  = tick_on  / m_perf_scale_x;
                    long x_off = tick_off / m_perf_scale_x;
                    int  w     = x_off - x_on + 1;

                    int x = x_on;
                    int y = m_names_y * a_track + 1;  // + 2
                    int h = m_names_y - 2; // - 4

                    // adjust to screen coordinates
                    x = x - x_offset;

                    if ( selected )
                    {
                        cr->set_source_rgba(c_track_color.r, c_track_color.g, c_track_color.b, 1.0);
                    }
                    else
                    {
                        cr->set_source_rgba(c_track_color.r, c_track_color.g, c_track_color.b, 0.5);
                    }

                    /* trigger background */
                    cr->rectangle(x, y, w, h);
                    cr->fill();

                    if ( selected )
                    {
                        cr->set_source_rgb(c_back_black.r, c_back_black.g, c_back_black.b);
                    }
                    else
                    {
                        cr->set_source_rgb(c_fore_white.r, c_fore_white.g, c_fore_white.b);
                    }

                    /* trigger outline */
                    cr->rectangle(x, y, w, h);
                    cr->stroke();

                    /* Scale the handle size to the zoom size */
                    m_resize_handle_w = c_perfroll_resize_box_default * ( (float)  c_perf_scale_x / m_perf_scale_x);
                    m_resize_handle_h = c_perfroll_resize_box_default * ( (float) m_names_y / c_names_y);

                    /* The resize handles */
                    if ( selected )
                    {
                        m_pixbuf = Gdk::Pixbuf::create_from_xpm_data(resize_black_xpm);
                    }
                    else
                    {
                        m_pixbuf = Gdk::Pixbuf::create_from_xpm_data(resize_white_xpm);
                    }

                    /* Scale the pixbuf to the zoom size */
                    m_pixbuf = m_pixbuf->scale_simple( (int) m_resize_handle_w,
                                          (int) m_resize_handle_h,
                                          Gdk::INTERP_BILINEAR);

                    /* resize handle - top left */
                    Gdk::Cairo::set_source_pixbuf (cr, m_pixbuf, x, y);
                    cr->paint();

                    /* resize handle - bottom right */
                    Gdk::Cairo::set_source_pixbuf (cr, m_pixbuf,
                                                   x + w - (int) m_resize_handle_w,
                                                   y + h - (int) m_resize_handle_h);
                    cr->paint();

                    /* sequence label and notes */
                    char label[40];
                    int max_label = (w / (c_font_width * m_x_zoom_ratio) ) - (2.0 / m_x_zoom_ratio) - 1;
                    if(max_label < 0) max_label = 0;
                    if(max_label > 39) max_label = 39;
                    seq = trk->get_sequence(seq_idx);

                    if(seq == NULL)
                    {
                        strncpy(label, "Empty", max_label);
                    }
                    else    // sequence notes
                    {
                        strncpy(label, seq->get_name(), max_label);
                        long sequence_length = seq->get_length();
                        int length_w = sequence_length / m_perf_scale_x;
                        long length_marker_first_tick = ( tick_on - (tick_on % sequence_length) + (offset % sequence_length) - sequence_length);

                        long tick_marker = length_marker_first_tick;

                        while ( tick_marker < tick_off )
                        {
                            long tick_marker_x = (tick_marker / m_perf_scale_x) - x_offset;

                            /* Sequence end marker line */
                            if ( tick_marker > tick_on )
                            {
                                if ( selected )
                                {
                                    cr->set_source_rgb(c_back_dark_grey.r, c_back_dark_grey.g, c_back_dark_grey.b);
                                }
                                else
                                {
                                    cr->set_source_rgb(c_fore_light_grey.r, c_fore_light_grey.g, c_fore_light_grey.b);
                                }

                                cr->rectangle(tick_marker_x, (y + 4), 1.0, (h - 8) );
                                cr->fill();
                            }

                            int lowest_note = seq->get_lowest_note_event( );
                            int highest_note = seq->get_highest_note_event( );

                            int height = highest_note - lowest_note;
                            height += 2;

                            int length = seq->get_length( );

                            long tick_s;
                            long tick_f;
                            int note;

                            bool t_selected;

                            int velocity;
                            draw_type dt;

                            seq->reset_draw_marker();

                            if ( selected )
                            {
                                cr->set_source_rgb(c_back_black.r, c_back_black.g, c_back_black.b);
                            }
                            else
                            {
                                cr->set_source_rgb(c_fore_white.r, c_fore_white.g, c_fore_white.b);
                            }

                            while ( (dt = seq->get_next_note_event( &tick_s, &tick_f, &note,
                                                                    &t_selected, &velocity )) != DRAW_FIN )
                            {
                                int note_y = ((m_names_y-6) -
                                              ((m_names_y-6)  * (note - lowest_note)) / height) + 1;

                                int tick_s_x = ((tick_s * length_w)  / length) + tick_marker_x;
                                int tick_f_x = ((tick_f * length_w)  / length) + tick_marker_x;

                                if ( dt == DRAW_NOTE_ON || dt == DRAW_NOTE_OFF )
                                    tick_f_x = tick_s_x + 1;
                                if ( tick_f_x <= tick_s_x )
                                    tick_f_x = tick_s_x + 1;

                                if ( tick_s_x < x )
                                {
                                    tick_s_x = x;
                                }

                                if ( tick_f_x > x + w )
                                {
                                    tick_f_x = x + w;
                                }

                                /*
                                        [           ]
                                 -----------
                                                 ---------
                                       ----------------
                                ------                      ------
                                */

                                //printf("tick_s [%ld]: tick_f[%ld]\n", tick_s, tick_f);
                                //printf("tick_f_x [%d]: tick_s_x[%d]\n", tick_f_x, tick_s_x);

                                if ( tick_f_x >= x && tick_s_x <= x+w )
                                {
                                    cr->move_to(tick_s_x, y + note_y);
                                    cr->line_to(tick_f_x, y + note_y);
                                    cr->stroke();
                                }
                            }

                            tick_marker += sequence_length;
                        }
                    }

                    /* Limit the label by the width of the trigger */
                    label[max_label] = '\0';

                    font::Color font_color = font::WHITE;
                    if (selected)
                    {
                        cr->set_source_rgb(c_fore_white.r, c_fore_white.g, c_fore_white.b);
                        font_color = font::BLACK;
                    }
                    else
                    {
                        cr->set_source_rgb(c_back_black.r, c_back_black.g, c_back_black.b);
                    }

                    /* draw the background for the labels */
                    cr->rectangle(x + m_resize_handle_w,
                                  y + 2,
                                  (int) (strlen(label) * (c_font_width * m_x_zoom_ratio)) - 1,
                                  (int) (c_font_height * m_vertical_zoom));

                    cr->stroke_preserve();
                    cr->fill();

                    /* print the sequence label */
                    p_font_renderer->render_string_on_drawable(x + m_resize_handle_w,
                                                               y + 2,
                                                               cr, label, font_color,
                                                               m_x_zoom_ratio,
                                                               m_vertical_zoom);
                }
            }
        }
    }
}

void perfroll::draw_background_on( int a_track )
{
    long tick_offset = m_4bar_offset * c_ppqn * 16;
    long first_measure = tick_offset / m_measure_length;

    a_track -= m_track_offset;
    
    int y = m_names_y * a_track;

    Cairo::RefPtr<Cairo::Context> cr = Cairo::Context::create(m_surface_track);
    cr->set_operator(Cairo::OPERATOR_DEST);
    cr->set_operator(Cairo::OPERATOR_OVER);
    
    /* apply the background grid - lines, etc */
    for ( int i = first_measure;
            i < first_measure + (m_window_x * m_perf_scale_x / (m_measure_length)) + 1; i++ )
    {
        int x_pos = ((i * m_measure_length) - tick_offset) / m_perf_scale_x;

        cr->set_source(m_surface_background, x_pos, y);
        cr->paint();
    }
}

void
perfroll::draw_track_on_window( int a_track )
{
    if(!m_have_realize)
        return;

    draw_background_on( a_track );
    draw_track_on( a_track );

    a_track -= m_track_offset;
    int y = m_names_y * a_track;

    m_surface_window->set_source(m_surface_track, 0.0, 0.0);
    m_surface_window->rectangle(a_track, y, m_window_x, m_names_y);
    m_surface_window->fill();
    
    /* If the transport is stopped, then we need to set this
     * to draw the progress line above the track */
    m_have_stop_reposition = true;
}

void
perfroll::redraw_dirty_tracks()
{
    int y_s = 0;
    int y_f = m_window_y / m_names_y;

    for ( int y=y_s; y<=y_f; y++ )
    {
        int track = y + m_track_offset;

        bool dirty = (m_mainperf->is_dirty_perf(track ));

        if (dirty)
        {
            draw_track_on_window(track);
        }
    }
}

bool
perfroll::on_button_press_event(GdkEventButton* a_ev)
{
    if(!trans_button_press) // to avoid double button press on normal seq42 method
    {
        transport_follow = m_mainperf->get_follow_transport();
        m_mainperf->set_follow_transport(false);
        trans_button_press = true;
    }

    bool result;

    switch (global_interactionmethod)
    {
    case e_fruity_interaction:
        result = m_fruity_interaction.on_button_press_event(a_ev, *this);
        break;
    case e_seq42_interaction:
        result = m_seq42_interaction.on_button_press_event(a_ev, *this);
        break;
    default:
        result = false;
    }
    return result;
}

bool
perfroll::on_button_release_event(GdkEventButton* a_ev)
{
    bool result;

    switch (global_interactionmethod)
    {
    case e_fruity_interaction:
        result = m_fruity_interaction.on_button_release_event(a_ev, *this);
        break;
    case e_seq42_interaction:
        result = m_seq42_interaction.on_button_release_event(a_ev, *this);
        break;
    default:
        result = false;
    }

    m_mainperf->set_follow_transport(transport_follow);
    trans_button_press = false;

    return result;
}

void
perfroll::auto_scroll_horz()
{
    if(!m_mainperf->get_follow_transport())
        return;

    long progress_tick = m_mainperf->get_tick();
    long tick_offset = m_4bar_offset * c_ppqn * 16;

    int progress_x =     ( progress_tick - tick_offset ) / m_horizontal_zoom;
    int page = progress_x / m_window_x;

    /* 50 so we rewind short of beginning and 0.75 short of the end on forward */
    if (page != 0 || ((progress_x - 50) < 0) || (progress_x > (m_window_x * 0.75)))
    {
        double left_tick = (double) progress_tick /m_horizontal_zoom/c_ppen;

        float adjust =  ( (float) m_horizontal_zoom / c_perf_scale_x  );

        double rewind = 0.0;

        /* The 50 is to scroll before the progress line goes behind the beginning if rewinding */
        if ( ((progress_x - 50) < 0) )
        {
            rewind = 1.0;
        }

        double set_hadjust = (left_tick * adjust) - rewind;

        if( left_tick > 0 && set_hadjust > 0.0)
        {
            m_hadjust->set_value( set_hadjust );
        }

        /* When stopped, reset scroll based on left tick */
        if ( !global_is_running )
        {
            if ( progress_tick == m_mainperf->get_left_tick() )
            {
                m_hadjust->set_value(set_hadjust);
            }
        }
    }
}

bool
perfroll::on_scroll_event( GdkEventScroll* a_ev )
{
    guint modifiers;    // Used to filter out caps/num lock etc.
    modifiers = gtk_accelerator_get_default_mod_mask ();

    if ((a_ev->state & modifiers) == GDK_CONTROL_MASK)
    {
        if (a_ev->direction == GDK_SCROLL_DOWN)
        {
            m_mainwnd->set_horizontal_zoom(m_horizontal_zoom + 2);
        }
        else if (a_ev->direction == GDK_SCROLL_UP)
        {
            m_mainwnd->set_horizontal_zoom(m_horizontal_zoom - 2);
        }
        return true;
    }

    /* Vertical zoom ALT key */
    if ((a_ev->state & modifiers) == GDK_MOD1_MASK)
    {
        if (a_ev->direction == GDK_SCROLL_DOWN)
        {
            m_mainwnd->set_vertical_zoom(m_vertical_zoom + c_vertical_zoom_step);
        }
        else if (a_ev->direction == GDK_SCROLL_UP)
        {
            m_mainwnd->set_vertical_zoom(m_vertical_zoom - c_vertical_zoom_step);
        }
        return true;
    }

    if ((a_ev->state & modifiers) == GDK_SHIFT_MASK)
    {
        double val = m_hadjust->get_value();

        if ( a_ev->direction == GDK_SCROLL_UP )
        {
            val -= m_hadjust->get_step_increment();
        }
        else if ( a_ev->direction == GDK_SCROLL_DOWN )
        {
            val += m_hadjust->get_step_increment();
        }

        m_hadjust->clamp_page(val, val + m_hadjust->get_page_size());
    }
    else
    {
        double val = m_vadjust->get_value();

        if ( a_ev->direction == GDK_SCROLL_UP )
        {
            val -= m_vadjust->get_step_increment();
        }
        else if ( a_ev->direction == GDK_SCROLL_DOWN )
        {
            val += m_vadjust->get_step_increment();
        }

        m_vadjust->clamp_page(val, val + m_vadjust->get_page_size());
    }
    return true;
}

bool
perfroll::on_motion_notify_event(GdkEventMotion* a_ev)
{
    bool result;

    switch (global_interactionmethod)
    {
    case e_fruity_interaction:
        result = m_fruity_interaction.on_motion_notify_event(a_ev, *this);
        break;
    case e_seq42_interaction:
        result = m_seq42_interaction.on_motion_notify_event(a_ev, *this);
        if(global_interaction_method_change)
        {
            get_window()->set_cursor(Gdk::Cursor::create(get_window()->get_display(), Gdk::LEFT_PTR ));
            global_interaction_method_change = false;
        }
        break;
    default:
        result = false;
    }
    return result;
}

bool
perfroll::on_key_press_event(GdkEventKey* a_p0)
{
    /* Horizontal zoom */
    if (a_p0->keyval == GDK_KEY_Z)         /* zoom in              */
    {
        m_mainwnd->set_horizontal_zoom(m_horizontal_zoom - 2);
        return true;
    }
    else if (a_p0->keyval == GDK_KEY_0)         /* reset to normal zoom */
    {
        m_mainwnd->set_horizontal_zoom(m_default_horizontal_zoom);
        return true;
    }
    else if (a_p0->keyval == GDK_KEY_z)         /* zoom out             */
    {
        m_mainwnd->set_horizontal_zoom(m_horizontal_zoom + 2);
        return true;
    }

    /* Vertical zoom */
    if ( !(a_p0->state & GDK_CONTROL_MASK) )
    {
        if (a_p0->keyval == GDK_KEY_V)         /* zoom in              */
        {
            m_mainwnd->set_vertical_zoom(m_vertical_zoom + c_vertical_zoom_step);
            return true;
        }
        else if (a_p0->keyval == GDK_KEY_9)         /* reset to user default zoom */
        {
            m_mainwnd->set_vertical_zoom(m_default_vertical_zoom);
            return true;
        }
        else if (a_p0->keyval == GDK_KEY_v)         /* zoom out             */
        {
            m_mainwnd->set_vertical_zoom(m_vertical_zoom - c_vertical_zoom_step);
            return true;
        }
    }

    if (a_p0->keyval == m_mainperf->m_key_pointer)         /* Move to mouse position */
    {
        int x = 0;
        int y = 0;

        long a_tick = 0;

        get_pointer(x, y);
        if(x < 0)
            x = 0;
        snap_x(&x);
        convert_x(x, &a_tick);

        if(global_song_start_mode)
        {
            if(m_mainperf->is_jack_running())
            {
                m_mainperf->set_reposition();
                m_mainperf->set_starting_tick(a_tick);
                m_mainperf->position_jack(true, a_tick);
            }
            else
            {
                m_mainperf->set_reposition();
                m_mainperf->set_starting_tick(a_tick);
            }
        }

        return true;
    }

    bool ret = false;

    if ( m_mainperf->is_active_track( m_drop_track))
    {
        if ( a_p0->type == GDK_KEY_PRESS )
        {
            if ( a_p0->keyval ==  GDK_KEY_Delete || a_p0->keyval == GDK_KEY_BackSpace )
            {
                m_mainperf->push_trigger_undo(m_drop_track);
                m_mainperf->get_track( m_drop_track )->del_selected_trigger();

                ret = true;
            }

            if ( a_p0->state & GDK_CONTROL_MASK )
            {
                /* cut */
                if ( a_p0->keyval == GDK_KEY_x || a_p0->keyval == GDK_KEY_X )
                {
                    m_mainperf->push_trigger_undo(m_drop_track);
                    m_mainperf->get_track( m_drop_track )->cut_selected_trigger();
                    ret = true;
                }
                /* copy */
                if ( a_p0->keyval == GDK_KEY_c || a_p0->keyval == GDK_KEY_C )
                {
                    /* for cross-track trigger paste we need to clear all previous trigger copies
                       or the cross track routine will just grab the earliest one if leftover */

                    for ( int t=0; t<c_max_track; ++t )
                    {
                        if (! m_mainperf->is_active_track( t ))
                        {
                            continue;
                        }
                        m_mainperf->get_track(t)->unset_trigger_copied(); // clear previous copies
                    }
                    m_mainperf->get_track( m_drop_track )->copy_selected_trigger();
                    cross_track_paste = false;                            // clear for new cross track - one per copy to avoid unwanted duplication
                    ret = true;
                }

                /* paste */
                if ( a_p0->keyval == GDK_KEY_v || a_p0->keyval == GDK_KEY_V )
                {
                    if (m_mainperf->get_track(m_drop_track)->get_trigger_clipboard()->m_sequence >= 0)
                    {
                        m_mainperf->push_trigger_undo(m_drop_track);
                        m_mainperf->get_track( m_drop_track )->paste_trigger();
                        ret = true;
                    }
                }
            }
        }
    }

    if ( ret == true )
    {
        fill_background_surface();
        m_redraw_tracks = true;
        return true;
    }
    else
        return false;
}

/* performs a 'snap' on x */
void
perfroll::snap_x( int *a_x )
{
    // snap = number pulses to snap to
    // m_scale = number of pulses per pixel
    //	so snap / m_scale  = number pixels to snap to

    int mod = (m_snap / m_perf_scale_x );

    if ( mod <= 0 )
        mod = 1;

    *a_x = *a_x - (*a_x % mod );
}

void
perfroll::convert_x( int a_x, long *a_tick )
{
    long tick_offset = m_4bar_offset * c_ppqn * 16;
    *a_tick = a_x * m_perf_scale_x;
    *a_tick += tick_offset;
}

void
perfroll::convert_xy( int a_x, int a_y, long *a_tick, int *a_track)
{
    long tick_offset = m_4bar_offset * c_ppqn * 16;

    *a_tick = a_x * m_perf_scale_x;
    *a_track = a_y / m_names_y;

    *a_tick += tick_offset;
    *a_track  += m_track_offset;

    if ( *a_track >= c_max_track )
        *a_track = c_max_track - 1;

    if ( *a_track < 0 )
        *a_track = 0;
}

bool
perfroll::on_focus_in_event(GdkEventFocus*)
{
    grab_focus();
    return false;
}

bool
perfroll::on_focus_out_event(GdkEventFocus*)
{
    return false;
}

void
perfroll::on_size_allocate(Gtk::Allocation& a_r )
{
    Gtk::DrawingArea::on_size_allocate( a_r );

    m_window_x = a_r.get_width();
    m_window_y = a_r.get_height();

    update_sizes();
}

void
perfroll::new_sequence( track *a_track, trigger *a_trigger )
{
    m_mainperf->push_track_undo(m_mainperf->get_track_index(a_track));
    int seq_idx = a_track->new_sequence();
    sequence *a_sequence = a_track->get_sequence(seq_idx);
    a_track->set_trigger_sequence(a_trigger, seq_idx);
    seqedit * a_seq_edit = new seqedit( a_sequence, m_mainperf, m_mainwnd );
#ifdef NSM_SUPPORT
    m_mainwnd->set_window_pointer(a_seq_edit);
#endif
}

void
perfroll::copy_sequence( track *a_track, trigger *a_trigger, sequence *a_seq )
{
    m_mainperf->push_track_undo(m_mainperf->get_track_index(a_track));
    bool same_track = a_track == a_seq->get_track();
    int seq_idx = a_track->new_sequence();
    sequence *a_sequence = a_track->get_sequence(seq_idx);
    *a_sequence = *a_seq;
    a_sequence->set_track(a_track);
    if(same_track)
    {
        char new_name[c_max_seq_name+1];
        snprintf(new_name, sizeof(new_name), "%s copy", a_sequence->get_name());
        a_sequence->set_name( new_name );
    }
    a_track->set_trigger_sequence(a_trigger, seq_idx);
    seqedit * a_seq_edit = new seqedit( a_sequence, m_mainperf, m_mainwnd );
#ifdef NSM_SUPPORT
    m_mainwnd->set_window_pointer(a_seq_edit);
#endif
}

void
perfroll::edit_sequence( track *a_track, trigger *a_trigger )
{
    sequence *a_seq = a_track->get_trigger_sequence(a_trigger);
    if(a_seq->get_editing())
    {
        a_seq->set_raise(true);
    }
    else
    {
        seqedit * a_seq_edit = new seqedit( a_seq, m_mainperf, m_mainwnd );
#ifdef NSM_SUPPORT
        m_mainwnd->set_window_pointer(a_seq_edit);
#endif
    }
}

void
perfroll::export_sequence( track *a_track, trigger *a_trigger)
{
    sequence *a_seq = a_track->get_trigger_sequence(a_trigger);
    m_mainwnd->export_sequence_midi(a_seq);
}

void
perfroll::export_trigger( track *a_track, trigger *a_trigger)
{
    a_track->set_trigger_export(a_trigger);
    m_mainwnd->export_trigger_midi(a_track);
}

void perfroll::play_sequence( track *a_track, trigger *a_trigger)
{
    sequence *a_seq =  a_track->get_trigger_sequence(a_trigger);
    a_seq->set_playing(!a_seq->get_playing());
}

void perfroll::set_trigger_sequence( track *a_track, trigger *a_trigger, int a_sequence )
{
    m_mainperf->push_trigger_undo(m_mainperf->get_track_index(a_track));
    a_track->set_trigger_sequence(a_trigger, a_sequence);
}

void
perfroll::del_trigger( track *a_track, long a_tick )
{
    m_mainperf->push_trigger_undo(m_mainperf->get_track_index(a_track));
    a_track->del_trigger( a_tick );
    a_track->set_dirty( );
}

void
perfroll::paste_trigger_mouse(long a_tick)
{
    for ( int t=0; t<c_max_track; ++t )
    {
        if (! m_mainperf->is_active_track( t ))
        {
            continue;
        }

        if(m_mainperf->get_track(t)->get_trigger_copied() &&    // do we have a copy to clipboard
                !cross_track_paste)                             // has cross track paste NOT been done for this ctrl-c copy
        {
            if(t != m_drop_track)                               // If clipboard and paste are on diff tracks - then cross track paste
            {
                cross_track_paste = true;
                paste_trigger_sequence
                (
                    m_mainperf->get_track( m_drop_track ),
                    m_mainperf->get_track(t)->get_sequence(m_mainperf->get_track(t)
                            ->get_trigger_clipboard()->m_sequence),
                    a_tick
                );
                break;
            }
        }
    }

    if(!cross_track_paste) // then regular on track paste
    {
        if (m_mainperf->get_track(m_drop_track)->get_trigger_clipboard()->m_sequence >= 0)
        {
            m_mainperf->push_trigger_undo(m_drop_track);
            m_mainperf->get_track( m_drop_track )->paste_trigger(a_tick);
        }
   }
   //cross_track_paste = false; - should not do this. If user is not careful they will duplicate sequences everywhere!
   // we require the user to ctrl-c again after every cross track paste to avoid unnoticed duplication of sequences.
}


void
perfroll::paste_trigger_sequence( track *p_track, sequence *a_sequence, long a_tick )
{
    // empty trigger = segfault via get_length - don't allow w/o sequence
    if (a_sequence == NULL)
        return;

    if ( a_sequence->get_track()->get_trigger_copied() )
    {
        if(a_sequence->get_track() != p_track)  // then we have to copy the sequence
        {
            m_mainperf->push_track_undo(m_mainperf->get_track_index(p_track));
            // Add the sequence
            int seq_idx = p_track->new_sequence();
            sequence *seq = p_track->get_sequence(seq_idx);
            *seq = *a_sequence;
            seq->set_track(p_track);
            trigger *a_trigger = NULL;
            a_trigger = a_sequence->get_track()->get_trigger_clipboard();

            //printf("a_trigger-m_offset[%ld]\na_trigger-m_tick_start[%ld]\nm_tick_end[%ld]\n",a_trigger->m_offset,
            //       a_trigger->m_tick_start,a_trigger->m_tick_end);
            //put trigger into current track clipboard
            p_track->get_trigger_clipboard()->m_offset = a_trigger->m_offset;
            p_track->get_trigger_clipboard()->m_selected = a_trigger->m_selected;
            p_track->get_trigger_clipboard()->m_sequence = seq_idx;
            p_track->get_trigger_clipboard()->m_tick_start = a_trigger->m_tick_start;
            p_track->get_trigger_clipboard()->m_tick_end = a_trigger->m_tick_end;

            long length =  p_track->get_trigger_clipboard()->m_tick_end - p_track->get_trigger_clipboard()->m_tick_start + 1;

            long offset_adjust = a_tick - p_track->get_trigger_clipboard()->m_tick_start;
            p_track->add_trigger(a_tick, length,
                                 p_track->get_trigger_clipboard()->m_offset + offset_adjust,
                                 p_track->get_trigger_clipboard()->m_sequence); // +/- distance to paste tick from start

            p_track->get_trigger_clipboard()->m_tick_start = a_tick;
            p_track->get_trigger_clipboard()->m_tick_end = p_track->get_trigger_clipboard()->m_tick_start + length - 1;
            p_track->get_trigger_clipboard()->m_offset += offset_adjust;

            long a_length = p_track->get_sequence(p_track->get_trigger_clipboard()->m_sequence)->get_length();
            p_track->get_trigger_clipboard()->m_offset = p_track->adjust_offset(p_track->get_trigger_clipboard()->m_offset,a_length);

            //printf("p-m_offset[%ld]\np-m_tick_start[%ld]\np-m_tick_end[%ld]\n",p_track->get_trigger_clipboard()->m_offset,
            //       p_track->get_trigger_clipboard()->m_tick_start,p_track->get_trigger_clipboard()->m_tick_end);

            p_track->set_trigger_copied();  // change to paste track
            a_sequence->get_track()->unset_trigger_copied(); // undo original
        }
    }
}

void
perfroll::trigger_menu_popup(perfroll& ths)
{
    using namespace Menu_Helpers;
    track *a_track = NULL;
    trigger *a_trigger = NULL;
    if ( ths.m_mainperf->is_active_track( ths.m_drop_track ))
    {
        a_track = ths.m_mainperf->get_track( ths.m_drop_track );
        a_trigger = a_track->get_trigger( ths.m_drop_tick );
    }
    if(a_trigger != NULL)
    {
        Menu * menu_trigger = new Menu();
        menu_trigger->attach_to_widget(*this);

        if(a_trigger->m_sequence > -1)
        {
            MenuItem * menu_item1 = new MenuItem("Edit sequence");
            menu_item1->signal_activate().connect(sigc::bind(mem_fun(ths, &perfroll::edit_sequence), a_track, a_trigger ));
            menu_trigger->append(*menu_item1);

            MenuItem * menu_item2 = new MenuItem("Export sequence");
            menu_item2->signal_activate().connect(sigc::bind(mem_fun(ths, &perfroll::export_sequence), a_track, a_trigger ));
            menu_trigger->append(*menu_item2);
            
            MenuItem * menu_item3 = new MenuItem("Export trigger");
            menu_item3->signal_activate().connect(sigc::bind(mem_fun(ths, &perfroll::export_trigger), a_track, a_trigger ));
            menu_trigger->append(*menu_item3);
        }

        if(a_trigger->m_sequence > -1 && !global_song_start_mode)
        {
            MenuItem * menu_item4 = new MenuItem("Set/Unset playing");
            menu_item4->signal_activate().connect(sigc::bind(mem_fun(ths, &perfroll::play_sequence), a_track, a_trigger ));
            menu_trigger->append(*menu_item4);
        }

        MenuItem * menu_item5 = new MenuItem("New sequence");
        menu_item5->signal_activate().connect(sigc::bind(mem_fun(ths, &perfroll::new_sequence), a_track, a_trigger));
        menu_trigger->append(*menu_item5);
        if(a_track->get_number_of_sequences())
        {
            char name[40];
            Menu *set_seq_menu = new Menu();

            for (unsigned s = 0; s < a_track->get_number_of_sequences(); s++ )
            {
                sequence *a_seq = a_track->get_sequence( s );
                snprintf(name, sizeof(name),"[%u] %s", s+1, a_seq->get_name());

                MenuItem * menu_item = new MenuItem(name);
                menu_item->signal_activate().connect(sigc::bind(mem_fun(ths, &perfroll::set_trigger_sequence), a_track, a_trigger, s));
                set_seq_menu->append(*menu_item);
            }

            MenuItem * menu_item6 = new MenuItem("Set sequence");
            menu_item6->set_submenu(*set_seq_menu);
            menu_trigger->append(*menu_item6);
        }

        MenuItem * menu_item7 = new MenuItem("Delete trigger");
        menu_item7->signal_activate().connect(sigc::bind(mem_fun(ths, &perfroll::del_trigger), a_track, ths.m_drop_tick ));
        menu_trigger->append(*menu_item7);

        Menu *copy_seq_menu = NULL;
        char name[40];
        for ( int t=0; t<c_max_track; ++t )
        {
            if (! ths.m_mainperf->is_active_track( t ))
            {
                continue;
            }
            track *some_track = ths.m_mainperf->get_track(t);

            Menu *menu_t = NULL;
            bool inserted = false;
            for (unsigned s=0; s< some_track->get_number_of_sequences(); s++ )
            {
                if ( !inserted )
                {
                    if(copy_seq_menu == NULL)
                    {
                        copy_seq_menu = new Menu();
                    }

                    inserted = true;
                    snprintf(name, sizeof(name), "[%d] %s", t+1, some_track->get_name());

                    menu_t = new Menu();
                    MenuItem * menu_item8 = new MenuItem(name);
                    menu_item8->set_submenu(*menu_t);
                    copy_seq_menu->append(*menu_item8);
                }

                sequence *a_seq = some_track->get_sequence( s );
                snprintf(name, sizeof(name),"[%u] %s", s+1, a_seq->get_name());

                MenuItem * menu_item9 = new MenuItem(name);
                menu_item9->signal_activate().connect(sigc::bind(mem_fun(ths, &perfroll::copy_sequence), a_track, a_trigger, a_seq));
                menu_t->append(*menu_item9);
            }
        }
        if(copy_seq_menu != NULL)
        {
            MenuItem * menu_item10 = new MenuItem("Copy sequence");
            menu_item10->set_submenu(*copy_seq_menu);
            menu_trigger->append(*menu_item10);
        }

        menu_trigger->show_all();
        menu_trigger->popup_at_pointer(NULL);
    }
}

void
perfroll::set_horizontal_zoom (int a_zoom)
{
    if (m_mainwnd->zoom_check(a_zoom))
    {
        m_horizontal_zoom = a_zoom;
        m_perf_scale_x = m_horizontal_zoom;
        
        /* For horizontal font resizing */
        m_x_zoom_ratio = (float) c_perf_scale_x/m_horizontal_zoom;

        if (m_perf_scale_x == 0)
            m_perf_scale_x = 1;

        update_sizes();
        fill_background_surface();
    }
}

void
perfroll::set_vertical_zoom (float a_zoom)
{
    if (m_mainwnd->zoom_check_vertical(a_zoom))
    {
        m_vertical_zoom = a_zoom;
        m_names_y = (int) (c_names_y * m_vertical_zoom);

        fill_background_surface();
        queue_draw();
    }
}

long
perfroll::get_default_trigger_length(perfroll& ths)
{
    //c_default_trigger_length_in_bars * 4 * c_ppqn
    float trig_ratio = (float) ths.m_mainperf->get_bp_measure() / (float)ths.m_mainperf->get_bw();
    //printf("trig_ratio [%f]\n",trig_ratio);

    long trig_measure = 4 * c_ppqn;
    long ret = (long) (trig_ratio * trig_measure);
    //printf("trig_length [%ld]\n", ret);

    return ret;
}

/**
 *  Set the user default from the options file - .seq32rc
 * 
 * @param z
 *      The user default adjusted to float zoom value.
 */
void
perfroll::set_default_vertical_zoom(float z)
{
    m_default_vertical_zoom = z;
}

/**
 *  Set the user default from the options file - .seq32rc
 * 
 * @param z
 *      The user default.
 */
void
perfroll::set_default_horizontal_zoom(int z)
{
    m_default_horizontal_zoom = z;
}

