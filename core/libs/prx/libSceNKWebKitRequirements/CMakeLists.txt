include(ExternalProject)
set(nkwebkit_jpeg_prefix "${CMAKE_BINARY_DIR}/libjpeg-nkwebkit")
set(nkwebkit_jpeg_library "${nkwebkit_jpeg_prefix}/lib/libjpeg.a")
set(nkwebkit_jpeg_c_flags "")
if(WIN32)
    set(nkwebkit_jpeg_c_flags "-D__RPCNDR_H__ -Dboolean=int")
endif()
file(MAKE_DIRECTORY "${nkwebkit_jpeg_prefix}/include")
ExternalProject_Add(nkwebkit_jpeg_external
    SOURCE_DIR "${CMAKE_SOURCE_DIR}/3rdparty/libjpeg-turbo"
    BINARY_DIR "${CMAKE_BINARY_DIR}/libjpeg-nkwebkit-build"
    INSTALL_DIR "${nkwebkit_jpeg_prefix}"
    CMAKE_ARGS
        -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
        -DCMAKE_INSTALL_PREFIX=${nkwebkit_jpeg_prefix}
        -DBUILD_SHARED_LIBS=OFF
        -DENABLE_STATIC=ON
        -DENABLE_SHARED=OFF
        -DWITH_SIMD=OFF
        -DWITH_TURBOJPEG=OFF
        -DWITH_TOOLS=OFF
        -DWITH_TESTS=OFF
        -DCMAKE_INSTALL_LIBDIR=lib
        -DCMAKE_C_FLAGS=${nkwebkit_jpeg_c_flags}
        -DCMAKE_C_VISIBILITY_PRESET=hidden
        -DCMAKE_POSITION_INDEPENDENT_CODE=ON
        -DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}
        -DCMAKE_MAKE_PROGRAM=${CMAKE_MAKE_PROGRAM}
        -DCMAKE_C_COMPILER_LAUNCHER=${CMAKE_C_COMPILER_LAUNCHER}
    BUILD_BYPRODUCTS "${nkwebkit_jpeg_library}"
)
add_library(nkwebkit_jpeg STATIC IMPORTED GLOBAL)
set_target_properties(nkwebkit_jpeg PROPERTIES
    IMPORTED_LOCATION "${nkwebkit_jpeg_library}"
    INTERFACE_INCLUDE_DIRECTORIES "${nkwebkit_jpeg_prefix}/include"
)
add_dependencies(nkwebkit_jpeg nkwebkit_jpeg_external)

add_library(libSceNKWebKitRequirements SHARED EXCLUDE_FROM_ALL Export.cpp Jpeg.cpp Zlib.cpp)
target_include_directories(libSceNKWebKitRequirements PRIVATE ${LIBS_INCLUDE_DIR})
set_target_properties(libSceNKWebKitRequirements PROPERTIES CXX_EXTENSIONS OFF)
configure_windows_unwind(libSceNKWebKitRequirements)
target_link_libraries(libSceNKWebKitRequirements PRIVATE zlibstatic nkwebkit_jpeg)
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

    add_test_executable(guest_webkit_jpeg_tests ${LIBS_INCLUDE_DIR}/tests/GuestWebKitJpeg.cpp)
    target_include_directories(guest_webkit_jpeg_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_webkit_jpeg_tests PRIVATE libSceNKWebKitRequirements libc nkwebkit_jpeg)
    configure_windows_unwind(guest_webkit_jpeg_tests)
    add_test(NAME guest_webkit_jpeg COMMAND guest_webkit_jpeg_tests)
    set_tests_properties(guest_webkit_jpeg PROPERTIES TIMEOUT 30)
    if(WIN32)
        set_tests_properties(guest_webkit_jpeg PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceNKWebKitRequirements>")
    endif()
endif()
