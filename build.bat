@echo off
setlocal
cd /d "%~dp0"
set "PROFILE=%~1"
if "%PROFILE%"=="" set "PROFILE=video-compressor"

echo.
echo FFmpeg WASM Builder
echo Profile: %PROFILE%
echo Build + browser smoke test
echo.

powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0scripts\build.ps1" -Profile "%PROFILE%"
if errorlevel 1 (
  echo.
  echo Build or smoke test failed. Check the error above.
  pause
  exit /b 1
)

echo.
echo Build and smoke test completed.
echo Output: dist\%PROFILE%\
pause
endlocal
