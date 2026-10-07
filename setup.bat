@echo off
rem Svencraft setup: builds the engine and the game, generates the assets and the sandbox map (tools\setup.py).
rem   setup.bat            whatever is missing     setup.bat --rebuild   everything again     setup.bat --check   only check
setlocal
cd /d "%~dp0"
set "PY="
where py >nul 2>nul && set "PY=py -3"
if not defined PY (
  where python >nul 2>nul && set "PY=python"
)
if not defined PY (
  echo Python 3 is not installed: get it from https://www.python.org/downloads/ ^(tick "Add python.exe to PATH"^),
  echo then run setup.bat again.
  pause
  exit /b 1
)
%PY% tools\setup.py %*
set RC=%ERRORLEVEL%
echo.
pause
exit /b %RC%
