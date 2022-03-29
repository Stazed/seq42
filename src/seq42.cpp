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

#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>

#ifndef CMAKE_BUILD_SUPPORT
#ifdef __WIN32__
#    include "configwin32.h"
#else
#    include "config.h"
#endif
#endif

#include "font.h"
#include "mainwnd.h"
#include "midifile.h"
#include "optionsfile.h"
#include "perform.h"
#include "userfile.h"


Glib::ustring global_client_name = PACKAGE; // default seq42
Glib::ustring global_filename = "";

Glib::RefPtr<Gtk::Application> application;

#ifdef NSM_SUPPORT
#include "nsm.h"

bool global_nsm_gui = false;
bool nsm_opional_gui_support = true;
static nsm_client_t *nsm = 0;
static int wait_nsm = 1;

void
nsm_hide_cb(void * /* userdata */)
{
    application->hold();
    global_nsm_gui = false;
}

void
nsm_show_cb(void * /* userdata */)
{
    global_nsm_gui = true;
}

int                                                                  
cb_nsm_open ( const char *save_file_path,   // See API Docs 2.2.2 
              const char *,                 // display_name
              const char *client_id,        // Use as JACK Client Name
              char **,                      // out_msg
              void *)                       // userdata
{
    global_filename = save_file_path;
    global_filename += ".s42";
    global_client_name = strdup(client_id);
    wait_nsm = 0;
    return ERR_OK;
}                                                                    
                                                                     
int
cb_nsm_save ( char **,  void *userdata)
{
    mainwnd *seq42_window =  (mainwnd*) userdata;

    seq42_window->file_save();

    return ERR_OK;
}
                                                                  
#endif  // NSM_SUPPORT

/* struct for command parsing */
static struct
    option long_options[] =
{
    {"help",     0, 0, 'h'},
    {"showmidi",     0, 0, 's'},
    {"show_keys",     0, 0, 'k' },
    {"stats",     0, 0, 'S' },
    {"priority", 0, 0, 'p' },
    {"ignore",required_argument, 0, 'i'},
    {"interaction_method",required_argument, 0, 'x'},
    {"playlist file", required_argument, 0, 'X'},
    {"jack_transport",0, 0, 'j'},
    {"jack_master",0, 0, 'J'},
    {"jack_master_cond",0,0,'C'},
    {"song_start_mode", required_argument, 0, 'M' },
    {"manual_alsa_ports", 0, 0, 'm' },
    {"pass_sysex", 0, 0, 'P'},
    {"version", 0, 0, 'v'},
    {"client_name", required_argument, 0, 'n'},
    {0, 0, 0, 0}
};

static const char versiontext[] = PACKAGE " " VERSION "\n";

bool global_manual_alsa_ports = true;
bool global_showmidi = false;
bool global_priority = false;
bool global_device_ignore = false;
int global_device_ignore_num = 0;
bool global_stats = false;
bool global_pass_sysex = false;
Glib::ustring last_used_dir ="/";
Glib::ustring last_midi_dir ="/";
std::string config_filename = ".seq42rc";
std::string user_filename = ".seq42usr";
Glib::ustring playlist_file = "";
bool global_print_keys = false;
interaction_method_e global_interactionmethod = e_seq42_interaction;

int global_sequence_editor_vertical_zoom = c_default_config_sequence_zoom;
int global_song_editor_vertical_zoom = c_default_config_song_zoom;
int global_song_editor_horizontal_zoom = c_default_config_song_horizontal_zoom;
bool global_with_jack_transport = false;
bool global_with_jack_master = false;
bool global_with_jack_master_cond = false;
bool global_song_start_mode = true;
bool playlist_mode = false;

user_midi_bus_definition   global_user_midi_bus_definitions[c_maxBuses];
user_instrument_definition global_user_instrument_definitions[c_max_instruments];

font *p_font_renderer;

#ifdef __WIN32__
#   define HOME "HOMEPATH"
#   define SLASH "\\"
#else
#   define HOME "HOME"
#   define SLASH "/"
#endif


