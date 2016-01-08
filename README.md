seq42 README
============

What is seq42?
--------------
It's a fork of [seq24](https://launchpad.net/seq24) (which is a fork of the [original seq24](http://filter24.org/seq24/)), but with a greater emphasis on song editing (as opposed to live looping) and some enhancements.
seq24 is great for sequence editing and live looping, but I found it cumbersome to edit songs as the number of sequences grew (I would quickly reach a point where there were more sequence rows in the song editor than would fit on my screen without scrolling, which made it difficult to keep track of the whole song).

seq42 adds a new concept of a "track".  It breaks down like this:

A song in seq24 is a collection of sequences.  Each sequence has a midi bus and midi channel that it sends events to.
A seq24 sequence also has a collection of triggers, which indicate times in a song where that sequence plays.  

A song in seq42 is a collection of tracks.  Each track has a midi bus and a midi channel (and also a transpose flag... more on that later).  Each seq42 track has a collection of sequences (which send their events to their track's midi bus and channel) and a collection of triggers, each of which triggers one of that track's sequences.
In seq42, the main window is the song editor, with one row per track.  After adding a trigger to a track, you can right-click on the trigger to show a context menu with options to set the trigger to an existing sequence or create a new sequence or copy an existing sequence (possibly even from another track).

Other than these conceptual changes, a seq24 user should feel at home.  The awesome sequence editor is pretty much the same.  The 8x4 sequence set window from seq24 is gone.  Instead there's a sequence list window that you can open from the main song editing window.  The sequence list displays all the sequences in the song; you can sort the list in various ways (it initializes sorted by track) and there's a checkbox for each sequence that you can use to toggle each sequence's play flag (when you're in live mode).  Here's a [screenshot](http://cloud.github.com/downloads/sbrauer/seq42/seq42-screenshot.png).

Here are some of the other enhancements of seq42:
* song-level transpose function: Each track has a transpose flag (true by default when adding a new track; you'll want to disable it on tracks that trigger drums or sample hits).  At any point (even while the sequencer is running) you can change the song's transpose value (in the range of -12 to +12) and it will affect the playback of all transposable tracks.  If you like, you can make the transposition permanent via a new song menu item.
* drum machine-like swing function: Each sequence has a swing setting (none, 1/8 or 1/16).  At the song level (along with BPM) there are two swing amount settings (one for 1/8 and one for 1/16).  These settings are evaluated while the sequencer is playing; the notes in each sequence remain "straight" on the grid.
* main window has a button to toggle between song and live modes (in seq24, this was in a tab in the Options dialog).
* sequence editor now supports use of the cursor keys to move selected notes up/down in pitch or left/right in time.  Up and down move by one semitone.  Hold down Shift to move up or down by octave.  Right and left move the selected notes by the current snap amount.  (Note that it doesn't automatically snap the notes to the grid; if you want that, use the quantize function first.)  You can also nudge the selected notes by individual ticks by holding down the Shift key with left or right.
* sequence editor has new menu items to randomize selected events (adding or subtracting a random value in a range from +/-1 to +/-16).  This can be used on velocity or CC values.
* sequence editor has new menu items to select notes that occur on specific beats (even 1/4, odd 1/4, even 1/8, odd 1/8, even 1/16, odd 1/16)
* song window has new menu items to mute all tracks, unmute all tracks and toggle track mute status.
* NEW - Add SIGINT and SIGUSR1 for session handling.
* NEW - Import seq24 files, and any midi files!!
* NEW - split trigger (middle mouse button) will now split on mouse pointer location - grid-snapped
* NEW - fix 'fruity' input method to work for trigger popup menu (middle mouse button). Split trigger is ctrl-L and paste is middle mouse or ctrl-L.
* NEW - merge sequence: right click on existing track name - select Merge Sequence - Select the sequence from the available list.  The Sequences AND song triggers will be added to the track. Now you can easily combine those seq24 tracks!! - any overlapping trigger will be overwritten and/or split - just create a new sequence for them...
* NEW - ctrl-C song trigger and paste to any location. Middle mouse button click to location ctrl-V will paste to the location grid-snapped. Subsequent ctrl-V will paste directly after previous paste location. If no paste location is selected, then default will paste after copied trigger. 
* NEW - Copy song triggers across different tracks. This will also copy the sequence into the new track.  Select trigger with mouse, ctrl-C: middle mouse click on new track location, ctrl-V. After paste, the subsequent ctrl-V will revert to default behavior - paste trigger only after previous paste. Paste across track is only allowed once per ctrl-C action... to prevent accidental duplication of sequence copies.
* NEW - Track row insert/delete/pack. Right click the track name - popup menu - all sequence editors and track edit must be closed.
* NEW - Tracks no longer auto packed on save. Track index location is saved and track mute status is saved.
* NEW - Undo/Redo completely re-written. All tirgger, sequence, track actions can be undone/redone. Redo is cleared upon any new edit of track items.  This is done to eliminate the possibility of undo pushing to redo items such as trigger edits, then deleting the track. If redo were not cleared, then attempting to redo items on a track that no longer exists would not work. Undo/redo does not change
the track edit items (name,channel,bus,transpose,mute)... except when undo/redo row insert/delete/pack or deleting midi import. Sequence copies will now copy the seq level undo/redo. For track level undo/redo, (insert/delete/pack,copy,seq copy), the track editor and sequence editor windows must be closed.
* NEW - Now beats-per-measure, beat-width song editor are saved to file. Also these now work for trigger default length selection. New grid-snaps were added to accomodate.
* NEW - Export midi files - sequences in seq24 format
* NEW - Note listen added to sequence draw notes and move notes.
* NEW - Fixed Jack master to work properly. Start as master on left tick marker and looping works.
* NEW - Jack slave and master conditional work - and will follow the jack frame without master BBT
* NEW - Fixed BBT calcualtions as master to send correct starting position.
* NEW - Added Jack sync button to main window and removed from options menu.
* NEW - Added sequence list button popup to main window and removed from edit.
* NEW - Added auto scroll to song editor and sequence editor(seq only on larger sequences)


What to do with a fresh repository checkout?
--------------------------------------------
Run the usual "./configure" then "make".  Finally copy the "seq42" binary (in the "src" directory) wherever you like.

