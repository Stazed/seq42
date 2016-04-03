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

#include "midifile.h"
#include "seqedit.h"
#include <iostream>
#include <math.h>


midifile::midifile(const Glib::ustring& a_name) :
    m_pos(0),
    m_name(a_name)
{
}

midifile::~midifile ()
{
}


unsigned long
midifile::read_long ()
{
    unsigned long ret = 0;

    ret += (read_byte () << 24);
    ret += (read_byte () << 16);
    ret += (read_byte () << 8);
    ret += (read_byte ());

    return ret;
}

unsigned short
midifile::read_short ()
{
    unsigned short ret = 0;

    ret += (read_byte () << 8);
    ret += (read_byte ());

    //printf( "read_short 0x%4x\n", ret );
    return ret;
}

unsigned char
midifile::read_byte ()
{
    return m_d[m_pos++];
}

unsigned long
midifile::read_var ()
{
    unsigned long ret = 0;
    unsigned char c;

    /* while bit #7 is set */
    while (((c = read_byte ()) & 0x80) != 0x00)
    {
        /* shift ret 7 bits */
        ret <<= 7;
        /* add bits 0-6 */
        ret += (c & 0x7F);
    }

    /* bit was clear */
    ret <<= 7;
    ret += (c & 0x7F);

    return ret;
}


