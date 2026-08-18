@echo off
setlocal
cd /d "%~dp0"
call "%~dp0build.bat" lossless-video-cutter
endlocal
