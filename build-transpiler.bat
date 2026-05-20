@echo off
setlocal

mkdir dist\bin 2>nul

set COMMIT=dev
for /f %%i in ('git rev-parse --short HEAD 2^>nul') do set COMMIT=%%i

echo Compiling transpiler...
c++ -std=c++17 -O2 -Wall -Wextra -Werror -DUHC_COMMIT=%COMMIT% -o dist\bin\unholyc.exe transpiler.cpp
if %ERRORLEVEL% neq 0 (
    echo FAILED
    exit /b 1
)

echo unholyc -^> dist\bin\unholyc.exe
