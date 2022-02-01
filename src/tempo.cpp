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
    m_measure_length(c_ppqn * 4),
    m_init_move(false),
    m_moving(false)
{
    add_events( Gdk::BUTTON_PRESS_MASK |
                Gdk::BUTTON_RELEASE_MASK |
                Gdk::POINTER_MOTION_MASK |
                Gdk::LEAVE_NOTIFY_MASK );

    // in the constructor you can only allocate colors,
    // get_window() returns 0 because we have not been realized
    Glib::RefPtr<Gdk::Colormap> colormap = get_default_colormap();
    colormap->alloc_color( m_black );
    colormap->alloc_color( m_white );
    colormap->alloc_color( m_grey );

    m_popup_tempo_wnd =  new tempopopup(this);
    m_hadjust->signal_value_changed().connect( mem_fun( *this, &tempo::change_horz ));

    set_double_buffered( false );
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
        //printf("tempo::set_guides\n");
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
    
    /* Draw the markers */
    list<tempo_mark>::iterator i;
    for ( i = m_list_marker.begin(); i != m_list_marker.end(); i++ )
    {
        tempo_mark current_marker = *(i);
        
        if(m_moving)
        {
            if(current_marker.tick == m_move_marker.tick)
            {
                /* then we use the motion notify marker instead of the list one */
                current_marker = m_current_mark;
            }
        }

        long tempo_marker = current_marker.tick;
        tempo_marker -= (m_4bar_offset * 16 * c_ppqn);
        tempo_marker /= m_perf_scale_x;

        if ( tempo_marker >=0 && tempo_marker <= m_window_x )
        {
            // Load the xpm image
            char str[10];

            if(current_marker.bpm == STOP_MARKER)         // Stop marker
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
                    current_marker.bpm
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

    /* Left mouse button - add or move marker */
    if ( p0->button == 1 )
    {
        /* Triggered by motion notify */
        if(m_init_move)
        {
            m_init_move = false;
            m_moving = true;
            return true;
        }
        
        /* If we moved and released button then clicked again without moving,
         * the check for m_init_move above will fail because of no motion.
         * So check if we are still on the marker and assume another move rather
         * than popping up the bpm window. */
        if(check_above_marker(tick, false, false))
        {
            this->get_window()->set_cursor( Gdk::Cursor( Gdk::CENTER_PTR ));
            m_current_mark = m_move_marker;
            m_current_mark.tick = tick;
            m_init_move = false;
            m_moving = true;
            return true;
        }
        
        /* We are not above a marker so trigger the popup bpm window to add */
        tick = tick - (tick % m_snap);  // snap only when adding, not when trying to delete
        set_tempo_marker(tick);
        /* don't queue_draw() here because they might escape key out */
        return true;

    }   // end button 1 (left mouse click)

    /* right mouse button delete marker */
    if ( p0->button == 3 )
    {
        check_above_marker(tick, true, false);
        return true;
    }

    return false;
}

bool
tempo::on_button_release_event(GdkEventButton* p0)
{
    if(m_moving)
    {
        m_moving = false;
        
        /* If they move to start or before, then just ignore them and reset everything*/
        if(p0->x <= 0)
        {
            /* Clear the move marker */
            m_move_marker.tick = 0; 
            queue_draw();
            return false;
        }
        
        uint64_t tick = (uint64_t) p0->x;
        tick *= m_perf_scale_x;
        tick += (m_4bar_offset * 16 * c_ppqn);
        tick = tick - (tick % m_snap);
        
        /* snapped tick may try to set to 0, so ignore that also */
        if(tick == 0)
        {
            /* Clear the move marker */
            m_move_marker.tick = 0; 
            queue_draw();
            return false;
        }
        
        /* We have a good location so set the move and delete the old */
        /* set the current mark to same as held bpm */
        tempo_mark current_mark = m_move_marker;
        /* update to the new location */
        current_mark.tick = tick;
        /* Delete the old marker based on exact location, not range */
        check_above_marker(m_move_marker.tick, true, true );
        /* add the moved marker */
        add_marker(current_mark);
        /* Clear the moved marker */
        m_move_marker.tick = 0;
        /* If user releases the button while off the tempo grid then
         * motion notify is not triggered to update the pointer. When
         * they return to the grid, the previous cursor state is still
         * active, so we adjust it here. */
        this->get_window()->set_cursor( Gdk::Cursor( Gdk::LEFT_PTR ));
    }
    
    return true;
}

bool
tempo::on_motion_notify_event(GdkEventMotion* a_ev)
{
    uint64_t tick = (uint64_t) a_ev->x;
    
    tick *= m_perf_scale_x;
    tick += (m_4bar_offset * 16 * c_ppqn);
    
    bool change_mouse = false;
    if(!m_moving)
        change_mouse = check_above_marker(tick, false, false);
    
    if(change_mouse || m_moving)
    {
        /* If we are not already moving ... */
        if(!m_moving)
        {
            m_init_move = true;     // to tell button press we are on a marker
            m_current_mark = m_move_marker; // load the marker for display movement
            this->get_window()->set_cursor( Gdk::Cursor( Gdk::CENTER_PTR ));
        }
        
        /* snap the movement so the user can see where it 
         * actually lands before button release */
        tick = tick - (tick % m_snap);
        /* m_current_mark is used to show the movement in draw background */
        m_current_mark.tick = tick;
        queue_draw();
    }
    else /* we are not over a marker and not moving so reset everything if not done already */
    {
        if(m_init_move)
        {
            m_init_move = false;
            m_move_marker.tick = 0;  // clear the move marker
            this->get_window()->set_cursor( Gdk::Cursor( Gdk::LEFT_PTR ));
        }
    }
    
    return false;
}

bool
tempo::on_leave_notify_event(GdkEventCrossing* a_ev)
{
    if(!m_moving)
        m_move_marker.tick = 0;
    
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
    //printf("tempo::set_BPM - calls add_marker\n");
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

    //printf("tempo::add_marker\n");
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
        
        tempo_mark marker;
        marker.bpm = a_bpm;
        marker.tick = STARTING_MARKER;

        if(!m_list_marker.size())
        {
            m_list_marker.push_front(marker);
        }
        else
        {
            (*m_list_marker.begin())= marker;
        }

        reset_tempo_list();
        m_mainperf->set_bpm( a_bpm );
        queue_draw();
    }
}

/* reset_tempo_list()
 * update marker jack start ticks, microseconds start and perform class lists.
 * Triggered any time user adds or deletes a marker or adjusts
 * the start marker bpm spin. Also on initial file loading, undo / redo.
 * also when measures are changed.
 * This should be called directly when the tempo class m_list_marker is
 * adjusted and then is pushed to m_mainperf class.
 * 
 * Use load_tempo_list() for pushing m_mainperf to the tempo class
 * m_list_marker. */
void
tempo::reset_tempo_list()
{
    //printf("tempo::reset_tempo_list()\n");
    lock();
    calculate_marker_start();

    m_mainperf->m_list_play_marker = m_list_marker;
    m_mainperf->m_list_total_marker = m_list_marker;
    m_mainperf->m_list_no_stop_markers = m_list_no_stop_markers;
    unlock();
    
}

/* load_tempo_list()
 * This should be called when the m_mainperf->m_list_total_marker is
 * correct and the tempo class m_list_marker needs to be adjusted to it.
 * Use for file loading & undo / redo on import.
 * Use reset_tempo_list() above directly when m_list_marker is correct
 * and m_mainperf needs to be adjusted. */
void
tempo::load_tempo_list()
{
    m_list_marker = m_mainperf->m_list_total_marker;    // update tempo class
    reset_tempo_list();                                 // needed to update m_list_no_stop_markers & calculate start frames
    m_mainperf->set_bpm(m_list_marker.begin()->bpm);
    queue_draw();
}

/* calculates for jack frame */
void
tempo::calculate_marker_start()
{
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
        
        /* Wall Clock offset */
        (*i).microseconds_start = ticks_to_delta_time_us(current.tick - previous.tick, previous.bpm, c_ppqn);
        (*i).microseconds_start += previous.microseconds_start;

#ifdef JACK_SUPPORT
        /* Jack frame offset*/
        (*i).start = tick_to_jack_frame(current.tick - previous.tick , previous.bpm, m_mainperf );
        (*i).start += previous.start;
#endif // JACK_SUPPORT
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
}

/* Clear the tempo map and set default start bpm.
   Called by mainwnd new file and load file to clear previous tempo items */
void
tempo::clear_tempo_list()
{
    m_list_marker.clear();
    m_mainperf->m_list_play_marker.clear();
    m_mainperf->m_list_total_marker.clear();
    m_mainperf->m_list_no_stop_markers.clear();
}

void
tempo::push_undo(bool a_hold)
{
    lock();
    if(a_hold)
    {
        if(m_list_undo_hold.size())
        {
            m_mainperf->m_list_undo.push( m_list_undo_hold );
            set_hold_undo (false);
            m_mainperf->push_bpm_undo();
            global_is_modified = true;
        }
    }
    else
    {
        m_mainperf->m_list_undo.push( m_list_marker );
        set_hold_undo (false);
        m_mainperf->push_bpm_undo();
        global_is_modified = true;
    }
    unlock();
}

void
tempo::pop_undo()
{
    lock();

    if (m_mainperf->m_list_undo.size() > 0 )
    {
        m_mainperf->pop_bpm_undo();
        m_mainperf->m_list_redo.push( m_list_marker );
        m_list_marker = m_mainperf->m_list_undo.top();
        m_mainperf->m_list_undo.pop();
        reset_tempo_list();
        m_mainperf->set_bpm(m_mainperf->get_start_tempo());
        queue_draw();
    }

    unlock();
}

void
tempo::pop_redo()
{
    lock();

    if (m_mainperf->m_list_redo.size() > 0 )
    {
        m_mainperf->pop_bpm_redo();
        m_mainperf->m_list_undo.push( m_list_marker );
        m_list_marker = m_mainperf->m_list_redo.top();
        m_mainperf->m_list_redo.pop();
        reset_tempo_list();
        m_mainperf->set_bpm(m_mainperf->get_start_tempo());
        queue_draw();
    }

    unlock();
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
        printf("mark tick %lu: start %u: bpm %f\n", (*i).tick, (*i).start, (*i).bpm);
    }
    printf("\n\n");
}

