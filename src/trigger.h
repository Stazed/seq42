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


#ifndef SEQ42_TRIGGER
#define SEQ42_TRIGGER

class trigger;

#include "sequence.h"

class trigger
{
public:

    long m_tick_start;
    long m_tick_end;
    bool m_selected;
    long m_offset;
    sequence *m_sequence;

    trigger (){
        m_tick_start = 0;
        m_tick_end = 0;
        m_offset = 0;
        m_selected = false;
        m_sequence = NULL;
    };

    bool operator< (trigger rhs){

        if (m_tick_start < rhs.m_tick_start)
            return true;

        return false;
    };
};

#endif
