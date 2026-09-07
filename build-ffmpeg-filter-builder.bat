@echo off
setlocal
cd /d "%~dp0"
call "%~dp0build.bat" ffmpeg-filter-builder
exit /b %errorlevel%
