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

#include "trackmenu.h"
#include "seqedit.h"
#include "mainwnd.h"


// Constructor

trackmenu::trackmenu( perform *a_p, mainwnd *a_main  ) :
    m_menu(NULL),
    m_mainperf(a_p),
    m_something_to_paste(false),
    m_mainwnd(a_main),
    m_current_trk(0),
    m_old_track(0)
{
    using namespace Menu_Helpers;

    // init the clipboard, so that we don't get a crash
    // on paste with no previous copy...
    m_clipboard.set_master_midi_bus( a_p->get_master_midi_bus() );
}

void
trackmenu::popup_menu()
{
    using namespace Menu_Helpers;

    if ( m_menu != NULL )
        delete m_menu;

    m_menu = manage( new Menu());

    m_menu_items.clear();
    m_menu_items.resize(16);

    if ( ! m_mainperf->is_active_track( m_current_trk ))
    {
        m_menu_items[0].set_label("_New");
        m_menu_items[0].set_use_underline(true);
        m_menu_items[0].signal_activate().connect(mem_fun(*this, &trackmenu::trk_new) );
        m_menu->append(m_menu_items[0]);

        bool can_we_delete = true;

        for(int i = 0; i < c_max_track; i++)
        {
            if( (m_mainperf->get_track(i) != NULL) &&
                    m_mainperf->is_track_in_edit(i) )
            {
                can_we_delete = false; // don't allow
            }
        }

        if(can_we_delete)
        {
            m_menu_items[1].set_label("_Insert row");
            m_menu_items[1].set_use_underline(true);
            m_menu_items[1].signal_activate().connect(sigc::bind(mem_fun(*this,&trackmenu::trk_insert), m_current_trk) );
            m_menu->append(m_menu_items[1]);

            m_menu_items[2].set_label("_Delete row");
            m_menu_items[2].set_use_underline(true);
            m_menu_items[2].signal_activate().connect(sigc::bind(mem_fun(*this,&trackmenu::trk_delete),m_current_trk) );
            m_menu->append(m_menu_items[2]);

            m_menu_items[3].set_label("_Pack tracks");
            m_menu_items[3].set_use_underline(true);
            m_menu_items[3].signal_activate().connect(mem_fun(*this, &trackmenu::pack_tracks) );
            m_menu->append(m_menu_items[3]);
        }
    }

    if ( m_mainperf->is_active_track( m_current_trk ))
    {
        m_menu_items[4].set_label("_Edit");
        m_menu_items[4].set_use_underline(true);
        m_menu_items[4].signal_activate().connect(mem_fun(*this,&trackmenu::trk_edit) );
        m_menu->append(m_menu_items[4]);

        m_menu_items[5].set_label("_Copy");
        m_menu_items[5].set_use_underline(true);
        m_menu_items[5].signal_activate().connect(mem_fun(*this,&trackmenu::trk_copy) );
        m_menu->append(m_menu_items[5]);

        if(! m_mainperf->is_track_in_edit( m_current_trk ))
        {
            m_menu_items[6].set_label("Cut");
            m_menu_items[6].signal_activate().connect(mem_fun(*this,&trackmenu::trk_cut) );
            m_menu->append(m_menu_items[6]);
        }

        if( m_mainperf->get_track( m_current_trk)->get_track_trigger_count() > 0 )
        {
            m_menu_items[7].set_label("Clear triggers");
            m_menu_items[7].signal_activate().connect(mem_fun(*this,&trackmenu::trk_clear_perf) );
            m_menu->append(m_menu_items[7]);

            m_menu_items[8].set_label("Export track");
            m_menu_items[8].signal_activate().connect(mem_fun(*this,&trackmenu::trk_export) );
            m_menu->append(m_menu_items[8]);
        }

        MenuItem * menu_separator1 = new SeparatorMenuItem();
        m_menu->append(*menu_separator1);

        if( m_mainperf->get_track( m_current_trk ) != NULL)
        {
            bool can_we_insert_delete = true;

            for(int i = 0; i < c_max_track; i++)
            {
                if( (m_mainperf->get_track(i) != NULL) &&
                        m_mainperf->is_track_in_edit(i) )
                {
                    can_we_insert_delete = false; // don't allow
                }
            }

            if(can_we_insert_delete)
            {
                m_menu_items[9].set_label("_Insert row");
                m_menu_items[9].set_use_underline(true);
                m_menu_items[9].signal_activate().connect(sigc::bind(mem_fun(*this,&trackmenu::trk_insert), m_current_trk) );
                m_menu->append(m_menu_items[9]);

                m_menu_items[10].set_label("_Delete row");
                m_menu_items[10].set_use_underline(true);
                m_menu_items[10].signal_activate().connect(sigc::bind(mem_fun(*this,&trackmenu::trk_delete), m_current_trk) );
                m_menu->append(m_menu_items[10]);

                m_menu_items[11].set_label("_Pack tracks");
                m_menu_items[11].set_use_underline(true);
                m_menu_items[11].signal_activate().connect(mem_fun(*this, &trackmenu::pack_tracks) );
                m_menu->append(m_menu_items[11]);
            }
        }

        MenuItem * menu_separator2 = new SeparatorMenuItem();
        m_menu->append(*menu_separator2);

        Menu *merge_seq_menu = NULL;
        char name[40];
        for ( int t=0; t<c_max_track; ++t )
        {
            if (! m_mainperf->is_active_track( t ) || t == m_current_trk)
            {
                continue;// don't list empty tracks or the current track
            }

            track *some_track = m_mainperf->get_track(t);

            Menu *menu_t = NULL;
            bool inserted = false;
            for (unsigned s=0; s< some_track->get_number_of_sequences(); s++ )
            {
                if ( !inserted )
                {
                    if(merge_seq_menu == NULL)
                    {
                        merge_seq_menu = manage( new Menu());
                    }

                    inserted = true;
                    snprintf(name, sizeof(name), "[%d] %s", t+1, some_track->get_name());
                    menu_t = manage( new Menu());

                    MenuItem * menu_item = new MenuItem(name);
                    menu_item->set_submenu(*menu_t);
                    merge_seq_menu->append(*menu_item);
                }

                sequence *a_seq = some_track->get_sequence( s );
                snprintf(name, sizeof(name),"[%u] %s", s+1, a_seq->get_name());

                MenuItem * menu_item = new MenuItem(name);
                menu_item->signal_activate().connect(sigc::bind(mem_fun(*this,&trackmenu::trk_merge_seq),some_track, a_seq) );
                menu_t->append(*menu_item);
            }
        }

        m_menu_items[12].set_label("_New sequence");
        m_menu_items[12].set_use_underline(true);
        m_menu_items[12].signal_activate().connect(mem_fun(*this, &trackmenu::new_sequence) );
        m_menu->append(m_menu_items[12]);

        if(merge_seq_menu != NULL)
        {
            m_menu_items[13].set_label("Merge sequence");
            m_menu_items[13].set_submenu(*merge_seq_menu);
            m_menu->append(m_menu_items[13]);
        }

    }
    else
    {
        if(m_something_to_paste)
        {
            m_menu_items[14].set_label("_Paste");
            m_menu_items[14].set_use_underline(true);
            m_menu_items[14].signal_activate().connect(mem_fun(*this, &trackmenu::trk_paste) );
            m_menu->append(m_menu_items[14]);
        }
    }

    if ( m_mainperf->is_active_track( m_current_trk ))
    {
        MenuItem * menu_separator3 = new SeparatorMenuItem();
        m_menu->append(*menu_separator3);

        Menu *menu_buses = manage( new Menu() );
        m_menu_items[15].set_label("Midi Bus");
        m_menu_items[15].set_submenu(*menu_buses);
        m_menu->append(m_menu_items[15]);

        /* midi buses */
        mastermidibus *masterbus = m_mainperf->get_master_midi_bus();
        for ( int i = 0; i < masterbus->get_num_out_buses(); i++ )
        {
            Menu *menu_channels = manage( new Menu() );

            MenuItem * menu_item = new MenuItem(masterbus->get_midi_out_bus_name(i));
            menu_item->set_submenu(*menu_channels);
            menu_buses->append(*menu_item);

            char b[4];

            /* midi channel menu */
            for( int j=0; j<16; j++ )
            {
                snprintf(b, sizeof(b), "%d", j + 1);
                std::string name = string(b);
                int instrument = global_user_midi_bus_definitions[i].instrument[j];

                if ( instrument >= 0 && instrument < c_maxBuses )
                {
                    name = name + (string(" (") +
                                   global_user_instrument_definitions[instrument].instrument +
                                   string(")") );
                }

                MenuItem * menu_item_name = new MenuItem(name);
                menu_item_name->signal_activate().connect(sigc::bind(mem_fun(*this,&trackmenu::set_bus_and_midi_channel), i, j));
                menu_channels->append(*menu_item_name);
            }
        }
    }

    m_menu->show_all();
    m_menu->popup_at_pointer(NULL);
}