bool midifile::parse (perform * a_perf)
{
    a_perf->push_perf_undo();

    /* open binary file */
    ifstream file(m_name.c_str(), ios::in | ios::binary | ios::ate);

    if (!file.is_open ()) {
        fprintf(stderr, "Error opening MIDI file\n");
        return false;
    }

    int file_size = file.tellg ();

    /* run to start */
    file.seekg (0, ios::beg);

    /* alloc data */
    try
	{
	    m_d.resize(file_size);
	}
	catch(std::bad_alloc& ex)
	{
        fprintf(stderr, "Memory allocation failed\n");
        return false;
    }
    file.read ((char *) &m_d[0], file_size);
    file.close ();

    /* set position to 0 */
    m_pos = 0;

    /* chunk info */
    unsigned long ID;
    unsigned long TrackLength;

    /* time */
    unsigned long Delta;
    unsigned long RunningTime;
    unsigned long CurrentTime;

    unsigned short Format;			/* 0,1,2 */
    unsigned short NumTracks;
    unsigned short ppqn;

    /* track name from file */
    char TrackName[256];

    /* used in small loops */
    int i;

    /* track pointer */
    track * a_track;
    event e;

    /* read in header */
    ID = read_long ();
    TrackLength = read_long ();
    Format = read_short ();
    NumTracks = read_short ();
    ppqn = read_short ();

    //printf( "[%8lX] len[%ld] fmt[%d] num[%d] ppqn[%d]\n",
    //      ID, TrackLength, Format, NumTracks, ppqn );

    /* magic number 'MThd' */
    if (ID != 0x4D546864) {
        fprintf(stderr, "Invalid MIDI header detected: %8lX\n", ID);
        return false;
    }

    /* we are only supporting format 1 for now */
    if (Format != 1) {
        fprintf(stderr, "Unsupported MIDI format detected: %d\n", Format);
        return false;
    }

    /* We should be good to load now   */
    /* for each Track in the midi file */
    for (int curTrack = 0; curTrack < NumTracks; curTrack++)
    {
        /* done for each track */
        bool done = false;

        /* events */
        unsigned char status = 0, type, data[2], laststatus;
        long len;
        unsigned long proprietary = 0;

        /* Get ID + Length */
        ID = read_long ();
        TrackLength = read_long ();
        //printf( "[%8lX] len[%8lX]\n", ID,  TrackLength );

        /* magic number 'MTrk' */
        if (ID == 0x4D54726B)
        {
            /* we know we have a good track, so we can create
               a new seq42 track to dump it */

            a_track = new track();

            if (a_track == NULL) {
                fprintf(stderr, "Memory allocation failed\n");
                return false;
            }
            a_perf->add_track(a_track,curTrack);
            a_track->set_name((char*)"Midi Import");
            a_track->set_master_midi_bus (&a_perf->m_master_bus);

            int seq_idx = a_track->new_sequence();
            sequence *seq = a_track->get_sequence(seq_idx);

            /* reset time */
            RunningTime = 0;

            /* this gets each event in the Trk */
            while (!done)
            {
                /* get time delta */
                Delta = read_var ();

                /* get status */
                laststatus = status;
                status = m_d[m_pos];

                /* is it a status bit ? */
                if ((status & 0x80) == 0x00)
                {
                    /* no, its a running status */
                    status = laststatus;
                }
                else
                {
                    /* its a status, increment */
                    m_pos++;
                }

                /* set the members in event */
                e.set_status (status);

                RunningTime += Delta;
                /* current time is ppqn according to the file,
                   we have to adjust it to our own ppqn.
                   PPQN / ppqn gives us the ratio */
                CurrentTime = (RunningTime * c_ppqn) / ppqn;

                //printf( "D[%6ld] [%6ld] %02X\n", Delta, CurrentTime, status);
                e.set_timestamp (CurrentTime);

                /* switch on the channelless status */
                switch (status & 0xF0)
                {
                    /* case for those with 2 data bytes */
                    case EVENT_NOTE_OFF:
                    case EVENT_NOTE_ON:
                    case EVENT_AFTERTOUCH:
                    case EVENT_CONTROL_CHANGE:
                    case EVENT_PITCH_WHEEL:

                        data[0] = read_byte ();
                        data[1] = read_byte ();

                        // some files have vel=0 as note off
                        if ((status & 0xF0) == EVENT_NOTE_ON && data[1] == 0)
                        {
                            e.set_status (EVENT_NOTE_OFF);
                        }

                        //printf( "%02X %02X\n", data[0], data[1] );

                        /* set data and add */
                        e.set_data (data[0], data[1]);
                        seq->add_event (&e);

                        /* set midi channel */
                        a_track->set_midi_channel (status & 0x0F);
                        break;

                        /* one data item */
                    case EVENT_PROGRAM_CHANGE:
                    case EVENT_CHANNEL_PRESSURE:

                        data[0] = read_byte ();
                        //printf( "%02X\n", data[0] );

                        /* set data and add */
                        e.set_data (data[0]);
                        seq->add_event (&e);

                        /* set midi channel */
                        a_track->set_midi_channel (status & 0x0F);
                        break;

                        /* meta midi events ---  this should be FF !!!!!  */

                    case 0xF0:

                        if (status == 0xFF)
                        {
                            // get meta type
                            type = read_byte ();
                            len = read_var ();

                            //printf( "%02X %08X ", type, (int) len );

                            switch (type)
                            {
                                // proprietary
                                case 0x7f:

                                    // FF 7F len data
                                    if (len > 4)
                                    {
                                        proprietary = read_long ();
                                        len -= 4;
                                    }

                                    if (proprietary == c_midibus)
                                    {
                                        a_track->set_midi_bus (read_byte ());
                                        len--;
                                    }

                                    else if (proprietary == c_midich)
                                    {
                                        a_track->set_midi_channel (read_byte ());
                                        len--;
                                    }

                                    else if (proprietary == c_timesig)
                                    {
                                        seq->set_bp_measure (read_byte ());
                                        seq->set_bw (read_byte ());
                                        len -= 2;
                                    }

                                    else if (proprietary == c_triggers)
                                    {
                                        int num_triggers = len / 4;

                                        for (int i = 0; i < num_triggers; i += 2)
                                        {
                                            unsigned long on = read_long ();
                                            unsigned long length = (read_long () - on);
                                            len -= 8;
                                            a_track->add_trigger(on, length, 0, false);
                                        }
                                    }

                                    else if (proprietary == c_triggers_new)
                                    {
                                        int num_triggers = len / 12;

                                        //printf( "num_triggers[%d]\n", num_triggers );
                                        for (int i = 0; i < num_triggers; i++)
                                        {
                                            unsigned long on = read_long ();
                                            unsigned long off = read_long ();
                                            unsigned long length = off - on + 1;
                                            unsigned long offset = read_long ();

                                            //printf( "< start[%d] end[%d] offset[%d]\n",
                                            //        on, off, offset );

                                            len -= 12;
                                            a_track->add_trigger (on, length, offset, false);
                                        }
                                    }
                                    /* eat the rest */
                                    m_pos += len;
                                    break;

                                    /* Trk Done */
                                case 0x2f:

                                    // If delta is 0, then another event happened at the same time
                                    // as the track end.  the sequence class will discard the last
                                    // note.  This is a fix for that.   Native Seq42 file will always
                                    // have a Delta >= 1
                                    if ( Delta == 0 ){
                                        CurrentTime += 1;
                                    }

                                    seq->set_length (CurrentTime, false);
                                    seq->zero_markers ();
                                    done = true;
                                    break;

                                    /* Track name */
                                case 0x03:
                                    for (i = 0; i < len; i++)
                                    {
                                        TrackName[i] = read_byte ();
                                    }

                                    TrackName[i] = '\0';

                                    //printf("Import track: [%s]\n", TrackName );
                                    seq->set_name (TrackName);
                                    break;

                                    /* sequence number */
                                case 0x00:
                                    if(len != 0x00)
                                        read_short();
                                    break;

                                default:
                                    for (i = 0; i < len; i++)
                                    {
                                        read_byte();
                                    }
                                    //printf("\n");
                                    break;
                            }
                        }
                        else if(status == 0xF0)
                        {
                            /* sysex */
                            len = read_var ();

                            /* skip it */
                            m_pos += len;

                            fprintf(stderr, "Warning, no support for SYSEX messages, discarding.\n");
                        }
                        else
                        {
                            fprintf(stderr, "Unexpected system event : 0x%.2X", status);
                            return false;
                        }

                        break;

                    default:
                        fprintf(stderr, "Unsupported MIDI event: %hhu\n", status);
                        return false;
                        break;
                }

            }			/* while ( !done loading Trk chunk */

            /* the sequence has been filled, add it  */
            seq->set_track(a_track);
        }

        /* dont know what kind of chunk */
        else
        {
            /* its not a MTrk, we dont know how to deal with it,
               so we just eat it */
            fprintf(stderr, "Unsupported MIDI header detected: %8lX\n", ID);
            m_pos += TrackLength;
            done = true;
        }

    }				/* for(eachtrack) */

    //printf ( "file_size[%d] m_pos[%d]\n", file_size, m_pos );

    if ((file_size - m_pos) > (int) sizeof (unsigned long))
    {
        ID = read_long ();
        if (ID == c_midictrl) // Not used: SEQ24 stuff -  change m_pos to correct position for c_bpmtag
        {
            unsigned long len = read_long ();
            m_pos += len;
        }
        /* Get ID + Length */
        ID = read_long ();
        if (ID == c_midiclocks)
        {
            TrackLength = read_long ();
            /* TrackLength is nyumber of buses */
            for (unsigned int x = 0; x < TrackLength; x++)
            {
                int bus_on = read_byte ();
                a_perf->get_master_midi_bus ()->set_clock (x, (clock_e) bus_on);
            }
        }
    }

    if ((file_size - m_pos) > (int) sizeof (unsigned long)) // SEQ24 stuff for matching
    {
        /* Get ID + Length */
        ID = read_long ();
        if (ID == c_notes)  // Not used: except to change m_pos to correct position for c_bpmtag
        {
            unsigned int screen_sets = read_short ();

            for (unsigned int x = 0; x < screen_sets; x++)
            {
                /* get the length of the string */
                unsigned int len = read_short ();
                m_pos += len;
            }
        }
    }


    if ((file_size - m_pos) > (int) sizeof (unsigned int))
    {
        /* Get ID + Length */
        ID = read_long ();
        if (ID == c_bpmtag)
        {
            long bpm = read_long ();
            a_perf->set_bpm (bpm);
        }
    }

    // read in the mute group info -- SEQ24 stuff
    if ((file_size - m_pos) > (int) sizeof (unsigned long))
    {
        ID = read_long ();
        if (ID == c_mutegroups)
        {
            long length = read_long ();

            if(length != 0) // means came from SEQ24
            {
                if (1024 != length) // c_gmute_tracks = 32 * 32 = 1024 (from seq24)
                {
                    printf( "corrupt data in mutegroup section\n" );
                }

                for (int i = 0; i < 32; i++) // c_seqs_in_set == 4 * 8 = 32 (from seq24)
                {
                    read_long (); // a_perf->select_group_mute(read_long ());
                    for (int k = 0; k < 32; ++k)
                    {
                        read_long ();// a_perf->set_group_mute_state(k, read_long ());
                    }
                }
            }
        }
    }

    if ((file_size - m_pos) > (int) sizeof (unsigned int))
    {
        /* Get ID + Length */
        ID = read_long ();
        if (ID == c_bp_measure)
        {
            long bp_mes = read_long ();
            a_perf->set_bp_measure(bp_mes);
        }
    }

    if ((file_size - m_pos) > (int) sizeof (unsigned int))
    {
        /* Get ID + Length */
        ID = read_long ();
        if (ID == c_beat_width)
        {
            long bw = read_long ();
            a_perf->set_bw(bw);
        }
    }

    // *** ADD NEW TAGS AT END **************/
    return true;
}


