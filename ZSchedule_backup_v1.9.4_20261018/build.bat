@echo off
rem ===========================================================================
rem  ZSchedule 构建脚本 (Windows)
rem  优先使用 MinGW-w64 / w64devkit 的 gcc；找不到时退回 MSVC 的 cl.exe
rem ===========================================================================
setlocal enabledelayedexpansion
cd /d "%~dp0"

set OUT=笺.exe
set SRC=main.c model.c image.c richtext.c weekview.c imgview.c stickies.c noteview.c paper.c ui_main.c ui_edit.c csvimport.c sound.c
set RC=zsched.rc
set RES=zsched_res.o
set LIBS=-lcomctl32 -lgdi32 -lcomdlg32 -lshell32 -lmsimg32 -lgdiplus -lole32 -lshlwapi -luser32 -lwinmm -lmfplat -lmf -lmfuuid

rem 若项目自带便携工具链（tools\w64devkit），优先使用它
if exist "%~dp0..\tools\w64devkit\bin\gcc.exe" set PATH=%~dp0..\tools\w64devkit\bin;%PATH%

where gcc >nul 2>nul
if %errorlevel%==0 goto mingw
where cl >nul 2>nul
if %errorlevel%==0 goto msvc

echo [错误] 未找到 C 编译器。
echo         请安装 MinGW-w64（推荐 https://github.com/skeeto/w64devkit/releases）
echo         并确保 gcc.exe 位于 PATH 中；或使用 Visual Studio 开发人员命令提示符。
exit /b 1

:mingw
echo [1/3] 编译资源 (windres) ...
set HAVE_RC=0
where windres >nul 2>nul
if %errorlevel%==0 (
    windres -i %RC% -o %RES%
    if !errorlevel!==0 set HAVE_RC=1
)
if "%HAVE_RC%"=="0" echo       [跳过] 未找到 windres，不嵌入清单（界面退回经典外观）。

echo [2/3] 编译 C 源码 (gcc) ...
if "%HAVE_RC%"=="1" (
    gcc -O2 -Wall -Wextra -DUNICODE -D_UNICODE -o %OUT% %SRC% %RES% -mwindows %LIBS%
) else (
    gcc -O2 -Wall -Wextra -DUNICODE -D_UNICODE -o %OUT% %SRC% -mwindows %LIBS%
)
if errorlevel 1 goto fail
goto ok

:msvc
echo [1/3] 编译资源 (rc) ...
rc /nologo /fo zsched.res %RC% >nul 2>nul
echo [2/3] 编译 C 源码 (cl) ...
cl /nologo /O2 /W3 /utf-8 /DUNICODE /D_UNICODE /Fe%OUT% %SRC% zsched.res ^
   /link /SUBSYSTEM:WINDOWS comctl32.lib gdi32.lib comdlg32.lib shell32.lib msimg32.lib gdiplus.lib user32.lib
if errorlevel 1 goto fail
del *.obj >nul 2>nul
goto ok

:ok
echo [3/3] 构建完成： %OUT%
echo.
echo   直接运行 笺.exe；数据保存在 exe 同目录的 zsched_data.txt
exit /b 0

:fail
echo.
echo [失败] 编译出错，请把上面的错误信息发给我。
exit /b 1
