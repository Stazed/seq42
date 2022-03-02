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
#include "perfnames.h"
#include "mainwnd.h"

perfnames::perfnames( perform *a_perf, mainwnd *a_main, Glib::RefPtr<Adjustment> a_vadjust ):
    trackmenu(a_perf, a_main),
    m_mainperf(a_perf),
    m_vadjust(a_vadjust),
    m_redraw_tracks(false),
    m_track_offset(0),
    m_button_down(false),
    m_moving(false)
{
    Gtk::Allocation allocation = get_allocation();
    m_surface = Cairo::ImageSurface::create(
        Cairo::Format::FORMAT_ARGB32,
        allocation.get_width(),
        allocation.get_height()
    );

    add_events( Gdk::BUTTON_PRESS_MASK |
                Gdk::BUTTON_RELEASE_MASK |
                Gdk::BUTTON_MOTION_MASK |
                Gdk::SCROLL_MASK );

    /* set default size */
    set_size_request( c_names_x, 100 );

    m_vadjust->signal_value_changed().connect( mem_fun( *(this), &perfnames::change_vert ));

    set_double_buffered( false );

    for( int i=0; i<c_max_track; ++i )
        m_track_active[i]=false;
}

void
perfnames::on_realize()
{
    // we need to do the default realize
    Gtk::DrawingArea::on_realize();
}

void
perfnames::change_vert( )
{
    if ( m_track_offset != (int) m_vadjust->get_value() )
    {
        m_track_offset = (int) m_vadjust->get_value();
        m_redraw_tracks = true;
        queue_draw();
    }
}

void
perfnames::redraw( int track )
{
    draw_track( track);
}

