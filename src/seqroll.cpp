//----------------------------------------------------------------------------
//
//  This file is part of seq42.
//
//  seq42 is free software; you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free So%ftware Foundation; either version 2 of the License, or
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
#include "seqroll.h"

const long c_handlesize = 16;

inline static long
clamp( long val, long low, long hi )
{
    return val < low ? low : hi < val ? hi : val;
}

seqroll::seqroll(perform *a_perf,
                 sequence *a_seq,
                 int a_zoom,
                 int a_snap,
                 seqdata *a_seqdata_wid,
                 seqevent *a_seqevent_wid,
                 seqkeys *a_seqkeys_wid,
                 Glib::RefPtr<Adjustment> a_hadjust,
                 Glib::RefPtr<Adjustment> a_vadjust,
                 ToggleButton *a_toggle_play):
    m_seq(a_seq),
    m_perform(a_perf),
    m_seqdata_wid(a_seqdata_wid),
    m_seqevent_wid(a_seqevent_wid),
    m_seqkeys_wid(a_seqkeys_wid),

    m_zoom(a_zoom),
    m_snap(a_snap),

    m_scale(0),
    m_chord(0),
    m_key(0),

    m_window_x(10),
    m_window_y(10),

    m_selecting(false),
    m_moving(false),
    m_moving_init(false),
    m_growing(false),
    m_painting(false),
    m_paste(false),
    m_is_drag_pasting(false),
    m_is_drag_pasting_start(false),
    m_justselected_one(false),

    m_old_progress_x(0),

    m_vadjust(a_vadjust),
    m_hadjust(a_hadjust),

    m_scroll_offset_ticks(0),
    m_scroll_offset_key(0),

    m_scroll_offset_x(0),
    m_scroll_offset_y(0),
    m_scroll_page(0),

    transport_follow(true),
    trans_button_press(false),

    m_background_track(0),
    m_background_sequence(0),
    m_drawing_background_seq(false),

    m_ignore_redraw(false),
    m_expanded_recording(false),
    m_initial_expose(false)
{
    Gtk::Allocation allocation = get_allocation();
    m_surface_edit = Cairo::ImageSurface::create(
        Cairo::Format::FORMAT_ARGB32,
        allocation.get_width(),
        allocation.get_height()
    );

    m_surface_background = Cairo::ImageSurface::create(
        Cairo::Format::FORMAT_ARGB32,
        allocation.get_width(),
        allocation.get_height()
    );

    using namespace Menu_Helpers;

    m_toggle_play = a_toggle_play;
    m_clipboard = new sequence( );

    add_events( Gdk::BUTTON_PRESS_MASK |
                Gdk::BUTTON_RELEASE_MASK |
                Gdk::POINTER_MOTION_MASK |
                Gdk::KEY_PRESS_MASK |
                Gdk::KEY_RELEASE_MASK |
                Gdk::FOCUS_CHANGE_MASK |
                Gdk::ENTER_NOTIFY_MASK |
                Gdk::LEAVE_NOTIFY_MASK |
                Gdk::SCROLL_MASK);

    set_double_buffered( false );
}

void
seqroll::set_background_sequence( bool a_state, int a_trk, int a_seq )
{
    m_drawing_background_seq = a_state;
    m_background_track = a_trk;
    m_background_sequence = a_seq;

    if ( m_ignore_redraw )
        return;

    update_background();
    update_surface();
}

seqroll::~seqroll( )
{
    delete m_clipboard;
}

void
seqroll::on_realize()
{
    // we need to do the default realize
    Gtk::DrawingArea::on_realize();

    set_can_focus();

    m_hadjust->signal_value_changed().connect( mem_fun( *this,
            &seqroll::change_horz ));
    m_vadjust->signal_value_changed().connect( mem_fun( *this,
            &seqroll::change_vert ));

    update_sizes();
}

/*

use m_zoom and

i % m_seq->get_bp_measure() == 0

 int numberLines = 128 / m_seq->get_bw() / m_zoom;
    int distance = c_ppqn / 32;
*/

void
seqroll::update_sizes()
{
    /* set default size */
    m_hadjust->set_lower( 0 );
    m_hadjust->set_upper( m_seq->get_length() );
    m_hadjust->set_page_size( m_window_x * m_zoom );

    /* The step increment is 1 semiquaver (1/16) note per zoom level */
    m_hadjust->set_step_increment( (c_ppqn / 4) * m_zoom);

    /* The page increment is always one bar */
    int page_increment = int( double(c_ppqn) *
                              double(m_seq->get_bp_measure()) *
                              (4.0 / double(m_seq->get_bw())) );
    m_hadjust->set_page_increment(page_increment);

    int h_max_value = ( m_seq->get_length() - (m_window_x * m_zoom));

    if ( m_hadjust->get_value() > h_max_value )
    {
        m_hadjust->set_value( h_max_value );
    }

    m_vadjust->set_lower( 0 );
    m_vadjust->set_upper( c_num_keys );
    m_vadjust->set_page_size( m_window_y / c_key_y );
    m_vadjust->set_step_increment( 12 );
    m_vadjust->set_page_increment( 12 );

    int v_max_value = c_num_keys - (m_window_y / c_key_y);

    if ( m_vadjust->get_value() > v_max_value )
    {
        m_vadjust->set_value(v_max_value);
    }

    /* create surfaces with window dimensions */
    if( get_realized() )
    {
        // resize handler
        if (m_window_x != m_surface_background->get_width() || m_window_y != m_surface_background->get_height())
        {
            m_surface_background = Cairo::ImageSurface::create(
                Cairo::Format::FORMAT_ARGB32,
                m_window_x, m_window_y
            );
        }

        if (m_window_x != m_surface_edit->get_width() || m_window_y != m_surface_edit->get_height())
        {
            m_surface_edit = Cairo::ImageSurface::create(
                Cairo::Format::FORMAT_ARGB32,
                    m_window_x,  m_window_y
            );
        }

        change_vert();
    }
}

void
seqroll::change_horz( )
{
    m_scroll_offset_ticks = (int) m_hadjust->get_value();
    m_scroll_offset_x = m_scroll_offset_ticks / m_zoom;

    if ( m_ignore_redraw )
        return;

    update_background();
    update_surface();
    force_draw();
}

void
seqroll::change_vert( )
{
    m_scroll_offset_key = (int) m_vadjust->get_value();
    m_scroll_offset_y = m_scroll_offset_key * c_key_y;

    if ( m_ignore_redraw )
        return;

    update_background();
    update_surface();
    force_draw();
}

/* basically resets the whole widget as if it was realized again */
void
seqroll::reset()
{
    m_scroll_offset_ticks = (int) m_hadjust->get_value();
    m_scroll_offset_x = m_scroll_offset_ticks / m_zoom;

    if ( m_ignore_redraw )
        return;

    update_sizes();
    update_background();
    update_surface();
}

void
seqroll::redraw()
{
    if ( m_ignore_redraw )
        return;

    m_scroll_offset_ticks = (int) m_hadjust->get_value();
    m_scroll_offset_x = m_scroll_offset_ticks / m_zoom;

    update_background();
    update_surface();
    force_draw();
}

void
seqroll::redraw_events()
{
    if ( m_ignore_redraw )
        return;

    update_surface();
    force_draw();
}

void
seqroll::set_ignore_redraw(bool a_ignore)
{
    m_ignore_redraw = a_ignore;
}

void
seqroll::draw_background_on_surface()
{
    Cairo::RefPtr<Cairo::Context> cr = Cairo::Context::create(m_surface_edit);

    cr->set_operator(Cairo::OPERATOR_CLEAR);
    cr->rectangle(0, 0, m_window_x, m_window_y);
    cr->paint_with_alpha(1.0);
    cr->set_operator(Cairo::OPERATOR_OVER);
    
    cr->set_source(m_surface_background, 0, 0);
    cr->paint();
}

