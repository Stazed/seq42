
#add_subdirectory(css)

install (FILES help.html
    contact.html 
    license.html
    main_window.html
    options_window.html
    playlist_mode.html 
    seq42rc.html
    sequence_editor.html
    sequence_list.html
    what_why.html
    who.html
    DESTINATION share/doc/seq42/html)


install (DIRECTORY images/
    DESTINATION share/doc/seq42/html/images)