void
trackmenu::set_bus_and_midi_channel( int a_bus, int a_ch )
{
    if ( m_mainperf->is_active_track( m_current_trk ))
    {
        m_mainperf->get_track( m_current_trk )->set_midi_bus( a_bus );
        m_mainperf->get_track( m_current_trk )->set_midi_channel( a_ch );
        m_mainperf->get_track( m_current_trk )->set_dirty();
        global_is_modified = true;
    }
}

// Makes a New track
void
trackmenu::trk_new()
{
    if ( ! m_mainperf->is_active_track( m_current_trk ))
    {
        m_mainperf->push_track_undo(m_current_trk);
        m_mainperf->new_track( m_current_trk );
        m_mainperf->get_track( m_current_trk )->set_dirty();
        trk_edit();
    }
}

// insert row at location
void
trackmenu::trk_insert(int a_track_location)
{
    /* Don't allow insert if any track is being edited */
    for (int i = 0; i < c_max_track; ++i)
    {
        if( (m_mainperf->get_track(i) != NULL) &&
                        m_mainperf->is_track_in_edit(i) )
        {
            track_is_being_edited();
            return;
        }
    }
    
    m_mainperf->push_perf_undo();
    // first copy all tracks to m_insert_clipboard[];
    for(int i = 0; i < c_max_track; i++)
    {
        if ( m_mainperf->is_active_track(i) == true )
        {
            m_mainperf->m_tracks_clipboard[i] = *m_mainperf->get_track(i);
            m_mainperf->m_tracks_clipboard[i].unselect_triggers();
        }
        else
        {
            m_mainperf->m_tracks_clipboard[i].m_is_NULL = true;
        }
    }

    m_mainperf->delete_track(a_track_location); // the new insert blank track

    for(int i = a_track_location + 1; i < c_max_track; i++)//now delete and replace the rest offset
    {
        if ( m_mainperf->is_active_track(i) == true )
        {
            m_mainperf->delete_track(i);
            if(!m_mainperf->m_tracks_clipboard[i-1].m_is_NULL)
            {
                m_mainperf->new_track(i);
                *m_mainperf->get_track(i) = m_mainperf->m_tracks_clipboard[i-1];
                m_mainperf->get_track(i)->set_dirty();
            }
        }
        else
        {
            if(!m_mainperf->m_tracks_clipboard[i-1].m_is_NULL)
            {
                m_mainperf->new_track(i);
                *m_mainperf->get_track(i) = m_mainperf->m_tracks_clipboard[i-1];
                m_mainperf->get_track(i)->set_dirty();
            }
        }
    }   // the last track will get lost if > c_max_track ????!!!!!
}

