seq42 README
============

What is seq42?
--------------
It's a fork of [seq24](https://launchpad.net/seq24) (which is a fork of the [original seq24](http://filter24.org/seq24/)), but with a greater emphasis on song editing (as opposed to live looping) and some enhancements.
seq24 is great for sequence editing and live looping, but I found it cumbersome to edit songs as the number of sequences grew (I would quickly reach a point where there were more sequence rows in the song editor that would fit on my screen without scrolling, which made it difficult to keep track of the whole song).

seq42 adds a new concept of a "track".  It breaks down like this:
A song in seq24 is a collection of sequences.  Each sequence has a midi bus and midi channel that it sends events to.
A seq24 sequence also has a collection of triggers, which indicate times in a song where that sequence plays.
A song in seq42 is a collection of tracks.  Each track has a midi bus and a midi channel (and also a transpose flag... more on that later).  Each seq42 track has a collection of sequences (which send their events to their track's midi bus and channel) and a collection of triggers, each of which triggers one of that track's sequences.
In seq42, the main window is the song editor, with one row per track.  You'd have to have a lot of tracks to need to scroll up and down!

Other than these conceptual changes, a seq24 user should feel at home.  The awesome sequence editor is pretty much the same.  The 8x4 sequence set window from seq24 is gone.  Instead there's a sequence list window that you can open from the main song editing window.  The sequence list displays all the sequences in the song; you can sort the list in various ways (it initializes sorted by track) and there's a checkbox for each sequence that you can use to toggle each sequence's play flag (when you're in live mode).

Here are some of the other enhancements of seq42:
* song-level transpose function: Each track has a transpose flag (true by default when adding a new track; you'll want to disable it on tracks that trigger drums or sample hits).  At any point (even while the sequencer is running) you can change the song's transpose value (in the range of -12 to +12) and it will affect the playback of all transposable tracks.  If you like, you can make the transposition permanent via a new song menu item.
* drum machine-like swing function: Each sequence has a swing setting (none, 1/8 or 1/16).  At the song level (along with BPM) there are two swing amount settings (one for 1/8 and one for 1/16).  These settings are evaluated while the sequencer is playing; the notes in each sequence remain "straight" on the grid.
* main window has a button to toggle between song and live modes (in seq24, this was buried in the options popup).
* sequence editor now supports use of the cursor keys to move selected notes up/down in pitch or left/right in time.  Up and down move by one semitone.  Hold down Shift to move up or down by octave.  Right and left move the selected notes by the current snap amount.  (Note that it doesn't automatically snap the notes to the grid; if you want that, use the quantize function first.)  You can also nudge the selected notes by individual ticks by holding down the Shift key with left or right.
* sequence editor has new menu items to randomize selected events (adding or subtracting a random value in a range from +/-1 to +/-16).  This can be used on velocity or CC values.
* sequence editor has new menu items to select notes that occur on specific beats (even 1/4, odd 1/4, even 1/8, odd 1/8, even 1/16, odd 1/16)
* song window has new menu items to mute all tracks, unmute all tracks and toggle track mute status.

There are a few caveats too:
* seq42 has a different file format; it can't open seq24 files and there's (at least currently) no way to convert seq24 files to seq42's format.
* seq24 could import and export midi files; seq42 currently cannot.

What to do with a fresh repository checkout?
--------------------------------------------
Apply "autoreconf" to get a configure script, then "./configure; make".  Finally copy the seq42 binary wherever you like.

