#CMake file for Seq42

cmake_minimum_required(VERSION 3.5.1)
project (seq42)

set (PACKAGE_VERSION "2.1.4")

set (CMAKE_SKIP_RULE_DEPENDENCY OFF)

add_definitions (-D'VERSION="${PACKAGE_VERSION}"')
add_definitions(-D'WEBSITE="https://github.com/Stazed/seq42"')
add_definitions(-D'PACKAGE="seq42"')
#add_definitions(-D'HELPDIR="${CMAKE_INSTALL_PREFIX}/share/doc/seq42"')
add_definitions(-D'CMAKE_BUILD_SUPPORT=1')

include (CheckCSourceCompiles)
include(CheckCXXCompilerFlag)

option (EnableNSM "Enable NSM session support" ON)
option (BuildForDebug "Enable gdb debugging support" OFF)
option (EnableMIDIControl "Enable MIDI transport control support" ON)
option (EnableJackSupport "Enable Jack transport control support" ON)


set(CMAKE_BUILD_TYPE "Release")

set (BuildOptionsBasic
    "-std=c++11 -O3"
    CACHE STRING "basic X86 complier options"
)

set (BuildOptionsDebug
    "-std=c++11 -O0 -g3 -ggdb -Wall -Wextra -Wpointer-arith"
    CACHE STRING "Debug build flags"
)

find_package (PkgConfig REQUIRED)

#dependencies
find_package (ALSA REQUIRED alsa>=1.0.17)
if (ALSA_FOUND)
    add_definitions(-D'HAVE_LIBASOUND=1')
endif (ALSA_FOUND)

pkg_check_modules (GTKMM REQUIRED gtkmm-3.0)
pkg_check_modules (SIGC REQUIRED sigc++-2.0)
pkg_check_modules (GLIBMM REQUIRED glibmm-2.4)
pkg_check_modules (CAIROMM REQUIRED cairomm-1.0)
pkg_check_modules (PANGOMM REQUIRED pangomm-1.4)
pkg_check_modules (ATKMM REQUIRED atkmm-1.6)

find_library (LIBRT rt)
if (LIBRT)
    message(STATUS "Found rt in ${LIBRT}")
else (LIBRT)
    message (FATAL_ERROR "rt required but not found")
endif (LIBRT)


if (EnableJackSupport)
    pkg_check_modules (JACK REQUIRED jack>=0.115.6)
    if (JACK_FOUND)
        add_definitions(-D'JACK_SUPPORT=1')
    endif (JACK_FOUND)
endif (EnableJackSupport)

if (EnableMIDIControl)
    add_definitions(-D'MIDI_CONTROL_SUPPORT=1')
endif (EnableMIDIControl)

if (EnableNSM)
    pkg_check_modules(LIBLO liblo>=0.26 REQUIRED)
    if (LIBLO_FOUND)
        add_definitions(-D'LIBLO_SUPPORT=1')
        add_definitions(-D'NSM_SUPPORT=1')
    endif (LIBLO_FOUND)
endif (EnableNSM)

# uninstall target
if(NOT TARGET uninstall)
    configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/cmake_uninstall.cmake.in"
        "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake"
        IMMEDIATE @ONLY)

    add_custom_target(uninstall
        COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake)
endif()

mark_as_advanced (CMAKE_BUILD_TYPE)
mark_as_advanced (CMAKE_INSTALL_PREFIX)
mark_as_advanced (LIBRT)


install(FILES README.md COPYING AUTHORS SEQ42 ChangeLog seq42usr.example
    DESTINATION share/doc/seq42)


if (BuildForDebug)
    set (CMAKE_BUILD_TYPE "Debug")
    set (CMAKE_CXX_FLAGS_DEBUG ${BuildOptionsDebug})
else (BuildForDebug)
    set (CMAKE_BUILD_TYPE "Release")
    set (CMAKE_CXX_FLAGS_RELEASE ${BuildOptionsBasic})
endif (BuildForDebug)

add_subdirectory(src)
add_subdirectory(man)
add_subdirectory(icons)


##Summarize The Full Configuration
message(STATUS)
message(STATUS "=========================================================")
message(STATUS "Build Configuration for Seq42-${PACKAGE_VERSION} for ${CMAKE_SYSTEM_NAME}")
message(STATUS "=========================================================")
message(STATUS)


macro(package_status foundvar pkg)
    if(${foundvar})
        message(STATUS "${pkg} Yes")
    else()
        message(STATUS "${pkg} No")
    endif()
endmacro()


message (STATUS                    "______________________Library____________________________"  )
package_status(PKG_CONFIG_FOUND    "PkgConfig. . . . . . . . . . . . . . . . . . . . . . . .:"  )
package_status(GTKMM_FOUND         "GTKMM-3. . . . . . . . . . . . . . . . . . . . . . . . .:"  )
package_status(ALSA_FOUND          "ALSA MIDI Sequencer support. . . . . . . . . . . . . . .:"  )
package_status(JACK_FOUND          "JACK Audio Connection Kit library. . . . . . . . . . . .:"  )
package_status(SIGC_FOUND          "Libsigc++-2.0. . . . . . . . . . . . . . . . . . . . . .:"  )
package_status(GLIBMM_FOUND        "Glibmm-2.4 . . . . . . . . . . . . . . . . . . . . . . .:"  )
package_status(CAIROMM_FOUND       "Cairomm-1.0. . . . . . . . . . . . . . . . . . . . . . .:"  )
package_status(PANGOMM_FOUND       "Pangomm-1.4. . . . . . . . . . . . . . . . . . . . . . .:"  )
package_status(ATKMM_FOUND         "Atkmm-1.6. . . . . . . . . . . . . . . . . . . . . . . .:"  )

if (EnableNSM)
    package_status(LIBLO_FOUND         "Liblo support. . . . . . . . . . . . . . . . . . . . . .:"  )
endif(EnableNSM)

message (STATUS)
message (STATUS                    "____________________ User Options________________________"  )
package_status(EnableNSM           "NSM session support. . . . . . . . . . . . . . . . . . .:"  )
package_status(EnableMIDIControl   "MIDI transport control . . . . . . . . . . . . . . . . .:"  )
package_status(EnableJackSupport   "Jack transport control . . . . . . . . . . . . . . . . .:"  )

message (STATUS)
message (STATUS)
message (STATUS "Building for ${CMAKE_BUILD_TYPE}")
if (BuildForDebug)
    message (STATUS "Flags: ${CMAKE_CXX_FLAGS_DEBUG}")
else (BuildForDebug)
    message (STATUS "Flags: ${CMAKE_CXX_FLAGS_RELEASE}")
endif (BuildForDebug)