// delete row at location
void
trackmenu::trk_delete(int a_track_location)
{
    /* Don't allow delete if any track is being edited */
    for (int i = 0; i < c_max_track; ++i)
    {
        if( (m_mainperf->get_track(i) != NULL) &&
                        m_mainperf->is_track_in_edit(i) )
        {
            track_is_being_edited();
            return;
        }
    }
    
    m_mainperf->push_perf_undo();
    // first copy all tracks to m_insert_clipboard[];
    for(int i = 0; i < c_max_track; i++)
    {
        if ( m_mainperf->is_active_track(i) == true )
        {
            m_mainperf->m_tracks_clipboard[i] = *m_mainperf->get_track(i);
            m_mainperf->m_tracks_clipboard[i].unselect_triggers();
        }
        else
        {
            m_mainperf->m_tracks_clipboard[i].m_is_NULL = true;
        }
    }

    for(int i = a_track_location; i < c_max_track - 1; i++)//now delete and replace the rest offset
    {
        if ( m_mainperf->is_active_track(i) == true )
        {
            m_mainperf->delete_track(i);
            if(!m_mainperf->m_tracks_clipboard[i+1].m_is_NULL)
            {
                m_mainperf->new_track(i);
                *m_mainperf->get_track(i) = m_mainperf->m_tracks_clipboard[i+1];
                m_mainperf->get_track(i)->set_dirty();
            }
        }
        else
        {
            if(!m_mainperf->m_tracks_clipboard[i+1].m_is_NULL)
            {
                m_mainperf->new_track(i);
                *m_mainperf->get_track(i) = m_mainperf->m_tracks_clipboard[i+1];
                m_mainperf->get_track(i)->set_dirty();
            }
        }
    }
}

