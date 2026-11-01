@echo off
rem Build helper tools for testing (not part of the product; binaries go to _tools\bin)
setlocal
cd /d "%~dp0"
set "PATH=%~dp0..\..\tools\w64devkit\bin;%PATH%"
if not exist bin mkdir bin
for %%f in (pwin pmsg pmouse pmenu) do (
    gcc -O2 -o bin\%%f.exe %%f.c -luser32 || exit /b 1
)
echo built:
dir /b bin
