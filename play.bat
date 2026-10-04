@echo off
rem Starts the recompiled PGR3. The settings window (game folder, language, resolution,
rem fullscreen) appears first; hold Shift while starting to show it again after hiding it.
rem   play.bat [game folder] [extra options...]
rem Log: pgr3.log, crash report: pgr3_crash.txt (both next to this file).
setlocal
cd /d "%~dp0"

set "EXE=%~dp0out\build\win-amd64-release\pgr3.exe"
if not exist "%EXE%" (
  echo pgr3.exe not found. Build it first with tools\build.bat
  pause
  exit /b 1
)

set "ARGS="
if not "%~1"=="" (
  set ARGS=--game_data_root="%~1"
  shift
)
:collect
if "%~1"=="" goto run
set ARGS=%ARGS% %1
shift
goto collect

:run
start "" "%EXE%" --log_file="%~dp0pgr3.log" %ARGS%