void
perfnames::draw_track( int track )
{
    int i = track - m_track_offset;
    Cairo::RefPtr<Cairo::Context> cr = Cairo::Context::create(m_surface);

    Pango::FontDescription font;
    int text_width;
    int text_height;

    font.set_family(c_font);
    font.set_size((c_key_fontsize - 1) * Pango::SCALE);
    font.set_weight(Pango::WEIGHT_NORMAL);

    cr->set_operator(Cairo::OPERATOR_CLEAR);
    cr->rectangle(0, (c_names_y * i), m_window_x - 1, c_names_y);
    cr->paint_with_alpha(0.0);
    cr->set_operator(Cairo::OPERATOR_OVER);

    cr->set_line_width(2.0);

    if ( track < c_max_track )
    {
        cr->set_source_rgb(0.0, 0.0, 0.0);        // Black
        cr->rectangle(0, (c_names_y * i), m_window_x - 1, c_names_y);
        cr->stroke_preserve();
        cr->fill();

        if ( m_mainperf->is_active_track( track ))
        {
            cr->set_source_rgb(1.0, 1.0, 1.0);    // White
        }
        else
        {
            cr->set_source_rgb(0.6, 0.8, 1.0);    // blue
        }

        cr->rectangle(3, (c_names_y * i) + 3, m_window_x - 4, c_names_y - 4);
        cr->stroke_preserve();
        cr->fill();

        if ( m_mainperf->is_active_track( track ))
        {
            m_track_active[track]=true;

            /* Track Bus */
            char str[30];
            snprintf
            (
                str, sizeof(str),
                "[%d] Bus %d Ch %d",
                track + 1,
                m_mainperf->get_track(track)->get_midi_bus()+1,
                m_mainperf->get_track(track)->get_midi_channel()+1
            );

            // set background for bus
            cr->set_source_rgb(1.0, 1.0, 1.0);    // White
            cr->set_line_width(1.0);

            // draw the white background for the bus
            cr->rectangle(5, c_names_y * i + 3, (strlen(str) * 5) + 1, 6.0);
            cr->stroke_preserve();
            cr->fill();

            // print the string
            auto t = create_pango_layout(str);
            t->set_font_description(font);
            t->get_pixel_size(text_width, text_height);

            cr->set_source_rgb(0.0, 0.0, 0.0);    // Black
            cr->move_to(5, (c_names_y * i) + 1);

            t->show_in_cairo_context(cr);

            /* Track name */
            char name[20];
            snprintf(name, sizeof(name), "%-16.16s",
                     m_mainperf->get_track(track)->get_name());

            // set background for name
            cr->set_source_rgb(1.0, 1.0, 1.0);    // White

            // draw the white background for the name
            cr->rectangle(5, c_names_y * i + 12, (strlen(name) * 5) + 1, 10.0);
            cr->stroke_preserve();
            cr->fill();

            // print the track name
            auto n = create_pango_layout(name);
            n->set_font_description(font);
            n->get_pixel_size(text_width, text_height);

            cr->set_source_rgb(0.0, 0.0, 0.0);    // Black
            cr->move_to(5, (c_names_y * i) + ( (c_names_y * .5) - (text_height * .5) ) + 6);

            n->show_in_cairo_context(cr);

            /* The Play, Mute, Solo button */
            bool fill = false;
            bool solo = m_mainperf->get_track(track)->get_song_solo();
            bool muted = m_mainperf->get_track(track)->get_song_mute();

            // solo or muted we fill the background, play we do not
            if(solo || muted)
                fill = true;

            cr->set_source_rgb(0.0, 0.0, 0.0);    // Black

            if(muted)
            {
                cr->set_source_rgb(1.0, 0.27, 0.0);    // Red
            }
            if(solo)
            {
                cr->set_source_rgb(0.5, 0.988, 0.0);    // Green
            }

            cr->rectangle( m_window_x - 11, (c_names_y * i) + 2, 11, c_names_y - 2);
            if ( fill )
            {
                cr->stroke_preserve();
                cr->fill();
            }
            else
            {
                cr->stroke();
            }

            /* Play, Mute, Solo label (P,M,S) */
            char smute[5];
 
            if ( muted )
            {
                snprintf(smute, sizeof(smute), "M" );
                // background
                cr->set_source_rgb(0.0, 0.0, 0.0);    // Black
            }
            else if ( solo )
            {
                snprintf(smute, sizeof(smute), "S" );
                // background
                cr->set_source_rgb(0.0, 0.0, 0.0);    // Black
            }
            else
            {
                snprintf(smute, sizeof(smute), "P" );
                // background 
                cr->set_source_rgb(1.0, 1.0, 1.0);    // White
            }
            
            auto m = create_pango_layout(smute);
            m->set_font_description(font);
            m->get_pixel_size(text_width, text_height);

            // draw the background for the mute label
            cr->rectangle(m_window_x - text_width - 3 , c_names_y * i + (text_height * .5)  , text_width + 1, (text_height * .5) + 5 );
            cr->stroke_preserve();
            cr->fill();

            // print the mute label
            if ( muted || solo )
            {
                cr->set_source_rgb(1.0, 1.0, 1.0);    // White
            }
            else
            {
                cr->set_source_rgb(0.0, 0.0, 0.0);    // Black
            }

            cr->move_to(m_window_x - text_width - 2, (c_names_y * i) +  ((c_names_y * .5) - (text_height * .5) + 1 ));

            m->show_in_cairo_context(cr);
        }
    }
    else
    {
        cr->set_source_rgb(0.6, 0.8, 1.0);    // blue
        cr->rectangle(0, (c_names_y * i) + 1, m_window_x, c_names_y);
        cr->stroke_preserve();
        cr->fill();
    }
}

bool
perfnames::on_draw(const Cairo::RefPtr<Cairo::Context>& cr)
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
        m_redraw_tracks = true;
    }

    if (m_redraw_tracks)
    {
        m_redraw_tracks = false;
        int trks = (m_window_y / c_names_y) + 1;

        for ( int i=0; i< trks; i++ )
        {
            int track = i + m_track_offset;
            draw_track(track);
        }
    }

   /* Clear previous background */
    cr->set_source_rgb(1.0, 1.0, 1.0);  // White
    cr->rectangle (0.0, 0.0, width, height);
    cr->stroke_preserve();
    cr->fill();

    /* Draw the new background */
    cr->set_source(m_surface, 0.0, 0.0);
    cr->paint();

    return true;
}

void
perfnames::convert_y( int a_y, int *a_trk)
{
    *a_trk = a_y / c_names_y;
    *a_trk  += m_track_offset;

    if ( *a_trk >= c_max_track )
        *a_trk = c_max_track - 1;

    if ( *a_trk < 0 )
        *a_trk = 0;
}

