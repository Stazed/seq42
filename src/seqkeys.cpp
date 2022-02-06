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
#include "seqkeys.h"
#include "font.h"


seqkeys::seqkeys(sequence *a_seq,
                 Gtk::Adjustment *a_vadjust ):
    m_black(Gdk::Color("black")),
    m_white(Gdk::Color("white")),
    m_red(Gdk::Color("red")),  // mute
    m_green(Gdk::Color("green")), // solo
    m_blue(Gdk::Color("blue")), // hint
    m_seq(a_seq),
    m_vadjust(a_vadjust),
    m_scroll_offset_key(0),
    m_scroll_offset_y(0),
    m_hint_state(false),
    m_hint_key(0),
    m_enter_piano_roll(false),
    m_keying(false),
    m_scale(0),
    m_key(0)
{
    add_events( Gdk::BUTTON_PRESS_MASK |
                Gdk::BUTTON_RELEASE_MASK |
                Gdk::ENTER_NOTIFY_MASK |
                Gdk::LEAVE_NOTIFY_MASK |
                Gdk::POINTER_MOTION_MASK |
                Gdk::SCROLL_MASK);

    /* set default size */
    set_size_request( c_keyarea_x +1, 10 );

    //m_window_x = 10;
    //m_window_y = c_keyarea_y;

    // in the construor you can only allocate colors,
    // get_window() returns 0 because we have not be realized
    Glib::RefPtr<Gdk::Colormap> colormap = get_default_colormap();

    colormap->alloc_color( m_black );
    colormap->alloc_color( m_white );
    colormap->alloc_color( m_red );
    colormap->alloc_color( m_green );
    colormap->alloc_color( m_blue );

    set_double_buffered( false );
}

void
seqkeys::on_realize()
{
    // we need to do the default realize
    Gtk::DrawingArea::on_realize();

    // Now we can allocate any additional resources we need
    m_window = get_window();
    m_window->clear();

    m_pixmap = Gdk::Pixmap::create(m_window,
                                   c_keyarea_x,
                                   c_keyarea_y,
                                   -1 );

    update_pixmap();

    m_vadjust->signal_value_changed().connect( mem_fun( *this, &seqkeys::change_vert ));

    change_vert();
}

/* sets the music scale */
void
seqkeys::set_scale( int a_scale )
{
    if ( m_scale != a_scale )
    {
        m_scale = a_scale;
        reset();
    }
}

/* sets the key */
void
seqkeys::set_key( int a_key )
{
    if ( m_key != a_key )
    {
        m_key = a_key;
        reset();
    }
}

void
seqkeys::reset()
{
    update_pixmap();
    queue_draw();
}

void
seqkeys::update_pixmap()
{
    cairo_t *cr = gdk_cairo_create (m_pixmap->gobj());
    cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);    // Black FIXME
    cairo_set_line_width(cr, 1.0);
    cairo_rectangle(cr, 0.0, 0.0, c_keyarea_x, c_keyarea_y);
    cairo_stroke_preserve(cr);
    cairo_fill(cr);

    cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);    // White FIXME
    cairo_rectangle(cr, 1.0, 1.0, c_keyoffset_x - 1, c_keyarea_y - 2);
    cairo_stroke_preserve(cr);
    cairo_fill(cr);

    for ( int i = 0; i < c_num_keys; i++ )
    {
        int note_color = White_Note;
        
        if(m_seq->is_note_mute(c_num_keys - i - 1))
        {
            note_color = Red_Note;      /* red for mute */
        }
        else if(m_seq->is_note_solo(c_num_keys - i - 1))
        {
            note_color = Green_Note;    /* green for solo */
        }
        
        cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);    // White FIXME
        cairo_rectangle(cr, c_keyoffset_x + 1,
                                 (c_key_y * i) + 1,
                                 c_key_x - 2,
                                 c_key_y - 1 );
        cairo_stroke_preserve(cr);
        cairo_fill(cr);

        /* the the key in the octave */
        int key = (c_num_keys - i - 1) % 12;

        /* These are the black keys */
        if ( key == 1 ||
                key == 3 ||
                key == 6 ||
                key == 8 ||
                key == 10 )
        {
            if(note_color == White_Note)    // Not a muted or solo note so set it to black 
            {
                cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);        // Black FIXME
            }
            else if (note_color == Green_Note)
            {
                cairo_set_source_rgb(cr, 0.0, 0.4, 0.0);        // Green FIXME
            }
            else
            {
                cairo_set_source_rgb(cr, 1.0, 0.27, 0.0);       // Red FIXME
            }
            
            cairo_rectangle(cr, c_keyoffset_x + 1,
                                     (c_key_y * i) + 2,
                                     c_key_x - 3,
                                     c_key_y - 3 );
            cairo_stroke_preserve(cr);
            cairo_fill(cr);
        }
        else if( note_color  != White_Note) // We have a mute or solo so use the appropriate color
        {
            if( note_color == Green_Note)
            {
                cairo_set_source_rgb(cr, 0.0, 0.4, 0.0);        // Green FIXME
            }
            else
            {
                cairo_set_source_rgb(cr, 1.0, 0.27, 0.0);       // Red FIXME
            }

            cairo_rectangle(cr, c_keyoffset_x + 1,
                         (c_key_y * i) + 2,
                         c_key_x - 3,
                         c_key_y - 3 );
            cairo_stroke_preserve(cr);
            cairo_fill(cr);
        }

        /* The key letter to the left of the piano roll */
        char notes[20];

        if ( key == m_key  )
        {
            /* notes */
            int octave = ((c_num_keys - i - 1) / 12) - 1;
            if ( octave < 0 )
                octave *= -1;

            snprintf(notes, sizeof(notes), "%2s%1d", c_key_text[key], octave);

            cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);    // Black FIXME
            cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
            cairo_set_font_size(cr, 9.0);
            cairo_move_to(cr, 2,  ((c_key_y * i) - 1) + 7);
            cairo_show_text( cr, notes);
        }

        //snprintf(notes, sizeof(notes), "%c %d", c_scales_symbol[m_scale][key], m_scale );

        //p_font_renderer->render_string_on_drawable(m_gc,
        //                                             2 + (c_text_x * 4),
        //                                             c_key_y * i - 1,
        //                                             m_pixmap, notes, font::BLACK );
    }

    cairo_destroy(cr);
}

