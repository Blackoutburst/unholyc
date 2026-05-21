@echo off
setlocal

cd /d "%~dp0"

if "%~1"=="" (
    set "PREFIX=%USERPROFILE%\.local"
) else (
    set "PREFIX=%~1"
)

if not exist "dist\bin\unholyc.exe" (
    echo Error: dist\bin\unholyc.exe not found. Run build-all.bat first.
    exit /b 1
)

if not exist "%PREFIX%\bin"     mkdir "%PREFIX%\bin"
if not exist "%PREFIX%\lib"     mkdir "%PREFIX%\lib"
if not exist "%PREFIX%\include" mkdir "%PREFIX%\include"

copy /y "dist\bin\unholyc.exe" "%PREFIX%\bin\unholyc.exe"

xcopy /e /y /q "dist\include\" "%PREFIX%\include\"

copy /y "dist\lib\libuhc.a" "%PREFIX%\lib\libuhc.a"
if exist "dist\lib\libuhcgraphics.a" (
    copy /y "dist\lib\libuhcgraphics.a" "%PREFIX%\lib\libuhcgraphics.a"
)

echo Installed to %PREFIX%
echo   bin:     %PREFIX%\bin\unholyc.exe
echo   headers: %PREFIX%\include\
echo   libs:    %PREFIX%\lib\

echo(%PATH% | find /i "%PREFIX%\bin" >nul 2>&1
if errorlevel 1 (
    powershell -NoProfile -Command ^
        "$p = [Environment]::GetEnvironmentVariable('PATH','User');" ^
        "if ($p -notlike '*%PREFIX%\bin*') {" ^
        "  [Environment]::SetEnvironmentVariable('PATH', $p + ';%PREFIX%\bin', 'User');" ^
        "  Write-Host 'Added %PREFIX%\bin to user PATH (restart terminal to apply)';" ^
        "} else {" ^
        "  Write-Host '%PREFIX%\bin already in PATH';" ^
        "}"
) else (
    echo %PREFIX%\bin already in PATH
)

endlocal