bool
perfnames::on_button_press_event(GdkEventButton *a_e)
{
    int track;
    int y = (int) a_e->y;

    convert_y( y, &track );

    m_current_trk = track;

    /*      left mouse button     */
    if ( a_e->button == 1 &&  m_mainperf->is_active_track( track ) )
    {
        m_button_down = true;
    }

    /* Middle mouse button toggle solo track */
    if ( a_e->button == 2 &&  m_mainperf->is_active_track( track ) )
    {
        bool solo = m_mainperf->get_track(m_current_trk)->get_song_solo();
        m_mainperf->get_track(m_current_trk)->set_song_solo( !solo );
        /* we want to shut off mute if we are setting solo, so if solo was
         * off (false) then we are turning it on(toggle), so unset the mute. */
        if (!solo)
            m_mainperf->get_track(m_current_trk)->set_song_mute( solo );

        check_global_solo_tracks();
    }
    
    return true;
}

bool
perfnames::on_button_release_event(GdkEventButton* p0)
{
    int track;
    int y = (int) p0->y;
    convert_y( y, &track );

    m_current_trk = track;
    m_button_down = false;

    /* left mouse button & not moving - toggle mute  */
    if ( p0->button == 1 && m_mainperf->is_active_track( m_current_trk ) && !m_moving )
    {
        bool muted = m_mainperf->get_track(m_current_trk)->get_song_mute();
        m_mainperf->get_track(m_current_trk)->set_song_mute( !muted );
       /* we want to shut off solo if we are setting mute, so if mute was
         * off (false) then we are turning it on(toggle), so unset the solo. */
        if (!muted)
            m_mainperf->get_track(m_current_trk)->set_song_solo( muted );

        check_global_solo_tracks();
        global_is_modified = true;
    }

    /* Left button and moving */
    if ( p0->button == 1 && m_moving )
    {
        m_moving = false;

        /* merge and NO delete  */
        if ( p0->state & GDK_SHIFT_MASK )    // shift key
        {
            merge_tracks( &m_moving_track );

            /* Put the merged track back to original position */
            m_mainperf->new_track( m_old_track  );
            *(m_mainperf->get_track( m_old_track )) = m_moving_track;
            m_mainperf->get_track(m_old_track)->set_dirty();
            return false;
        } 

        /* merge and delete  */
        if ( p0->state & GDK_CONTROL_MASK )     // control key
        {
            bool valid = merge_tracks( &m_moving_track );

            /* we do not have a valid merge (i.e. they tried to merge into a track being edited or
             * did a merge to an inactive track) then just ignore everything */
            if(!valid)
            {
                /* Put the merged track back to original position */
                m_mainperf->new_track( m_old_track  );
                *(m_mainperf->get_track( m_old_track )) = m_moving_track;
                m_mainperf->get_track(m_old_track)->set_dirty();
            }
            return false;
        }

        /* If we did not land on another active track, then move to new location */
        if ( ! m_mainperf->is_active_track( m_current_trk ) )
        {
            m_mainperf->new_track( m_current_trk  );
            *(m_mainperf->get_track( m_current_trk )) = m_moving_track;
            m_mainperf->get_track( m_current_trk )->unselect_triggers();
            m_mainperf->get_track(m_current_trk)->set_dirty();
        }
        /* If we did land on an active track and it is not being edited, then swap places. */
        else if ( !m_mainperf->is_track_in_edit( m_current_trk ) )
        {
            m_clipboard = *(m_mainperf->get_track( m_current_trk ));        // hold the current for swap to old location
            m_mainperf->new_track( m_old_track  );                          // The old location
            *(m_mainperf->get_track( m_old_track )) = m_clipboard;          // put the current track into the old location
            m_mainperf->get_track( m_old_track )->unselect_triggers();
            m_mainperf->get_track(m_old_track)->set_dirty();

            m_mainperf->delete_track( m_current_trk );                      // delete the current for replacement
            m_mainperf->new_track( m_current_trk  );                        // add a new blank one
            *(m_mainperf->get_track( m_current_trk )) = m_moving_track;     // replace with the old
            m_mainperf->get_track( m_current_trk )->unselect_triggers();
            m_mainperf->get_track(m_current_trk)->set_dirty();
        }
       /* They landed on another track but it is being edited, so ignore the move 
         * and put the old track back to original location. */
        else
        {
            track_is_being_edited();
            m_mainperf->new_track( m_old_track  );
            *(m_mainperf->get_track( m_old_track )) = m_moving_track;
            m_mainperf->get_track(m_old_track)->set_dirty();
        }  
    }

    /*  launch menu - right mouse button   */
    if ( p0->button == 3 )
    {
        popup_menu();
    }

    return false;
}

