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
                Gdk::BUTTON_RELEASE_MASK |
                Gdk::POINTER_MOTION_MASK );

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
tempo::lock( )
{
    m_mutex.lock();
}

void
tempo::unlock( )
{
    m_mutex.unlock();
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
    if(a_measure != m_measure_length)
    {
        m_measure_length = a_measure;
        reset_tempo_list();
    }
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

            if((i)->bpm == STOP_MARKER)         // Stop marker
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

    if ( p0->button == 1 )              // left mouse button add marker or drag - FIXME
    {
        tick = tick - (tick % m_snap);  // snap only when adding, not when trying to delete
        set_tempo_marker(tick);
        /* don't queue_draw() here because they might escape key out */
    }

    if ( p0->button == 3 )              // right mouse button delete marker
    {
        list<tempo_mark>::iterator i;
        for ( i = m_list_marker.begin(); i != m_list_marker.end(); i++ )
        {
            uint64_t start_marker = (i)->tick - (120.0 * (float) (m_perf_scale_x / 32.0) );
            uint64_t end_marker = (i)->tick + (120.0 * (float) (m_perf_scale_x / 32.0) );
            
            if(tick >= start_marker && tick <= end_marker)
            {
                if((i)->tick != STARTING_MARKER)    // Don't allow erase of first start marker
                {
                    push_undo();
                    m_list_marker.erase(i);
                    reset_tempo_list();
                    queue_draw();
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

bool
tempo::on_motion_notify_event(GdkEventMotion* a_ev)
{
    uint64_t tick = (uint64_t) a_ev->x;
    
    tick *= m_perf_scale_x;
    tick += (m_4bar_offset * 16 * c_ppqn);
    
    //printf("motion_tick %ld\n",tick);
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
    push_undo();
    m_current_mark.bpm = a_bpm;
    add_marker(m_current_mark);
    queue_draw();
}

bool
tempo::sort_tempo_mark(const tempo_mark &a, const tempo_mark &b)
{
    return a.tick < b.tick;
}

/* not currently used */
bool
tempo::reverse_sort_tempo_mark(const tempo_mark &a, const tempo_mark &b)
{
    return b.tick < a.tick;
}

void
tempo::add_marker(tempo_mark a_mark)
{
    lock();
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
        //printf("add_marker bpm %f\n", a_mark.bpm);
        m_list_marker.push_back(a_mark);
    }

    reset_tempo_list();
    unlock();
}

/* Called by mainwnd spinbutton callback & key-bind */
void
tempo::set_start_BPM(double a_bpm)
{
    if ( a_bpm < c_bpm_minimum ) a_bpm = c_bpm_minimum;
    if ( a_bpm > c_bpm_maximum ) a_bpm = c_bpm_maximum;
    
    if ( ! (m_mainperf->is_jack_running() && global_is_running ))
    {
        //push_undo(true);
        m_mainperf->set_bpm( a_bpm );
        m_list_marker.begin()->bpm = a_bpm;

        reset_tempo_list();
        queue_draw();
    }
}

/* update marker jack start ticks and perform class lists.
 * triggered any time user adds or deletes a marker or adjusts
 * the start marker bpm spin. Also on initial file loading.
 * also when measures are changed */
void
tempo::reset_tempo_list(bool play_list_only)
{
    if(play_list_only)
    {
        m_mainperf->m_list_play_marker = m_list_marker;
    }
    else
    {
        lock();
        calculate_marker_start();

        m_mainperf->m_list_play_marker = m_list_marker;
        m_mainperf->m_list_total_marker = m_list_marker;
        m_mainperf->m_list_no_stop_markers = m_list_no_stop_markers;
        unlock();
    }
}

/* file loading */
void
tempo::load_tempo_list()
{
    m_list_marker = m_mainperf->m_list_total_marker;    // update tempo class
    reset_tempo_list();                                 // needed to update m_list_no_stop_markers
    queue_draw();
}

/* calculates for jack frame */
void
tempo::calculate_marker_start()
{
    lock();
    /* sort first always */
    m_list_marker.sort(&sort_tempo_mark);
    
    /* clear the old junk */
    m_list_no_stop_markers.clear();
    
    /* vector to hold stop markers */
    std::vector< tempo_mark > v_stop_markers;

    list<tempo_mark>::iterator i;
    for ( i = m_list_marker.begin(); i != m_list_marker.end(); ++i )
    {
        if((*i).bpm != STOP_MARKER) // hold the playing markers 
        {
            m_list_no_stop_markers.push_back((*i));
        }
        else                        // hold the stop markers
        {
            v_stop_markers.push_back((*i));
        }
    }

    /* calculate the jack start tick without the stop markers */
    for ( i = ++m_list_no_stop_markers.begin(); i != m_list_no_stop_markers.end(); ++i )
    {
        if(i == m_list_no_stop_markers.end())
            break;
        
        list<tempo_mark>::iterator n = i; 
            --n;

        tempo_mark current = (*i);
        tempo_mark previous = (*n);
        
        (*i).start = tick_to_jack_frame(current.tick - previous.tick , previous.bpm, m_mainperf );
//        printf("from tick to %d: previous.start %d\n",(*i).start, previous.start );
        (*i).start += previous.start;
    }
    
    /* reset the main list with the calculated starts */
    m_list_marker = m_list_no_stop_markers;
    
    /* add back the stop markers */
    for(unsigned n = 0; n < v_stop_markers.size(); ++n )
    {
        m_list_marker.push_back(v_stop_markers[n]);
    }
    
    /* sort again since stops will be out of order */
    m_list_marker.sort(&sort_tempo_mark);
    unlock();
}

void
tempo::push_undo(bool a_hold)
{
    lock();
    if(a_hold)
    {
        m_list_undo.push( m_list_undo_hold );
    }
    else
    {
        m_list_undo.push( m_list_marker );
    }
    m_mainperf->push_bpm_undo();
    global_is_modified = true;
    unlock();
   // set_have_undo();
}

void
tempo::pop_undo()
{
    lock();

    if (m_list_undo.size() > 0 )
    {
        m_mainperf->pop_bpm_undo();
        m_list_redo.push( m_list_marker );
        m_list_marker = m_list_undo.top();
        m_list_undo.pop();
        reset_tempo_list();
        m_mainperf->set_bpm(m_mainperf->get_start_tempo());
        queue_draw();
    }

    unlock();
  //  set_have_undo();
  //  set_have_redo();
}

void
tempo::pop_redo()
{
    lock();

    if (m_list_redo.size() > 0 )
    {
        m_mainperf->pop_bpm_redo();
        m_list_undo.push( m_list_marker );
        m_list_marker = m_list_redo.top();
        m_list_redo.pop();
        reset_tempo_list();
        m_mainperf->set_bpm(m_mainperf->get_start_tempo());
        queue_draw();
    }

    unlock();
   // set_have_redo();
   // set_have_undo();
}

void
tempo::set_hold_undo (bool a_hold)
{
    lock();

    if(a_hold)
    {
        m_list_undo_hold = m_list_marker;
    }
    else
       m_list_undo_hold.clear( );

    unlock();
}

int
tempo::get_hold_undo ()
{
    return m_list_undo_hold.size();
}


void
tempo::print_marker_info(list<tempo_mark> a_list)
{
    list<tempo_mark>::iterator i;
    for ( i = a_list.begin(); i != a_list.end(); ++i)
    {
        printf("mark tick %lu: start %lu: bpm %f\n", (*i).tick, (*i).start, (*i).bpm);
    }
    printf("\n\n");
}