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

#include "HelpWindow.h"

#include <gtkmm.h>
#include <webkit2/webkit2.h>
#include <glib.h>
#include <cstring>

/*
 * HelpWindow: gtkmm-3 + WebKitGTK help viewer
 *  - Back/Forward
 *  - Location label (basename of current URI)
 *  - Ctrl+F find bar using WebKitFindController
 */
HelpWindow::HelpWindow()
:
    m_root(Gtk::ORIENTATION_VERTICAL),
    m_topbar(Gtk::ORIENTATION_HORIZONTAL),
    m_findbar(Gtk::ORIENTATION_HORIZONTAL),
    m_view(nullptr),
    m_find_controller(nullptr)
{
    set_title("Seq42 Help");
    set_default_size(1080, 740);

    m_help_root = std::string(SEQ42_HELPDIR) + "/html";

    // ----- Top bar (Back / Forward / Location) -----
    m_topbar.set_spacing(6);
    m_topbar.set_margin_start(6);
    m_topbar.set_margin_end(6);
    m_topbar.set_margin_top(6);
    m_topbar.set_margin_bottom(6);

    m_btn_back.set_label("◀");
    m_btn_fwd.set_label("▶");
    m_btn_back.set_sensitive(false);
    m_btn_fwd.set_sensitive(false);

    m_btn_back.signal_clicked().connect([this]{
        if (webkit_web_view_can_go_back(m_view))
            webkit_web_view_go_back(m_view);
    });
    m_btn_fwd.signal_clicked().connect([this]{
        if (webkit_web_view_can_go_forward(m_view))
            webkit_web_view_go_forward(m_view);
    });

    m_location.set_text("help.html");
    m_location.set_xalign(0.0f);
    m_location.set_ellipsize(Pango::ELLIPSIZE_MIDDLE);

    m_topbar.pack_start(m_btn_back, Gtk::PACK_SHRINK);
    m_topbar.pack_start(m_btn_fwd, Gtk::PACK_SHRINK);
    m_topbar.pack_start(m_location, Gtk::PACK_EXPAND_WIDGET);

    // ----- Find bar (Ctrl+F) -----
    m_findbar.set_spacing(6);
    m_findbar.set_margin_start(6);
    m_findbar.set_margin_end(6);
    m_findbar.set_margin_bottom(6);

    m_find_label.set_text("Find:");
    m_find_entry.set_hexpand(true);

    m_find_prev.set_label("Prev");
    m_find_next.set_label("Next");
    m_find_close.set_label("✕");

    m_find_prev.signal_clicked().connect([this]{ find_previous(); });
    m_find_next.signal_clicked().connect([this]{ find_next(); });
    m_find_close.signal_clicked().connect([this]{ hide_findbar(); });

    // Enter = next, Shift+Enter = previous
    m_find_entry.signal_key_press_event().connect(
        sigc::mem_fun(*this, &HelpWindow::on_find_entry_key_press), false);

    m_findbar.pack_start(m_find_label, Gtk::PACK_SHRINK);
    m_findbar.pack_start(m_find_entry, Gtk::PACK_EXPAND_WIDGET);
    m_findbar.pack_start(m_find_prev, Gtk::PACK_SHRINK);
    m_findbar.pack_start(m_find_next, Gtk::PACK_SHRINK);
    m_findbar.pack_start(m_find_close, Gtk::PACK_SHRINK);

    m_findbar.set_visible(false);

    // ----- WebKit view (C widget wrapped into gtkmm) -----
    m_view = WEBKIT_WEB_VIEW(webkit_web_view_new());

    // “Doc viewer” profile
    WebKitSettings* s = webkit_web_view_get_settings(m_view);
    webkit_settings_set_enable_javascript(s, FALSE);

    m_find_controller = webkit_web_view_get_find_controller(m_view);

    g_signal_connect(
        m_view, "decide-policy",
        G_CALLBACK(+[](WebKitWebView*,
                       WebKitPolicyDecision* decision,
                       WebKitPolicyDecisionType type,
                       gpointer user_data) -> gboolean
        {
            if (type != WEBKIT_POLICY_DECISION_TYPE_NAVIGATION_ACTION)
                return FALSE;

            auto* nav = WEBKIT_NAVIGATION_POLICY_DECISION(decision);
            WebKitNavigationAction* action =
                webkit_navigation_policy_decision_get_navigation_action(nav);

            WebKitURIRequest* req =
                webkit_navigation_action_get_request(action);

            const char* uri = webkit_uri_request_get_uri(req);

            if (!uri) return FALSE;

            auto* self = static_cast<HelpWindow*>(user_data);

            // ---- Internal file:// navigation ----
            if (g_str_has_prefix(uri, "file://"))
            {
                gchar* path = g_filename_from_uri(uri, nullptr, nullptr);
                if (path)
                {
                    const bool inside_help =
                        g_str_has_prefix(path, self->m_help_root.c_str());
                    g_free(path);

                    if (!inside_help)
                    {
                        webkit_policy_decision_ignore(decision);
                        return TRUE;
                    }
                }
                return FALSE; // allow
            }

            // ---- External URI: open via XDG ----
            gtk_show_uri_on_window(
                GTK_WINDOW(self->gobj()),
                uri,
                GDK_CURRENT_TIME,
                nullptr
            );

            webkit_policy_decision_ignore(decision);
            return TRUE;
        }),
        this);

    // Update UI state when URI / back-forward capability changes
    g_signal_connect(
        m_view, "notify::uri",
        G_CALLBACK(+[](GObject*, GParamSpec*, gpointer user_data){
            static_cast<HelpWindow*>(user_data)->update_location();
        }),
        this);

    g_signal_connect(
        m_view, "notify::can-go-back",
        G_CALLBACK(+[](GObject*, GParamSpec*, gpointer user_data){
            static_cast<HelpWindow*>(user_data)->update_nav_buttons();
        }),
        this);

    g_signal_connect(
        m_view, "notify::can-go-forward",
        G_CALLBACK(+[](GObject*, GParamSpec*, gpointer user_data){
            static_cast<HelpWindow*>(user_data)->update_nav_buttons();
        }),
        this);

    auto* wrapped_view = Gtk::manage(Glib::wrap(GTK_WIDGET(m_view)));

    // ----- Layout -----
    m_root.pack_start(m_topbar, Gtk::PACK_SHRINK);
    m_root.pack_start(*wrapped_view, Gtk::PACK_EXPAND_WIDGET);
    m_root.pack_start(m_findbar, Gtk::PACK_SHRINK);

    add(m_root);
    m_root.show_all();

    // Start hidden
    m_findbar.hide();

    // Load initial page
    load_page("help.html");

    // Keyboard shortcuts (Ctrl+F, Esc)
    add_events(Gdk::KEY_PRESS_MASK);
}

