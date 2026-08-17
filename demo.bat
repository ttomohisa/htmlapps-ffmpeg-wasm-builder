@echo off
setlocal
cd /d "%~dp0"
set "PROFILE=%~1"
if "%PROFILE%"=="" set "PROFILE=video-compressor"

if not exist "dist\%PROFILE%\ffmpeg.wasm.gz" (
  echo Build output was not found. Run build.bat first.
  pause
  exit /b 1
)

powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0scripts\pack-single-html.ps1" -Profile "%PROFILE%"
if errorlevel 1 (
  echo Packaging failed.
  pause
  exit /b 1
)

start "" "%~dp0dist\single-html-%PROFILE%.html"
endlocal
