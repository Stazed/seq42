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

#include "configfile.h"
#include <iostream>

configfile::configfile(const Glib::ustring& a_name) :
    m_pos(0),
    m_name(a_name)
{
}

configfile::~configfile( )
{
}

void
configfile::next_data_line( ifstream *a_file)
{
    a_file->getline( m_line, sizeof(m_line) );
    while (( m_line[0] == '#' || m_line[0] == ' ' || m_line[0] == 0 ) &&
            !a_file->eof() )
    {
        a_file->getline( m_line, sizeof(m_line) );
    }
}

void
configfile::line_after( ifstream *a_file, string a_tag)
{
    /* run to start */
    a_file->clear();
    a_file->seekg( 0, ios::beg );

    a_file->getline( m_line, sizeof(m_line) );
    while ( strncmp( m_line, a_tag.c_str(), a_tag.length()) != 0  &&
            !a_file->eof() )
    {
        a_file->getline( m_line, sizeof(m_line) );
    }
    next_data_line( a_file );
}