/* updates background */
void
seqroll::update_background()
{
    Cairo::RefPtr<Cairo::Context> cr = Cairo::Context::create(m_surface_background);

    cr->set_operator(Cairo::OPERATOR_CLEAR);
    cr->rectangle(0, 0, m_window_x, m_window_y);
    cr->paint_with_alpha(1.0);
    cr->set_operator(Cairo::OPERATOR_OVER);

    /* clear background */
    cr->set_source_rgb(1.0, 1.0, 1.0);    // White FIXME
    cr->rectangle(0.0, 0.0, m_window_x, m_window_y );
    cr->stroke_preserve();
    cr->fill();
    
    /* dotted line vectors */
    static const std::vector<double> dashed = {1.0};
    static const std::vector<double> clear;

    /* draw horizontal grey lines */
    cr->set_source_rgb(0.6, 0.6, 0.6);    // Grey FIXME
    cr->set_line_join(Cairo::LINE_JOIN_MITER);
    cr->set_line_width(1.0);
    cr->set_dash(dashed, 1.0);

    for ( int i = 0; i < (m_window_y / c_key_y) + 1; i++ )
    {
        if (0 == (((c_num_keys - i) - m_scroll_offset_key + ( 12 - m_key )) % 12))
        {
            /* draw horz black lines at C */
            cr->set_line_width(1.0);
            cr->set_source_rgb(0.0, 0.0, 0.0);    // Black FIXME
            cr->set_dash(clear, 0.0);
            cr->set_line_join(Cairo::LINE_JOIN_MITER);
        }
        else if (11 == (((c_num_keys - i) - m_scroll_offset_key + ( 12 - m_key )) % 12))
        {
            /* draw horz grey lines for the other notes */
            cr->set_source_rgb(0.6, 0.6, 0.6);    // Grey FIXME
            cr->set_line_join(Cairo::LINE_JOIN_MITER);
            cr->set_line_width(1.0);
            cr->set_dash(dashed, 1.0);
        }

        cr->move_to(0.0, i * c_key_y);
        cr->line_to(m_window_x, i * c_key_y );
        cr->stroke();

        if ( m_scale != c_scale_off )
        {
            cr->set_source_rgb(0.5, 0.5, 0.5);    // Dark grey FIXME

            if ( !c_scales_policy[m_scale][ ((c_num_keys - i)
                                             - m_scroll_offset_key
                                             - 1 + ( 12 - m_key )) % 12] )
            {
                cr->rectangle(0.0, i * c_key_y + 1,
                                             m_window_x,
                                             c_key_y - 1 );
                cr->stroke_preserve();
                cr->fill();
            }
        }
    }

    /*int measure_length_64ths =  m_seq->get_bp_measure() * 64 /
        m_seq->get_bw();*/

    //printf ( "measure_length_64ths[%d]\n", measure_length_64ths );

    //int measures_per_line = (256 / measure_length_64ths) / (32 / m_zoom);
    //if ( measures_per_line <= 0
    int measures_per_line = 1;

    int ticks_per_measure =  m_seq->get_bp_measure() * (4 * c_ppqn) / m_seq->get_bw();
    int ticks_per_beat =  (4 * c_ppqn) / m_seq->get_bw();
    int ticks_per_step = 6 * m_zoom;
    int ticks_per_m_line =  ticks_per_measure * measures_per_line;
    int start_tick = m_scroll_offset_ticks -
                     (m_scroll_offset_ticks % ticks_per_step );
    int end_tick = (m_window_x * m_zoom) + m_scroll_offset_ticks;

    //printf ( "ticks_per_step[%d] start_tick[%d] end_tick[%d]\n",
    //         ticks_per_step, start_tick, end_tick );

    for ( int i = start_tick; i < end_tick; i += ticks_per_step )
    {
        int base_line = i / m_zoom;

        if ( i % ticks_per_m_line == 0 )
        {
            /* solid line on every beat */
            cr->set_source_rgb(0.0, 0.0, 0.0);    // Black FIXME
            cr->set_dash(clear, 0.0);
            cr->set_line_join(Cairo::LINE_JOIN_MITER);
        }
        else if (i % ticks_per_beat == 0 )
        {
            cr->set_source_rgb(0.5, 0.5, 0.5);    // Dark grey FIXME
            cr->set_dash(clear, 0.0);
            cr->set_line_join(Cairo::LINE_JOIN_MITER);
        }
        else
        {
            cr->set_source_rgb(0.6, 0.6, 0.6);    // Grey FIXME
            cr->set_line_join(Cairo::LINE_JOIN_MITER);
            cr->set_line_width(1.0);
            cr->set_dash(dashed, 1.0);

            int i_snap = i - (i % m_snap);

            if( i == i_snap )
            {
                cr->set_source_rgb(0.5, 0.5, 0.5);    // Dark grey FIXME
            }
            else
            {
                cr->set_source_rgb(0.6, 0.6, 0.6);    // Grey FIXME
            }
        }

        cr->move_to(base_line - m_scroll_offset_x, 0);
        cr->line_to(base_line - m_scroll_offset_x, m_window_y);
        cr->stroke();
    }
}

/* sets zoom, resets */
void
seqroll::set_zoom( int a_zoom )
{
    if ( m_zoom != a_zoom )
    {
        m_zoom = a_zoom;
        reset();
    }
}

/* simply sets the snap member */
void
seqroll::set_snap( int a_snap )
{
    m_snap = a_snap;
    reset();
}

void
seqroll::set_note_length( int a_note_length )
{
    m_note_length = a_note_length;
}

/* sets the music scale */
void
seqroll::set_scale( int a_scale )
{
    if ( m_scale != a_scale )
    {
        m_scale = a_scale;
        reset();
    }
}

/* sets the music chord */
void
seqroll::set_chord( int a_chord )
{
    if ( m_chord != a_chord )
    {
        m_chord = a_chord;
    }
}

/* sets the key */
void
seqroll::set_key( int a_key )
{
    if ( m_key != a_key )
    {
        m_key = a_key;
        reset();
    }
}

/* draws background surface on main surface,
   then puts the events on */
void
seqroll::update_surface()
{
    draw_background_on_surface();
    draw_events_on_surface();
}

/**
 * timeout
 */
void
seqroll::draw_progress_on_window()
{
    queue_draw();
}

/**
 * Gets called when queue_draw() is sent.
 * 
 * @param cr
 *      Gtkmm supplied cairo context for the window drawing.
 * 
 * @return 
 *      true.
 */
bool
seqroll::on_draw(const Cairo::RefPtr<Cairo::Context>& cr)
{
    static int last_scroll = 0;

    /* draw old */
    cr->set_source(m_surface_edit, 0.0, 0.0);
    cr->paint();

    long last_progress = m_old_progress_x;
    if(last_scroll < m_scroll_offset_x)
    {
        last_progress -= m_scroll_offset_x;
        last_scroll = m_scroll_offset_x;
    }

    m_old_progress_x = (m_seq->get_last_tick() / m_zoom) - m_scroll_offset_x;

//    printf("last_scroll %d\n", last_scroll);
//    printf("last_progress %ld: old_progress %d: scroll_offset %d\n", last_progress, m_old_progress_x, m_scroll_offset_x);
    
    if(m_old_progress_x < last_progress)
    {
        m_seq->set_loop_reset( true ); // for overwrite recording
        last_scroll = 0;
    }

    if ( m_old_progress_x != 0 )
    {
        cr->set_source_rgb(0.0, 0.0, 0.0);            // Black  FIXME
        cr->set_line_width(2.0);
        cr->move_to(m_old_progress_x, 0.0);
        cr->line_to(m_old_progress_x, m_window_y);
        cr->stroke();
    }
    
    draw_selection_on_window(cr);

    return true;
}