/**
 * This is never called
 */
void
seqkeys::draw_area()
{
    update_pixmap();
    
    cairo_t *cr = gdk_cairo_create (m_window->gobj());
    gdk_cairo_set_source_pixmap(cr,
                                m_pixmap->gobj(),
                                0,
                                -m_scroll_offset_y);
    cairo_rectangle(cr, 0,
                        0,
                        c_keyarea_x,
                        c_keyarea_y );
    cairo_fill(cr);
    cairo_destroy(cr);
}

bool
seqkeys::on_expose_event(GdkEventExpose* a_e)
{
    cairo_t *cr = gdk_cairo_create (m_window->gobj());
    gdk_cairo_set_source_pixmap(cr,
                                m_pixmap->gobj(), 0.0,
                                -m_scroll_offset_y);
    cairo_rectangle(cr, a_e->area.x,
                            a_e->area.y,
                            a_e->area.width,
                            a_e->area.height );
    cairo_fill(cr);
    cairo_destroy(cr);

    return true;
}

void
seqkeys::force_draw()
{
    cairo_t *cr = gdk_cairo_create (m_window->gobj());
    gdk_cairo_set_source_pixmap(cr, m_pixmap->gobj(), 0.0, -m_scroll_offset_y);
    cairo_rectangle(cr, 0.0, 0.0, m_window_x, m_window_y);
    cairo_fill(cr);
    cairo_destroy(cr);
}

/* takes screen corrdinates, give us notes and ticks */
void
seqkeys::convert_y( int a_y, int *a_note)
{
    *a_note = (c_rollarea_y - a_y - 2) / c_key_y;
}

bool
seqkeys::on_button_press_event(GdkEventButton *a_e)
{
    int y,note;

    if ( a_e->type == GDK_BUTTON_PRESS )
    {
        y = (int) a_e->y + m_scroll_offset_y;

        /* Left mouse button - play the note */
        if ( a_e->button == 1 )
        {
            m_keying = true;

            convert_y( y,&note );
            m_seq->play_note_on(  note );

            m_keying_note = note;
        }
        
        /* Right mouse button - mute the note */
        if (m_enter_piano_roll &&  a_e->button == 3 )
        {
            convert_y( y,&note );
            m_seq->set_mute_note( note );
            update_pixmap();
        }
        
        /* middle mouse button, or left-ctrl click (for 2 button mice) */
        if ( m_enter_piano_roll && (a_e->button == 2   ||
                            (a_e->button == 1 && (a_e->state & GDK_CONTROL_MASK))))
        {
            convert_y( y,&note );
            m_seq->set_solo_note( note );
            update_pixmap();
        }
    }
    return true;
}

