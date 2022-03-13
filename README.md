seq42 README
============

Screenshot
----------

![screenshot](https://raw.github.com/Stazed/seq42/wip/icons/seq42-2.0.0.png "Seq42 release-2.0.0")

What is seq42?
--------------
It's a fork of [seq24](https://launchpad.net/seq24) (which is a fork of the [original seq24](http://filter24.org/seq24/)), but with a greater emphasis on song editing (as opposed to live looping) and some enhancements.
Seq24/seq32 are great for sequence editing and live looping, but it can be cumbersome to edit songs as the number of sequences grow. You can quickly reach a point where there were more sequence rows in the song editor than would fit on the screen without scrolling. This makes it difficult to keep track of the whole song alignment.

Seq42 adds a new concept of a "track".  It breaks down like this:

A song in seq42 is a collection of tracks.  Each track has a midi bus and a midi channel.  Each seq42 track has a collection of sequences (which send their events to their track's midi bus and channel) and a collection of triggers. Each trigger plays one of that track's sequences.
In seq42, the main window is the song editor, with one row per track.  After adding a trigger to a track, you can right-click on the trigger to show a context menu with options to set the trigger to an existing sequence or create a new sequence or copy an existing sequence (possibly even from another track).

Other than these conceptual changes, a seq24 user should feel at home.  The awesome sequence editor is pretty much the same.  The 8x4 sequence set window from seq24 is gone.  Instead there's a sequence list window that you can open from the main song editing window.  The sequence list displays all the sequences in the song. You can sort the list in various ways (it initializes sorted by track) and there's a check box for each sequence that you can use to toggle each sequence's play flag (when you're in live mode).

Version 2.0.0 changes:

Beginning with version 2.0.0 seq42 has been ported to Gtkmm-3. 

New colors.

The build system has been moved to CMAKE.

Additional editing features.

NSM support now has optional gui, and dirty flag.

See the SEQ42 document for additional information.

Install
-------

The dependencies are:

*   cmake
*   Gtkmm-3.0
*   Jackd
*   libasound2
*   liblo   (NSM support)
*   librt

Additional dependencies required by Gtkmm-3
*   sigc++-2.0
*   glibmm-2.4
*   cairomm-1.0
*   pangomm-1.4
*   atkmm-1.6

For compiling you will also need the development packages for the above dependencies.

What to do with a fresh repository checkout?
--------------------------------------------
Using CMAKE:
```bash
    mkdir build
    cd build
    cmake ..
    make
    make install (as root)
```
To remove:
```bash
    make uninstall (as root)
```

--------------------------------------------
Using old autotools:

Apply "autoreconf -i" to get a configure script, then:
Run the usual "./configure" then "make".

To install: As root, "make install".
Or you can copy the "seq42" binary (in the "src" directory) wherever you like.

Re: macro `AM_PATH_ALSA' not found in library

Please run 'aclocal -I m4' to regenerate the aclocal.m4 file.
You need aclocal from GNU automake 1.5 (or newer) to do this.
Then run 'autoreconf -i' to regenerate the configure file.


