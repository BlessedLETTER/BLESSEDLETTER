cmake_minimum_required(VERSION 3.20)

project(ZSchedule VERSION 2.0.0 LANGUAGES C RC)

# 统一使用 UTF-8 源码、Windows GUI 子系统；实际 exe 名在下面设置为「笺」
set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)

# v2.0 三层结构：core 原子 / modules 功能 / app 应用
set(ZS_APP_SOURCES
    src/app/main.c
    src/app/main_window.c
    src/modules/schedule/schedule.c
    src/modules/notes/notes.c
    src/modules/wall/wall.c
    src/modules/week/week.c
    src/modules/editor/editor.c
    src/modules/csv/csv.c
    src/modules/viewer/viewer.c
    src/core/strbuf.c
    src/core/cal.c
    src/core/period.c
    src/core/fmt_time.c
    src/core/fmt_text.c
    src/core/textfile.c
    src/core/paths.c
    src/core/status.c
    src/core/board.c
    src/core/font.c
    src/core/geometry.c
    src/core/style/theme.c
    src/core/media/image.c
    src/core/media/richtext.c
    src/core/media/sound.c
    src/core/style/paper.c
    src/modules/countdown/countdown.c
    src/modules/timetable/timetable.c
)

add_executable(zsched WIN32 ${ZS_APP_SOURCES} zsched.rc)

target_include_directories(zsched PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_compile_definitions(zsched PRIVATE UNICODE _UNICODE)

if(MSVC)
    target_compile_options(zsched PRIVATE /W3 /utf-8)
else()
    target_compile_options(zsched PRIVATE -Wall -Wextra $<$<CONFIG:Release>:-O2>)
endif()

# 资源：清单/图标与 .rc 同目录；out-of-source 时 windres 需要源目录作为 include 路径
if(MSVC)
    set_source_files_properties(zsched.rc PROPERTIES COMPILE_OPTIONS "/I${CMAKE_CURRENT_SOURCE_DIR}")
else()
    set_source_files_properties(zsched.rc PROPERTIES COMPILE_OPTIONS "-I${CMAKE_CURRENT_SOURCE_DIR}")
endif()
set_property(SOURCE zsched.rc PROPERTY OBJECT_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/zsched.manifest;${CMAKE_CURRENT_SOURCE_DIR}/app.ico")

set_target_properties(zsched PROPERTIES
    OUTPUT_NAME "zsched"
    WIN32_EXECUTABLE TRUE
)

# MinGW Makefiles 的 makefile 是 ANSI 编码，直接 OUTPUT_NAME "笺" 会写成乱码文件名。
# 先生成 zsched.exe，再用 PowerShell 以 Unicode 码点 0x7B3A（笺）改名到工程根目录，
# 保持“数据在 exe 同目录”的既有行为。
add_custom_command(TARGET zsched POST_BUILD
    COMMAND powershell -NoProfile -ExecutionPolicy Bypass -Command
        "Move-Item -LiteralPath '$<TARGET_FILE:zsched>' -Destination ('${CMAKE_CURRENT_SOURCE_DIR}/' + [string][char]0x7B3A + '.exe') -Force"
    VERBATIM
)

target_link_libraries(zsched PRIVATE
    comctl32 gdi32 comdlg32 shell32 gdiplus ole32 shlwapi user32
    winmm mfplat mf mfuuid advapi32
)

# 测试辅助工具（可选）：代替已删除的 _tools/build_tools.bat
option(ZSCHED_BUILD_TEST_TOOLS "Build _tools helper executables (pwin/pmsg/pmouse/pmenu)" OFF)
if(ZSCHED_BUILD_TEST_TOOLS)
    foreach(zs_tool pwin pmsg pmouse pmenu)
        add_executable(${zs_tool} "_tools/${zs_tool}.c")
        target_link_libraries(${zs_tool} PRIVATE user32)
        set_target_properties(${zs_tool} PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/_tools/bin")
    endforeach()
endif()
