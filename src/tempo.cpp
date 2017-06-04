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

#include "tempo.h"
#include "font.h"
#include "pixmaps/tempo_marker.xpm"
#include "pixmaps/stop_marker.xpm"


tempo::tempo( perform *a_perf, mainwnd *a_main, Adjustment *a_hadjust ) :
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

    m_popup_tempo_wnd =  new tempopopup(this);
    m_hadjust->signal_value_changed().connect( mem_fun( *this, &tempo::change_horz ));

    set_double_buffered( false );
    
    m_current_mark.bpm = m_mainperf->get_bpm();
    m_current_mark.tick = STARTING_MARKER;
    add_marker(m_current_mark);
}

tempo::~tempo()
{
    delete m_popup_tempo_wnd;
}

void
tempo::increment_size()
{

}

void
tempo::update_sizes()
{

}

void
tempo::set_zoom (int a_zoom)
{
    if (m_mainwnd->zoom_check(a_zoom))
    {
        m_perf_scale_x = a_zoom;
        draw_background();
    }
}

void
tempo::on_realize()
{
    // we need to do the default realize
    Gtk::DrawingArea::on_realize();

    // Now we can allocate any additional resources we need
    m_window = get_window();
    m_gc = Gdk::GC::create( m_window );
    m_window->clear();

    set_size_request( 10, c_timearea_y );
}

void
tempo::change_horz( )
{
    if ( m_4bar_offset != (int) m_hadjust->get_value() )
    {
        m_4bar_offset = (int) m_hadjust->get_value();
        queue_draw();
    }
}

void
tempo::set_guides( int a_snap, int a_measure )
{
    m_snap = a_snap;
    m_measure_length = a_measure;
    queue_draw();
}

int
tempo::idle_progress( )
{
    return true;
}

void
tempo::update_pixmap()
{

}

void
tempo::draw_pixmap_on_window()
{

}

bool
tempo::on_expose_event (GdkEventExpose * /* ev */ )
{
    draw_background();
    return true;
}

void
tempo::draw_background()
{
    /* clear background */
    m_gc->set_foreground(m_white);
    m_window->draw_rectangle(m_gc,true,
                             0,
                             0,
                             m_window_x,
                             m_window_y );

    m_gc->set_foreground(m_black);
    m_window->draw_line(m_gc,
                        0,
                        m_window_y - 1,
                        m_window_x,
                        m_window_y - 1 );


    /* draw vert lines */
    m_gc->set_foreground(m_grey);

    long tick_offset = (m_4bar_offset * 16 * c_ppqn);
    long first_measure = tick_offset / m_measure_length;

    for ( int i=first_measure;
            i<first_measure+(m_window_x * m_perf_scale_x / (m_measure_length)) + 1; i++ )
    {
        int x_pos = ((i * m_measure_length) - tick_offset) / m_perf_scale_x;

        /* beat */
        m_window->draw_line(m_gc,
                            x_pos,
                            0,
                            x_pos,
                            m_window_y );

    }
    
    list<tempo_mark>::iterator i;
    for ( i = m_list_marker.begin(); i != m_list_marker.end(); i++ )
    {
        long tempo_marker = (i)->tick;
        tempo_marker -= (m_4bar_offset * 16 * c_ppqn);
        tempo_marker /= m_perf_scale_x;

        if ( tempo_marker >=0 && tempo_marker <= m_window_x )
        {
            // Load the xpm image
            char str[10];

            if((i)->bpm == STOP_MARKER)       // Stop marker
            {
                m_pixbuf = Gdk::Pixbuf::create_from_xpm_data(stop_marker_xpm);
                m_window->draw_pixbuf(m_pixbuf,0,0,tempo_marker -4,0, -1,-1,Gdk::RGB_DITHER_NONE, 0, 0);

                snprintf
                (
                    str, sizeof(str),
                    "[Stop]"
                );
            }
            else                                // tempo marker
            {
                m_pixbuf = Gdk::Pixbuf::create_from_xpm_data(tempo_marker_xpm);
                m_window->draw_pixbuf(m_pixbuf,0,0,tempo_marker -4,0, -1,-1,Gdk::RGB_DITHER_NONE, 0, 0);

                // set the tempo BPM value
                snprintf
                (
                    str, sizeof(str),
                    "[%0.2f]",
                    (i)->bpm
                );
            }

            // print the tempo or 'Stop'
            m_gc->set_foreground(m_white);
            p_font_renderer->render_string_on_drawable(m_gc,
                    tempo_marker + 5,
                    0,
                    m_window, str, font::WHITE );
        }
    }
}