double
tempo::pulse_length_us (double bpm, int ppqn)
{
    double bw = (double) m_mainwnd->get_bw();
    return 60000000.0 / ppqn / bpm * ( bw / 4.0 );
}

/* called by motion notify and when right mouse click for delete */
bool
tempo::check_above_marker(uint64_t mouse_tick, bool a_delete, bool exact )
{
    bool ret = false;
    lock();
    
    list<tempo_mark>::iterator i;
    for ( i = m_list_marker.begin(); i != m_list_marker.end(); i++ )
    {
        uint64_t start_marker = (i)->tick - (60.0 * (float) (m_perf_scale_x / 32.0) );
        uint64_t end_marker = (i)->tick + (260.0 * (float) (m_perf_scale_x / 32.0) );
        
        /* exact: used when deleting for move. We have the exact marker location from the 
           held m_move_marker so do not use range because it is inaccurate for delete
           of markers that are very close together and the ranges overlap. */
        if(exact)
        {
            start_marker = (i)->tick;
            end_marker = (i)->tick;
        }

        if(mouse_tick >= start_marker && mouse_tick <= end_marker)
        {
            /* Don't allow delete or move of first marker (STARTING_MARKER) */
            if((i)->tick != STARTING_MARKER)
            {
                /* If !a_delete, context sensitive mouse (on_motion_notify_event */
                if(!a_delete)
                {
                    /* we may be moving the marker - so hold it if we do not already have a hold */
                    if(!m_move_marker.tick)
                    {
                        m_move_marker = *(i);
                    }
                    /* return true which tells motion_notify to change the mouse pointer */
                    ret = true;
                    break;
                }
                
                /* Deleting the marker (on_button_press_event - right click on marker) 
                   or when moving and released left button for landing location */
                push_undo();
                m_list_marker.erase(i);
                reset_tempo_list();
                queue_draw();
                ret = true;
                break;
            }
            break;
        }
    }
    /* we are not above any existing markers */
    
    unlock();
    return ret;
}


