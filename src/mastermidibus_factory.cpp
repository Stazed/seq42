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

#include "mastermidibus_factory.h"

#ifdef HAVE_LIBASOUND
#include "midibus_alsa.h"
#endif

#ifdef JACK_MIDI_SUPPORT
#include "midibus_jack.h"
#endif

std::unique_ptr<mastermidibus_iface>
create_mastermidibus(midi_backend which)
{
    switch (which)
    {
    case midi_backend::jack:
#ifdef JACK_MIDI_SUPPORT
        return std::make_unique<mastermidibus_jack>();
#else
        return nullptr;
#endif

    case midi_backend::alsa:
#ifdef HAVE_LIBASOUND
        return std::make_unique<mastermidibus_alsa>();
#else
        return nullptr;
#endif
    }
    return nullptr;
}