/* fills main surface with events */
void
seqroll::draw_events_on_surface()
{
    Cairo::RefPtr<Cairo::Context> cr = Cairo::Context::create(m_surface_edit);
    cr->set_operator(Cairo::OPERATOR_DEST);
    cr->set_operator(Cairo::OPERATOR_OVER);

    long tick_s;
    long tick_f;
    int note;

    int note_x;
    int note_width;
    int note_off_width = 0; // needed for wrapped note OFF
    int note_y;
    int note_height;

    bool selected;
    int velocity;
    draw_type dt;

    int start_tick = m_scroll_offset_ticks ;
    int end_tick = (m_window_x * m_zoom) + m_scroll_offset_ticks;

    sequence *seq = NULL;
    for( int method=0; method<2; ++method )
    {
        if ( method == 0 && m_drawing_background_seq  )
        {
            seq = m_perform->get_sequence( m_background_track, m_background_sequence );
            if ( seq == NULL )
            {
                method++;
            }
        }
        else if ( method == 0 )
        {
            method++;
        }

        if ( method==1)
        {
            seq = m_seq;
        }

        /* draw boxes from sequence */
        seq->reset_draw_marker();

        while ( (dt = seq->get_next_note_event( &tick_s, &tick_f, &note, &selected, &velocity )) != DRAW_FIN )
        {
            if ((tick_s >= start_tick && tick_s <= end_tick) ||
                    ( (dt == DRAW_NORMAL_LINKED) && (tick_f >= start_tick && tick_f <= end_tick))
               )
            {
                /* turn into screen corrids */
                note_x = tick_s / m_zoom;
                note_y = c_rollarea_y -(note * c_key_y) - c_key_y - 1 + 2;
                note_height = c_key_y - 3;

                				//printf( "DEBUG: drawing note[%d] tick_s[%ld] tick_f[%ld] start_tick[%d] end_tick[%d]\n",
                				//note, tick_s, tick_f, start_tick, end_tick );
                				//printf( "DEBUG: seq.get_length() = %ld\n",  m_seq->get_length());

                int in_shift = 0;
                int length_add = 0;

                if ( dt == DRAW_NORMAL_LINKED )
                {
                    if (tick_f >= tick_s)
                    {
                        note_width = (tick_f - tick_s) / m_zoom;
                        if ( note_width < 1 ) note_width = 1;
                    }
                    else    // this is wrap around
                    {
                        note_width = (m_seq->get_length() - tick_s) / m_zoom; // this is note ON width
                        note_off_width = tick_f / m_zoom;                     // needed for wrapped note OFF
                    }
                }
                else
                {
                    note_width = 16 / m_zoom;
                }

                if ( dt == DRAW_NOTE_ON )
                {
                    in_shift = 0;
                    length_add = 2;
                }

                if ( dt == DRAW_NOTE_OFF )
                {
                    in_shift = -1;
                    length_add = 1;
                }

                note_x -= m_scroll_offset_x;
                note_y -= m_scroll_offset_y;

                cr->set_source_rgb(0.0, 0.0, 0.6);    // Dark blue  FIXME

                /* draw boxes from sequence */
                /* method 0 is background sequence */
                if ( method == 0 )
                {
                    cr->set_source_rgb(0.0, 0.6, 0.0);    // Dark Green  FIXME
                }

                cr->rectangle(note_x,
                                    note_y,
                                    note_width,
                                    note_height);
                cr->stroke_preserve();
                cr->fill();

                /* if note wraps around to the beginning */
                if (tick_f < tick_s)
                {
                    cr->rectangle(0,
                                        note_y,
                                        tick_f/m_zoom,
                                        note_height);
                    cr->stroke_preserve();
                    cr->fill();
                }

                /*
                    Draw inside box if there is room.
                    The check for note_width is based on the note ON width. If the note ON
                    is less than 3 and there is a wrapped note OFF of width > 3 then the note
                    OFF portion would not draw the inside rectangle. Thus the need for the additional
                    note_off_width check.
                 */
                if ( note_width > 3  || note_off_width > 3)
                {
                    if ( selected )
                    {
                        cr->set_source_rgb(1.0, 0.27, 0.0);       // Red FIXME
                    }
                    else
                    {
                        cr->set_source_rgb(1.0, 1.0, 1.0);        // White FIXME
                    }

                    if ( method == 1 )
                    {
                        //printf("tick_f [%ld]: tick_s [%ld]: m_zoom [%d]:note_width [%d]\n", tick_f, tick_s, m_zoom, note_width);
                        if (tick_f >= tick_s)   // note is not wrapped
                        {
                            cr->rectangle(note_x + 1 + in_shift,
                                                note_y + 1,
                                                note_width - 3 + length_add,
                                                note_height - 3);
                            cr->stroke_preserve();
                            cr->fill();
                        }
                        else    // note is wrapped
                        {
                            /* note ON */
                            cr->rectangle(note_x + 1 + in_shift,
                                                note_y + 1,
                                                note_width,
                                                note_height - 3);
                            cr->stroke_preserve();
                            cr->fill();

                            /*
                                note OFF - wrapped. If the off_note_width is < 0 then the draw would
                                span the entire sequence (presumably because the negative is converted to
                                a positive??). This would occur occasionally when there was
                                a very small wrapped note OFF - due to rounding.
                             */
                            long off_note_width = (tick_f/m_zoom) - 3 + length_add;

                            if(off_note_width >= 0 )
                            {
                                cr->rectangle(0,
                                                    note_y + 1,
                                                    off_note_width,
                                                    note_height - 3);
                                cr->stroke_preserve();
                                cr->fill();
                            }
                        }
                    }
                }
            }
        }
    }
}

/**
 * The selection box
 * 
 * @param cr
 *  The on_draw() window context.
 */
void
seqroll::draw_selection_on_window(const Cairo::RefPtr<Cairo::Context>& cr)
{
    if ( !(m_selecting  ||  m_moving || m_paste ||  m_growing) )
    {
        return;
    }

    int x,y,w,h;

    /* Set line attributes */
    cr->set_line_join(Cairo::LINE_JOIN_MITER);
    cr->set_source_rgb(1.0, 0.27, 0.0);    // Red FIXME

    if ( m_selecting )
    {
        xy_to_rect ( m_drop_x,
                     m_drop_y,
                     m_current_x,
                     m_current_y,
                     &x, &y,
                     &w, &h );

        x -= m_scroll_offset_x;
        y -= m_scroll_offset_y;

        m_old.x = x;
        m_old.y = y;
        m_old.width = w;
        m_old.height = h + c_key_y;

        cr->rectangle(x, y, w, h + c_key_y );
        cr->stroke();
    }

    if ( m_moving || m_paste )
    {
        int delta_x = m_current_x - m_drop_x;
        int delta_y = m_current_y - m_drop_y;

        x = m_selected.x + delta_x;
        y = m_selected.y + delta_y;

        x -= m_scroll_offset_x;
        y -= m_scroll_offset_y;

        cr->rectangle(x, y, m_selected.width, m_selected.height );
        cr->stroke();

        m_old.x = x;
        m_old.y = y;
        m_old.width = m_selected.width;
        m_old.height = m_selected.height;
    }

    if ( m_growing )
    {
        int delta_x = m_current_x - m_drop_x;
        int width = delta_x + m_selected.width;

        if ( width < 1 )
            width = 1;

        x = m_selected.x;
        y = m_selected.y;

        x -= m_scroll_offset_x;
        y -= m_scroll_offset_y;

        cr->rectangle(x, y, width, m_selected.height );
        cr->stroke();

        m_old.x = x;
        m_old.y = y;
        m_old.width = width;
        m_old.height = m_selected.height;
    }
}


void
seqroll::force_draw()
{
    m_seqevent_wid->reset();    // needed for final refresh to draw over above changes
}

/* takes screen coordinates, give us notes and ticks */
void
seqroll::convert_xy( int a_x, int a_y, long *a_tick, int *a_note)
{
    *a_tick = a_x * m_zoom;
    *a_note = (c_rollarea_y - a_y - 2) / c_key_y;
}

/* notes and ticks to screen coordinates */
void
seqroll::convert_tn( long a_ticks, int a_note, int *a_x, int *a_y)
{
    *a_x = a_ticks /  m_zoom;
    *a_y = c_rollarea_y - ((a_note + 1) * c_key_y) - 1;
}

/* checks mins / maxes..  the fills in x,y
   and width and height */
