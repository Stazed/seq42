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
#include <iostream>
#include <gtkmm.h>
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

bool midifile::parse (perform * a_perf, int screen_set)
{
    /* open binary file */
    ifstream file(m_name.c_str(), ios::in | ios::binary | ios::ate);

    if (!file.is_open ())
    {
        error_message_gtk("Error opening MIDI file");
        return false;
    }

    unsigned int file_size = file.tellg ();

    if(file_size < sizeof(unsigned long))
    {
        Glib::ustring message = "Error - Invalid file size: ";
        message += NumberToString(file_size);
        error_message_gtk(message);
        file.close ();
        return false;
    }

    /* run to start */
    file.seekg (0, ios::beg);

    /* alloc data */
    try
    {
        m_d.resize(file_size);
    }
    catch(std::bad_alloc& ex)
    {
        error_message_gtk("Memory allocation failed");
        return false;
    }
    file.read ((char *) &m_d[0], file_size);
    file.close ();
    
    /* for import tempo, time signature verify change */
    long bp_measure = a_perf->get_bp_measure();
    long bw = a_perf->get_bw();
    double bpm = a_perf->get_bpm();

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
    if (ID != 0x4D546864)
    {
        Glib::ustring message = "Invalid MIDI header detected: ";
        message += Ulong_To_String_Hex(ID);
        error_message_gtk(message);
        return false;
    }

    /* we are only supporting format 1 for now */
    if (Format != 1)
    {
        Glib::ustring message = "Unsupported MIDI format detected: ";
        message += NumberToString(Format);
        error_message_gtk(message);
        return false;
    }

    /* We should be good to load now   */
    a_perf->push_perf_undo();

    /* seq24 screen set import */
    int screen_set_start = 0;
    unsigned short Track_End = NumTracks;
    if(screen_set >= 0)
    {
        screen_set_start = screen_set * SEQ24_SCREEN_SET_SIZE;

        if((screen_set_start + SEQ24_SCREEN_SET_SIZE) < Track_End)
            Track_End = screen_set_start + SEQ24_SCREEN_SET_SIZE;
    }

    /* for each Track in the midi file */
    int track_count = 0; // necessary for screen set offset

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

        /* Seq24 import using screen set */
        if(track_count >= c_max_track || curTrack >= Track_End ||
           (screen_set >= 0 && track_count >= (SEQ24_SCREEN_SET_SIZE - 1)))
        {
            m_pos += TrackLength; // eat the unused tracks
            continue;
        }

        /* magic number 'MTrk' */
        if (ID == 0x4D54726B)
        {
            /* we know we have a good track, so we can create
               a new seq42 track to dump it */

            a_track = new track();

            if (a_track == NULL)
            {
                error_message_gtk("Memory allocation failed");
                return false;
            }

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
                            else if (proprietary == c_transpose)
                            {
                                a_track->set_transposable (read_byte ());
                                len--;
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

                        case 0x58:    /* Time Signature  bp_measure / bw */
                            /*
                                If the midi file contains both proprietary (c_timesig)
                                and Midi type 0x58 then it came from seq42 or seq32.
                                In this case the Midi type is parsed first (because it is listed first)
                                then it gets overwritten by the proprietary, above.
                            */
                            if (len == 4)
                            {
                                long import_bp_measure = long(read_byte());     // nn
                                int logbase2 = int(read_byte());                // dd

                                read_byte();                                    // cc eat it
                                read_byte();                                    // bb eat it

                                long import_bw = long(pow2(logbase2));          // convert dd to bw

                                if(import_bp_measure == 0 || import_bw == 0)    // spec assumes 4 x 4 as we do
                                    break;

                                if(curTrack == 0)                               // set for main perform if first track
                                {
                                    bp_measure = import_bp_measure;             // these will be checked for user approval if different
                                    bw = import_bw;                             // from current project amounts along with bpm
                                }

                                seq->set_bp_measure(import_bp_measure);         // sets the sequence always
                                seq->set_bw(import_bw);

                                /*printf
                                (
                                   "Time Signature set to %d/%d\n",
                                    int(seq->get_bp_measure()),
                                    int(seq->get_bw())
                                );*/
                            }
                            else
                                m_pos += len;           /* eat it           */
                            break;

                        case 0x51:                      /* Set Tempo  = bpm      */
                            if (len == 3)
                            {
                                unsigned tempo = unsigned(read_byte());
                                tempo = (tempo * 256) + unsigned(read_byte());
                                tempo = (tempo * 256) + unsigned(read_byte());
                                //printf("tempo %d\n",tempo);
                                if(tempo == 0 || curTrack != 0)          /* Midi spec & seq42 assumes 120 bpm if tempo == 0 */
                                    break;                               /* If not first track don't use because we
                                                                            don't support tempo change */

                                bpm = (double) 60000000.0 / tempo;              // this will be checked for user approval with time signature
                                //printf("BPM (SMF) set to %f\n", bpm);
                            }
                            else
                                m_pos += len;           /* eat it           */
                            break;

                        /* Trk Done */
                        case 0x2f:

                            // If delta is 0, then another event happened at the same time
                            // as the track end.  the sequence class will discard the last
                            // note.  This is a fix for that.   Native Seq42 file will always
                            // have a Delta >= 1
                            if ( Delta == 0 )
                            {
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
                            if (len == 0x00)
                                track_count = 0;
                            else
                            {
                                int seq_number = read_short();
                                //printf ( "seq_number[%d]\n",  seq_number );
                                if(screen_set >= 0)
                                {
                                    seq_number -= (screen_set * 32);
                                }
                                track_count = seq_number;
                            }

                            //printf ( "track_count [%d]\n", track_count );

                            break;

                        default:
                            for (i = 0; i < len; i++)
                            {
                                read_byte();
                            }
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
                        //fprintf(stderr, "Unexpected system event : 0x%.2X", status);
                        Glib::ustring message = "Unexpected system event : ";
                        message += Ulong_To_String_Hex((unsigned long)status);
                        error_message_gtk(message);
                        return false;
                    }

                    break;

                default:
                    //fprintf(stderr, "Unsupported MIDI event: %hhu\n", status);
                    Glib::ustring message = "Unsupported MIDI event:  ";
                    message += Ulong_To_String_Hex((unsigned long)status);
                    error_message_gtk(message);
                    return false;
                    break;
                }
            }			/* while ( !done loading Trk chunk */

            /* the track has been filled - add it */
            if(track_count < c_max_track  &&
               (screen_set < 0 || (screen_set >= 0 && (track_count < SEQ24_SCREEN_SET_SIZE) && (track_count >= 0))))
            {
                a_perf->add_track(a_track,track_count);
            }
            else // for seq24/32 screen set import we can't tell the screen set until we load the track
            {
                delete a_track; // Not in the correct screen set or > c_max_track
            }
        }

        /* don't know what kind of chunk */
        else
        {
            /* its not a MTrk, we don't know how to deal with it,
               so we just eat it */
            fprintf(stderr, "Unsupported MIDI header detected: %8lX\n", ID);
            m_pos += TrackLength;
            done = true;
        }
    }				/* for(each track) */

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
            /* TrackLength is number of buses */
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
            bpm = (double) read_long ();
            if(bpm > (c_bpm_scale_factor - 1.0))
                bpm /= c_bpm_scale_factor;
            //printf("bpm long %f\n",bpm);
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
        if (ID == c_perf_bp_mes)
        {
            bp_measure = read_long ();
        }
    }

    if ((file_size - m_pos) > (int) sizeof (unsigned int))
    {
        /* Get ID + Length */
        ID = read_long ();
        if (ID == c_perf_bw)
        {
            bw = read_long ();
        }
    }

    bool is_changed = false;
    
    if(a_perf->get_bpm() != bpm || a_perf->get_bp_measure() != bp_measure || a_perf->get_bw() != bw)
        is_changed = true;
    
    if(is_changed)
    {
        if(verify_change_tempo_timesig(bpm, bp_measure, bw))
        {
            a_perf->set_bpm(bpm);
            a_perf->set_bp_measure(bp_measure);
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

void
midifile::write_header( int numtracks)
{
    /* 'MThd' and length of 6 */
    write_long (0x4D546864);
    write_long (0x00000006);

    /* format 1, number of tracks, ppqn */
    write_short (0x0001);
    write_short (numtracks);
    write_short (c_ppqn);
}

void
midifile::write_tempo(perform * a_perf)
{
    write_byte(0x00); // delta time
    write_short(0xFF51);
    write_byte(0x03); // length of bytes - must be 3
    write_mid(60000000/a_perf->get_bpm());
}

void
midifile::write_time_sig(perform * a_perf)
{
    write_byte(0x00); // delta time
    write_short(0xFF58);
    write_byte(0x04); // length of remaining bytes
    write_byte(a_perf->get_bp_measure());           // nn
    write_byte(log(a_perf->get_bw())/log(2.0));     // dd
    write_short(0x1808);                            // cc bb
}

bool midifile::write_sequences (perform * a_perf, sequence *a_solo_seq)
{
    int numtracks = 0;
    bool write_triggers = true;             // default normal seq24 format
    
    file_type_e type = E_MIDI_SEQ24_FORMAT; // default
    
    if(a_solo_seq != nullptr)
    {
        type = E_MIDI_SOLO_SEQUENCE;
        numtracks = 1;
        write_triggers = false;             // solo sequence - don't write triggers
    }
    
    if(type == E_MIDI_SEQ24_FORMAT)         // no need to count when solo
    {
        /* get number of track sequences */
        for (int i = 0; i < c_max_track; i++)
        {
            if (a_perf->is_active_track(i) && !a_perf->get_track(i)->get_song_mute())
            {
                numtracks += a_perf->get_track(i)->get_number_of_sequences();
            }
        }
    }
    write_header(numtracks);                // numtracks will be 1 if solo sequence

    /* We should be good to load now   */
    /* for each Track Sequence in the midi file */

    numtracks = 0;                          // reset for seq->fill_list position

    for (int curTrack = 0; curTrack < c_max_track; curTrack++)
    {
        if ((a_perf->is_active_track(curTrack) && !a_perf->get_track(curTrack)->get_song_mute()) ||
            type == E_MIDI_SOLO_SEQUENCE)
        {
            unsigned int num_seq = 0;
            
            if(type == E_MIDI_SOLO_SEQUENCE )
                num_seq = 1;
            else
                num_seq = a_perf->get_track(curTrack)->get_number_of_sequences();

            /* sequence pointer */
            for (unsigned int a_seq = 0; a_seq < num_seq; a_seq++ )
            {
                sequence * seq = a_solo_seq; // will be nullptr if not solo
                        
                if(type == E_MIDI_SEQ24_FORMAT)
                    seq = a_perf->get_track(curTrack)->get_sequence(a_seq);

                list<char> l;
                seq->fill_list (&l, numtracks, write_triggers);

                /* magic number 'MTrk' */
                write_long (0x4D54726B);

                int size_tempo_time_sig = 0;
                if(numtracks == 0)
                    size_tempo_time_sig = 15; // size, (s/b 19(total) - 4(trk end) = 15 bytes)

                write_long (l.size () + size_tempo_time_sig);

                /*
                    Add the bpm and timesignature stuff here to the first track (0).
                    So we don't have an extra one...
                */

                if(numtracks == 0)
                {
                    write_time_sig(a_perf);
                    write_tempo(a_perf);
                }

                while (l.size () > 0)
                {
                    write_byte (l.back ());
                    l.pop_back ();
                }
                
                if(type == E_MIDI_SOLO_SEQUENCE)
                    break;
                
                numtracks++;
            }
        }
        if(type == E_MIDI_SOLO_SEQUENCE)
            break;
    }

    /* No need to send proprietary if solo sequence */
    if(type == E_MIDI_SEQ24_FORMAT)
    {
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
        /* From sequencer64 for consistency...
         * We now encode the Sequencer64-specific BPM value by multiplying it
         *  by 1000.0 first, to get more implicit precision in the number.
         */
        long scaled_bpm = long(a_perf->get_bpm() * c_bpm_scale_factor);
        write_long (scaled_bpm);

        /* write out the mute groups */
        write_long(c_mutegroups);
        write_long(0);

         /* write out the beats per measure */
        write_long(c_perf_bp_mes);
        write_long(a_perf->get_bp_measure());

        /* write out the beat width */
        write_long(c_perf_bw);
        write_long(a_perf->get_bw());
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

bool midifile::write_song (perform * a_perf)
{
    int numtracks = 0;

    /* get number of tracks  */
    for (int i = 0; i < c_max_track; i++)
    {
        if(a_perf->track_is_song_exportable(i))
            numtracks++;
    }

    if(numtracks == 0)
    {
        error_message_gtk("There are NO exportable tracks!\nDo any have triggers?\nAre all tracks muted?\nAny sequences?");
        return true;    // true so we don't generate a second error about "Error writing file".
    }

    write_header(numtracks);

    /* We should be good to load now   */
    /* for each Track Sequence in the midi file */

    numtracks = 0; // reset for seq->fill_list position

    for (int curTrack = 0; curTrack < c_max_track; curTrack++)
    {
        if(a_perf->track_is_song_exportable(curTrack))
        {
            /* all track triggers */
            track *a_track = a_perf->get_track(curTrack);
            trigger *a_trig = NULL;

            std::vector<trigger> trig_vect;
            a_track->get_trak_triggers(trig_vect); // all triggers for the track

            int vect_size = trig_vect.size();

            list<char> l;
            sequence * seq = NULL;

            /*  sequence name - this will assume the first sequence used is the name for the whole track */
            for (int i = 0; i < vect_size; i++)
            {
                a_trig = &trig_vect[i]; // get the trigger
                seq = a_perf->get_track(curTrack)->get_sequence(a_trig->m_sequence); // get trigger sequence

                if(seq == NULL)
                    continue;  		   // keep checking until we find one

                seq->seq_number_fill_list( &l, numtracks );
                seq->seq_name_fill_list( &l );

                break;                // we found one so get out
            }

            if(seq == NULL) // this is the case of a track has only empty triggers(but has sequences!)
            {
                seq = a_perf->get_track(curTrack)->get_sequence(0); // so just use the first one

                seq->seq_number_fill_list( &l, numtracks );
                seq->seq_name_fill_list( &l );
            }

            // now for each trigger get sequence and add events to list char below - fill_list one by one in order,
            // essentially creating a single long sequence.
            // then set a single trigger for the big sequence - start at zero, end at last trigger end + snap.

            long total_seq_length = 0;
            long prev_timestamp = 0;

            for (int i = 0; i < vect_size; i++)
            {
                a_trig = &trig_vect[i]; // get the trigger
                sequence * trigger_seq = a_perf->get_track(curTrack)->get_sequence(a_trig->m_sequence); // get trigger sequence

                if(trigger_seq == NULL) // skip empty triggers
                    continue;

                prev_timestamp = trigger_seq->song_fill_list_seq_event(&l,a_trig,prev_timestamp); // put events on list
            }

            total_seq_length = trig_vect[vect_size-1].m_tick_end;

            /* adjust sequence length to snap nearest measure past end */
            long measure_ticks = (c_ppqn * 4) * seq->get_bp_measure() / seq->get_bw();
            long remainder = total_seq_length % measure_ticks;
            if(remainder != measure_ticks -1)
                total_seq_length += (measure_ticks - remainder - 1);

            /*
                The sequence trigger is NOT part of the standard midi format and is proprietary to seq24.
                It is added here because the trigger combining has an alternative benefit for editing.
                The user can split, slice and rearrange triggers to form a new sequence. Then mute all
                other tracks and export to a temporary midi file. Now they can import the combined
                triggers/sequence as a new item. This makes editing of long improvised sequences into
                smaller or modified sequences as well as combining several sequence parts painless. Also,
                if the user has a variety of common items such as drum beats, control codes, etc that
                can be used in other projects, this method is very convenient. The common items can
                be kept in one file and exported all, individually, or in part by creating triggers and muting.
            */
            seq->song_fill_list_seq_trigger(&l,a_trig,total_seq_length,prev_timestamp); // the big sequence trigger

            /* magic number 'MTrk' */
            write_long (0x4D54726B);

            int size_tempo_time_sig = 0;
            if(numtracks == 0)
                size_tempo_time_sig = 15; // size, (s/b 19(total) - 4(trk end) = 15 bytes)

            write_long (l.size () + size_tempo_time_sig);

            /*
                Add the bpm and timesignature stuff here to the first track (0).
                So we don't have an extra one...
            */

            if(numtracks == 0)
            {
                write_time_sig(a_perf);
                write_tempo(a_perf);
            }

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

/**
 *  From Sequencer64 project
 *  Internal function for simple calculation of a power of 2 without a lot of
 *  math.  Use for calculating the denominator of a time signature.
 *
 * \param logbase2
 *      Provides the power to which 2 is to be raised.  This integer is
 *      probably only rarely greater than 4 (which represents a denominator of
 *      16).
 *
 * \return
 *      Returns 2 raised to the logbase2 power.
 */

int
midifile::pow2 (int logbase2)
{
    int result;
    if (logbase2 == 0)
        result = 1;
    else
    {
        result = 2;
        for (int c = 1; c < logbase2; c++)
            result *= 2;
    }
    return result;
}

Glib::ustring
midifile::Ulong_To_String_Hex ( unsigned long Number )
{
    char bus_num[12];
    snprintf(bus_num, sizeof(bus_num), "%8lX", Number);

    Glib::ustring str(bus_num);
    return str;
}

void
midifile::error_message_gtk( Glib::ustring message)
{
    Gtk::MessageDialog errdialog
    (
        message,
        false,
        Gtk::MESSAGE_ERROR,
        Gtk::BUTTONS_OK,
        true
    );
    errdialog.run();
}

bool
midifile::verify_change_tempo_timesig(double tempo, long bp_measure, long bw)
{
    std::string str_number = "";
    Glib::ustring message = "From Import file:  ";
    message += m_name.c_str();
    
    str_number = std::to_string(tempo);
    message += "\n\nTempo:  "; message += str_number.c_str();
    
    str_number = std::to_string(bp_measure);
    message += "\nBeats per measure:  "; message += str_number.c_str();
    
    str_number = std::to_string(bw);
    message += "\nBeat width:  "; message += str_number.c_str();
    
    message += "\n\nTempo or time signature is different from current project!\n\n";
    message += "Do you want to change the current project tempo or time signature to the import values?";

    Gtk::MessageDialog warning(message,
                           false,
                           Gtk::MESSAGE_WARNING, Gtk::BUTTONS_YES_NO, true);

    auto result = warning.run();

    if (result == Gtk::RESPONSE_NO )
    {
        return false;
    }
    return true;
}