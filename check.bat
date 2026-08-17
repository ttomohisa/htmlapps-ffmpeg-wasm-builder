@echo off
setlocal
cd /d "%~dp0"
powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0scripts\check-repository.ps1"
if errorlevel 1 (echo.&echo Check failed.&pause&exit /b 1)
echo.&echo Checks passed.&pause
endlocal
