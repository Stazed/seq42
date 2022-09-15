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


seqkeys::seqkeys(sequence *a_seq, Glib::RefPtr<Adjustment> a_vadjust ):
    m_seq(a_seq),
    m_vadjust(a_vadjust),
    m_scroll_offset_key(0),
    m_scroll_offset_y(0),
    m_window_x(),
    m_window_y(),
    m_hint_state(false),
    m_hint_key(0),
    m_enter_piano_roll(false),
    m_keying(false),
    m_keying_note(),
    m_scale(0),
    m_key(0),
    m_key_y(c_key_y),
    m_keyarea_y(c_keyarea_y),
    m_rollarea_y(c_rollarea_y),
    m_vertical_zoom(c_default_vertical_zoom)
{
    m_surface = Cairo::ImageSurface::create(
        Cairo::Format::FORMAT_ARGB32,
        c_keyarea_x,
        m_keyarea_y
    );

    add_events( Gdk::BUTTON_PRESS_MASK |
                Gdk::BUTTON_RELEASE_MASK |
                Gdk::ENTER_NOTIFY_MASK |
                Gdk::LEAVE_NOTIFY_MASK |
                Gdk::POINTER_MOTION_MASK |
                Gdk::SCROLL_MASK);

    /* set default size */
    set_size_request( c_keyarea_x +1, 10 );

    set_double_buffered( false );
}

void
seqkeys::on_realize()
{
    // we need to do the default realize
    Gtk::DrawingArea::on_realize();

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
seqkeys::set_vertical_zoom(int key_y, int keyarea_y, int rollarea_y, float vertical_zoom)
{
    if ( m_key_y != key_y)
    {
        m_key_y = key_y;
        m_keyarea_y = keyarea_y;
        m_rollarea_y = rollarea_y;
        m_vertical_zoom = vertical_zoom;
        reset();
    }
}

void
seqkeys::reset()
{
    change_vert();
    update_surface();
    queue_draw();
}

void
seqkeys::update_surface()
{
    Cairo::RefPtr<Cairo::Context> cr = Cairo::Context::create(m_surface);

    cr->set_operator(Cairo::OPERATOR_CLEAR);
    cr->rectangle(0.0, 0.0, c_keyarea_x, m_keyarea_y);
    cr->paint_with_alpha(1.0);
    cr->set_operator(Cairo::OPERATOR_OVER);

    /* the outline around the whole piano area */
    cr->set_source_rgb(c_back_black.r, c_back_black.g, c_back_black.b);
    cr->set_line_width(1.0);
    cr->rectangle(0.0, 0.0, c_keyarea_x, m_keyarea_y);
    cr->stroke_preserve();
    cr->fill();

    /* The area behind the keyboard */
    cr->set_source_rgb(c_back_light_grey.r, c_back_light_grey.g, c_back_light_grey.b);
    cr->rectangle(1.0, 1.0, c_keyoffset_x - 1, m_keyarea_y - 2);
    cr->stroke_preserve();
    cr->fill();

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

        cr->set_source_rgb(c_fore_white.r, c_fore_white.g, c_fore_white.b);
        cr->rectangle(c_keyoffset_x + 1,
                                 (m_key_y * i) + 2,
                                 c_key_x - 3,
                                 m_key_y - 3 );
        cr->stroke_preserve();
        cr->fill();

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
                cr->set_source_rgb(c_back_black.r, c_back_black.g, c_back_black.b);
            }
            else if (note_color == Green_Note)
            {
                cr->set_source_rgb(c_solo_green.r, c_solo_green.g, c_solo_green.b);
            }
            else
            {
                cr->set_source_rgb(c_mute_red.r, c_mute_red.g, c_mute_red.b);
            }
            
            cr->rectangle(c_keyoffset_x + 1,
                                     (m_key_y * i) + 2,
                                     c_key_x - 3,
                                     m_key_y - 3 );
            cr->stroke_preserve();
            cr->fill();
        }
        else if( note_color  != White_Note) // We have a mute or solo so use the appropriate color
        {
            if( note_color == Green_Note)
            {
                cr->set_source_rgb(c_solo_green.r, c_solo_green.g, c_solo_green.b);
            }
            else
            {
                cr->set_source_rgb(c_mute_red.r, c_mute_red.g, c_mute_red.b);
            }

            cr->rectangle(c_keyoffset_x + 1,
                         (m_key_y * i) + 2,
                         c_key_x - 3,
                         m_key_y - 3 );
            cr->stroke_preserve();
            cr->fill();
        }

        if ( key == m_key  )
        {
            /* The key letter to the left of the piano roll */
            char notes[20];

            /* notes */
            int octave = ((c_num_keys - i - 1) / 12) - 1;
            if ( octave < 0 )
                octave *= -1;

            snprintf(notes, sizeof(notes), "%2s%1d", c_key_text[key], octave);

            p_font_renderer->render_string_on_drawable(2,
                                                       m_key_y * i,
                                                       cr, notes, font::BLACK,
                                                       c_default_horizontal_zoom,
                                                       m_vertical_zoom);
        }
    }
}

