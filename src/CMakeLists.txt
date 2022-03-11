#CMake file for Seq42

project (seq42)

set (ProgSources
    configfile.cpp
    event.cpp
    keybindentry.cpp
    lfownd.cpp
    maintime.cpp
    mainwnd.cpp
    midibus.cpp
    midibus_portmidi.cpp
    midifile.cpp
    s42file.cpp
    mutex.cpp
    options.cpp
    optionsfile.cpp
    perfnames.cpp
    perform.cpp
    perfroll.cpp
    perfroll_input.cpp
    perftime.cpp
    seqdata.cpp
    seqedit.cpp
    seqevent.cpp
    seqkeys.cpp
    seqlist.cpp
    seqroll.cpp
    seqtime.cpp
    sequence.cpp
    tempo.cpp
    tempopopup.cpp
    track.cpp
    trackedit.cpp
    trackmenu.cpp
    userfile.cpp
)


set(ExternLibraries
    ${GTKMM_LIBRARIES}
    ${ALSA_LIBRARIES}
    ${JACK_LIBRARIES}
    ${LIBLO_LIBRARIES}
    rt
)

add_executable (seq42 ${ProgSources} seq42.cpp)

target_include_directories (
    seq42 PRIVATE
    ${GTKMM_INCLUDE_DIRS}
    ${ALSA_INCLUDE_DIRS}
    ${JACK_INCLUDE_DIRS}
    ${LIBLO_INCLUDE_DIRS}
    "${CMAKE_CURRENT_SOURCE_DIR}/pixmaps"
    "${CMAKE_CURRENT_BINARY_DIR}/pixmaps"
)

target_link_libraries (seq42 ${ExternLibraries})


install (TARGETS seq42 RUNTIME DESTINATION bin)
