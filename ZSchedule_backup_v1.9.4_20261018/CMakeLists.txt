cmake_minimum_required(VERSION 3.21)

project(ZSchedule VERSION 1.9.4 LANGUAGES C)

if(NOT WIN32)
    message(FATAL_ERROR
        "ZSchedule requires a Windows target. Build on Windows or configure "
        "a Windows cross-compilation toolchain with CMAKE_TOOLCHAIN_FILE.")
endif()

enable_language(RC)

if(NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build configuration" FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS
        Debug Release RelWithDebInfo MinSizeRel)
endif()

option(ZSCHEDULE_BUILD_TOOLS "Build the Win32 window inspection/input tools" OFF)

function(zschedule_configure_target target)
    set_target_properties(${target} PROPERTIES
        C_STANDARD 11
        C_STANDARD_REQUIRED YES
        C_EXTENSIONS NO
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<CONFIG>"
    )
    target_compile_definitions(${target} PRIVATE UNICODE _UNICODE)
    if(MSVC)
        target_compile_options(${target} PRIVATE
            "$<$<COMPILE_LANGUAGE:C>:/W4;/utf-8>")
        set_property(TARGET ${target} PROPERTY MSVC_RUNTIME_LIBRARY
            "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    else()
        target_compile_options(${target} PRIVATE
            "$<$<COMPILE_LANGUAGE:C>:-Wall;-Wextra;-finput-charset=UTF-8>")
    endif()
endfunction()

add_executable(zschedule WIN32
    main.c
    model.c
    image.c
    richtext.c
    weekview.c
    imgview.c
    stickies.c
    noteview.c
    paper.c
    ui_main.c
    ui_edit.c
    csvimport.c
    sound.c
    zsched.h
    zsched.rc
)

zschedule_configure_target(zschedule)
set_target_properties(zschedule PROPERTIES OUTPUT_NAME "笺")

# Resource filenames in zsched.rc are relative to the source directory.
target_include_directories(zschedule PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
set_property(SOURCE zsched.rc PROPERTY OBJECT_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/zsched.manifest"
    "${CMAKE_CURRENT_SOURCE_DIR}/app.ico"
)

target_link_libraries(zschedule PRIVATE
    comctl32
    gdi32
    comdlg32
    shell32
    msimg32
    gdiplus
    ole32
    shlwapi
    user32
    advapi32
    winmm
    mfplat
    mf
    mfuuid
)

if(MSVC)
    # zsched.rc already embeds the application manifest as resource 1.
    target_link_options(zschedule PRIVATE /MANIFEST:NO)
endif()

if(ZSCHEDULE_BUILD_TOOLS)
    foreach(tool IN ITEMS pwin pmsg pmouse pmenu)
        add_executable(${tool} "_tools/${tool}.c")
        zschedule_configure_target(${tool})
        target_link_libraries(${tool} PRIVATE user32)
    endforeach()
endif()
