@echo off
setlocal
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0android\tools\Setup-Android.ps1" %*
set "result=%errorlevel%"
if not "%result%"=="0" echo Setup stopped. See the message above; rerun this guide after correcting it.
pause
exit /b %result%
