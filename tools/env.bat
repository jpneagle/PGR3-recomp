@echo off
rem Runs a command with MSVC (headers/libs), LLVM clang, CMake and Ninja on PATH.
rem   tools\env.bat <command> [args...]
rem Override locations with PGR3_LLVM / PGR3_TOOLS / PGR3_VCVARS.
if not defined PGR3_LLVM set "PGR3_LLVM=%~dp0..\..\ext\llvm"
if not defined PGR3_TOOLS set "PGR3_TOOLS=%~dp0..\..\..\xbox_redump\kt_recomp\deps"
if not defined PGR3_VCVARS set "PGR3_VCVARS=C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
if not defined VCToolsInstallDir call "%PGR3_VCVARS%" >nul
set "PATH=%PGR3_LLVM%\bin;%PGR3_TOOLS%\cmake\bin;%PGR3_TOOLS%\ninja;%PATH%"
%*
