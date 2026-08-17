@echo off
setlocal
cd /d "%~dp0"
set "PROFILE=%~1"
if "%PROFILE%"=="" set "PROFILE=video-compressor"
powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0scripts\smoke-test.ps1" -Profile "%PROFILE%"
if errorlevel 1 (
  echo.
  echo Smoke test failed.
  pause
  exit /b 1
)
echo.
echo Smoke test passed.
pause
endlocal
