@echo off
setlocal

mkdir dist\bin 2>nul

echo Compiling transpiler...
clang++ -std=c++17 -O2 -Wall -Wextra -Werror -o dist\bin\unholyc.exe transpiler.cpp
if %ERRORLEVEL% neq 0 (
    echo FAILED
    exit /b 1
)

echo unholyc -^> dist\bin\unholyc.exe