void
midifile::write_long (unsigned long a_x)
{
    write_byte ((a_x & 0xFF000000) >> 24);
    write_byte ((a_x & 0x00FF0000) >> 16);
    write_byte ((a_x & 0x0000FF00) >> 8);
    write_byte ((a_x & 0x000000FF));
}

void
midifile::write_mid (unsigned long a_x)
{
    write_byte ((a_x & 0xFF0000) >> 16);
    write_byte ((a_x & 0x00FF00) >> 8);
    write_byte ((a_x & 0x0000FF));
}

void
midifile::write_short (unsigned short a_x)
{
    write_byte ((a_x & 0xFF00) >> 8);
    write_byte ((a_x & 0x00FF));
}


void
midifile::write_byte (unsigned char a_x)
{
    m_l.push_back (a_x);
}

bool midifile::write_sequences (perform * a_perf)
{
    int numtracks = 0;

    /* get number of track sequences */
    for (int i = 0; i < c_max_track; i++)
    {
        if (a_perf->is_active_track(i) && !a_perf->get_track(i)->get_song_mute())
        {
            numtracks += a_perf->get_track(i)->get_number_of_sequences();
        }
    }

    //printf ("numtracks[%d]\n", numtracks );

    /* write header */
    /* 'MThd' and length of 6 */
    write_long (0x4D546864);
    write_long (0x00000006);

    /* format 1, number of tracks, ppqn */
    write_short (0x0001);
    write_short (numtracks);
    write_short (c_ppqn);

    /* We should be good to load now   */
    /* for each Track Sequence in the midi file */

    numtracks = 0; // reset for seq->fill_list position

    for (int curTrack = 0; curTrack < c_max_track; curTrack++)
    {
        //printf ("track[%d]\n", curTrack );
        if (a_perf->is_active_track(curTrack) && !a_perf->get_track(curTrack)->get_song_mute())
        {
            unsigned int num_seq = a_perf->get_track(curTrack)->get_number_of_sequences();

            /* sequence pointer */
            for (unsigned int a_seq = 0; a_seq < num_seq; a_seq++ )
            {
                sequence * seq = a_perf->get_track(curTrack)->get_sequence(a_seq);

                //printf ("seq[%d]\n", a_seq );

                list<char> l;
                seq->fill_list (&l, numtracks);

                /* magic number 'MTrk' */
                write_long (0x4D54726B);
                write_long (l.size ());

                //printf("MTrk len[%d]\n", l.size());

                while (l.size () > 0)
                {
                    write_byte (l.back ());
                    l.pop_back ();
                }

                numtracks++;
            }
        }
    }


    /* midi control */
    write_long (c_midictrl);
    write_long (0);

    /* bus mute/unmute data */
    write_long (c_midiclocks);
    write_long (0);

    /* notepad data */
    write_long (c_notes);
    write_short (0);

    /* bpm */
    write_long (c_bpmtag);
    write_long (a_perf->get_bpm ());

    /* write out the mute groups */
    write_long(c_mutegroups);
    write_long(0);

    /* write out the beats per measure */
    write_long(c_bp_measure);
    write_long(a_perf->get_bp_measure());

    /* write out the beat width */
    write_long(c_beat_width);
    write_long(a_perf->get_bw());

    /* open binary file */
    ofstream file (m_name.c_str (), ios::out | ios::binary | ios::trunc);

    if (!file.is_open ())
        return false;

    /* enable bufferization */
    char file_buffer[1024];
    file.rdbuf()->pubsetbuf(file_buffer, sizeof file_buffer);

    for (list < unsigned char >::iterator i = m_l.begin ();
            i != m_l.end (); i++)
    {
      char c = *i;
      file.write(&c, 1);
    }

    m_l.clear ();

    return true;

}


