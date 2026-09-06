@echo off
setlocal
cd /d "%~dp0"
call "%~dp0build.bat" video-speed-changer
exit /b %errorlevel%