void
trackmenu::track_is_being_edited()
{
    Glib::ustring query_str = "You cannot move/swap/delete/insert tracks if being edited!\n"
            "Please close all editing windows.";
    
    Gtk::MessageDialog dialog( query_str, false,
                              Gtk::MESSAGE_INFO,
                              Gtk::BUTTONS_OK, true);

    dialog.run();
}

void
trackmenu::pack_tracks()
{
    m_mainperf->push_perf_undo();

    // first copy all active tracks to m_insert_clipboard[];
    int active_tracks = 0;
    for(int i = 0; i < c_max_track; i++)
    {
        if ( m_mainperf->is_active_track(i) == true )
        {
            m_mainperf->m_tracks_clipboard[active_tracks] = *m_mainperf->get_track(i);
            m_mainperf->m_tracks_clipboard[active_tracks].unselect_triggers();
            active_tracks++;
        }
    }

    for(int i = active_tracks; i < c_max_track; i++) // NULL the remainder
    {
        m_mainperf->m_tracks_clipboard[i].m_is_NULL = true;
    }

    for(int i = 0; i < c_max_track; i++)//now delete and replace in order
    {
        if ( m_mainperf->is_active_track(i) == true )
        {
            m_mainperf->delete_track(i);
            if(!m_mainperf->m_tracks_clipboard[i].m_is_NULL)
            {
                m_mainperf->new_track(i);
                *m_mainperf->get_track(i) = m_mainperf->m_tracks_clipboard[i];
                m_mainperf->get_track(i)->set_dirty();
            }
        }
        else
        {
            if(!m_mainperf->m_tracks_clipboard[i].m_is_NULL)
            {
                m_mainperf->new_track(i);
                *m_mainperf->get_track(i) = m_mainperf->m_tracks_clipboard[i];
                m_mainperf->get_track(i)->set_dirty();
            }
        }
    }
}

// Copies selected to clipboard track */
void
trackmenu::trk_copy()
{
    if ( m_mainperf->is_active_track( m_current_trk ))
    {
        m_clipboard = *(m_mainperf->get_track( m_current_trk ));
        m_clipboard.unselect_triggers();
        m_something_to_paste = true;
    }
}

