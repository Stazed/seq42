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
#include "font.h"


perfnames::perfnames( perform *a_perf, Adjustment *a_vadjust ):
    trackmenu(a_perf),
    m_black(Gdk::Color( "black" )),
    m_white(Gdk::Color( "white" )),
    m_grey(Gdk::Color( "grey" )),
    m_mainperf(a_perf),
    m_vadjust(a_vadjust),
    m_track_offset(0)
{

    add_events( Gdk::BUTTON_PRESS_MASK |
                Gdk::BUTTON_RELEASE_MASK |
                Gdk::SCROLL_MASK );

    /* set default size */
    set_size_request( c_names_x, 100 );

    // in the construor you can only allocate colors,
    // get_window() returns 0 because we have not be realized
    Glib::RefPtr<Gdk::Colormap>  colormap= get_default_colormap();
    colormap->alloc_color( m_black );
    colormap->alloc_color( m_white );
    colormap->alloc_color( m_grey );

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

    // Now we can allocate any additional resources we need
    m_window = get_window();
    m_gc = Gdk::GC::create( m_window );
    m_window->clear();

    m_pixmap = Gdk::Pixmap::create(m_window,
                                   c_names_x,
                                   c_names_y  * c_max_track + 1,
                                   -1);
}

void
perfnames::change_vert( )
{
    if ( m_track_offset != (int) m_vadjust->get_value() )
    {

        m_track_offset = (int) m_vadjust->get_value();
        queue_draw();
    }
}

void
perfnames::update_pixmap()
{

}

void
perfnames::draw_area()
{

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

    if ( track < c_max_track )
    {



        m_gc->set_foreground(m_black);
        m_window->draw_rectangle(m_gc,true,
                                 0,
                                 (c_names_y * i),
                                 c_names_x,
                                 c_names_y + 1 );


        if ( m_mainperf->is_active_track( track ))
            m_gc->set_foreground(m_white);
        else
            m_gc->set_foreground(m_grey);

        m_window->draw_rectangle(m_gc,true,
                                 1,
                                 (c_names_y * i) + 1,
                                 c_names_x-1,
                                 c_names_y - 1  );

        if ( m_mainperf->is_active_track( track ))
        {

            m_track_active[track]=true;

            char str[20];
            snprintf(str, sizeof(str),
                     "[%d] Bus %d Ch %d",
                     track + 1,
                     m_mainperf->get_track(track)->get_midi_bus()+1,
                     m_mainperf->get_track(track)->get_midi_channel()+1 );

            p_font_renderer->render_string_on_drawable(m_gc,
                    5,
                    c_names_y * i + 2,
                    m_window, str, font::BLACK );

            char name[20];
            snprintf(name, sizeof(name), "%-16.16s",
                     m_mainperf->get_track(track)->get_name());

            p_font_renderer->render_string_on_drawable(m_gc,
                    5,
                    c_names_y * i + 12,
                    m_window, name, font::BLACK );

            bool muted = m_mainperf->get_track(track)->get_song_mute();

            m_gc->set_foreground(m_black);
            m_window->draw_rectangle(m_gc,muted,
                                     104,
                                     (c_names_y * i),
                                     10,
                                     c_names_y  );

            if ( muted )
            {

                p_font_renderer->render_string_on_drawable(m_gc,
                        107,
                        c_names_y * i + 2,
                        m_window, "M", font::WHITE );
            }
            else
            {

                p_font_renderer->render_string_on_drawable(m_gc,
                        107,
                        c_names_y * i + 2,
                        m_window, "M", font::BLACK );
            }
        }
    }
    else
    {

        m_gc->set_foreground(m_grey);
        m_window->draw_rectangle(m_gc,true,
                                 0,
                                 (c_names_y * i) + 1,
                                 c_names_x,
                                 c_names_y );

    }

}

bool
perfnames::on_expose_event(GdkEventExpose* a_e)
{
    int trks = (m_window_y / c_names_y) + 1;

    for ( int i=0; i< trks; i++ )
    {

        int track = i + m_track_offset;

        draw_track(track);

    }
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

    /*int x = (int) a_e->x;*/
    int y = (int) a_e->y;

    convert_y( y, &track );

    m_current_trk = track;

    /*      left mouse button     */
    if ( a_e->button == 1 )
    {

        if ( m_mainperf->is_active_track( track ))
        {

            bool muted = m_mainperf->get_track(track)->get_song_mute();
            m_mainperf->get_track(track)->set_song_mute( !muted );
            global_is_modified = true;
            queue_draw();
        }
    }

    return true;
}

bool
perfnames::on_button_release_event(GdkEventButton* p0)
{
    /*     right mouse button      */
    if ( p0->button == 3 )
    {
        popup_menu();
    }

    return false;
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

        int trk = y + m_track_offset; // 4am

        if ( trk < c_max_track)
        {

            bool dirty = (m_mainperf->is_dirty_names( trk ));

            if (dirty)
            {
                draw_track( trk );
            }
        }
    }
}
