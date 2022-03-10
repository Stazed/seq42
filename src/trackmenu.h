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

/* forward declarations */
class trackmenu;
class mainwnd;
class perfnames;


using namespace Gtk;

class trackmenu : public virtual Glib::ObjectBase
{

private:

    Menu        *m_menu;
    perform     *m_mainperf;
    mainwnd     *m_mainwnd;
    bool m_something_to_paste;

    std::vector<MenuItem> m_menu_items;

    void on_realize();

    void trk_new();
    void pack_tracks();
    void trk_copy();
    void trk_cut();
    void trk_paste();
    void trk_merge_seq(track *a_track, sequence *a_seq );
    void trk_edit();
    void new_sequence();

    void trk_clear_perf();
    void trk_export();

    void set_bus_and_midi_channel( int a_bus, int a_ch );
//    void mute_all_tracks();

    virtual void redraw( int a_track ) = 0;

protected:

    track       m_moving_track;
    track       m_clipboard;
    int         m_current_trk;
    int         m_old_track;
    
    void popup_menu();

public:

    trackmenu( perform *a_p, mainwnd *a_main );
    virtual ~trackmenu( ) { };

    void trk_insert(int a_track_location);
    void trk_delete(int a_track_location);
};
