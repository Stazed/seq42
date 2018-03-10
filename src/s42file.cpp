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

#include <iostream>
#include "s42file.h"
#include "mainwnd.h"

s42file::s42file()
{
}

s42file::~s42file()
{
}

bool
s42file::save(const Glib::ustring& a_filename, perform *a_perf)
{
    m_mutex.lock();

    ofstream file(a_filename.c_str(), ios::out | ios::binary | ios::trunc);

    if (!file.is_open()) return false;

    /* file version 5 */
    file.write((const char *) &c_file_identification, sizeof (uint64_t)); // magic number file ID

    char p_version[global_VERSION_array_size];
    strncpy(p_version, VERSION, global_VERSION_array_size);
    file.write((const char *) p_version, sizeof (char)* global_VERSION_array_size);

    char time[global_time_array_size];
    std::string s_time = current_date_time();
    strncpy(time, s_time.c_str(), global_time_array_size);
    file.write(time, sizeof (char)* global_time_array_size);

    /* end file version 5 */

    file.write((const char *) &c_file_version, global_file_int_size);

    /* version 7 use tempo list */
    uint32_t list_size = a_perf->m_list_total_marker.size();
    file.write((const char *) &list_size, sizeof (list_size));
    list<tempo_mark>::iterator i;
    for (i = a_perf->m_list_total_marker.begin(); i != a_perf->m_list_total_marker.end(); i++)
    {
        file.write((const char *) &(*i).tick, sizeof ((*i).tick));
        file.write((const char *) &(*i).bpm, sizeof ((*i).bpm));
        file.write((const char *) &(*i).bw, sizeof ((*i).bw)); // not currently used - future use maybe
        file.write((const char *) &(*i).bp_measure, sizeof ((*i).bp_measure)); // ditto
        // we don't need to write the start frame since it will be recalculated when loaded.
    }

    int bp_measure = a_perf->get_bp_measure(); // version 4
    file.write((const char *) &bp_measure, global_file_int_size);

    int bw = a_perf->get_bw(); // version 4
    file.write((const char *) &bw, global_file_int_size);

    int swing_amount8 = a_perf->get_swing_amount8();
    file.write((const char *) &swing_amount8, global_file_int_size);
    int swing_amount16 = a_perf->get_swing_amount16();
    file.write((const char *) &swing_amount16, global_file_int_size);

    int active_tracks = 0;
    for (int i = 0; i < c_max_track; i++)
    {
        if (a_perf->is_active_track(i)) active_tracks++;
    }
    file.write((const char *) &active_tracks, global_file_int_size);

    for (int i = 0; i < c_max_track; i++)
    {
        if (a_perf->is_active_track(i))
        {
            int trk_idx = i; // file version 3
            file.write((const char *) &trk_idx, global_file_int_size);

            if (!a_perf->get_track(i)->save(&file))
            {
                return false;
            }
        }
    }

    file.close();

    m_mutex.unlock();
    return true;
}

