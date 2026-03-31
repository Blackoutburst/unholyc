@echo off
setlocal enabledelayedexpansion

set OUT=out_graphics
set DIST=dist

set I_UHC=%DIST%\include
set I_VK=C:\VulkanSDK\1.4.341.1\Include
set I_GLFW=C:\GLFW\include

dist/bin/unholyc -I"%I_UHC%" graphics %OUT%

mkdir "%OUT%\obj" 2>nul
for /r "%OUT%\src" %%f in (*.cc) do (
    if /i not "%%~nf"=="main" (
        c++ -std=c++17 -c ^
            -I"%OUT%\include" ^
            -I"%I_UHC%" ^
            -I"%I_VK%" ^
            -I"%I_GLFW%" ^
            "%%f" -o "%OUT%\obj\%%~nf.o"
    )
)

mkdir "%DIST%\lib" 2>nul
llvm-ar rcs "%DIST%\lib\libuhcgraphics.a" "%OUT%\obj\*.o"

mkdir "%DIST%\include" 2>nul
xcopy /s /y "%OUT%\include\*" "%DIST%\include\" >nul

rmdir /s /q "%OUT%"
echo graphics -^> %DIST%