/* Modified spinbutton for using the mainwnd bpm spinner to allow for better undo support.
 * This allows user to spin and won't push to undo on every changed value, but will only
 * push undo when user leaves the widget. Modified to work with typed entries as well.
 */

Bpm_spinbutton::Bpm_spinbutton(Adjustment& adjustment, double climb_rate, guint digits):
    Gtk::SpinButton(adjustment,climb_rate, digits),
    m_have_enter(false),
    m_have_leave(false),
    m_is_typing(false),
    m_hold_bpm(0.0)
{
    
}

bool
Bpm_spinbutton::on_enter_notify_event(GdkEventCrossing* event)
{
    m_have_enter = true;
    m_have_leave = false;
    m_is_typing = false;
    m_hold_bpm = this->get_value();

    return Gtk::Widget::on_enter_notify_event(event);
}

bool 
Bpm_spinbutton::on_leave_notify_event(GdkEventCrossing* event)
{
    m_have_leave = true;
    m_have_enter = false;
    m_is_typing = false;

    return Gtk::Widget::on_leave_notify_event(event);
}

bool
Bpm_spinbutton::on_key_press_event( GdkEventKey* a_ev )
{
    if (a_ev->keyval == GDK_KEY_Return || a_ev->keyval == GDK_KEY_KP_Enter)
    {
        m_is_typing = true;
        m_hold_bpm = 0.0;
    }
    return Gtk::Widget::on_key_press_event(a_ev);
}

void
Bpm_spinbutton::set_have_enter(bool a_enter)
{
    m_have_enter = a_enter;
}

bool
Bpm_spinbutton::get_have_enter()
{
    return m_have_enter;
}

void
Bpm_spinbutton::set_have_leave(bool a_leave)
{
    m_have_leave = a_leave;
}
bool
Bpm_spinbutton::get_have_leave()
{
    return m_have_leave;
}

void
Bpm_spinbutton::set_have_typing(bool a_type)
{
    m_is_typing = a_type;
}

bool
Bpm_spinbutton::get_have_typing()
{
    return m_is_typing;
}

void
Bpm_spinbutton::set_hold_bpm(double a_bpm)
{
    m_hold_bpm = a_bpm;
}

double
Bpm_spinbutton::get_hold_bpm()
{
    return m_hold_bpm;
}

