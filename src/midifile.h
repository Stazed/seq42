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

#include "perform.h"
#include <fstream>
#include <string>
#include <list>
#include <vector>

class midifile
{

private:

    int m_pos;
    const std::string m_name;

    /* holds our data */
    std::vector<unsigned char> m_d;

    list<unsigned char> m_l;

    unsigned long read_long();
    unsigned short read_short();
    unsigned char read_byte();
    unsigned long read_var();

    void write_long( unsigned long );
    void write_mid( unsigned long );
    void write_short( unsigned short );
    void write_byte( unsigned char );

    void write_header( int numtracks);
    int pow2 (int logbase2);
    string UlongToStringHex( unsigned long Number );

public:

    midifile(const Glib::ustring&);

    ~midifile();

    bool parse( perform *a_perf );
    bool write_sequences( perform *a_perf );
    bool write_song( perform *a_perf );

};