// Deletes and Copies to Clipboard */
void
trackmenu::trk_cut()
{
    if ( m_mainperf->is_active_track( m_current_trk ) &&
            !m_mainperf->is_track_in_edit( m_current_trk ) )
    {
        m_clipboard = *(m_mainperf->get_track( m_current_trk ));
        m_clipboard.unselect_triggers();
        m_something_to_paste = true;
        m_mainperf->push_track_undo(m_current_trk);
        m_mainperf->delete_track( m_current_trk );
        global_seqlist_need_update = true;
        redraw( m_current_trk );
    }
}

// Puts clipboard into location
void
trackmenu::trk_paste()
{
    if ( m_something_to_paste && ! m_mainperf->is_active_track( m_current_trk ))
    {
        m_mainperf->push_track_undo(m_current_trk);
        m_mainperf->new_track( m_current_trk  );
        *(m_mainperf->get_track( m_current_trk )) = m_clipboard;
        m_mainperf->get_track( m_current_trk )->set_song_mute(false); // since this is copied for undo/redo and file load - reset here
        m_mainperf->get_track( m_current_trk )->set_dirty();
    }
}

void    // merge selected sequence into current track with triggers
trackmenu::trk_merge_seq(track * a_track, sequence *a_seq )
{
    if ( m_mainperf->is_active_track( m_current_trk ))
    {
        m_mainperf->push_track_undo(m_current_trk);

        std::vector<trigger> trig_vect;
        a_track->get_trak_triggers(trig_vect); // all triggers for the track
        //printf("trig_vect.size()[%d]\n",trig_vect.size());

        trigger *a_trig = NULL;

        int seq_idx = m_mainperf->get_track( m_current_trk )->new_sequence();
        sequence *seq = m_mainperf->get_track( m_current_trk )->get_sequence(seq_idx);
        *seq = *a_seq;
        seq->set_track(m_mainperf->get_track( m_current_trk ));

        for(unsigned ii = 0; ii < trig_vect.size(); ii++)
        {
            a_trig = &trig_vect[ii];

            if(a_trig->m_sequence == a_track->get_sequence_index(a_seq))
            {
                m_mainperf->get_track( m_current_trk )->add_trigger(a_trig->m_tick_start,
                        a_trig->m_tick_end - a_trig->m_tick_start,a_trig->m_offset,
                        seq_idx);

                //printf( "tick_start[%ld]: tick_end[%ld]: offset[%ld]\n", a_trig->m_tick_start,
                //                a_trig->m_tick_end,a_trig->m_offset );
            }
            a_trig = NULL;
        }
        m_mainperf->get_track( m_current_trk )->set_dirty();
    }
}

void
trackmenu::trk_edit()
{
    if ( m_mainperf->is_active_track( m_current_trk ))
    {
        track *a_track = m_mainperf->get_track( m_current_trk );

        if(a_track->get_editing())
        {
            a_track->set_raise(true);
        }
        else
        {
            trackedit * a_trackedit = new trackedit(a_track, m_mainwnd);
#ifdef NSM_SUPPORT
            m_mainwnd->set_window_pointer(a_trackedit);
#endif
        }
    }
}

void
trackmenu::new_sequence()
{
    m_mainperf->push_track_undo(m_current_trk);
    track *a_track = m_mainperf->get_track( m_current_trk );
    int seq_idx = a_track->new_sequence();
    sequence *a_sequence = a_track->get_sequence(seq_idx);
    seqedit * a_seq_edit =  new seqedit( a_sequence, m_mainperf, m_mainwnd );
#ifdef NSM_SUPPORT
    m_mainwnd->set_window_pointer(a_seq_edit);
#endif
}

void
trackmenu::trk_clear_perf()
{
    if ( m_mainperf->is_active_track( m_current_trk ))
    {
        m_mainperf->push_trigger_undo(m_current_trk);

        m_mainperf->clear_track_triggers( m_current_trk  );
        m_mainperf->get_track( m_current_trk )->set_dirty();
    }
}

void
trackmenu::trk_export()
{
    if ( m_mainperf->is_active_track( m_current_trk ))
    {
        m_mainwnd->export_track_midi( m_current_trk);
    }
}