int
main (int argc, char *argv[])
{
    /* Scan the argument vector and strip off all parameters known to
     * GTK+. */
    Gtk::Main kit(argc, argv);

    /*prepare global MIDI definitions*/
    for ( int i=0; i<c_maxBuses; i++ )
    {
        for ( int j=0; j<16; j++ )
            global_user_midi_bus_definitions[i].instrument[j] = -1;
    }

    for ( int i=0; i<c_max_instruments; i++ )
    {
        for ( int j=0; j<128; j++ )
            global_user_instrument_definitions[i].controllers_active[j] = false;
    }

    /* parse parameters */
    int c;

    while (true)
    {
        /* getopt_long stores the option index here. */
        int option_index = 0;

        c = getopt_long (argc, argv, "Chi:jJkmM:pPsSvx:X:n:", long_options, &option_index);

        /* Detect the end of the options. */
        if (c == -1)
            break;

        switch (c)
        {

        case '?':
        case 'h':

            printf( "   -h, --help: show this message\n" );
            printf( "   -v, --version: show program version information\n" );
            printf( "   -m, --manual_alsa_ports: seq42 won't attach alsa ports\n" );
            printf( "   -s, --showmidi: dumps incoming midi events to screen\n" );
            printf( "   -p, --priority: runs higher priority with FIFO scheduler (must be root)\n" );
            printf( "   -P, --pass_sysex: passes any incoming sysex messages to all outputs \n" );
            printf( "   -i, --ignore <number>: ignore ALSA device\n" );
            printf( "   -k, --show_keys: prints pressed key value\n" );
            printf( "   -x, --interaction_method <number>: see .seq42rc for methods to use\n" );
            printf( "   -X, --playlist file: path to playlist file and name\n" );
            printf( "   -j, --jack_transport: seq42 will sync to jack transport\n" );
            printf( "   -J, --jack_master: seq42 will try to be jack master\n" );
            printf( "   -C, --jack_master_cond: jack master will fail if there is already a master\n" );
            printf( "   -M, --song_start_mode <mode>: The following play\n" );
            printf( "                          modes are available (0 = live mode)\n");
            printf( "                                              (1 = song mode) (default)\n" );
            printf( "   -n, --client_name <name>: Set alsa client name: Default = seq42\n");
            printf( "   -S, --stats: show statistics\n" );
            printf( "\n\n\n" );

            return EXIT_SUCCESS;
            break;

        case 'S':
            global_stats = true;
            break;

        case 's':
            global_showmidi = true;
            break;

        case 'p':
            global_priority = true;
            break;

        case 'P':
            global_pass_sysex = true;
            break;

        case 'k':
            global_print_keys = true;
            break;

        case 'j':
            global_with_jack_transport = true;
            break;

        case 'J':
            global_with_jack_master = true;
            break;

        case 'C':
            global_with_jack_master_cond = true;
            break;

        case 'M':
            if (atoi( optarg ) > 0)
            {
                global_song_start_mode = true;
            }
            else
            {
                global_song_start_mode = false;
            }
            break;

        case 'm':
            global_manual_alsa_ports = true;
            break;

        case 'n':
            global_client_name = Glib::ustring( optarg );
            break;

        case 'i':
            /* ignore alsa device */
            global_device_ignore = true;
            global_device_ignore_num = atoi( optarg );
            break;

        case 'v':
            printf("%s", versiontext);
            return EXIT_SUCCESS;
            break;

        case 'x':
            global_interactionmethod = (interaction_method_e)atoi( optarg );
            break;

        case 'X':
            playlist_mode = true;
            playlist_file = Glib::ustring(optarg);
            break;

        default:
            break;
        }
    } /* end while */

    /* the main performance object */
    perform p;

    /* read user preferences files */
    if ( getenv( HOME ) != NULL )
    {
        Glib::ustring home( getenv( HOME ));
        last_used_dir = home;
        Glib::ustring total_file = home + SLASH + config_filename;

        if (Glib::file_test(total_file, Glib::FILE_TEST_EXISTS))
        {
            printf( "Reading [%s]\n", total_file.c_str());

            optionsfile options( total_file );

            if ( !options.parse( &p ) )
            {
                printf( "Error Reading [%s]\n", total_file.c_str());
            }
        }

        total_file = home + SLASH + user_filename;
        if (Glib::file_test(total_file, Glib::FILE_TEST_EXISTS))
        {
            printf( "Reading [%s]\n", total_file.c_str());

            userfile user( total_file );

            if ( !user.parse( &p ) )
            {
                printf( "Error Reading [%s]\n", total_file.c_str());
            }
        }
    }
    else
        printf( "Error calling getenv( \"%s\" )\n", HOME );

#ifdef NSM_SUPPORT
    // Initialize NSM before creation of alsa ports with p.init()
    // so we can get the global_client_name for setting the port names.
    // Also gets the global_filename so it can be loaded or created on startup.
    const char *nsm_url = getenv( "NSM_URL" );
    
    if ( nsm_url )
    {
        nsm = nsm_new();

        nsm_set_open_callback( nsm, cb_nsm_open, 0 );

        if ( 0 == nsm_init( nsm, nsm_url ) )
        {
            nsm_send_announce( nsm, PACKAGE, ":optional-gui:dirty:", argv[0] );
        }

        int timeout = 0;
        while ( wait_nsm )
        {
            nsm_check_wait( nsm, 500 );
            timeout += 1;

            if ( timeout > 200 )
                exit ( 1 );
        }
    }
#endif // NSM_SUPPORT

    p.init();

    p.launch_input_thread();
    p.launch_output_thread();
    p.init_jack();
    
    p_font_renderer = new font();

    application = Gtk::Application::create();
    mainwnd seq42_window( &p, application );

#ifdef NSM_SUPPORT
    if ( nsm_url )
    {
        // Set the save callback and nsm client now that the mainwnd is created.
        nsm_set_save_callback( nsm, cb_nsm_save, (void*) &seq42_window );
        if (nsm_opional_gui_support)
        {
            nsm_set_show_callback(nsm, nsm_show_cb, 0);
            nsm_set_hide_callback(nsm, nsm_hide_cb, 0);
            if (!global_nsm_gui) nsm_hide_cb(0);
            else nsm_send_is_shown(nsm);
        } 
        else 
        {
            global_nsm_gui = true;
        }

        // set client and limited file menus
        seq42_window.set_nsm_client(nsm, nsm_opional_gui_support);
        
        // Open the NSM session file
        if (Glib::file_test(global_filename, Glib::FILE_TEST_EXISTS))
        {
            seq42_window.open_file(global_filename);
        }
        else    // file does not exists, so create it.
        {
            seq42_window.file_save();
            seq42_window.update_window_title();
        }
        
        signal(SIGTERM, [](int /* param */)
        {
            global_is_running = false;
            application->quit();
        });
    }
#endif // NSM_SUPPORT

    // Do not use command line file if if in NSM session
#ifdef NSM_SUPPORT
    if(!nsm)
    {
#endif
        if (optind < argc)
        {
            if (Glib::file_test(argv[optind], Glib::FILE_TEST_EXISTS))
                seq42_window.open_file(argv[optind]);
            else
                printf("File not found: %s\n", argv[optind]);
        }

        if(playlist_mode)
        {
            p.set_playlist_mode(playlist_mode);
            p.set_playlist_file(playlist_file);

            if(p.get_playlist_mode())    // true means file load with no errors
            {
                if(seq42_window.verify_playlist_dialog())
                {
                    seq42_window.playlist_verify();
                }
                else
                {
                    seq42_window.playlist_jump(PLAYLIST_ZERO);
                }
            }
        }
#ifdef NSM_SUPPORT
    }
#endif
    
    int status = 0;
    status = application->run(seq42_window);

    p.deinit_jack();

    if ( getenv( HOME ) != NULL )
    {
        string home( getenv( HOME ));
        Glib::ustring total_file = home + SLASH + config_filename;
        printf( "Writing [%s]\n", total_file.c_str());

        optionsfile options( total_file );

        if ( !options.write( &p ) )
        {
            printf( "Error writing [%s]\n", total_file.c_str());
        }
    }
    else
    {
        printf( "Error calling getenv( \"%s\" )\n", HOME );
    }

#ifdef NSM_SUPPORT
    if(nsm)
    {
        nsm_free( nsm );
        nsm = NULL;
    }
#endif

    return status;
}


