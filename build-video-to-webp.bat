@echo off
setlocal
cd /d "%~dp0"
call "%~dp0build.bat" video-to-webp
set "EXIT_CODE=%errorlevel%"
endlocal & exit /b %EXIT_CODE%