bool
perfnames::on_motion_notify_event(GdkEventMotion* a_ev)
{
    int track;
    int y = (int) a_ev->y;

    convert_y( y, &track );
    
    /* If we are dragging off the original track then we are trying to move. */
    if ( m_button_down )
    {
        if ( track != m_current_trk && !m_moving &&
                !m_mainperf->is_track_in_edit( m_current_trk ) )
        {
            if ( m_mainperf->is_active_track( m_current_trk ))
            {
                m_mainperf->push_perf_undo();
                m_old_track = m_current_trk;
                m_moving = true;

                m_moving_track = *(m_mainperf->get_track( m_current_trk ));
                m_mainperf->delete_track( m_current_trk );
            }
        }
    }  

    return true;
}

bool
perfnames::on_scroll_event( GdkEventScroll* a_ev )
{
    double val = m_vadjust->get_value();

    if (  a_ev->direction == GDK_SCROLL_UP )
    {
        val -= m_vadjust->get_step_increment();
    }
    if (  a_ev->direction == GDK_SCROLL_DOWN )
    {
        val += m_vadjust->get_step_increment();
    }

    m_vadjust->clamp_page( val, val + m_vadjust->get_page_size());
    return true;
}

void
perfnames::on_size_allocate(Gtk::Allocation &a_r )
{
    Gtk::DrawingArea::on_size_allocate( a_r );

    m_window_x = a_r.get_width();
    m_window_y = a_r.get_height();
}

void
perfnames::redraw_dirty_tracks()
{
    int y_s = 0;
    int y_f = m_window_y / c_names_y;

    for ( int y=y_s; y<=y_f; y++ )
    {
        int trk = y + m_track_offset;

        if ( trk < c_max_track)
        {
            bool dirty = (m_mainperf->is_dirty_names( trk ));

            if (dirty)
            {
                draw_track( trk );
                queue_draw();
            }
        }
    }
}

void
perfnames::check_global_solo_tracks()
{
    global_solo_track_set = false;
    for (int i = 0; i < c_max_track; i++)
    {
        if (m_mainperf->is_active_track(i))
        {
            if (m_mainperf->get_track(i)->get_song_solo())
            {
                global_solo_track_set = true;
                break;
            }
        }
    }
}

bool
perfnames::merge_tracks( track *a_merge_track )
{
    bool is_merge_valid = false;
    
    if ( m_mainperf->is_active_track( m_current_trk ) && !m_mainperf->is_track_in_edit( m_current_trk ))
    {
        is_merge_valid = true;
        m_mainperf->push_track_undo(m_current_trk);

        std::vector<trigger> trig_vect;
        a_merge_track->get_trak_triggers(trig_vect); // all triggers for the track
        
        uint number_of_merged_sequences = a_merge_track->get_number_of_sequences();

        trigger *a_trig = NULL;

        for (uint jj = 0; jj < number_of_merged_sequences; ++jj )
        {
            /* For each sequence of the merged track get the sequence index */
            int seq_idx = m_mainperf->get_track( m_current_trk )->new_sequence();
            
            /* Create a landing sequence for the merging sequence */
            sequence *seq = m_mainperf->get_track( m_current_trk )->get_sequence(seq_idx);
            
            /* Put the merging sequence into the new landing sequence location */
            sequence *a_merge_sequence = a_merge_track->get_sequence(jj);
            *seq = *a_merge_sequence;
            
            /* Put the new landing sequence onto the current track */
            seq->set_track(m_mainperf->get_track( m_current_trk ));

            /* For each trigger of the current merging sequence - add it to the track */
            for(unsigned ii = 0; ii < trig_vect.size(); ii++)
            {
                a_trig = &trig_vect[ii];

                if(a_trig->m_sequence == a_merge_track->get_sequence_index(a_merge_sequence))
                {
                    m_mainperf->get_track( m_current_trk )->add_trigger(a_trig->m_tick_start,
                            a_trig->m_tick_end - a_trig->m_tick_start,a_trig->m_offset,
                            seq_idx);
                }
                a_trig = NULL;
            }
        }
        m_mainperf->get_track( m_current_trk )->set_dirty();
    }
    
    return is_merge_valid;
}

void
perfnames::track_is_being_edited()
{
    Glib::ustring query_str = "Cannot swap tracks if being edited!";
    
    Gtk::MessageDialog dialog( query_str, false,
                              Gtk::MESSAGE_INFO,
                              Gtk::BUTTONS_OK, true);

    dialog.run();
}