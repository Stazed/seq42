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

#include "mutex.h"

namespace seq42
{
    
const pthread_mutex_t mutex::recmutex = PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP;
const pthread_cond_t condition_var::cond  = PTHREAD_COND_INITIALIZER;

mutex::mutex( )
{
    m_mutex_lock = recmutex;
}

void 
mutex::lock( )
{
    pthread_mutex_lock( &m_mutex_lock );
}

void 
mutex::unlock( )
{      
    pthread_mutex_unlock( &m_mutex_lock );
}

condition_var::condition_var( )
{
    m_cond = cond;
}

void
condition_var::signal( )
{
    pthread_cond_signal( &m_cond );
}

void
condition_var::wait( )
{
    pthread_cond_wait( &m_cond, &m_mutex_lock );
}

} // namespace seq42