bool
s42file::load(const Glib::ustring& a_filename, perform *a_perf, mainwnd *a_main)
{
    ifstream file(a_filename.c_str(), ios::in | ios::binary);

    if (!file.is_open()) return false;

    bool ret = true;

    int64_t file_id = 0;
    file.read((char *) &file_id, sizeof (int64_t));

    if (file_id != c_file_identification) // if it does not match, maybe its old file so reset to beginning
    {
        file.clear();
        file.seekg(0, ios::beg);
        /* since we are checking for the < version 5 files now, then reset int sizes to original */
        global_file_int_size = sizeof (int);
        global_file_long_int_size = sizeof (long);
    }
    else // we have version 5 or greater file
    {
        char program_version[global_VERSION_array_size + 1];
        file.read(program_version, sizeof (char)* global_VERSION_array_size);
        program_version[global_VERSION_array_size] = '\0';

        char time[global_time_array_size + 1];
        file.read(time, sizeof (char)* global_time_array_size);
        time[global_time_array_size] = '\0';

        printf("SEQ42 Release Version [%s]\n", program_version);
        printf("File Created [%s]\n", time);
    }

    int version;
    file.read((char *) &version, global_file_int_size);

    printf("File Version [%d]\n", version);

    if (version < 0 || version > c_file_version)
    {
        fprintf(stderr, "Invalid file version detected: %d\n", version);
        /*reset to defaults */
        global_file_int_size = sizeof (int32_t);
        global_file_long_int_size = sizeof (int32_t);
        file.close();
        return false;
    }

    if (version > 6) // version 7 uses tempo markers
    {
        uint32_t list_size;
        file.read((char *) &list_size, sizeof (list_size));

        tempo_mark marker;
        for (unsigned i = 0; i < list_size; ++i)
        {
            file.read((char *) &marker.tick, sizeof (marker.tick));
            file.read((char *) &marker.bpm, sizeof (marker.bpm));
            file.read((char *) &marker.bw, sizeof (marker.bw));
            file.read((char *) &marker.bp_measure, sizeof (marker.bp_measure));
            // we don't need start marker since set_tempo_load() will recalculate it

            a_perf->m_list_total_marker.push_back(marker);
        }

        /* load_tempo_list() will set tempo class markers, set perform midibus
        * and trigger mainwnd timeout to update the mainwnd bpm spinner */
        a_main->load_tempo_list();
    }
    else
    {
        if (version > 5)
        {
            double bpm; // file version 6 uses double
            file.read((char *) &bpm, sizeof (bpm));
            /* update_start_BPM() will set tempo markers, set perform midibus
             * and trigger mainwnd timeout to update the mainwnd bpm spinner */
            a_main->update_start_BPM(bpm);
        }
        else
        {
            int bpm; // prior to version 6 uses int
            file.read((char *) &bpm, global_file_int_size);
            /* update_start_BPM() will set tempo markers, set perform midibus
             * and trigger mainwnd timeout to update the mainwnd bpm spinner */
            a_main->update_start_BPM(bpm);
        }
    }

    int bp_measure = 4;
    if (version > 3)
    {
        file.read((char *) &bp_measure, global_file_int_size);
    }

    a_main->set_bp_measure(bp_measure);

    int bw = 4;
    if (version > 3)
    {
        file.read((char *) &bw, global_file_int_size);
    }

    a_main->set_bw(bw);

    int swing_amount8 = 0;
    if (version > 1)
    {
        file.read((char *) &swing_amount8, global_file_int_size);
    }

    a_main->set_swing_amount8(swing_amount8);
    
    int swing_amount16 = 0;
    if (version > 1)
    {
        file.read((char *) &swing_amount16, global_file_int_size);
    }

    a_main->set_swing_amount16(swing_amount16);

    int active_tracks;
    file.read((char *) &active_tracks, global_file_int_size);

    int trk_index = 0;
    for (int i = 0; i < active_tracks; i++)
    {
        trk_index = i;
        if (version > 2)
        {
            file.read((char *) &trk_index, global_file_int_size);
        }

        a_perf->new_track(trk_index);
        if (!a_perf->get_track(trk_index)->load(&file, version))
        {
            ret = false;
        }
    }

    file.close();

    /* reset to default if ID does not match since
    it would have been changed for prior < 5 version check */
    if (file_id != c_file_identification)
    {
        global_file_int_size = sizeof (int32_t);
        global_file_long_int_size = sizeof (int32_t);
    }

    return ret;
}

std::string
s42file::current_date_time()
{
    std::string mystring;
    std::string strTemp;

    time_t now = time(0);
    struct tm tstruct;
    char buf[80];
    tstruct = *localtime(&now);
    // Visit http://en.cppreference.com/w/cpp/chrono/c/strftime
    // for more information about date/time format
    strftime(buf, sizeof (buf), "%Y-%m-%d.%X", &tstruct);

    mystring = std::string(buf);

    // Format: = "2012-05-06.01:03:59 PM" of mystring Does NOT use military time
    return mystring;
}