bool
seqkeys::on_draw(const Cairo::RefPtr<Cairo::Context>& cr)
{
    cr->set_source(m_surface, 0.0, -m_scroll_offset_y);
    cr->paint();

    return true;
}

/* takes screen coordinates, give us notes and ticks */
void
seqkeys::convert_y( int a_y, int *a_note)
{
    *a_note = (m_rollarea_y - a_y - 2) / m_key_y;
}

bool
seqkeys::on_button_press_event(GdkEventButton *a_e)
{
    int note;

    if ( a_e->type == GDK_BUTTON_PRESS )
    {
        int y = (int) a_e->y + m_scroll_offset_y;

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
            update_surface();
            queue_draw();
        }

        /* middle mouse button, or left-ctrl click (for 2 button mice) */
        if ( m_enter_piano_roll && (a_e->button == 2   ||
                            (a_e->button == 1 && (a_e->state & GDK_CONTROL_MASK))))
        {
            convert_y( y,&note );
            m_seq->set_solo_note( note );
            update_surface();
            queue_draw();
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
seqkeys::on_enter_notify_event(GdkEventCrossing* /* a_p0 */)
{
    m_enter_piano_roll = true;
    set_hint_state( true );
    return false;
}

bool
seqkeys::on_leave_notify_event(GdkEventCrossing* /* p0 */)
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
    Cairo::RefPtr<Cairo::Context> cr = Cairo::Context::create(m_surface);
    cr->set_operator(Cairo::OPERATOR_DEST);
    cr->set_operator(Cairo::OPERATOR_OVER);

    cr->set_line_width(1.0);

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
        cr->set_source_rgb(c_back_black.r, c_back_black.g, c_back_black.b);
    }
    else
    {
        cr->set_source_rgb(c_fore_white.r, c_fore_white.g, c_fore_white.b);
    }

    /* Mute or solo keys */
    if(m_seq->is_note_mute(base_key))
    {
        cr->set_source_rgb(c_mute_red.r, c_mute_red.g, c_mute_red.b);
    }
    else if(m_seq->is_note_solo(base_key))
    {
        cr->set_source_rgb(c_solo_green.r, c_solo_green.g, c_solo_green.b);
    }

    cr->rectangle(c_keyoffset_x + 1,
                             (m_key_y * a_key) + 2 /* - m_scroll_offset_y*/,
                             c_key_x - 3,
                             m_key_y - 3 );
    cr->stroke_preserve();
    cr->fill();

    if ( a_state ) // piano hint key
    {
        cr->set_source_rgb(c_note_highlight.r, c_note_highlight.g, c_note_highlight.b);          // Blue 
        cr->rectangle(c_keyoffset_x + 1,
                             (m_key_y * a_key) + 2 /* - m_scroll_offset_y*/,
                             c_key_x - 3,
                             m_key_y - 3 );
        cr->stroke_preserve();
        cr->fill();
    }

    queue_draw();
}

void
seqkeys::change_vert( )
{
    if ( m_surface->get_width() != c_keyarea_x || m_surface->get_height() != m_keyarea_y )
    {
        m_surface = Cairo::ImageSurface::create(
        Cairo::Format::FORMAT_ARGB32,
        c_keyarea_x,
        m_keyarea_y);
    }

    m_scroll_offset_key = (int) m_vadjust->get_value();
    m_scroll_offset_y = m_scroll_offset_key * m_key_y,

    update_surface();
    queue_draw();
}

void
seqkeys::on_size_allocate(Gtk::Allocation& a_r )
{
    Gtk::DrawingArea::on_size_allocate( a_r );

    m_window_x = a_r.get_width();
    m_window_y = a_r.get_height();

    update_surface();
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

