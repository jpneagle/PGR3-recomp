@echo off
rem Regenerates the recompiled C++ (if inputs changed) and builds pgr3.exe at low priority with 2 jobs.
rem   tools\build.bat [release|relwithdebinfo]
setlocal
set "CFG=%~1"
if "%CFG%"=="" set "CFG=release"
cd /d "%~dp0.."
set "SDK=%~dp0..\..\ext\rexglue-sdk\out\install\win-amd64"
"%SDK%\bin\rexglue.exe" codegen pgr3_manifest.toml || exit /b 1
if not exist out\build\win-amd64-%CFG%\build.ninja call "%~dp0env.bat" cmake --preset win-amd64-%CFG% || exit /b 1
start "" /belownormal /b /wait cmd /c ""%~dp0env.bat" cmake --build out\build\win-amd64-%CFG% -j 2"
rem The SDK DLLs are only staged when pgr3.exe relinks; keep them current after an SDK rebuild.
copy /y "%SDK%\bin\rex*.dll" "out\build\win-amd64-%CFG%\" >nul
