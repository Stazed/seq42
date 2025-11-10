
install (FILES hicolor/48x48/apps/seq42.png
    hicolor/scalable/apps/seq42.svg
    DESTINATION share/pixmaps)

install(DIRECTORY hicolor/ DESTINATION share/icons/hicolor)

install (FILES seq42.desktop
    DESTINATION share/applications)