void HelpWindow::load_page(const std::string& file)
{
    const std::string full = m_help_root + "/" + file;
    char* uri = g_filename_to_uri(full.c_str(), nullptr, nullptr);
    if (uri)
    {
        webkit_web_view_load_uri(m_view, uri);
        g_free(uri);
    }
}

bool HelpWindow::on_key_press_event(GdkEventKey* e)
{
    // Ctrl+F => toggle/show find bar
    if ((e->state & GDK_CONTROL_MASK) &&
        (e->keyval == GDK_KEY_f || e->keyval == GDK_KEY_F))
    {
        show_findbar();
        return true;
    }

    // Esc => close find bar (and stop highlighting)
    if (e->keyval == GDK_KEY_Escape && m_findbar.get_visible())
    {
        hide_findbar();
        return true;
    }

    return Gtk::Window::on_key_press_event(e);
}

// --- Find helpers ---
void HelpWindow::show_findbar()
{
    if (!m_findbar.get_visible())
        m_findbar.show();

    m_find_entry.grab_focus();
    m_find_entry.select_region(0, -1);
}

void HelpWindow::hide_findbar()
{
    if (m_find_controller)
        webkit_find_controller_search_finish(m_find_controller);

    // return focus to web content
    m_findbar.hide();
    gtk_widget_grab_focus(GTK_WIDGET(m_view));
}

void HelpWindow::find_next()
{
    const auto text = m_find_entry.get_text();
    if (text.empty() || !m_find_controller) return;

    // Start a new search each time (works fine for simple doc viewer usage)
    webkit_find_controller_search(
        m_find_controller,
        text.c_str(),
        WEBKIT_FIND_OPTIONS_CASE_INSENSITIVE,
        G_MAXUINT);

    // Advance to next match
    webkit_find_controller_search_next(m_find_controller);
}

void HelpWindow::find_previous()
{
    const auto text = m_find_entry.get_text();
    if (text.empty() || !m_find_controller) return;

    webkit_find_controller_search(
        m_find_controller,
        text.c_str(),
        WEBKIT_FIND_OPTIONS_CASE_INSENSITIVE,
        G_MAXUINT);

    webkit_find_controller_search_previous(m_find_controller);
}

bool HelpWindow::on_find_entry_key_press(GdkEventKey* e)
{
    if (e->keyval == GDK_KEY_Return || e->keyval == GDK_KEY_KP_Enter)
    {
        if (e->state & GDK_SHIFT_MASK) find_previous();
        else find_next();
        return true;
    }
    if (e->keyval == GDK_KEY_Escape)
    {
        hide_findbar();
        return true;
    }
    return false;
}

// --- UI updates ---
void HelpWindow::update_nav_buttons()
{
    m_btn_back.set_sensitive(webkit_web_view_can_go_back(m_view));
    m_btn_fwd.set_sensitive(webkit_web_view_can_go_forward(m_view));
}

void HelpWindow::update_location()
{
    const char* uri = webkit_web_view_get_uri(m_view);
    if (!uri)
    {
        m_location.set_text("");
        return;
    }

    // Show basename (playlist_mode.html) for file:// URIs
    if (g_str_has_prefix(uri, "file://"))
    {
        gchar* path = g_filename_from_uri(uri, nullptr, nullptr);
        if (path)
        {
            gchar* base = g_path_get_basename(path);
            m_location.set_text(base ? base : "");
            g_free(base);
            g_free(path);
        }
        else
        {
            m_location.set_text(uri);
        }
    }
    else
    {
        m_location.set_text(uri);
    }

    update_nav_buttons();
}
