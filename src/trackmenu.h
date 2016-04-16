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


#pragma once

#include "globals.h"
#include "perform.h"
#include "trackedit.h"

class trackmenu;

#include <gtkmm/button.h>
#include <gtkmm/window.h>
#include <gtkmm/accelgroup.h>
#include <gtkmm/box.h>
#include <gtkmm/main.h>
#include <gtkmm/menu.h>
#include <gtkmm/menubar.h>
#include <gtkmm/eventbox.h>
#include <gtkmm/window.h>
#include <gtkmm/table.h>
#include <gtkmm/drawingarea.h>
#include <gtkmm/widget.h>
#include <gtkmm/style.h>

using namespace Gtk;

class trackmenu : public virtual Glib::ObjectBase
{

private:

    Menu        *m_menu;
    perform     *m_mainperf;
    track       m_clipboard;
    bool m_something_to_paste;

    void on_realize();

    void trk_new();

    void trk_insert(int a_track_location);
    void trk_delete(int a_track_location);
    void pack_tracks();
    void trk_copy();
    void trk_cut();
    void trk_paste();
    void trk_merge_seq(track *a_track, sequence *a_seq );
    void trk_edit();
    void new_sequence();

    void trk_clear_perf();

    void set_bus_and_midi_channel( int a_bus, int a_ch );
    void mute_all_tracks();

    virtual void redraw( int a_track ) = 0;

protected:

    int m_current_trk;
    void popup_menu();

public:

    trackmenu( perform *a_p );
    virtual ~trackmenu( ) { };
};
