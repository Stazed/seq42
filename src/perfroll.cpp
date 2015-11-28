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
#include "seqedit.h"
#include "font.h"

const int c_perfroll_background_x = (c_ppqn * 4 * 16) / c_perf_scale_x;
const int c_perfroll_size_box_w = 3;
const int c_perfroll_size_box_click_w = c_perfroll_size_box_w+1 ;

perfroll::perfroll( perform *a_perf,
		    Adjustment * a_hadjust,
		    Adjustment * a_vadjust  ) : DrawingArea()
{

    Glib::RefPtr<Gdk::Colormap> colormap = get_default_colormap();

    m_black = Gdk::Color( "black" );
    m_white = Gdk::Color( "white" );
    m_grey = Gdk::Color( "grey" );
    m_lt_grey = Gdk::Color( "light grey" );

    //m_text_font_6_12 = Gdk_Font( c_font_6_12 );

    colormap->alloc_color( m_black );
    colormap->alloc_color( m_white );
    colormap->alloc_color( m_grey );
    colormap->alloc_color( m_lt_grey );

    m_mainperf = a_perf;
    m_vadjust = a_vadjust;
    m_hadjust = a_hadjust;

    m_moving = false;
    m_growing = false;

    m_old_progress_ticks = 0;

    add_events( Gdk::BUTTON_PRESS_MASK |
		Gdk::BUTTON_RELEASE_MASK |
		Gdk::POINTER_MOTION_MASK |
		Gdk::KEY_PRESS_MASK |
		Gdk::KEY_RELEASE_MASK |
		Gdk::FOCUS_CHANGE_MASK |
		Gdk::SCROLL_MASK );


    set_size_request( 10, 10 );

    m_4bar_offset = 0;
    m_track_offset = 0;
    m_roll_length_ticks = 0;

    m_drop_track = 0;

    set_double_buffered( false );

    for( int i=0; i<c_max_track; ++i )
        m_track_active[i]=false;

    cross_track_paste = false;
}

perfroll::~perfroll( )
{

}


void
perfroll::change_horz( )
{
    if ( m_4bar_offset != (int) m_hadjust->get_value() ){

	m_4bar_offset = (int) m_hadjust->get_value();
	queue_draw();
    }
}

void
perfroll::change_vert( )
{
    if ( m_track_offset != (int) m_vadjust->get_value() ){

	m_track_offset = (int) m_vadjust->get_value();
	queue_draw();
    }
}

void
perfroll::on_realize()
{
    // we need to do the default realize
    Gtk::DrawingArea::on_realize();

    set_flags( Gtk::CAN_FOCUS );

    // Now we can allocate any additional resources we need
    m_window = get_window();
    m_gc = Gdk::GC::create( m_window );
    m_window->clear();

    update_sizes();

    m_hadjust->signal_value_changed().connect( mem_fun( *this, &perfroll::change_horz ));
    m_vadjust->signal_value_changed().connect( mem_fun( *this, &perfroll::change_vert ));

    m_background = Gdk::Pixmap::create( m_window,
                                        c_perfroll_background_x,
                                        c_names_y, -1 );

    /* and fill the background ( dotted lines n' such ) */
    fill_background_pixmap();

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
    int h_bars_visable = (m_window_x * c_perf_scale_x) / (c_ppqn * 16);

    m_hadjust->set_lower( 0 );
    m_hadjust->set_upper( h_bars );
    m_hadjust->set_page_size( h_bars_visable );
    m_hadjust->set_step_increment( 1 );
    m_hadjust->set_page_increment( 1 );

    int h_max_value = h_bars - h_bars_visable;

    if ( m_hadjust->get_value() > h_max_value ){
       m_hadjust->set_value( h_max_value );
    }


    m_vadjust->set_lower( 0 );
    m_vadjust->set_upper( c_max_track );
    m_vadjust->set_page_size( m_window_y / c_names_y );
    m_vadjust->set_step_increment( 1 );
    m_vadjust->set_page_increment( 1 );

    int v_max_value = c_max_track - (m_window_y / c_names_y);

    if ( m_vadjust->get_value() > v_max_value ){
        m_vadjust->set_value(v_max_value);
    }

    if ( is_realized() ){
	m_pixmap = Gdk::Pixmap::create( m_window,
                                        m_window_x,
                                        m_window_y, -1 );
    }

    queue_draw();
}

void
perfroll::increment_size()
{
    m_roll_length_ticks += (c_ppqn * 512);
    update_sizes( );
}

