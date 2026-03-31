@echo off
setlocal enabledelayedexpansion

set OUT=out_std
set DIST=dist

dist/bin/unholyc uhcstd %OUT%

set INCLUDES=-I"%OUT%"
for /d /r "%OUT%" %%d in (*) do set INCLUDES=!INCLUDES! -I"%%d"

mkdir "%OUT%\obj" 2>nul
for /r "%OUT%" %%f in (*.cc) do (
    clang++ -std=c++17 -c !INCLUDES! "%%f" -o "%OUT%\obj\%%~nf.o"
)

mkdir "%DIST%\lib" 2>nul
llvm-ar rcs "%DIST%\lib\libuhc.a" "%OUT%\obj\*.o"

mkdir "%DIST%\include" 2>nul
for /r "%OUT%" %%f in (*.hh) do (
    copy "%%f" "%DIST%\include\%%~nxf" >nul
)

rmdir /s /q "%OUT%"
echo uhcstd -^> %DIST%