bool
seqkeys::on_button_release_event(GdkEventButton* a_e)
{
    if ( a_e->type == GDK_BUTTON_RELEASE )
    {
        if ( a_e->button == 1 && m_keying )
        {
            m_keying = false;
            m_seq->play_note_off( m_keying_note );
        }
    }
    return true;
}

bool
seqkeys::on_motion_notify_event(GdkEventMotion* a_p0)
{
    int y, note;

    y = (int) a_p0->y + m_scroll_offset_y;
    convert_y( y,&note );

    set_hint_key( note );

    if ( m_keying )
    {
        if ( note != m_keying_note )
        {
            m_seq->play_note_off( m_keying_note );
            m_seq->play_note_on(  note );
            m_keying_note = note;
        }
    }

    return false;
}

bool
seqkeys::on_enter_notify_event(GdkEventCrossing* a_p0)
{
    m_enter_piano_roll = true;
    set_hint_state( true );
    return false;
}

bool
seqkeys::on_leave_notify_event(GdkEventCrossing* p0)
{
    m_enter_piano_roll = false;
    if ( m_keying )
    {
        m_keying = false;
        m_seq->play_note_off( m_keying_note );
    }
    set_hint_state( false );

    return true;
}

/* sets key to grey */
void
seqkeys::set_hint_key( int a_key )
{
    draw_key( m_hint_key, false );

    m_hint_key = a_key;

    if ( m_hint_state )
        draw_key( a_key, true );
}

/* true == on, false == off */
void
seqkeys::set_hint_state( bool a_state )
{
    m_hint_state = a_state;

    if ( !a_state )
        draw_key( m_hint_key, false );
}

/* a_state, false = normal, true = blue */
void
seqkeys::draw_key( int a_key, bool a_state )
{
    cairo_t *cr = gdk_cairo_create (m_window->gobj());
    cairo_set_line_width(cr, 1.0);

    int base_key = a_key;

    /* the the key in the octave */
    int key = a_key % 12;

    a_key = c_num_keys - a_key - 1;

    if ( key == 1 ||
            key == 3 ||
            key == 6 ||
            key == 8 ||
            key == 10 )
    {
        cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);            // Black  FIXME
    }
    else
    {
        cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);            // White  FIXME
    }

   
    /* Mute or solo keys */
    if(m_seq->is_note_mute(base_key))
    {
        cairo_set_source_rgb(cr, 1.0, 0.27, 0.0);       // Red FIXME
    }
    else if(m_seq->is_note_solo(base_key))
    {
        cairo_set_source_rgb(cr, 0.0, 0.4, 0.0);        // Green FIXME
    }

    cairo_rectangle(cr, c_keyoffset_x + 1,
                             (c_key_y * a_key) + 2 -  m_scroll_offset_y,
                             c_key_x - 3,
                             c_key_y - 3 );
    cairo_stroke_preserve(cr);
    cairo_fill(cr);

    if ( a_state ) // piano hint key
    {
        cairo_set_source_rgb(cr, 0.0, 0.0, 0.6);    // Dark blue  FIXME
        cairo_rectangle(cr, c_keyoffset_x + 1,
                             (c_key_y * a_key) + 2 -  m_scroll_offset_y,
                             c_key_x - 3,
                             c_key_y - 3 );
        cairo_stroke_preserve(cr);
        cairo_fill(cr);
    }

    cairo_destroy(cr);
}

void
seqkeys::change_vert( )
{
    m_scroll_offset_key = (int) m_vadjust->get_value();
    m_scroll_offset_y = m_scroll_offset_key * c_key_y,

    force_draw();
}

void
seqkeys::on_size_allocate(Gtk::Allocation& a_r )
{
    Gtk::DrawingArea::on_size_allocate( a_r );

    m_window_x = a_r.get_width();
    m_window_y = a_r.get_height();

    queue_draw();
}

bool
seqkeys::on_scroll_event( GdkEventScroll* a_ev )
{
    double val = m_vadjust->get_value();

    if ( a_ev->direction == GDK_SCROLL_UP )
    {
        val -= m_vadjust->get_step_increment()/6;
    }
    else if (  a_ev->direction == GDK_SCROLL_DOWN )
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

void
seqkeys::set_listen_button_press(GdkEventButton* a_ev)
{
    on_button_press_event(a_ev);
}

void
seqkeys::set_listen_button_release(GdkEventButton* a_ev)
{
    on_button_release_event(a_ev);
}

void
seqkeys::set_listen_motion_notify(GdkEventMotion* a_p0)
{
    on_motion_notify_event(a_p0);
}

