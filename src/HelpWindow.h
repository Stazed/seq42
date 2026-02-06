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

#include <gtkmm/window.h>
#include <gtkmm/box.h>
#include <gtkmm/button.h>
#include <gtkmm/label.h>
#include <gtkmm/entry.h>

#include <string>

// Forward declarations to avoid pulling WebKit headers into users
struct _WebKitWebView;
struct _WebKitFindController;

using WebKitWebView        = struct _WebKitWebView;
using WebKitFindController = struct _WebKitFindController;

/*
 * HelpWindow: gtkmm-3 + WebKitGTK help viewer
 *
 * Features:
 *  - Back / Forward navigation
 *  - Location label (current HTML filename)
 *  - Ctrl+F find bar using WebKitFindController
 */
class HelpWindow : public Gtk::Window
{
public:
    HelpWindow();

    void load_page(const std::string& file);

protected:
    bool on_key_press_event(GdkEventKey* e) override;

private:
    // --- Find helpers ---
    void show_findbar();
    void hide_findbar();
    void find_next();
    void find_previous();
    bool on_find_entry_key_press(GdkEventKey* e);

    // --- UI updates ---
    void update_nav_buttons();
    void update_location();

private:
    // Layout
    Gtk::Box    m_root;
    Gtk::Box    m_topbar;
    Gtk::Button m_btn_back;
    Gtk::Button m_btn_fwd;
    Gtk::Label  m_location;

    Gtk::Box    m_findbar;
    Gtk::Label  m_find_label;
    Gtk::Entry  m_find_entry;
    Gtk::Button m_find_prev;
    Gtk::Button m_find_next;
    Gtk::Button m_find_close;

    // WebKit (C API)
    WebKitWebView*        m_view;
    WebKitFindController* m_find_controller;

    std::string m_help_root;
};