bool
tempo::on_button_press_event(GdkEventButton* p0)
{
    uint64_t tick = (uint64_t) p0->x;
    tick *= m_perf_scale_x;
    tick += (m_4bar_offset * 16 * c_ppqn);

    tick = tick - (tick % m_snap);

    if ( p0->button == 1 )  // left mouse button add marker or drag - FIXME
    {
        set_tempo_marker(tick);
        /* don't queue_draw() here because they might escape key out */
    }

    if ( p0->button == 3 )                          // right mouse button delete marker
    {
        list<tempo_mark>::iterator i;
        for ( i = m_list_marker.begin(); i != m_list_marker.end(); i++ )
        {
            if(tick >= (i)->tick - (100 * (m_perf_scale_x / 32) ) &&
                    tick <= (i)->tick + (100 * (m_perf_scale_x / 32)))
            {
                if((i)->tick != STARTING_MARKER)    // Don't allow erase of first start marker
                {
                    m_list_marker.erase(i);
                    m_list_marker.sort(&sort_tempo_mark);
                    calculate_marker_start();
                    queue_draw();
                    reset_tempo_list();
                    global_is_modified = true;
                    return true;
                }
                break;
            }
        }
    }

    return true;
}

bool
tempo::on_button_release_event(GdkEventButton* p0)
{
    return false;
}

void
tempo::on_size_allocate(Gtk::Allocation &a_r )
{
    Gtk::DrawingArea::on_size_allocate( a_r );

    m_window_x = a_r.get_width();
    m_window_y = a_r.get_height();
}

/* Raises the spinbutton entry window */
void
tempo::set_tempo_marker(long a_tick)
{
    m_popup_tempo_wnd->popup_tempo_win();
    m_current_mark.tick = a_tick;
}

/* Called by tempopopup when a value is accepted */
void
tempo::set_BPM(double a_bpm)
{
    m_current_mark.bpm = a_bpm;
    add_marker(m_current_mark);
    queue_draw();
}

bool
tempo::sort_tempo_mark(const tempo_mark &a, const tempo_mark &b)
{
    return a.tick < b.tick;
}

bool
tempo::reverse_sort_tempo_mark(const tempo_mark &a, const tempo_mark &b)
{
    return b.tick < a.tick;
}

void
tempo::add_marker(tempo_mark a_mark)
{
    bool start_tick = false;
    list<tempo_mark>::iterator i;
    for ( i = m_list_marker.begin(); i != m_list_marker.end(); i++ )
    {
        if((i)->tick == a_mark.tick )       // don't allow duplicates - last one wins 
        {
            if((i)->tick == STARTING_MARKER)// don't allow replacement of start marker
            {
                start_tick = true;
            }
            else                            // erase duplicate location
            {
                m_list_marker.erase(i);
            }
            break;
        }
    }
    if(!start_tick)                         // add the new one
    {
        m_list_marker.push_back(a_mark);
    }

    m_list_marker.sort(&sort_tempo_mark);
    calculate_marker_start();
    
    reset_tempo_list();
}

/* Called by mainwnd spinbutton callback */
void
tempo::set_start_BPM(double a_bpm)
{
    m_list_marker.begin()->bpm = a_bpm;
    
    reset_tempo_list();
    queue_draw();
}

/* update perform */
void
tempo::reset_tempo_list()
{
    m_mainperf->m_list_play_marker = m_list_marker;
    m_mainperf->m_list_total_marker = m_list_marker;
    
    //m_mainperf->m_list_total_marker.sort(&reverse_sort_tempo_mark);
}

/* file loading */
void
tempo::load_tempo_list()
{
    m_list_marker = m_mainperf->m_list_total_marker;    // update tempo class
    m_mainperf->m_list_play_marker = m_list_marker;     // update the m_mainperf play list
    queue_draw();
}

/* calculates for jack frame */
void
tempo::calculate_marker_start()
{
    list<tempo_mark>::iterator i;
    for ( i = ++m_list_marker.begin(); i != m_list_marker.end(); ++i )
    {
        if(i == m_list_marker.end())
            break;
        
        list<tempo_mark>::iterator n = i; 
            --n;
// FIXME STOPMARKER !!!
        tempo_mark current = (*i);
        tempo_mark previous = (*n);
        
        (*i).start = tick_to_jack_frame(current.tick, previous.bpm, m_mainperf );
        printf("from tick to %d: previous.start %d\n",(*i).start, previous.start );
        (*i).start -= previous.start;
    }
    reset_tempo_list();
//#ifdef RDEBUG
    print_marker_info();
//#endif
}

void
tempo::print_marker_info()
{
    list<tempo_mark>::iterator i;
    for ( i = m_list_marker.begin(); i != m_list_marker.end(); ++i )
    {
        printf("mark tick %lu: start %lu: bpm %f\n", (*i).tick, (*i).start, (*i).bpm);
    }
}