set(nkwebkit_sqlite_source "${CMAKE_SOURCE_DIR}/3rdparty/sqlite")
set(nkwebkit_sqlite_generated "${CMAKE_CURRENT_BINARY_DIR}/sqlite")
file(MAKE_DIRECTORY "${nkwebkit_sqlite_generated}")
add_executable(nkwebkit_sqlite_jimsh EXCLUDE_FROM_ALL "${nkwebkit_sqlite_source}/autosetup/jimsh0.c")
if(WIN32)
    target_compile_definitions(nkwebkit_sqlite_jimsh PRIVATE HAVE__FULLPATH)
else()
    target_compile_definitions(nkwebkit_sqlite_jimsh PRIVATE HAVE_REALPATH)
endif()
add_executable(nkwebkit_sqlite_lemon EXCLUDE_FROM_ALL "${nkwebkit_sqlite_source}/tool/lemon.c")
add_executable(nkwebkit_sqlite_keywordhash EXCLUDE_FROM_ALL "${nkwebkit_sqlite_source}/tool/mkkeywordhash.c")
add_executable(nkwebkit_sqlite_sourceid EXCLUDE_FROM_ALL "${nkwebkit_sqlite_source}/tool/mksourceid.c")
foreach(tool nkwebkit_sqlite_jimsh nkwebkit_sqlite_lemon nkwebkit_sqlite_keywordhash nkwebkit_sqlite_sourceid)
    set_target_properties(${tool} PROPERTIES C_STANDARD 99)
endforeach()
add_custom_command(
    OUTPUT "${nkwebkit_sqlite_generated}/sqlite3.c" "${nkwebkit_sqlite_generated}/sqlite3.h"
    COMMAND ${CMAKE_COMMAND}
        "-DTOP=${nkwebkit_sqlite_source}"
        "-DOUT=${nkwebkit_sqlite_generated}"
        "-DJIMSH=$<TARGET_FILE:nkwebkit_sqlite_jimsh>"
        "-DLEMON=$<TARGET_FILE:nkwebkit_sqlite_lemon>"
        "-DKEYWORDHASH=$<TARGET_FILE:nkwebkit_sqlite_keywordhash>"
        "-DSOURCEID=$<TARGET_FILE:nkwebkit_sqlite_sourceid>"
        -P "${CMAKE_SOURCE_DIR}/cmake/SqliteAmalgamation.cmake"
    DEPENDS nkwebkit_sqlite_jimsh nkwebkit_sqlite_lemon nkwebkit_sqlite_keywordhash nkwebkit_sqlite_sourceid
        "${CMAKE_SOURCE_DIR}/cmake/SqliteAmalgamation.cmake" "${nkwebkit_sqlite_source}/VERSION" "${nkwebkit_sqlite_source}/manifest"
    VERBATIM
)
add_library(nkwebkit_sqlite STATIC EXCLUDE_FROM_ALL "${nkwebkit_sqlite_generated}/sqlite3.c")
target_include_directories(nkwebkit_sqlite PUBLIC "${nkwebkit_sqlite_generated}")
target_compile_definitions(nkwebkit_sqlite PRIVATE SQLITE_THREADSAFE=1 SQLITE_OMIT_LOAD_EXTENSION)
set_target_properties(nkwebkit_sqlite PROPERTIES C_VISIBILITY_PRESET hidden POSITION_INDEPENDENT_CODE ON)
if(NOT WIN32)
    target_link_libraries(nkwebkit_sqlite PRIVATE ${CMAKE_DL_LIBS} m)
endif()

add_library(libSceNKWebKitRequirements SHARED EXCLUDE_FROM_ALL Export.cpp Sqlite.cpp Zlib.cpp)
target_include_directories(libSceNKWebKitRequirements PRIVATE ${LIBS_INCLUDE_DIR}
    ${CMAKE_SOURCE_DIR}/3rdparty/libjpeg-turbo/src/spng/zlib)
set_target_properties(libSceNKWebKitRequirements PROPERTIES CXX_EXTENSIONS OFF)
configure_windows_unwind(libSceNKWebKitRequirements)
target_link_libraries(libSceNKWebKitRequirements PRIVATE jpeg_turbo nkwebkit_sqlite)
if(MINGW OR CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_link_options(libSceNKWebKitRequirements PRIVATE LINKER:--exclude-libs,ALL)
endif()

if(BUILD_TESTING)
    add_test_executable(guest_webkit_zlib_tests ${LIBS_INCLUDE_DIR}/tests/GuestWebKitZlib.cpp)
    target_include_directories(guest_webkit_zlib_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_webkit_zlib_tests PRIVATE libSceNKWebKitRequirements libc)
    configure_windows_unwind(guest_webkit_zlib_tests)
    add_test(NAME guest_webkit_zlib COMMAND guest_webkit_zlib_tests)
    set_tests_properties(guest_webkit_zlib PROPERTIES TIMEOUT 30)
    if(WIN32)
        set_tests_properties(guest_webkit_zlib PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceNKWebKitRequirements>")
    endif()

    add_test_executable(guest_webkit_sqlite_tests ${LIBS_INCLUDE_DIR}/tests/GuestWebKitSqlite.cpp)
    target_include_directories(guest_webkit_sqlite_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_webkit_sqlite_tests PRIVATE libSceNKWebKitRequirements libc)
    configure_windows_unwind(guest_webkit_sqlite_tests)
    add_test(NAME guest_webkit_sqlite COMMAND guest_webkit_sqlite_tests)
    set_tests_properties(guest_webkit_sqlite PROPERTIES TIMEOUT 30)
    if(WIN32)
        set_tests_properties(guest_webkit_sqlite PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceNKWebKitRequirements>")
    endif()
endif()
