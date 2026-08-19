@echo off
setlocal
cd /d "%~dp0"
call "%~dp0build.bat" video-contact-sheet
set "EXIT_CODE=%errorlevel%"
endlocal & exit /b %EXIT_CODE%
