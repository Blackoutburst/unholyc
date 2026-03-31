@echo off
call build-transpiler.bat
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%
call build-std.bat
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%
call build-graphics.bat
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%
