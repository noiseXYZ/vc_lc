@echo off
rem Builds build\vc_lc.asi. Run it from a "x86 Native Tools Command Prompt for VS"
rem (or set VCVARS to your vcvars32.bat). Needs CMake and Ninja (both come with Visual Studio).
if defined VCVARS call "%VCVARS%" >nul || exit /b 1
cd /d "%~dp0"
if not exist build\build.ninja cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release || exit /b 1
cmake --build build