/* updates background */
void
perfroll::fill_background_pixmap()
{
    /* clear background */
    m_gc->set_foreground(m_white);
    m_background->draw_rectangle(m_gc,true,
 				0,
 				0,
 				c_perfroll_background_x,
 				c_names_y );

    /* draw horz grey lines */
    m_gc->set_foreground(m_grey);

    gint8 dash = 1;
    m_gc->set_dashes( 0, &dash, 1 );

    m_gc->set_line_attributes( 1,
                               Gdk::LINE_ON_OFF_DASH,
                               Gdk::CAP_NOT_LAST,
                               Gdk::JOIN_MITER );

    m_background->draw_line(m_gc,
			   0,
			   0,
			   c_perfroll_background_x,
			   0 );

    int beats = m_measure_length / m_beat_length;

    /* draw vert lines */
    for ( int i=0; i< beats ; ){

 	if ( i == 0 ){
            m_gc->set_line_attributes( 1,
                                       Gdk::LINE_SOLID,
                                       Gdk::CAP_NOT_LAST,
                                       Gdk::JOIN_MITER );
        }
        else
        {
            m_gc->set_line_attributes( 1,
                                       Gdk::LINE_ON_OFF_DASH,
                                       Gdk::CAP_NOT_LAST,
                                       Gdk::JOIN_MITER );
        }

        m_gc->set_foreground(m_grey);

 	/* solid line on every beat */
 	m_background->draw_line(m_gc,
 			       i * m_beat_length / c_perf_scale_x,
 			       0,
 			       i * m_beat_length / c_perf_scale_x,
 			       c_names_y );

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

    /* reset line style */

    m_gc->set_line_attributes( 1,
                               Gdk::LINE_SOLID,
                               Gdk::CAP_NOT_LAST,
                               Gdk::JOIN_MITER );
}


/* simply sets the snap member */
void
perfroll::set_guides( int a_snap, int a_measure, int a_beat )
{
    m_snap = a_snap;
    m_measure_length = a_measure;
    m_beat_length = a_beat;

    if ( is_realized() ){
        fill_background_pixmap();
    }

    queue_draw();
}

void
perfroll::draw_progress()
{
    long tick = m_mainperf->get_tick();
    long tick_offset = m_4bar_offset * c_ppqn * 16;

    int progress_x =     ( tick - tick_offset ) / c_perf_scale_x ;
    int old_progress_x = ( m_old_progress_ticks - tick_offset ) / c_perf_scale_x ;

    /* draw old */
    m_window->draw_drawable(m_gc,
			 m_pixmap,
			 old_progress_x, 0,
			 old_progress_x, 0,
			 1, m_window_y );

    m_gc->set_foreground(m_black);
    m_window->draw_line(m_gc,
		       progress_x, 0,
		       progress_x, m_window_y);

    m_old_progress_ticks = tick;
}



void perfroll::draw_track_on( Glib::RefPtr<Gdk::Drawable> a_draw, int a_track )
{

    long tick_on;
    long tick_off;
    long offset;
    bool selected;
    int seq_idx;
    sequence *seq;

    long tick_offset = m_4bar_offset * c_ppqn * 16;
    long x_offset = tick_offset / c_perf_scale_x;

    if ( a_track < c_max_track ){

	if ( m_mainperf->is_active_track( a_track )){

            m_track_active[a_track] = true;

	    track *trk =  m_mainperf->get_track( a_track );

	    trk->reset_draw_trigger_marker();

	    a_track -= m_track_offset;

	    while ( trk->get_next_trigger( &tick_on, &tick_off, &selected, &offset, &seq_idx )){

            if ( tick_off > 0 ){

		    long x_on  = tick_on  / c_perf_scale_x;
		    long x_off = tick_off / c_perf_scale_x;
		    int  w     = x_off - x_on + 1;

		    int x = x_on;
		    int y = c_names_y * a_track + 1;  // + 2
		    int h = c_names_y - 2; // - 4

                    // adjust to screen corrids
		    x = x - x_offset;

                    if ( selected )
                        m_gc->set_foreground(m_grey);
                    else
                        m_gc->set_foreground(m_white);

		    a_draw->draw_rectangle(m_gc,true,
					   x,
					   y,
					   w,
					   h );

		    m_gc->set_foreground(m_black);
		    a_draw->draw_rectangle(m_gc,false,
					   x,
					   y,
					   w,
					   h );

                    m_gc->set_foreground(m_black);
                    a_draw->draw_rectangle(m_gc,false,
					   x,
					   y,
					   c_perfroll_size_box_w,
					   c_perfroll_size_box_w );

                    a_draw->draw_rectangle(m_gc,false,
					   x+w-c_perfroll_size_box_w,
					   y+h-c_perfroll_size_box_w,
					   c_perfroll_size_box_w,
					   c_perfroll_size_box_w );

		    m_gc->set_foreground(m_black);

            char label[40];
            int max_label = (w / 6)-1;
            if(max_label < 0) max_label = 0;
            if(max_label > 39) max_label = 39;
            seq = trk->get_sequence(seq_idx);
            if(seq == NULL) {
                strncpy(label, "Empty", max_label);
            } else {
                strncpy(label, seq->get_name(), max_label);
	            long sequence_length = seq->get_length();
	            int length_w = sequence_length / c_perf_scale_x;
                long length_marker_first_tick = ( tick_on - (tick_on % sequence_length) + (offset % sequence_length) - sequence_length);


                long tick_marker = length_marker_first_tick;

                while ( tick_marker < tick_off ){

                    long tick_marker_x = (tick_marker / c_perf_scale_x) - x_offset;

                    if ( tick_marker > tick_on ){

                        m_gc->set_foreground(m_lt_grey);
                        a_draw->draw_rectangle(m_gc,true,
                                               tick_marker_x,
                                               y+4,
                                               1,
                                               h-8 );
                    }

                    int lowest_note = seq->get_lowest_note_event( );
                    int highest_note = seq->get_highest_note_event( );

                    int height = highest_note - lowest_note;
                    height += 2;

                    int length = seq->get_length( );

                    long tick_s;
                    long tick_f;
                    int note;

                    bool selected;

                    int velocity;
                    draw_type dt;

                    seq->reset_draw_marker();

                    m_gc->set_foreground(m_black);
                    while ( (dt = seq->get_next_note_event( &tick_s, &tick_f, &note,
                                                            &selected, &velocity )) != DRAW_FIN ){

                        int note_y = ((c_names_y-6) -
                            ((c_names_y-6)  * (note - lowest_note)) / height) + 1;

                        int tick_s_x = ((tick_s * length_w)  / length) + tick_marker_x;
                        int tick_f_x = ((tick_f * length_w)  / length) + tick_marker_x;

                        if ( dt == DRAW_NOTE_ON || dt == DRAW_NOTE_OFF )
                            tick_f_x = tick_s_x + 1;
                        if ( tick_f_x <= tick_s_x )
                            tick_f_x = tick_s_x + 1;

                        if ( tick_s_x < x ){
                            tick_s_x = x;
                        }

                        if ( tick_f_x > x + w ){
                            tick_f_x = x + w;
                        }

                        /*
                                [           ]
                         -----------
                                         ---------
                               ----------------
                     ------                      ------
                        */

                        if ( tick_f_x >= x && tick_s_x <= x+w )
                            m_pixmap->draw_line(m_gc, tick_s_x,
                                                y + note_y,
                                                tick_f_x,
                                                y + note_y );
                    }



                    tick_marker += sequence_length;
		    }
		    }
            label[max_label] = '\0';
            p_font_renderer->render_string_on_drawable(m_gc,
                                                       x+5,
                                                       y+2,
                                                       a_draw, label, font::BLACK );
		}
	    }
	}
    }
}




void perfroll::draw_background_on( Glib::RefPtr<Gdk::Drawable> a_draw, int a_track )
{
    long tick_offset = m_4bar_offset * c_ppqn * 16;
    long first_measure = tick_offset / m_measure_length;

    a_track -= m_track_offset;

    int y = c_names_y * a_track;
    int h = c_names_y;


    m_gc->set_foreground(m_white);
    a_draw->draw_rectangle(m_gc,true,
                         0,
                         y,
                         m_window_x,
                         h );

    m_gc->set_foreground(m_black);
    for ( int i = first_measure;
              i < first_measure +
                  (m_window_x * c_perf_scale_x /
                   (m_measure_length)) + 1;

              i++ )
    {
        int x_pos = ((i * m_measure_length) - tick_offset) / c_perf_scale_x;


           a_draw->draw_drawable(m_gc, m_background,
                                 0,
                                 0,
                                 x_pos,
                                 y,
                                 c_perfroll_background_x,
                                 c_names_y );


    }



}



bool
perfroll::on_expose_event(GdkEventExpose* e)
{

    int y_s = e->area.y / c_names_y;
    int y_f = (e->area.y  + e->area.height) / c_names_y;

    for ( int y=y_s; y<=y_f; y++ ){
        draw_background_on(m_pixmap, y + m_track_offset );
	    draw_track_on(m_pixmap, y + m_track_offset );
    }

    m_window->draw_drawable( m_gc, m_pixmap,
			  e->area.x,
			  e->area.y,
			  e->area.x,
			  e->area.y,
			  e->area.width,
			  e->area.height );
    return true;
}


void
perfroll::redraw_dirty_tracks( void )
{
    bool draw = false;

    int y_s = 0;
    int y_f = m_window_y / c_names_y;

    for ( int y=y_s; y<=y_f; y++ ){

        int track = y + m_track_offset; // 4am


            bool dirty = (m_mainperf->is_dirty_perf(track ));

            if (dirty)
            {
                draw_background_on(m_pixmap,track );
                draw_track_on(m_pixmap,track);
                draw = true;
            }
    }

    if ( draw )
        m_window->draw_drawable( m_gc, m_pixmap,
                                 0,
                                 0,
                                 0,
                                 0,
                                 m_window_x,
                                 m_window_y );
}




void
perfroll::draw_drawable_row( Glib::RefPtr<Gdk::Drawable> a_dest, Glib::RefPtr<Gdk::Drawable> a_src,  long a_y )
{
    int s = a_y / c_names_y;
    a_dest->draw_drawable(m_gc, a_src,
                          0,
                          c_names_y * s,
                          0,
                          c_names_y * s,
                          m_window_x,
                          c_names_y );
}


void
perfroll::start_playing( void )
{
    // keep in sync with perfedit's start_playing... wish i could call it directly...
    m_mainperf->position_jack( true );
    m_mainperf->start_jack( );
    m_mainperf->start( true );
}

void
perfroll::stop_playing( void )
{
    // keep in sync with perfedit's stop_playing... wish i could call it directly...
    m_mainperf->stop_jack();
    m_mainperf->stop();
}


bool
perfroll::on_button_press_event(GdkEventButton* a_ev)
{
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
    return result;
}

bool
perfroll::on_scroll_event( GdkEventScroll* a_ev )
{
    guint modifiers;    // Used to filter out caps/num lock etc.
    modifiers = gtk_accelerator_get_default_mod_mask ();

    if ((a_ev->state & modifiers) == GDK_SHIFT_MASK)
    {
        double val = m_hadjust->get_value();

        if ( a_ev->direction == GDK_SCROLL_UP ){
            val -= m_hadjust->get_step_increment();
        }
        else if ( a_ev->direction == GDK_SCROLL_DOWN ){
            val += m_hadjust->get_step_increment();
        }

        m_hadjust->clamp_page(val, val + m_hadjust->get_page_size());
    }
    else
    {
        double val = m_vadjust->get_value();

        if ( a_ev->direction == GDK_SCROLL_UP ){
            val -= m_vadjust->get_step_increment();
        }
        else if ( a_ev->direction == GDK_SCROLL_DOWN ){
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
             break;
        default:
             result = false;
    }
    return result;
}

bool
perfroll::on_key_press_event(GdkEventKey* a_p0)
{
    bool ret = false;

    if ( m_mainperf->is_active_track( m_drop_track)){

        if ( a_p0->type == GDK_KEY_PRESS ){

            if ( a_p0->keyval ==  GDK_Delete || a_p0->keyval == GDK_BackSpace ){

                m_mainperf->push_trigger_undo();
                m_mainperf->get_track( m_drop_track )->del_selected_trigger();

                ret = true;
            }

            if ( a_p0->state & GDK_CONTROL_MASK ){

                /* cut */
                if ( a_p0->keyval == GDK_x || a_p0->keyval == GDK_X ){

                    m_mainperf->push_trigger_undo();
                    m_mainperf->get_track( m_drop_track )->cut_selected_trigger();
                    ret = true;
                }
                /* copy */
                if ( a_p0->keyval == GDK_c || a_p0->keyval == GDK_C ){

                    m_mainperf->get_track( m_drop_track )->copy_selected_trigger();
                    cross_track_paste = false;
                    ret = true;
                }

                /* paste */
                if ( a_p0->keyval == GDK_v || a_p0->keyval == GDK_V ){
                    m_mainperf->push_trigger_undo();    // FIXME should not push unless something actually gets pasted

                    bool cross_track = false;
                    for ( int t=0; t<c_max_track; ++t )
                    {
                        if (! m_mainperf->is_active_track( t ))
                        {
                            continue;
                        }

                        if(m_mainperf->get_track(t)->get_trigger_copied() &&    // do we have a copy to clipboard
                            m_mainperf->get_track( m_drop_track)->get_trigger_paste_tick() >= 0 &&  // do we have a paste
                           !cross_track_paste) // has cross track paste NOT been done for this copy
                        {
                            if(t != m_drop_track) // If clipboard and paste are on diff tracks - then cross track paste
                            {
                                paste_trigger_sequence( m_mainperf->get_track( m_drop_track ),
                                                        m_mainperf->get_track(t)->get_sequence(m_mainperf->get_track(t)
                                                                               ->get_trigger_clipboard()->m_sequence ));
                                ret = true;
                                cross_track_paste = true;
                                cross_track = true;
                                break;
                            }
                        }

                    }
                    if(!cross_track)
                    {
                        m_mainperf->get_track( m_drop_track )->paste_trigger();
                        cross_track = false; // is this necessary?
                        ret = true;
                    }
                }
            }
        }
    }

    if ( ret == true ){

        fill_background_pixmap();
        queue_draw();
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

    int mod = (m_snap / c_perf_scale_x );

    if ( mod <= 0 )
 	mod = 1;

    *a_x = *a_x - (*a_x % mod );
}


void
perfroll::convert_x( int a_x, long *a_tick )
{

    long tick_offset = m_4bar_offset * c_ppqn * 16;
    *a_tick = a_x * c_perf_scale_x;
    *a_tick += tick_offset;
}


void
perfroll::convert_xy( int a_x, int a_y, long *a_tick, int *a_track)
{

    long tick_offset = m_4bar_offset * c_ppqn * 16;

    *a_tick = a_x * c_perf_scale_x;
    *a_track = a_y / c_names_y;

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
    set_flags(Gtk::HAS_FOCUS);
    return false;
}


bool
perfroll::on_focus_out_event(GdkEventFocus*)
{
    unset_flags(Gtk::HAS_FOCUS);
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
perfroll::on_size_request(GtkRequisition* a_r )
{
}


void
perfroll::new_sequence( track *a_track, trigger *a_trigger )
{
    int seq_idx = a_track->new_sequence();
    m_mainperf->push_trigger_undo();
    sequence *a_sequence = a_track->get_sequence(seq_idx);
    a_track->set_trigger_sequence(a_trigger, seq_idx);
    new seqedit( a_sequence, m_mainperf );
}

void
perfroll::copy_sequence( track *a_track, trigger *a_trigger, sequence *a_seq )
{
    bool same_track = a_track == a_seq->get_track();
    int seq_idx = a_track->new_sequence();
    m_mainperf->push_trigger_undo();
    sequence *a_sequence = a_track->get_sequence(seq_idx);
    *a_sequence = *a_seq;
    a_sequence->set_track(a_track);
    if(same_track) {
        char new_name[c_max_seq_name+1];
        snprintf(new_name, sizeof(new_name), "%s copy", a_sequence->get_name());
        a_sequence->set_name( new_name );
    }
    a_track->set_trigger_sequence(a_trigger, seq_idx);
    new seqedit( a_sequence, m_mainperf );
}

void
perfroll::edit_sequence( track *a_track, trigger *a_trigger )
{
    sequence *a_seq = a_track->get_trigger_sequence(a_trigger);
    if(a_seq->get_editing()) {
        a_seq->set_raise(true);
    } else {
        new seqedit( a_seq, m_mainperf );
    }
}

void perfroll::set_trigger_sequence( track *a_track, trigger *a_trigger, int a_sequence )
{
    m_mainperf->push_trigger_undo();
    a_track->set_trigger_sequence(a_trigger, a_sequence);
}


void
perfroll::del_trigger( track *a_track, long a_tick )
{
    m_mainperf->push_trigger_undo();
    a_track->del_trigger( a_tick );
    a_track->set_dirty( );
}

void
perfroll::paste_trigger_sequence( track *p_track, sequence *a_sequence )
{
     // empty trigger = segfault via get_length - don't allow w/o sequence
    if (a_sequence == NULL)
        return;

    if ( a_sequence->get_track()->get_trigger_copied() ){

            if(a_sequence->get_track() != p_track)  // then we have to copy the sequence
            {
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

                long offset_adjust = p_track->get_trigger_paste_tick() - p_track->get_trigger_clipboard()->m_tick_start;
                p_track->add_trigger(p_track->get_trigger_paste_tick(),
                         length,
                         p_track->get_trigger_clipboard()->m_offset + offset_adjust,
                         p_track->get_trigger_clipboard()->m_sequence); // +/- distance to paste tick from start

                p_track->get_trigger_clipboard()->m_tick_start = p_track->get_trigger_paste_tick();
                p_track->get_trigger_clipboard()->m_tick_end = p_track->get_trigger_clipboard()->m_tick_start + length - 1;
                p_track->get_trigger_clipboard()->m_offset += offset_adjust;

                long a_length = p_track->get_sequence(p_track->get_trigger_clipboard()->m_sequence)->get_length();
                p_track->get_trigger_clipboard()->m_offset = p_track->adjust_offset(p_track->get_trigger_clipboard()->m_offset,a_length);

                //printf("p-m_offset[%ld]\np-m_tick_start[%ld]\np-m_tick_end[%ld]\n",p_track->get_trigger_clipboard()->m_offset,
                //       p_track->get_trigger_clipboard()->m_tick_start,p_track->get_trigger_clipboard()->m_tick_end);

                p_track->set_trigger_paste_tick(-1); // reset to default
                p_track->set_trigger_copied();  // change to paste track
                a_sequence->get_track()->unset_trigger_copied(); // undo original
            }
    }
}

//////////////////////////
// interaction methods
//////////////////////////


void FruityPerfInput::updateMousePtr( perfroll& ths )
{
    // context sensitive mouse
    long drop_tick;
    int drop_track;
    ths.convert_xy( m_current_x, m_current_y, &drop_tick, &drop_track );
    if (ths.m_mainperf->is_active_track( drop_track ))
    {
         long start, end;
         if (ths.m_mainperf->get_track(drop_track)->intersectTriggers( drop_tick, start, end ))
	     {
             if (start <= drop_tick && drop_tick <= start + (c_perfroll_size_box_click_w * c_perf_scale_x) &&
                 (m_current_y % c_names_y) <= c_perfroll_size_box_click_w + 1)
             {
                ths.get_window()->set_cursor( Gdk::Cursor( Gdk::RIGHT_PTR ));
             }
             else if (end - (c_perfroll_size_box_click_w * c_perf_scale_x) <= drop_tick && drop_tick <= end &&
                      (m_current_y % c_names_y) >= c_names_y - c_perfroll_size_box_click_w - 1)
             {
                ths.get_window()->set_cursor( Gdk::Cursor( Gdk::LEFT_PTR ));
             }
             else
             {
                ths.get_window()->set_cursor( Gdk::Cursor( Gdk::CENTER_PTR ));
             }
         }
         else
         {
             ths.get_window()->set_cursor( Gdk::Cursor( Gdk::PENCIL ));
         }
    }
    else
    {
        ths.get_window()->set_cursor( Gdk::Cursor( Gdk::CROSSHAIR ));
    }
}


bool FruityPerfInput::on_button_press_event(GdkEventButton* a_ev, perfroll& ths)
{
    ths.grab_focus( );

    if ( ths.m_mainperf->is_active_track( ths.m_drop_track ))
    {
        ths.m_mainperf->get_track( ths.m_drop_track )->unselect_triggers( );
        ths.draw_background_on( ths.m_pixmap, ths.m_drop_track );
        ths.draw_track_on( ths.m_pixmap, ths.m_drop_track );
        ths.draw_drawable_row( ths.m_window, ths.m_pixmap, ths.m_drop_y);
    }

    ths.m_drop_x = (int) a_ev->x;
    ths.m_drop_y = (int) a_ev->y;
    m_current_x = (int) a_ev->x;
    m_current_y = (int) a_ev->y;


    ths.convert_xy( ths.m_drop_x, ths.m_drop_y, &ths.m_drop_tick, &ths.m_drop_track );

    /*      left mouse button     */
    if ( a_ev->button == 1 && !(a_ev->state & GDK_CONTROL_MASK)){

        long tick = ths.m_drop_tick;

        /* add a new note if we didnt select anything */
        //if (  m_adding )
        {

            m_adding_pressed = true;

            if ( ths.m_mainperf->is_active_track( ths.m_drop_track )){

                bool state = ths.m_mainperf->get_track( ths.m_drop_track )->get_trigger_state( tick );

                // resize the event, or move it, depending on where clicked.
                if ( state )
                {
                    //m_adding = false;
                    m_adding_pressed = false;
                    ths.m_mainperf->push_trigger_undo();
                    ths.m_mainperf->get_track( ths.m_drop_track )->select_trigger( tick );

                    long start_tick = ths.m_mainperf->get_track( ths.m_drop_track )->get_selected_trigger_start_tick();
                    long end_tick = ths.m_mainperf->get_track( ths.m_drop_track )->get_selected_trigger_end_tick();

                    if ( tick >= start_tick &&
                            tick <= start_tick + (c_perfroll_size_box_click_w * c_perf_scale_x) &&
                            (ths.m_drop_y % c_names_y) <= c_perfroll_size_box_click_w + 1 )
                    {
                        // clicked left side: begin a grow/shrink for the left side
                        ths.m_growing = true;
                        ths.m_grow_direction = true;
                        ths.m_drop_tick_trigger_offset = ths.m_drop_tick -
                            ths.m_mainperf->get_track( ths.m_drop_track )->
                            get_selected_trigger_start_tick( );
                    }
                    else
                        if ( tick >= end_tick - (c_perfroll_size_box_click_w * c_perf_scale_x) &&
                                tick <= end_tick &&
                                (ths.m_drop_y % c_names_y) >= c_names_y - c_perfroll_size_box_click_w - 1 )
                        {
                            // clicked right side: grow/shrink the right side
                            ths.m_growing = true;
                            ths.m_grow_direction = false;
                            ths.m_drop_tick_trigger_offset =
                                ths.m_drop_tick -
                                ths.m_mainperf->get_track( ths.m_drop_track )->get_selected_trigger_end_tick( );
                        }
                        else
                        {
                             // clicked in the middle - move it
                            ths.m_moving = true;
                            ths.m_drop_tick_trigger_offset = ths.m_drop_tick -
                                ths.m_mainperf->get_track( ths.m_drop_track )->
                                get_selected_trigger_start_tick( );

                        }

                    ths.draw_background_on( ths.m_pixmap, ths.m_drop_track );
                    ths.draw_track_on( ths.m_pixmap, ths.m_drop_track );
                    ths.draw_drawable_row( ths.m_window, ths.m_pixmap, ths.m_drop_y);
                }

                // add an event:
                else
                {
                    tick = tick - (tick % c_default_trigger_length);

                    ths.m_mainperf->push_trigger_undo();
                    ths.m_mainperf->get_track( ths.m_drop_track )->add_trigger( tick, c_default_trigger_length );
                    ths.draw_background_on( ths.m_pixmap, ths.m_drop_track );
                    ths.draw_track_on( ths.m_pixmap, ths.m_drop_track );
                    ths.draw_drawable_row( ths.m_window, ths.m_pixmap, ths.m_drop_y);

                }
            }
        }
    }

    /*     right mouse button      */
    if ( a_ev->button == 3 ){
        //set_adding( false );

        long tick = ths.m_drop_tick;

        if ( ths.m_mainperf->is_active_track( ths.m_drop_track )){


            bool state = ths.m_mainperf->get_track( ths.m_drop_track )->get_trigger_state( tick );

            if ( state )
            {
                ths.m_mainperf->push_trigger_undo();
                ths.m_mainperf->get_track( ths.m_drop_track )->del_trigger( tick );
            }
        }
    }

    /* left-ctrl: split -- button 2: paste */
    if ( a_ev->button == 2 ||
        (a_ev->button == 1 && (a_ev->state & GDK_CONTROL_MASK) ))
    {
        long tick = ths.m_drop_tick;
        tick = tick - tick % ths.m_snap; // grid snap

        if ( ths.m_mainperf->is_active_track( ths.m_drop_track )){
            bool state = ths.m_mainperf->get_track( ths.m_drop_track )->get_trigger_state( tick );

            if ( state && a_ev->button != 2)// clicked on trigger for split
            {
                ths.m_mainperf->push_trigger_undo();

                ths.m_mainperf->get_track( ths.m_drop_track )->split_trigger( tick );

                ths.draw_background_on( ths.m_pixmap, ths.m_drop_track );
                ths.draw_track_on( ths.m_pixmap, ths.m_drop_track );
                ths.draw_drawable_row( ths.m_window, ths.m_pixmap, ths.m_drop_y);
            }
            else // clicked off trigger for paste
            {
                ths.m_mainperf->get_track( ths.m_drop_track )->set_trigger_paste_tick(tick);
            }
        }
    }
    updateMousePtr( ths );
    return true;
}

bool FruityPerfInput::on_button_release_event(GdkEventButton* a_ev, perfroll& ths)
{
    m_current_x = (int) a_ev->x;
    m_current_y = (int) a_ev->y;

    if ( a_ev->button == 1 || a_ev->button == 3 )
    {
        m_adding_pressed = false;
    }

    if ( a_ev->button == 2 )
    {
        ths.trigger_menu_popup(a_ev,ths);
    }

    ths.m_moving = false;
    ths.m_growing = false;
    m_adding_pressed = false;

    if ( ths.m_mainperf->is_active_track( ths.m_drop_track  )){

        ths.draw_background_on( ths.m_pixmap, ths.m_drop_track );
        ths.draw_track_on( ths.m_pixmap, ths.m_drop_track );
        ths.draw_drawable_row( ths.m_window, ths.m_pixmap, ths.m_drop_y );
    }

    updateMousePtr( ths );
    return true;
}

bool FruityPerfInput::on_motion_notify_event(GdkEventMotion* a_ev, perfroll& ths)
{
    long tick;
    int x = (int) a_ev->x;
    m_current_x = (int) a_ev->x;
    m_current_y = (int) a_ev->y;

    if (  m_adding_pressed ){

    	ths.convert_x( x, &tick );

        if ( ths.m_mainperf->is_active_track( ths.m_drop_track )){

            tick = tick - (tick % c_default_trigger_length);

            /*long min_tick = (tick < m_drop_tick) ? tick : m_drop_tick;*/

    	    ths.m_mainperf->get_track( ths.m_drop_track )
                          ->grow_trigger( ths.m_drop_tick, tick, c_default_trigger_length);
    	    ths.draw_background_on( ths.m_pixmap, ths.m_drop_track );
    	    ths.draw_track_on( ths.m_pixmap, ths.m_drop_track );
            ths.draw_drawable_row( ths.m_window, ths.m_pixmap, ths.m_drop_y);
    	}
    }
    else if ( ths.m_moving || ths.m_growing )
    {
        if ( ths.m_mainperf->is_active_track( ths.m_drop_track))
        {
            ths.convert_x( x, &tick );
            tick -= ths.m_drop_tick_trigger_offset;

            tick = tick - tick % ths.m_snap;

            if ( ths.m_moving )
            {
                ths.m_mainperf->get_track( ths.m_drop_track )
                              ->move_selected_triggers_to( tick, true );
            }
            if ( ths.m_growing )
            {
                if ( ths.m_grow_direction )
                    ths.m_mainperf->get_track( ths.m_drop_track )
                                  ->move_selected_triggers_to( tick, false, 0 );
                else
                    ths.m_mainperf->get_track( ths.m_drop_track )
                                  ->move_selected_triggers_to( tick-1, false, 1 );
            }


            ths.draw_background_on( ths.m_pixmap, ths.m_drop_track );
            ths.draw_track_on( ths.m_pixmap, ths.m_drop_track );
            ths.draw_drawable_row( ths.m_window, ths.m_pixmap, ths.m_drop_y);
        }
    }

    updateMousePtr( ths );
    return true;
}


/* popup menu calls this */
void
Seq42PerfInput::set_adding( bool a_adding, perfroll& ths )
{
    if ( a_adding )
    {
	   ths.get_window()->set_cursor(  Gdk::Cursor( Gdk::PENCIL ));
	   m_adding = true;
    }
    else
    {
	    ths.get_window()->set_cursor( Gdk::Cursor( Gdk::LEFT_PTR ));
	    m_adding = false;
    }
}

bool
Seq42PerfInput::on_button_press_event(GdkEventButton* a_ev, perfroll& ths)
{
    ths.grab_focus( );


    if ( ths.m_mainperf->is_active_track( ths.m_drop_track ))
    {
        ths.m_mainperf->get_track( ths.m_drop_track )->unselect_triggers( );
        ths.draw_background_on( ths.m_pixmap, ths.m_drop_track );
        ths.draw_track_on( ths.m_pixmap, ths.m_drop_track );
        ths.draw_drawable_row( ths.m_window, ths.m_pixmap, ths.m_drop_y);
    }

    ths.m_drop_x = (int) a_ev->x;
    ths.m_drop_y = (int) a_ev->y;

    ths.convert_xy( ths.m_drop_x, ths.m_drop_y, &ths.m_drop_tick, &ths.m_drop_track );

    /*      left mouse button     */
    if ( a_ev->button == 1 ){

        long tick = ths.m_drop_tick;

        /* add a new note if we didnt select anything */
        if (  m_adding ){

            m_adding_pressed = true;

            if ( ths.m_mainperf->is_active_track( ths.m_drop_track )){

                bool state = ths.m_mainperf->get_track( ths.m_drop_track )->get_trigger_state( tick );

                if ( state )
                {
                    ths.m_mainperf->push_trigger_undo();
                    ths.m_mainperf->get_track( ths.m_drop_track )->del_trigger( tick );
                }
                else
                {

                    // snap to length of sequence
                    tick = tick - (tick % c_default_trigger_length);

                    ths.m_mainperf->push_trigger_undo();
                    ths.m_mainperf->get_track( ths.m_drop_track )->add_trigger( tick, c_default_trigger_length );
                    ths.draw_background_on( ths.m_pixmap, ths.m_drop_track );
                    ths.draw_track_on( ths.m_pixmap, ths.m_drop_track );
                    ths.draw_drawable_row( ths.m_window, ths.m_pixmap, ths.m_drop_y);

                }
            }
        }
        else {

            if ( ths.m_mainperf->is_active_track( ths.m_drop_track )){

                ths.m_mainperf->push_trigger_undo();
                ths.m_mainperf->get_track( ths.m_drop_track )->select_trigger( tick );

                long start_tick = ths.m_mainperf->get_track( ths.m_drop_track )->get_selected_trigger_start_tick();
                long end_tick = ths.m_mainperf->get_track( ths.m_drop_track )->get_selected_trigger_end_tick();

                if ( tick >= start_tick &&
                        tick <= start_tick + (c_perfroll_size_box_click_w * c_perf_scale_x) &&
                        (ths.m_drop_y % c_names_y) <= c_perfroll_size_box_click_w + 1 )
                {
                    ths.m_growing = true;
                    ths.m_grow_direction = true;
                    ths.m_drop_tick_trigger_offset = ths.m_drop_tick -
                        ths.m_mainperf->get_track( ths.m_drop_track )->
                                        get_selected_trigger_start_tick( );
                }
                else
                    if ( tick >= end_tick - (c_perfroll_size_box_click_w * c_perf_scale_x) &&
                            tick <= end_tick &&
                            (ths.m_drop_y % c_names_y) >= c_names_y - c_perfroll_size_box_click_w - 1 )
                    {
                        ths.m_growing = true;
                        ths.m_grow_direction = false;
                        ths.m_drop_tick_trigger_offset =
                            ths.m_drop_tick -
                            ths.m_mainperf->get_track( ths.m_drop_track )->get_selected_trigger_end_tick( );
                    }
                    else
                    {
                        ths.m_moving = true;
                        ths.m_drop_tick_trigger_offset = ths.m_drop_tick -
                            ths.m_mainperf->get_track( ths.m_drop_track )->
                                            get_selected_trigger_start_tick( );

                    }

                ths.draw_background_on( ths.m_pixmap, ths.m_drop_track );
                ths.draw_track_on( ths.m_pixmap, ths.m_drop_track );
                ths.draw_drawable_row( ths.m_window, ths.m_pixmap, ths.m_drop_y);
            }
        }
    }

    /*     right mouse button      */
    if ( a_ev->button == 3 ){
        track *a_track = NULL;
        trigger *a_trigger = NULL;
        if ( ths.m_mainperf->is_active_track( ths.m_drop_track )){
            a_track = ths.m_mainperf->get_track( ths.m_drop_track );
            a_trigger = a_track->get_trigger( ths.m_drop_tick );
        }
        if(a_trigger == NULL) {
            set_adding( true, ths );
        }
    }

    /* middle, split */
    if ( a_ev->button == 2 )
    {
        long tick = ths.m_drop_tick;
        tick = tick - tick % ths.m_snap; // grid snap

        if ( ths.m_mainperf->is_active_track( ths.m_drop_track )){
            bool state = ths.m_mainperf->get_track( ths.m_drop_track )->get_trigger_state( tick );

            if ( state )    // clicked on trigger for split
            {
                ths.m_mainperf->push_trigger_undo();

                ths.m_mainperf->get_track( ths.m_drop_track )->split_trigger( tick );

                ths.draw_background_on( ths.m_pixmap, ths.m_drop_track );
                ths.draw_track_on( ths.m_pixmap, ths.m_drop_track );
                ths.draw_drawable_row( ths.m_window, ths.m_pixmap, ths.m_drop_y);
            }
            else    // clicked off trigger for paste
            {
                ths.m_mainperf->get_track( ths.m_drop_track )->set_trigger_paste_tick(tick);
            }
        }
    }
    return true;
}

bool Seq42PerfInput::on_button_release_event(GdkEventButton* a_ev, perfroll& ths)
{
    using namespace Menu_Helpers;

    if ( a_ev->button == 1 ){

        if ( m_adding ){
            m_adding_pressed = false;
        }
    }

    if ( a_ev->button == 3 ){
        if(m_adding)
        {
	        m_adding_pressed = false;
 	        set_adding( false, ths );
        }
        else
        {
            ths.trigger_menu_popup(a_ev, ths);
        }
    }

    ths.m_moving = false;
    ths.m_growing = false;
    m_adding_pressed = false;

    if ( ths.m_mainperf->is_active_track( ths.m_drop_track  )){

        ths.draw_background_on( ths.m_pixmap, ths.m_drop_track );
        ths.draw_track_on( ths.m_pixmap, ths.m_drop_track );
        ths.draw_drawable_row( ths.m_window, ths.m_pixmap, ths.m_drop_y );
    }

    return true;
}

bool Seq42PerfInput::on_motion_notify_event(GdkEventMotion* a_ev, perfroll& ths)
{
    long tick;
    int x = (int) a_ev->x;

    if (  m_adding && m_adding_pressed ){

    	ths.convert_x( x, &tick );

    	if ( ths.m_mainperf->is_active_track( ths.m_drop_track )){

            tick = tick - (tick % c_default_trigger_length);

    	    ths.m_mainperf->get_track( ths.m_drop_track )
                          ->grow_trigger( ths.m_drop_tick, tick, c_default_trigger_length);
    	    ths.draw_background_on( ths.m_pixmap, ths.m_drop_track );
    	    ths.draw_track_on( ths.m_pixmap, ths.m_drop_track );
            ths.draw_drawable_row( ths.m_window, ths.m_pixmap, ths.m_drop_y);
    	}
    }
    else if ( ths.m_moving || ths.m_growing ){

        if ( ths.m_mainperf->is_active_track( ths.m_drop_track)){

            ths.convert_x( x, &tick );
            tick -= ths.m_drop_tick_trigger_offset;

            tick = tick - tick % ths.m_snap;

            if ( ths.m_moving )
            {
                ths.m_mainperf->get_track( ths.m_drop_track )
                              ->move_selected_triggers_to( tick, true );
            }
            if ( ths.m_growing )
            {
                if ( ths.m_grow_direction )
                    ths.m_mainperf->get_track( ths.m_drop_track )
                                  ->move_selected_triggers_to( tick, false, 0 );
                else
                    ths.m_mainperf->get_track( ths.m_drop_track )
                                  ->move_selected_triggers_to( tick-1, false, 1 );
            }


            ths.draw_background_on( ths.m_pixmap, ths.m_drop_track );
            ths.draw_track_on( ths.m_pixmap, ths.m_drop_track );
            ths.draw_drawable_row( ths.m_window, ths.m_pixmap, ths.m_drop_y);
        }
    }

    return true;
}

void
perfroll::trigger_menu_popup(GdkEventButton* a_ev, perfroll& ths)
{
    using namespace Menu_Helpers;
    track *a_track = NULL;
    trigger *a_trigger = NULL;
    if ( ths.m_mainperf->is_active_track( ths.m_drop_track )){
        a_track = ths.m_mainperf->get_track( ths.m_drop_track );
        a_trigger = a_track->get_trigger( ths.m_drop_tick );
    }
    if(a_trigger != NULL) {
        Menu *menu_trigger =   manage( new Menu());
        //menu_trigger->items().push_back(SeparatorElem());
        if(a_trigger->m_sequence > -1) {
            menu_trigger->items().push_back(MenuElem("Edit sequence", sigc::bind(mem_fun(ths,&perfroll::edit_sequence), a_track, a_trigger )));
        }
        menu_trigger->items().push_back(MenuElem("New sequence", sigc::bind(mem_fun(ths,&perfroll::new_sequence), a_track, a_trigger )));
        if(a_track->get_number_of_sequences()) {
            char name[40];
            Menu *set_seq_menu = manage( new Menu());
            for (unsigned s=0; s< a_track->get_number_of_sequences(); s++ ){
                sequence *a_seq = a_track->get_sequence( s );
                snprintf(name, sizeof(name),"[%d] %s", s+1, a_seq->get_name());
                set_seq_menu->items().push_back(MenuElem(name,
                    sigc::bind(mem_fun(ths, &perfroll::set_trigger_sequence), a_track, a_trigger, s)));

            }
            menu_trigger->items().push_back(MenuElem("Set sequence", *set_seq_menu));
        }
        menu_trigger->items().push_back(MenuElem("Delete trigger", sigc::bind(mem_fun(ths,&perfroll::del_trigger), a_track, ths.m_drop_tick )));


        Menu *copy_seq_menu = NULL;
        char name[40];
        for ( int t=0; t<c_max_track; ++t ){
                if (! ths.m_mainperf->is_active_track( t )){
                    continue;
                }
            track *some_track = ths.m_mainperf->get_track(t);

            Menu *menu_t = NULL;
            bool inserted = false;
            for (unsigned s=0; s< some_track->get_number_of_sequences(); s++ ){
                if ( !inserted ){
                    if(copy_seq_menu == NULL) {
                        copy_seq_menu = manage( new Menu());
                    }
                    inserted = true;
                    snprintf(name, sizeof(name), "[%d] %s", t+1, some_track->get_name());
                    menu_t = manage( new Menu());
                    copy_seq_menu->items().push_back(MenuElem(name, *menu_t));
                }

                sequence *a_seq = some_track->get_sequence( s );
                snprintf(name, sizeof(name),"[%d] %s", s+1, a_seq->get_name());
                menu_t->items().push_back(MenuElem(name,
                    sigc::bind(mem_fun(ths, &perfroll::copy_sequence), a_track, a_trigger, a_seq)));

            }
        }
        if(copy_seq_menu != NULL) {
            menu_trigger->items().push_back(MenuElem("Copy sequence", *copy_seq_menu));
        }

        menu_trigger->popup(0,0);
    }
}
