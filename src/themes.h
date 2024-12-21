//----------------------------------------------------------------------------
//
//  This file is part of seq42.
//
//  seq42 is free software; you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation; either version 2 of the License, or
//  (at your option) any later version.
//
//  seq32 is distributed in the hope that it will be useful,
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

/* Global colors */
struct color { double r; double g; double b; };

const Gdk::RGBA c_color_track_blue =  Gdk::RGBA("#8FF2F5");
const Gdk::RGBA c_color_data_red =  Gdk::RGBA("#F5544D");
const Gdk::RGBA c_color_note_blue =  Gdk::RGBA("#44DBFA");
const Gdk::RGBA c_color_solo_green = Gdk::RGBA("#7FFE00");
const Gdk::RGBA c_color_mute_red = Gdk::RGBA("#FF4500");
const Gdk::RGBA c_color_marker_green = Gdk::RGBA("#00FF00");

const color c_track_color = { c_color_track_blue.get_red(), c_color_track_blue.get_green(), c_color_track_blue.get_blue()};
const color c_note_color_selected = { c_color_data_red.get_red(), c_color_data_red.get_green(), c_color_data_red.get_blue()};
const color c_note_color = { c_color_note_blue.get_red(), c_color_note_blue.get_green(), c_color_note_blue.get_blue()};
const color c_note_highlight = { 0.0, 0.0, 1.0 };   // blue
const color c_solo_green = { c_color_solo_green.get_red(), c_color_solo_green.get_green(), c_color_solo_green.get_blue() };
const color c_mute_red = { c_color_mute_red.get_red(), c_color_mute_red.get_green(), c_color_mute_red.get_blue() };
const color c_fore_white = { 1.0, 1.0, 1.0 };
const color c_back_black = { 0.0, 0.0, 0.0 };
const color c_back_dark_grey = { 0.3, 0.3, 0.3 };
const color c_back_medium_grey = { 0.5, 0.5, 0.5 };
const color c_back_light_grey = { 0.6, 0.6, 0.6 };
const color c_fore_light_grey = { 0.8, 0.8, 0.8 };
const color c_background_keys = { 0.0, 0.6, 0.0 };  // dark green
const color c_progress_line = { 1.0, 0.0, 0.0 };    // red
const color c_marker_lines = { c_color_marker_green.get_red(), c_color_marker_green.get_green(), c_color_marker_green.get_blue() };
