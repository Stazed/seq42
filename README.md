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

Other than these conceptual changes, a seq24 user should feel at home.  The awesome sequence editor is pretty much the same.  The 8x4 sequence set window from seq24 is gone.  Instead there's a sequence list window that you can open from the main song editing window.  The sequence list displays all the sequences in the song; you can sort the list in various ways (it initializes sorted by track) and there's a check box for each sequence that you can use to toggle each sequence's play flag (when you're in live mode).  See seq42.png for a screenshot.

Here are some of the other enhancements of seq42:
* song-level transpose function: Each track has a transpose flag (true by default when adding a new track; you'll want to disable it on tracks that trigger drums or sample hits).  At any point (even while the sequencer is running) you can change the song's transpose value (in the range of -12 to +12) and it will affect the playback of all transposable tracks.  If you like, you can make the transposition permanent via a new song menu item.
* drum machine-like swing function: Each sequence has a swing setting (none, 1/8 or 1/16).  At the song level (along with BPM) there are two swing amount settings (one for 1/8 and one for 1/16).  These settings are evaluated while the sequencer is playing; the notes in each sequence remain "straight" on the grid.
* main window has a button to toggle between song and live modes (in seq24, this was in a tab in the Options dialog).
* sequence editor now supports use of the cursor keys to move selected notes up/down in pitch or left/right in time.  Up and down move by one semitone.  Hold down Shift to move up or down by octave.  Right and left move the selected notes by the current snap amount.  (Note that it doesn't automatically snap the notes to the grid; if you want that, use the quantize function first.)  You can also nudge the selected notes by individual ticks by holding down the Shift key with left or right.
* sequence editor has new menu items to randomize selected events (adding or subtracting a random value in a range from +/-1 to +/-16).  This can be used on velocity or CC values.
* sequence editor has new menu items to select notes that occur on specific beats (even 1/4, odd 1/4, even 1/8, odd 1/8, even 1/16, odd 1/16)
* song window has new menu items to mute all tracks, un-mute all tracks and toggle track mute status.
* NEW - Add SIGINT and SIGUSR1 for session handling.
* NEW - Import seq24/32/64 files, and any type 1 midi files. Maximum number of tracks is currently 64. 
* NEW - Import now supports selection of seq24/32 by set number.
* NEW - split trigger (middle mouse button) will now split on mouse pointer location - grid-snapped
* NEW - fix 'fruity' input method to work for trigger pop-up menu (middle mouse button). Split trigger is ctrl-L and paste is middle mouse or ctrl-L.
* NEW - merge sequence: right click on existing track name - select Merge Sequence - Select the sequence from the available list.  The Sequences AND song triggers will be added to the track. Now you can easily combine those seq24/32/64 tracks!! - any overlapping trigger will be overwritten and/or split - just create a new sequence for them...
* NEW - ctrl-C song trigger and paste to any location. Middle mouse button click to location ctrl-V will paste to the location grid-snapped. Subsequent ctrl-V will paste directly after previous paste location. If no paste location is selected, then default will paste after copied trigger. 
* NEW - Copy song triggers across different tracks. This will also copy the sequence into the new track.  Select trigger with mouse, ctrl-C: middle mouse click on new track location, ctrl-V. After paste, the subsequent ctrl-V will revert to default behavior - paste trigger only after previous paste. Paste across track is only allowed once per ctrl-C action... to prevent accidental duplication of sequence copies.
* NEW - Track row insert/delete/pack. Right click the track name - popup menu - all sequence editors and track edit must be closed.
* NEW - Tracks no longer auto packed on save. Track index location is saved and track mute status is saved.
* NEW - Undo/Redo completely re-written. All trigger, sequence, track actions can be undone/redone. Redo is cleared upon any new edit of track items.  This is done to eliminate the possibility of undo pushing to redo items such as trigger edits, then deleting the track. If redo were not cleared, then attempting to redo items on a track that no longer exists would not work. Undo/redo does not change
the track edit items (name,channel,bus,transpose,mute)... except when undo/redo row insert/delete/pack or deleting midi import. Sequence copies will now copy the seq level undo/redo. For track level undo/redo, (insert/delete/pack,copy,seq copy), the track editor and sequence editor windows must be closed.
* NEW - Now beats-per-measure, beat-width song editor are saved to file. Also these now work for trigger default length selection. New grid-snaps were added to accomodate.
* NEW - Export midi files - sequences in seq24/32/64 format - edit menu, export(seq24/32/64).
* NEW - Export midi song render - combines all triggers on track together - can be used in conventional midi players. From edit menu, export song.
		Muted tracks and tracks with NO triggers will NOT be exported.
		This feature can also be used as a powerful editing feature as follows:
			The user can split, slice and rearrange triggers to form a new sequence. Then mute all
			other tracks and export to a temporary midi file. Now they can import the combined
		    triggers/sequence as a new item. This makes editing of long improvised sequences into
		    smaller or modified sequences as well as combining several sequence parts painless. Also,
		    if the user has a variety of common items such as drum beats, control codes, etc that
		    can be used in other projects, this method is very convenient. The common items can
		    be kept in one file and exported all, individually, or in part by creating triggers and muting.
		    
* NEW - Note listen added to sequence draw notes and move notes.
* NEW - Fixed Jack master to work properly. Start as master on left tick marker and looping works.
* NEW - Jack slave and master conditional work - and will follow the jack frame without master BBT
* NEW - Fixed BBT calculations as master to send correct starting position.
* NEW - Added Jack sync button to main window and removed from options menu.
* NEW - Added sequence list button pop-up to main window and removed from edit.
* NEW - Added auto scroll to song editor and sequence editor (Thanks to Chris Ahlstrom - sequencer64 for the better code).
* NEW - Midi step edit is added on record(seq42 not playing), starting at transport line. Sequence editor Ctrl-right and Ctrl-left key moves transport line by snap. Home key moves transport line to start.
* NEW - Data event edit handles added for individual adjustment.
* NEW - New colors for sequence editor.
* NEW - Added Fast Forward / Rewind buttons that work with or without jack.
* NEW - Added Zoom of song editor (Thanks to Chris Ahlstrom - sequencer64) - ctrl mouse wheel, z and Z when focus on track editor.
* NEW - Added Chord note selection to sequence editor (Thanks to LMMS for the lookup table).
* NEW - Added Song editor will display play position marker even when stopped and show position changes from other jack clients.
* NEW - Added Auto scroll will follow position marker even when stopped.
* New - Fixed Beat width to work when NOT in jack mode.
* NEW - Added Non-timeline like key-p reposition song editor play location. With mouse focus on song editor(tracks), press 'p' to move play position to mouse location.
* NEW - Added Edit menu item to create new triggers between L and R play markers for triggers with their 'playing' box checked in the sequence list.
* NEW - Added in the trigger popup menu an option to set the pattern's play flag (if it's off) or unset it (if on). The option only shows if not in song mode.
* NEW - Added new edit menu item to "Delete unused sequences".
* NEW - Added midi bus and midi channel to sequence list.
* NEW - Add sequence data selected note Ons to draw last in case multiple events cover the selection.
* NEW - Move midi import to edit menu.
* NEW - Midi import and export now read/supply type 1 midi tempo and time signature (Thanks to Chris Ahlstrom - sequencer64).
* NEW - Add support for transposable flag on seq32 import.
* NEW - Add pause to follow transport when editing with button press on song editor and sequence editor.
* NEW - Added additional music scale items in sequence editor (Thanks to Chris Ahlstrom - sequencer64).
* NEW - Add key binding to FF/Rewind (keys f,r -- works best without key repeat) - configurable, also made key-p move to mouse position configurable.
* NEW - Fixed midi song position to work when not starting from beginning of song. Play conditional upon song button.
* NEW - Moved to jack_process_callback() - seq42 is not slow sync.
* NEW - File version 5 - portable, file ID, date and time timestamp, program version added.
* NEW - Midi record now supports simultaneous multi-sequence record, channel specific, up to 16 channels at once.
* NEW - Command line -n <client name> option added to support user changes to alsa client name.
* NEW - BPM now supports two decimal precision - also added to import and export midi.
* NEW - Added BPM tap button - sequencer64 type, thanks to Chris Ahlstrom.
* NEW - Added additional midi recording types. New 'Rec' button in sequence editor to select type: Legacy merged looped (notes are added upon looping of sequence): Overwrite looped record (notes will be cleared upon looping after the first note played and overwrite previous pass): Expand sequence length to fit recording (sequence does not loop and will expand after reaching last 1/4 measure)
* NEW - Fix FF/Rewind to work with key-repeat properly.
* NEW - Add CAPS to midi file extension checks.
* NEW - Add export individual solo sequence to trigger menu pop-up. Export will not include song triggers, or proprietary performance tags.
* NEW - Add user verification pop-up on imported files when tempo and or time signature is different from main performance values.
* NEW - Optimized code for file loading to greatly speed up midi import and .s42 loading.
* NEW - Add solo track export (song format).
* NEW - Add solo trigger export to trigger pop-up menu.
* NEW - Add very limited support for using sysex for transport control.
* NEW - Fix midi song position change when sync midiclock (no jack).
* NEW - Add Tempo change to song editor (See SEQ42 document for details).
* NEW - Add Stop markers to tempo change (See SEQ42 document for details).
* NEW - Add setlist file support (See SEQ42 document for details).
* NEW - BPM, tempo changes added to undo/redo.
* NEW - Clean up and beautify the LFO window.
* NEW - Add recent files list to menu.


What to do with a fresh repository checkout?
--------------------------------------------
Apply "autoreconf -i" to get a configure script, then:
Run the usual "./configure" then "make".  Finally copy the "seq42" binary (in the "src" directory) wherever you like.


Re: macro `AM_PATH_ALSA' not found in library

Please run 'aclocal -I m4' to regenerate the aclocal.m4 file.
You need aclocal from GNU automake 1.5 (or newer) to do this.
Then run 'autoreconf -i' to regenerate the configure file.