void
seqroll::xy_to_rect( int a_x1,  int a_y1,
                     int a_x2,  int a_y2,
                     int *a_x,  int *a_y,
                     int *a_w,  int *a_h )
{
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

void
seqroll::convert_tn_box_to_rect( long a_tick_s, long a_tick_f,
                                 int a_note_h, int a_note_l,
                                 int *a_x, int *a_y,
                                 int *a_w, int *a_h )
{
    int x1, y1, x2, y2;

    /* convert box to X,Y values */
    convert_tn( a_tick_s, a_note_h, &x1, &y1 );
    convert_tn( a_tick_f, a_note_l, &x2, &y2 );

    xy_to_rect( x1, y1, x2, y2, a_x, a_y, a_w, a_h );

    *a_h += c_key_y;
}

void
seqroll::start_paste( )
{
    long tick_s;
    long tick_f;
    int note_h;
    int note_l;

    /* get the box that selected elements are in */
    if(!m_seq->get_clipboard_box( &tick_s, &note_h,&tick_f, &note_l ))
        return; // if nothing selected

    snap_x( &m_current_x );
    snap_y( &m_current_x );

    m_drop_x = m_current_x;
    m_drop_y = m_current_y;

    m_paste = true;

    convert_tn_box_to_rect( tick_s, tick_f, note_h, note_l,
                            &m_selected.x,
                            &m_selected.y,
                            &m_selected.width,
                            &m_selected.height );

    /* adjust for clipboard being shifted to tick 0 */
    m_selected.x += m_drop_x;
    m_selected.y += (m_drop_y - m_selected.y);
}

bool
seqroll::on_button_press_event(GdkEventButton* a_ev)
{
    if(!trans_button_press) // to avoid double button press on normal seq42 method
    {
        transport_follow = m_perform->get_follow_transport();
        m_perform->set_follow_transport(false);
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
seqroll::on_button_release_event(GdkEventButton* a_ev)
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

    m_perform->set_follow_transport(transport_follow);
    trans_button_press = false;

    return result;
}

bool
seqroll::on_motion_notify_event(GdkEventMotion* a_ev)
{
    bool result;

    switch (global_interactionmethod)
    {
    case e_fruity_interaction:
        result = m_fruity_interaction.on_motion_notify_event(a_ev, *this);
        break;
    case e_seq42_interaction:
        result = m_seq42_interaction.on_motion_notify_event(a_ev, *this);
        break;
    default:
        result = false;
    }
    return result;
}

/* performs a 'snap' on y */
void
seqroll::snap_y( int *a_y )
{
    *a_y = *a_y - (*a_y % c_key_y);
}

/* performs a 'snap' on x */
void
seqroll::snap_x( int *a_x )
{
    //snap = number pulses to snap to
    //m_zoom = number of pulses per pixel
    //so snap / m_zoom  = number pixels to snap to
    int mod = (m_snap / m_zoom);
    if ( mod <= 0 )
        mod = 1;

    *a_x = *a_x - (*a_x % mod );
}

bool
seqroll::on_enter_notify_event(GdkEventCrossing* a_p0)
{
    m_seqkeys_wid->set_hint_state( true );
    return false;
}

bool
seqroll::on_leave_notify_event(GdkEventCrossing* a_p0)
{
    m_seqkeys_wid->set_hint_state( false );
    return false;
}

bool
seqroll::on_focus_in_event(GdkEventFocus*)
{
    grab_focus();
    //m_seq->clear_clipboard();
    return false;
}

bool
seqroll::on_focus_out_event(GdkEventFocus*)
{
    //unset_flags(Gtk::HAS_FOCUS);
    return false;
}

bool
seqroll::on_key_press_event(GdkEventKey* a_p0)
{
    bool ret = false;
    // the start/end key may be the same key (i.e. SPACEBAR)
    // allow toggling when the same key is mapped to both triggers (i.e. SPACEBAR)
    bool dont_toggle = m_perform->m_key_start != m_perform->m_key_stop;
    if ( a_p0->keyval ==  m_perform->m_key_start && (dont_toggle || !global_is_running) )
    {
        if(!global_song_start_mode)
            m_seq->set_playing( m_toggle_play->get_active(),true );

        m_perform->start_playing();
        return true;
    }
    else if ( a_p0->keyval ==  m_perform->m_key_stop && (dont_toggle || global_is_running) )
    {
        m_perform->stop_playing();
        return true;
    }

    if ( a_p0->type == GDK_KEY_PRESS )
    {
        if ( a_p0->keyval ==  GDK_KEY_Delete || a_p0->keyval == GDK_KEY_BackSpace )
        {
            if(m_seq->mark_selected())
            {
                m_seq->push_undo();
                m_seq->remove_marked();
                ret = true;
            }
        }

        if ( a_p0->keyval ==  m_perform->m_key_follow_trans )
        {
            m_perform->toggle_follow_transport();
            return true;
        }

        if (!global_is_running)
        {
            if ( a_p0->keyval == GDK_KEY_Home )
            {
                m_seq->set_orig_tick(0);
                ret = true;
            }
            if ( ( a_p0->keyval == GDK_KEY_Left) && (a_p0->state & GDK_CONTROL_MASK))
            {
                m_seq->set_orig_tick(m_seq->get_last_tick()- m_snap);
                ret = true;
            }
            if ( (a_p0->keyval == GDK_KEY_Right) && (a_p0->state & GDK_CONTROL_MASK))
            {
                m_seq->set_orig_tick(m_seq->get_last_tick() + m_snap);
                ret = true;
            }

            if ( a_p0->keyval ==  m_perform->m_key_song )
            {
                m_perform->toggle_song_mode();
                return true;
            }
#ifdef JACK_SUPPORT
            if ( a_p0->keyval ==  m_perform->m_key_jack )
            {
                m_perform->toggle_jack_mode();
                return true;
            }
#endif // JACK_SUPPORT
        }

        if ( a_p0->keyval ==  GDK_KEY_Up )
        {
            if ( a_p0->state & GDK_SHIFT_MASK )
            {
                m_seq->transpose_notes(12, 0);
            }
            else
            {
                m_seq->transpose_notes(1, 0);
            }
            ret = true;
        }

        if ( a_p0->keyval ==  GDK_KEY_Down )
        {
            if ( a_p0->state & GDK_SHIFT_MASK )
            {
                m_seq->transpose_notes(-12, 0);
            }
            else
            {
                m_seq->transpose_notes(-1, 0);
            }
            ret = true;
        }

        if ( a_p0->keyval ==  GDK_KEY_Right )
        {
            if ( a_p0->state & GDK_SHIFT_MASK )
            {
                m_seq->shift_notes(1);
            }
            else if(!(a_p0->state & GDK_CONTROL_MASK))
            {
                m_seq->shift_notes(m_snap);
            }
            ret = true;
        }

        if ( a_p0->keyval ==  GDK_KEY_Left )
        {
            if ( a_p0->state & GDK_SHIFT_MASK )
            {
                m_seq->shift_notes(-1);
            }
            else if(!(a_p0->state & GDK_CONTROL_MASK))
            {
                m_seq->shift_notes(-m_snap);
            }
            ret = true;
        }

        if ( a_p0->state & GDK_MOD1_MASK )  // Alt key
        {
            if ( a_p0->keyval ==  GDK_KEY_q )
            {
                m_seq->quanize_events(EVENT_NOTE_ON, 0, m_snap, 1, true);
                ret = true;
            }
        }

        if ( a_p0->state & GDK_CONTROL_MASK )
        {
            /* cut */
            if ( a_p0->keyval == GDK_KEY_x || a_p0->keyval == GDK_KEY_X )
            {
                if(m_seq->mark_selected())
                {
                    m_seq->push_undo();
                    m_seq->copy_selected();
                    m_seq->remove_marked();
                    ret = true;
                }
            }
            /* copy */
            if ( a_p0->keyval == GDK_KEY_c || a_p0->keyval == GDK_KEY_C )
            {
                m_seq->copy_selected();
                ret = true;
            }

            /* paste */
            if ( a_p0->keyval == GDK_KEY_v || a_p0->keyval == GDK_KEY_V )
            {
                if(this->has_focus())
                {
                    start_paste();
                    ret = true;
                }
            }

            /* Undo */
            if ( a_p0->keyval == GDK_KEY_z || a_p0->keyval == GDK_KEY_Z )
            {
                m_seq->pop_undo();
                ret = true;
            }

            /* Redo */
            if ( a_p0->keyval == GDK_KEY_r || a_p0->keyval == GDK_KEY_R )
            {
                m_seq->pop_redo();
                ret = true;
            }

            /* select all events */
            if ( a_p0->keyval == GDK_KEY_a || a_p0->keyval == GDK_KEY_A )
            {
                m_seq->select_all();
                ret = true;
            }
        }
    }

    if ( ret == true )
    {
        m_seq->set_dirty();
        //redraw_events();
        return true;
    }

    return false;
}

void
seqroll::set_data_type( unsigned char a_status, unsigned char a_control = 0 )
{
    m_status = a_status;
    m_cc = a_control;
}

void
seqroll::on_size_allocate(Gtk::Allocation& a_r )
{
    Gtk::DrawingArea::on_size_allocate( a_r );

    m_window_x = a_r.get_width();
    m_window_y = a_r.get_height();

    update_sizes();
}

void
seqroll::follow_progress ()
{
    if( m_expanded_recording && m_seq->get_recording())
    {
        double h_max_value = ( m_seq->get_length() - (m_window_x * m_zoom));
        m_hadjust->set_value(h_max_value);
        
    }else    /* use for non recording */
    {
        long progress_tick = m_seq->get_last_tick();

        if (progress_tick > 0)
        {
            int progress_x = progress_tick / m_zoom + 10;
            int page = progress_x / m_window_x;
 
            if (page != m_scroll_page || (page == 0 && m_hadjust->get_value() != 0))
            {
                long left_tick = page * m_window_x * m_zoom;
                m_scroll_page = page;

                if((left_tick + m_hadjust->get_page_size()) >= m_hadjust->get_upper()) // don't scroll past upper
                    m_hadjust->set_value(m_hadjust->get_upper() - m_hadjust->get_page_size());
                else
                    m_hadjust->set_value(double(left_tick));
            }
        }
    }
}

void
seqroll::set_expanded_recording(bool a_record)
{
    m_expanded_recording = a_record;
}

bool
seqroll::get_expanded_record()
{
    return m_expanded_recording;
}

bool
seqroll::on_scroll_event( GdkEventScroll* a_ev )
{
    guint modifiers;    // Used to filter out caps/num lock etc.
    modifiers = gtk_accelerator_get_default_mod_mask ();

    /* This scroll event only handles basic scrolling without any
     * modifier keys such as GDK_CONTROL_MASK or GDK_SHIFT_MASK */
    if ((a_ev->state & modifiers) != 0)
        return false;

    double val = m_vadjust->get_value();

    if (a_ev->direction == GDK_SCROLL_UP)
    {
        val -= m_vadjust->get_step_increment()/6;
    }
    else if (a_ev->direction == GDK_SCROLL_DOWN)
    {
        val += m_vadjust->get_step_increment()/6;
    }
    else
    {
        return true;
    }

    m_vadjust->clamp_page( val, val + m_vadjust->get_page_size() );
    return true;
}

//////////////////////////
// interaction methods
//////////////////////////

void FruitySeqRollInput::updateMousePtr(seqroll& ths)
{
    // context sensitive mouse
    {
        long drop_tick;
        int drop_note;
        ths.convert_xy( ths.m_current_x, ths.m_current_y, &drop_tick, &drop_note );
        long start, end, note;
        if (ths.m_is_drag_pasting || ths.m_selecting || ths.m_moving || ths.m_growing || ths.m_paste)
        {
            ths.get_window()->set_cursor(Gdk::Cursor::create(ths.get_window()->get_display(),  Gdk::LEFT_PTR ));
        }
        else if (!m_adding &&
                 ths.m_seq->intersectNotes( drop_tick, drop_note, start, end, note ) && note == drop_note)
        {
            long handle_size = clamp( c_handlesize, 0, (end-start)/3 );
            if (start <= drop_tick && drop_tick <= start + handle_size)
            {
                ths.get_window()->set_cursor(Gdk::Cursor::create(ths.get_window()->get_display(),  Gdk::CENTER_PTR ));
            }
            else if (end - handle_size <= drop_tick && drop_tick <= end)
            {
                ths.get_window()->set_cursor(Gdk::Cursor::create(ths.get_window()->get_display(),  Gdk::LEFT_PTR ));
            }
            else
            {
                ths.get_window()->set_cursor(Gdk::Cursor::create(ths.get_window()->get_display(),  Gdk::CENTER_PTR ));
            }
        }
        else
        {
            ths.get_window()->set_cursor(Gdk::Cursor::create(ths.get_window()->get_display(),  Gdk::PENCIL ));
        }
    }
}

bool FruitySeqRollInput::on_button_press_event(GdkEventButton* a_ev, seqroll& ths)
{
    int numsel;

    long tick_s;
    long tick_f;
    int note_h;
    int note_l;

    int norm_x, norm_y, snapped_x, snapped_y;

    ths.grab_focus(  );

    bool needs_update = false;

    snapped_x = norm_x = (int) (a_ev->x + ths.m_scroll_offset_x );
    snapped_y = norm_y = (int) (a_ev->y + ths.m_scroll_offset_y );

    ths.snap_x( &snapped_x );
    ths.snap_y( &snapped_y );

    /* y is always snapped */
    ths.m_current_y = ths.m_drop_y = snapped_y;

    /* reset box that holds dirty redraw spot */
    ths.m_old.x = 0;
    ths.m_old.y = 0;
    ths.m_old.width = 0;
    ths.m_old.height = 0;

    // ctrl-v pressed, we're waiting for a click to show where to paste
    if ( ths.m_paste )
    {
        ths.convert_xy( snapped_x, snapped_y, &tick_s, &note_h );
        ths.m_paste = false;
        ths.m_seq->push_undo();
        ths.m_seq->paste_selected( tick_s, note_h );

        needs_update = true;
    }
    else
    {
        /*  left mouse button     */
        if ( a_ev->button == 1)
        {
            /* selection, normal x */
            ths.m_current_x = ths.m_drop_x = norm_x;

            /* turn x,y in to tick/note */
            ths.convert_xy( ths.m_drop_x, ths.m_drop_y, &tick_s, &note_h );

            // if not on top of event then add one...
            if ( m_canadd && ! ths.m_seq->select_note_events( tick_s, note_h,
                    tick_s, note_h,
                    sequence::e_would_select ) &&
                    !(a_ev->state & GDK_CONTROL_MASK) )
            {
                /* start the paint job */
                ths.m_painting = true;
                m_adding = true;

                /* adding, snapped x */
                ths.m_current_x = ths.m_drop_x = snapped_x;
                ths.convert_xy( ths.m_drop_x, ths.m_drop_y, &tick_s, &note_h );

                ths.m_seqkeys_wid->set_listen_button_press(a_ev); // play note

                // test if a note is already there
                // fake select, if so, no add
                if ( ! ths.m_seq->select_note_events( tick_s, note_h,
                                                      tick_s, note_h,
                                                      sequence::e_would_select ))
                {
                    /* add note, length = little less than snap */
                    ths.m_seq->push_undo();

                    if(ths.m_chord == 0) // single note
                    {
                        ths.m_seq->add_note( tick_s, ths.m_note_length - c_note_off_margin, note_h, true );
                    }
                    else                 // chords
                    {
                        for(int i = 0; i < c_chord_size; i++)
                        {
                            if(c_chord_table[ths.m_chord][i] == -1)
                                break;

                            ths.m_seq->add_note(tick_s,
                                                ths.m_note_length - c_note_off_margin,
                                                note_h + c_chord_table[ths.m_chord][i],
                                                false );
                        }
                    }
                    needs_update = true;
                }
            }
            else /* selecting */
            {
                // if the under the cursor is not a selected note...
                if ( !ths.m_seq->select_note_events( tick_s, note_h,
                                                     tick_s, note_h,
                                                     sequence::e_is_selected ))
                {
                    // if clicking a note ...
                    if (ths.m_seq->select_note_events( tick_s,note_h,tick_s,note_h,
                                                       sequence::e_would_select ) )
                    {
                        // ... unselect all if ctrl not held
                        if (! (a_ev->state & GDK_CONTROL_MASK))
                            ths.m_seq->unselect();
                    }
                    // if clicking empty space ...
                    else
                    {
                        // ... unselect all if ctrl-shift not held
                        if (! ((a_ev->state & GDK_CONTROL_MASK) &&
                                (a_ev->state & GDK_SHIFT_MASK)) )
                            ths.m_seq->unselect();
                    }

                    /* on direct click select only one event */
                    numsel = ths.m_seq->select_note_events( tick_s,note_h,tick_s,note_h,
                                                            sequence::e_select_one );
                    // prevent deselect in button_release()
                    if (numsel)
                        ths.m_justselected_one = true;

                    // if nothing selected, start the selection box
                    if (numsel == 0 && (a_ev->state & GDK_CONTROL_MASK))
                        ths.m_selecting = true;

                    needs_update = true;
                }

                // if note under cursor is selected
                if ( ths.m_seq->select_note_events( tick_s, note_h,
                                                    tick_s, note_h,
                                                    sequence::e_is_selected ))
                {
                    // context sensitive mouse
                    //bool left_mouse_handle = false;
                    bool right_mouse_handle = false;
                    bool center_mouse_handle = false;
                    {
                        long drop_tick;
                        int drop_note;
                        ths.convert_xy( ths.m_drop_x, ths.m_drop_y, &drop_tick, &drop_note );
                        long start, end, note;
                        if (ths.m_seq->intersectNotes( drop_tick, drop_note, start, end, note ) && note == drop_note)
                        {
                            long handle_size = clamp( c_handlesize, 0, (end-start)/3 ); // 16 wide unless very small...
                            if (start <= drop_tick && drop_tick <= start + handle_size)
                            {
                                //left_mouse_handle = true; // not supported yet
                                center_mouse_handle = true;
                            }
                            else if (end - handle_size <= drop_tick && drop_tick <= end)
                            {
                                right_mouse_handle = true;
                            }
                            else
                            {
                                center_mouse_handle = true;
                            }
                        }
                    }

                    // grab/move the note
                    if ( center_mouse_handle &&
                            a_ev->button == 1 && !(a_ev->state & GDK_CONTROL_MASK) )
                    {
                        ths.m_seqkeys_wid->set_listen_button_press(a_ev); // play note

                        ths.m_moving_init = true;
                        needs_update = true;

                        /* get the box that selected elements are in */
                        ths.m_seq->get_selected_box( &tick_s, &note_h,
                                                     &tick_f, &note_l );


                        ths.convert_tn_box_to_rect( tick_s, tick_f, note_h, note_l,
                                                    &ths.m_selected.x,
                                                    &ths.m_selected.y,
                                                    &ths.m_selected.width,
                                                    &ths.m_selected.height );

                        /* save offset that we get from the snap above */
                        int adjusted_selected_x = ths.m_selected.x;
                        ths.snap_x( &adjusted_selected_x );
                        ths.m_move_snap_offset_x = ( ths.m_selected.x - adjusted_selected_x);

                        /* align selection for drawing */
                        ths.snap_x( &ths.m_selected.x );

                        ths.m_current_x = ths.m_drop_x = snapped_x;
                    }
                    // ctrl left click when stuff is already selected
                    else if (a_ev->button == 1 && (a_ev->state & GDK_CONTROL_MASK) &&
                             ths.m_seq->select_note_events( tick_s, note_h,
                                                            tick_s, note_h,
                                                            sequence::e_is_selected ))
                    {
                        ths.m_is_drag_pasting_start = true;
                        m_drag_paste_start_pos[0] = (long)a_ev->x;
                        m_drag_paste_start_pos[1] = (long)a_ev->y;
                        //printf( "start: %lf  %lf\n", a_ev->x, a_ev->y );
                    }

                    /* left click on the right handle - grow/resize event  */
                    if ( (right_mouse_handle &&
                            a_ev->button == 1 && ! (a_ev->state & GDK_CONTROL_MASK)) ||
                            a_ev->button == 2 )
                    {
                        ths.m_growing = true;

                        /* get the box that selected elements are in */
                        ths.m_seq->get_selected_box( &tick_s, &note_h,
                                                     &tick_f, &note_l );

                        ths.convert_tn_box_to_rect( tick_s, tick_f, note_h, note_l,
                                                    &ths.m_selected.x,
                                                    &ths.m_selected.y,
                                                    &ths.m_selected.width,
                                                    &ths.m_selected.height );
                    }
                }
            }
        }

        /*     right click      */
        if ( a_ev->button == 3 )
        {
            /* selection, normal x */
            ths.m_current_x = ths.m_drop_x = norm_x;

            /* turn x,y in to tick/note */
            ths.convert_xy( ths.m_drop_x, ths.m_drop_y, &tick_s, &note_h );

            // erase event(s) under cursor if there is one
            if ( ths.m_seq->select_note_events( tick_s, note_h,
                                                tick_s, note_h,
                                                sequence::e_would_select) )
            {
                /* right ctrl click: remove all selected notes */
                if (a_ev->state & GDK_CONTROL_MASK)
                {
                    ths.m_seq->select_note_events( tick_s, note_h,
                                                   tick_s, note_h,
                                                   sequence::e_select_one );

                    if(ths.m_seq->mark_selected())
                    {
                        ths.m_seq->push_undo();
                        ths.m_seq->remove_marked();
                    }
                }
                /* right click: remove only the note under the cursor,
                   leave the selection intact */
                else
                {
                    ths.m_seq->push_undo();
                    ths.m_seq->select_note_events( tick_s, note_h,
                                                   tick_s, note_h,
                                                   sequence::e_remove_one );
                }

                // hold down the right button, drag mouse around erasing notes:
                m_erase_painting = true;

                // repaint... we've changed the notes.
                needs_update = true;
            }
            else /* selecting */
            {
                if  ( ! (a_ev->state & GDK_CONTROL_MASK) )
                    ths.m_seq->unselect();

                // nothing selected, start the selection box
                ths.m_selecting = true;
                needs_update = true;
            }
        }
    }

    // context sensative mouse pointer...
    updateMousePtr( ths );

    /* if they clicked, something changed */
    if ( needs_update )
    {
        ////printf( "needs update\n" );
        ths.m_seq->set_dirty();
        //redraw_events();
    }
    return true;
}

bool FruitySeqRollInput::on_button_release_event(GdkEventButton* a_ev, seqroll& ths)
{
    ths.m_seqkeys_wid->set_listen_button_release(a_ev); // play note off

    long tick_s;
    long tick_f;
    int note_h;
    int note_l;
    int x,y,w,h;

    bool needs_update = false;

    ths.m_current_x = (int) (a_ev->x + ths.m_scroll_offset_x );
    ths.m_current_y = (int) (a_ev->y + ths.m_scroll_offset_y );

    ths.snap_y( &ths.m_current_y );

    if ( ths.m_moving || ths.m_is_drag_pasting )
        ths.snap_x( &ths.m_current_x );

    int delta_x = ths.m_current_x - ths.m_drop_x;
    int delta_y = ths.m_current_y - ths.m_drop_y;

    long delta_tick;
    int delta_note;

    // middle click, or ctrlleft click button up
    if ( a_ev->button == 1 ||
            a_ev->button == 2 )
    {
        if ( ths.m_growing )
        {
            /* convert deltas into screen corridinates */
            ths.convert_xy( delta_x, delta_y, &delta_tick, &delta_note );

            if ( a_ev->state & GDK_SHIFT_MASK )
                ths.m_seq->stretch_selected( delta_tick );
            else
                ths.m_seq->grow_selected( delta_tick );

            needs_update = true;
        }
    }
    long int current_tick;
    int current_note;
    ths.convert_xy( ths.m_current_x, ths.m_current_y, &current_tick, &current_note );

    // ctrl-left click button up for select/drag copy/paste
    // left click button up for ending a move of selected notes
    if ( a_ev->button == 1 )
    {
        m_adding = false;
        if ( ths.m_is_drag_pasting )
        {
            ths.m_is_drag_pasting = false;
            ths.m_is_drag_pasting_start = false;

            /* convert deltas into screen corridinates */
            ths.m_paste = false;
            ths.m_seq->push_undo();
            ths.m_seq->paste_selected( current_tick, current_note );

            needs_update = true;
            //m_seq->unselect();
        }
        // ctrl-left click but without movement - select a note
        if (ths.m_is_drag_pasting_start)
        {
            ths.m_is_drag_pasting_start = false;

            // if ctrl-left click without movement and
            // if note under cursor is selected, and ctrl is held
            // and buttondown didn't just select one
            if (!ths.m_justselected_one &&
                    ths.m_seq->select_note_events( current_tick, current_note,
                                                   current_tick, current_note,
                                                   sequence::e_is_selected ) &&
                    (a_ev->state & GDK_CONTROL_MASK))
            {
                // deselect the note
                ths.m_seq->select_note_events( current_tick, current_note,
                                               current_tick, current_note,
                                               sequence::e_deselect );
                needs_update = true;
            }
        }
        ths.m_justselected_one = false; // clear flag on left button up

        if (  ths.m_moving  )
        {
            /* adjust for snap */
            delta_x -= ths.m_move_snap_offset_x;

            /* convert deltas into screen corridinates */
            ths.convert_xy( delta_x, delta_y, &delta_tick, &delta_note );

            /* since delta_note was from delta_y, it will be filpped
               ( delta_y[0] = note[127], etc.,so we have to adjust */
            delta_note = delta_note - (c_num_keys-1);

            ths.m_seq->move_selected_notes( delta_tick, delta_note );
            needs_update = true;
        }
    }

    // right click or leftctrl click button up for selection box
    if ( a_ev->button == 3 || a_ev->button == 1 )
    {
        if ( ths.m_selecting )
        {
            ths.xy_to_rect ( ths.m_drop_x,
                             ths.m_drop_y,
                             ths.m_current_x,
                             ths.m_current_y,
                             &x, &y,
                             &w, &h );

            ths.convert_xy( x,     y, &tick_s, &note_h );
            ths.convert_xy( x+w, y+h, &tick_f, &note_l );

            ths.m_seq->select_note_events( tick_s, note_h, tick_f, note_l,
                                           sequence::e_toggle_selection );

            needs_update = true;
        }
    }
    if ( a_ev->button == 3 )
    {
        m_erase_painting = false;
    }

    /* turn off */
    ths.m_selecting = false;
    ths.m_moving = false;
    ths.m_growing = false;
    ths.m_paste = false;
    ths.m_moving_init = false;
    ths.m_painting = false;

    ths.m_seq->unpaint_all();

    // context sensative mouse pointer...
    updateMousePtr( ths );

    /* if they clicked, something changed */
    if (  needs_update )
    {
        ////printf( "needs_update2\n" );
        ths.m_seq->set_dirty();
        //redraw_events();
    }
    return true;
}

bool FruitySeqRollInput::on_motion_notify_event(GdkEventMotion* a_ev, seqroll& ths)
{
    ths.m_current_x = (int) (a_ev->x  + ths.m_scroll_offset_x );
    ths.m_current_y = (int) (a_ev->y  + ths.m_scroll_offset_y );

    int note;
    long tick;

    if ( ths.m_moving_init )
    {
        ths.m_moving_init = false;
        ths.m_moving = true;
    }

    // context sensitive mouse pointer...
    updateMousePtr( ths );

    // ctrl-left click drag on selected note(s) starts a copy/unselect/paste
    // don't begin the paste until mouse moves a few pixels, filter out the unsteady hand
    if ( ths.m_is_drag_pasting_start &&
            (6 <= abs(m_drag_paste_start_pos[0] - (long)a_ev->x) ||
             6 <= abs(m_drag_paste_start_pos[1] - (long)a_ev->y))  )
    {
        ths.m_seq->copy_selected();
        ths.m_seq->unselect();
        ths.start_paste();

        ths.m_is_drag_pasting_start = false;
        ths.m_is_drag_pasting = true;
    }

    ths.snap_y( &ths.m_current_y );
    ths.convert_xy( 0, ths.m_current_y, &tick, &note );

    ths.m_seqkeys_wid->set_hint_key( note );

    if ( ths.m_selecting || ths.m_moving || ths.m_growing || ths.m_paste )
    {
        if ( ths.m_moving || ths.m_paste )
        {
            ths.snap_x( &ths.m_current_x );
        }

        if(ths.m_moving)
        {
            ths.m_seqkeys_wid->set_listen_motion_notify(a_ev);//  play note
        }
        return true;
    }

    if ( ths.m_painting )
    {
        if(ths.m_chord != 0) // chord, don't allow move paint
            return true;

        ths.snap_x( &ths.m_current_x );
        ths.convert_xy( ths.m_current_x, ths.m_current_y, &tick, &note );

        ths.m_seq->add_note( tick, ths.m_note_length - c_note_off_margin, note, true );

        ths.m_seqkeys_wid->set_listen_motion_notify(a_ev);//  play note

        return true;
    }

    if (m_erase_painting)
    {
        ths.convert_xy( ths.m_current_x, ths.m_current_y, &tick, &note );
        if ( ths.m_seq->select_note_events( tick, note,
                                            tick, note,
                                            sequence::e_would_select) )
        {
            /* remove only the note under the cursor,
               leave the selection intact */
            ths.m_seq->push_undo();
            ths.m_seq->select_note_events( tick, note,
                                           tick, note,
                                           sequence::e_remove_one );
            ths.m_seq->set_dirty();
        }
    }
    return false;
}

/* popup menu calls this */
void
Seq42SeqRollInput::set_adding( bool a_adding, seqroll& ths )
{
    if ( a_adding )
    {
        ths.get_window()->set_cursor(Gdk::Cursor::create(ths.get_window()->get_display(),  Gdk::PENCIL ));
        m_adding = true;
    }
    else
    {
        ths.get_window()->set_cursor(Gdk::Cursor::create(ths.get_window()->get_display(),  Gdk::LEFT_PTR ));
        m_adding = false;
    }
}

bool Seq42SeqRollInput::on_button_press_event(GdkEventButton* a_ev, seqroll& ths)
{
    int numsel;

    long tick_s;
    long tick_f;
    int note_h;
    int note_l;

    int norm_x, norm_y, snapped_x, snapped_y;

    ths.grab_focus(  );

    bool needs_update = false;

    snapped_x = norm_x = (int) (a_ev->x + ths.m_scroll_offset_x );
    snapped_y = norm_y = (int) (a_ev->y + ths.m_scroll_offset_y );

    ths.snap_x( &snapped_x );
    ths.snap_y( &snapped_y );

    /* y is always snapped */
    ths.m_current_y = ths.m_drop_y = snapped_y;

    /* reset box that holds dirty redraw spot */
    ths.m_old.x = 0;
    ths.m_old.y = 0;
    ths.m_old.width = 0;
    ths.m_old.height = 0;

    if ( ths.m_paste )
    {
        ths.convert_xy( snapped_x, snapped_y, &tick_s, &note_h );
        ths.m_paste = false;
        ths.m_seq->push_undo();
        ths.m_seq->paste_selected( tick_s, note_h );
        needs_update = true;
    }
    else
    {
        /*  left mouse button     */
        if ( a_ev->button == 1 || a_ev->button == 2 )
        {
            /* selection, normal x */
            ths.m_current_x = ths.m_drop_x = norm_x;

            /* turn x,y in to tick/note */
            ths.convert_xy( ths.m_drop_x, ths.m_drop_y, &tick_s, &note_h );

            if ( m_adding )
            {
                /* start the paint job */
                ths.m_painting = true;

                /* adding, snapped x */
                ths.m_current_x = ths.m_drop_x = snapped_x;
                ths.convert_xy( ths.m_drop_x, ths.m_drop_y, &tick_s, &note_h );

                ths.m_seqkeys_wid->set_listen_button_press(a_ev); // play note
                // test if a note is already there
                // fake select, if so, no add
                if ( ! ths.m_seq->select_note_events( tick_s, note_h,
                                                      tick_s, note_h,
                                                      sequence::e_would_select ))
                {
                    /* add note, length = little less than snap */
                    ths.m_seq->push_undo();

                    if(ths.m_chord == 0) // single note
                    {
                        ths.m_seq->add_note( tick_s, ths.m_note_length - c_note_off_margin, note_h, true );
                    }
                    else                 // chords
                    {
                        for(int i = 0; i < c_chord_size; i++)
                        {
                            if(c_chord_table[ths.m_chord][i] == -1)
                                break;

                            ths.m_seq->add_note(tick_s,
                                                ths.m_note_length - c_note_off_margin,
                                                note_h + c_chord_table[ths.m_chord][i],
                                                false );
                        }
                    }

                    needs_update = true;
                }
            }
            else /* selecting */
            {
                if ( !ths.m_seq->select_note_events( tick_s, note_h,
                                                     tick_s, note_h,
                                                     sequence::e_is_selected ))
                {
                    if ( ! (a_ev->state & GDK_CONTROL_MASK) )
                    {
                        ths.m_seq->unselect();
                    }

                    /* on direct click select only one event */
                    numsel = ths.m_seq->select_note_events( tick_s,note_h,tick_s,note_h,
                                                            sequence::e_select_one );

                    /* none selected, start selection box */
                    if ( numsel == 0 )
                    {
                        if ( a_ev->button == 1 )
                            ths.m_selecting = true;
                    }
                    else
                    {
                        needs_update = true;
                    }
                }

                if ( ths.m_seq->select_note_events( tick_s, note_h,
                                                    tick_s, note_h,
                                                    sequence::e_is_selected ))
                {
                    ths.m_seqkeys_wid->set_listen_button_press(a_ev); // play note
                    // moving - left click only
                    if ( a_ev->button == 1 && !(a_ev->state & GDK_CONTROL_MASK) )
                    {
                        ths.m_moving_init = true;
                        needs_update = true;

                        /* get the box that selected elements are in */
                        ths.m_seq->get_selected_box( &tick_s, &note_h,
                                                     &tick_f, &note_l );


                        ths.convert_tn_box_to_rect( tick_s, tick_f, note_h, note_l,
                                                    &ths.m_selected.x,
                                                    &ths.m_selected.y,
                                                    &ths.m_selected.width,
                                                    &ths.m_selected.height );

                        /* save offset that we get from the snap above */
                        int adjusted_selected_x = ths.m_selected.x;
                        ths.snap_x( &adjusted_selected_x );
                        ths.m_move_snap_offset_x = ( ths.m_selected.x - adjusted_selected_x);

                        /* align selection for drawing */
                        ths.snap_x( &ths.m_selected.x );

                        ths.m_current_x = ths.m_drop_x = snapped_x;
                    }

                    /* middle mouse button, or left-ctrl click (for 2button mice) */
                    if ( a_ev->button == 2 ||
                            (a_ev->button == 1 && (a_ev->state & GDK_CONTROL_MASK)) )
                    {
                        /* moving, normal x */
                        //m_current_x = m_drop_x = norm_x;
                        //convert_xy( m_drop_x, m_drop_y, &tick_s, &note_h );

                        ths.m_growing = true;

                        /* get the box that selected elements are in */
                        ths.m_seq->get_selected_box( &tick_s, &note_h,
                                                     &tick_f, &note_l );

                        ths.convert_tn_box_to_rect( tick_s, tick_f, note_h, note_l,
                                                    &ths.m_selected.x,
                                                    &ths.m_selected.y,
                                                    &ths.m_selected.width,
                                                    &ths.m_selected.height );
                    }
                }
            }
        }

        /*     right mouse button      */
        if ( a_ev->button == 3 )
        {
            set_adding( true, ths );
        }
    }

    /* if they clicked, something changed */
    if ( needs_update )
    {
        ////printf( "needs update\n" );
        ths.m_seq->set_dirty();
        //redraw_events();
    }
    return true;
}

bool Seq42SeqRollInput::on_button_release_event(GdkEventButton* a_ev, seqroll& ths)
{
    ths.m_seqkeys_wid->set_listen_button_release(a_ev); // play note off

    long tick_s;
    long tick_f;
    int note_h;
    int note_l;
    int x,y,w,h;

    bool needs_update = false;

    ths.m_current_x = (int) (a_ev->x + ths.m_scroll_offset_x );
    ths.m_current_y = (int) (a_ev->y + ths.m_scroll_offset_y );

    ths.snap_y ( &ths.m_current_y );

    if ( ths.m_moving )
        ths.snap_x( &ths.m_current_x );

    int delta_x = ths.m_current_x - ths.m_drop_x;
    int delta_y = ths.m_current_y - ths.m_drop_y;

    long delta_tick;
    int delta_note;

    if ( a_ev->button == 1 )
    {
        if ( ths.m_selecting )
        {
            ths.xy_to_rect ( ths.m_drop_x,
                             ths.m_drop_y,
                             ths.m_current_x,
                             ths.m_current_y,
                             &x, &y,
                             &w, &h );

            ths.convert_xy( x,     y, &tick_s, &note_h );
            ths.convert_xy( x+w, y+h, &tick_f, &note_l );

            ths.m_seq->select_note_events( tick_s, note_h, tick_f, note_l, sequence::e_select );
            needs_update = true;
        }

        if (  ths.m_moving  )
        {
            /* adjust for snap */
            delta_x -= ths.m_move_snap_offset_x;

            /* convert deltas into screen corridinates */
            ths.convert_xy( delta_x, delta_y, &delta_tick, &delta_note );

            /* since delta_note was from delta_y, it will be filpped
               ( delta_y[0] = note[127], etc.,so we have to adjust */
            delta_note = delta_note - (c_num_keys-1);

            ths.m_seq->move_selected_notes( delta_tick, delta_note );
            needs_update = true;
        }
    }

    if ( a_ev->button == 2 || a_ev->button == 1 )
    {
        if ( ths.m_growing )
        {
            /* convert deltas into screen corridinates */
            ths.convert_xy( delta_x, delta_y, &delta_tick, &delta_note );

            if ( a_ev->state & GDK_SHIFT_MASK )
            {
                ths.m_seq->stretch_selected( delta_tick );
            }
            else
            {
                ths.m_seq->grow_selected( delta_tick );
            }

            needs_update = true;
        }
    }

    if ( a_ev->button == 3 )
    {
        set_adding( false, ths );
    }

    /* turn off */
    ths.m_selecting = false;
    ths.m_moving = false;
    ths.m_growing = false;
    ths.m_paste = false;
    ths.m_moving_init = false;
    ths.m_painting = false;

    ths.m_seq->unpaint_all();

    /* if they clicked, something changed */
    if (  needs_update )
    {
        ////printf( "needs_update2\n" );
        ths.m_seq->set_dirty();
        //redraw_events();
    }
    return true;
}

bool Seq42SeqRollInput::on_motion_notify_event(GdkEventMotion* a_ev, seqroll& ths)
{
    ths.m_current_x = (int) (a_ev->x  + ths.m_scroll_offset_x );
    ths.m_current_y = (int) (a_ev->y  + ths.m_scroll_offset_y );

    int note;
    long tick;

    if ( ths.m_moving_init )
    {
        ths.m_moving_init = false;
        ths.m_moving = true;
    }

    ths.snap_y( &ths.m_current_y );
    ths.convert_xy( 0, ths.m_current_y, &tick, &note );

    ths.m_seqkeys_wid->set_hint_key( note );

    if ( ths.m_selecting || ths.m_moving || ths.m_growing || ths.m_paste )
    {
        if ( ths.m_moving || ths.m_paste )
        {
            ths.snap_x( &ths.m_current_x );
        }

        if(ths.m_moving)
        {
            ths.m_seqkeys_wid->set_listen_motion_notify(a_ev);//  play note
        }
        return true;
    }

    if ( ths.m_painting )
    {
        if(ths.m_chord != 0) // chord, don't allow move paint
            return true;

        ths.snap_x( &ths.m_current_x );
        ths.convert_xy( ths.m_current_x, ths.m_current_y, &tick, &note );

        ths.m_seq->add_note( tick, ths.m_note_length - c_note_off_margin, note, true );

        ths.m_seqkeys_wid->set_listen_motion_notify(a_ev);//  play note

        return true;
    }

    return false;
}