bool midifile::write_song (perform * a_perf)
{
    int numtracks = 0;

    /* get number of tracks  */
    for (int i = 0; i < c_max_track; i++)
    {
        if (a_perf->is_active_track(i) && a_perf->get_track(i)->get_track_trigger_count() > 0 &&
            !a_perf->get_track(i)->get_song_mute()) // don't count tracks with NO triggers or muted
        {
            if(a_perf->get_track(i)->get_number_of_sequences() > 0) // don't count tracks with NO sequences(even if they have a trigger)
                numtracks ++;
        }
    }

    numtracks ++; // + 1 for signature/tempo track

    //printf ("numtracks[%d]\n", numtracks );

    /* write header */
    /* 'MThd' and length of 6 */
    write_long (0x4D546864);
    write_long (0x00000006);

    /* format 1, number of tracks, ppqn */
    write_short (0x0001);
    write_short (numtracks);
    write_short (c_ppqn);

    //First, the track chunk for the time signature/tempo track.
    /* magic number 'MTrk' */
    write_long (0x4D54726B);
    write_long (0x00000013); // s/b [00 00 00 13] = chunk length 19 bytes

    // 00 FF 58 04 04 02 18 08      time signature - 4/4
    // 00 FF 51 03 07 A1 20         tempo - last three bytes are the bpm - 140 here in hex
    // 00 FF 2F 00                  end of track

    /* time signature */
    write_byte(0x00); // delta time
    write_short(0xFF58);
    write_byte(0x04); // length of remaining bytes
    write_byte(a_perf->get_bp_measure());           // nn
    write_byte(log(a_perf->get_bw())/log(2.0));     // dd
    write_short(0x1808);            // FIXME ???

    /* Tempo */
    write_byte(0x00); // delta time
    write_short(0xFF51);
    write_byte(0x03); // length of bytes - must be 3
    write_mid(60000000/a_perf->get_bpm());

    /* track end */
    write_long(0x00FF2F00);

    numtracks = 1; // 0 is signature/tempo track

    /* We should be good to load now   */
    /* for each Track Sequence in the midi file */

    for (int curTrack = 0; curTrack < c_max_track; curTrack++)
    {
        //printf ("track[%d]\n", curTrack );
        if (a_perf->is_active_track(curTrack) && !a_perf->get_track(curTrack)->get_song_mute())
        {
            if(a_perf->get_track(curTrack)->get_number_of_sequences() < 1) // skip tracks with NO sequences
                continue;

            /* all track triggers */
            track *a_track = a_perf->get_track(curTrack);
            list < trigger > seq_list_trigger;
            trigger *a_trig;

            std::vector<trigger> trig_vect;
            a_track->get_trak_triggers(trig_vect); // all triggers for the track

            int vect_size = trig_vect.size();
            if(vect_size < 1)
                continue; // skip tracks with no triggers

            list<char> l;
            sequence * seq = NULL;

            /*  sequence name - this will assume the first sequence used is the name for the whole track */
            for (int i = 0; i < vect_size; i++)
            {
                a_trig = &trig_vect[i]; // get the trigger
                seq = a_perf->get_track(curTrack)->get_sequence(a_trig->m_sequence); // get trigger sequence

                if(seq == NULL)
                    continue;

                seq->song_fill_list_track_name(&l,numtracks);
                break;
            }

            if(seq == NULL) // this is the case of a track has only empty triggers(but has sequences!)
            {
                seq = a_perf->get_track(curTrack)->get_sequence(0); // so just use the first one
                seq->song_fill_list_track_name(&l,numtracks);
            }

            // now for each trigger get sequence and add events to list char below - fill_list one by one in order,
            // essentially creating a single long sequence.
            // then set a single trigger for the big sequence - start at zero, end at last trigger end.

            long total_seq_length = 0;
            long prev_timestamp = 0;

            for (int i = 0; i < vect_size; i++)
            {
                a_trig = &trig_vect[i]; // get the trigger
                sequence * seq = a_perf->get_track(curTrack)->get_sequence(a_trig->m_sequence); // get trigger sequence

                if(seq == NULL) // skip empty triggers
                    continue;

                prev_timestamp = seq->song_fill_list_seq_event(&l,a_trig,prev_timestamp); // put events on list
            }

            total_seq_length = trig_vect[vect_size-1].m_tick_end;

            //printf("total_seq_length = [%ld]\n",total_seq_length);
            seq->song_fill_list_seq_trigger(&l,a_trig,total_seq_length,prev_timestamp); // the big sequence trigger

            /* magic number 'MTrk' */
            write_long (0x4D54726B);
            write_long (l.size ());

            //printf("MTrk len[%d]\n", l.size());

            while (l.size () > 0)
            {
                write_byte (l.back ());
                l.pop_back ();
            }
            numtracks++;
        }
    }


    /* open binary file */
    ofstream file (m_name.c_str (), ios::out | ios::binary | ios::trunc);

    if (!file.is_open ())
        return false;

    /* enable bufferization */
    char file_buffer[1024];
    file.rdbuf()->pubsetbuf(file_buffer, sizeof file_buffer);

    for (list < unsigned char >::iterator i = m_l.begin ();
            i != m_l.end (); i++)
    {
      char c = *i;
      file.write(&c, 1);
    }

    m_l.clear ();

    return true;
}

