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

#ifdef __WIN32__
#include "configwin32.h"
#else
#include "config.h"
#endif

#ifdef LASH_SUPPORT
#include "perform.h"
#include <lash/lash.h>

class mainwnd;

class lash
{
private:
    perform       *m_perform;
    mainwnd       *m_mainwnd;
    lash_client_t *m_client;

    bool process_events();
    void handle_event(lash_event_t* conf);
    void handle_config(lash_config_t* conf);

public:
    lash(int *argc, char ***argv);
    void set_mainwnd(mainwnd * a_main);
    void set_alsa_client_id(int id);
    void start(perform* perform);
};

/* global lash driver, defined in seq42.cpp */
extern lash *lash_driver;
#endif // LASH_SUPPORT