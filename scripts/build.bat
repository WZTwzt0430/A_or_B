@echo off
setlocal EnableDelayedExpansion
pushd "%~dp0.."

rem ============================================================
rem  Entry point selection
rem    default : src\main.cpp                (full host: GPL notice, banner)
rem    "ref"   : src\main_dll_reference.cpp  (minimal reference host)
rem  Both link aorb_core.lib: all game logic lives in the DLL.
rem ============================================================
set "APP_SRC=src\main.cpp"
if /i "%~1"=="ref" set "APP_SRC=src\main_dll_reference.cpp"

if not exist "bin\" mkdir "bin"
if errorlevel 1 goto directory_error
if not exist "build\" mkdir "build"
if errorlevel 1 goto directory_error

call "%~dp0_toolchain.bat"
if errorlevel 1 goto compiler_environment_error

rem ============================================================
rem  Stage 1: core DLL
rem    src\aorb_core.cpp  core logic (levels, flow, save)
rem    src\aorb_log.cpp   log sink (forwards to host, writes nothing)
rem    Output: bin\aorb_core.dll
rem            build\aorb_core.lib  (import lib for the EXE)
rem            build\aorb_core.exp  (auto-generated alongside the lib)
rem ============================================================
echo [1/2] building core DLL ...
if exist "bin\aorb_core.dll"   del /q "bin\aorb_core.dll"
if exist "build\aorb_core.lib" del /q "build\aorb_core.lib"
if exist "build\aorb_core.exp" del /q "build\aorb_core.exp"

cl /nologo /EHsc /std:c++23preview /utf-8 /LD ^
   /I"include" /I"third_party" /I"src" ^
   /Fo"build/" /Fe"bin\aorb_core.dll" ^
   /Fd"build\aorb_core.pdb" ^
   "src\aorb_core.cpp" "src\aorb_log.cpp" ^
   /link /IMPLIB:"build\aorb_core.lib"
if errorlevel 1 goto build_error

rem ============================================================
rem  Stage 2: EXE
rem    src\host_logger.cpp is always linked: it is the sole
rem    log writer in this process, and it must survive even if
rem    the core DLL fails to load.
rem    Both entry points link the core import library.
rem ============================================================
echo [2/2] building EXE from %APP_SRC% ...
if exist "bin\A_or_B.exe" del /q "bin\A_or_B.exe"

cl /nologo /EHsc /std:c++23preview /utf-8 ^
   /I"include" /I"third_party" /I"src" ^
   /Fo"build/" /Fe"bin\A_or_B.exe" ^
   /Fd"build\A_or_B.pdb" ^
   "%APP_SRC%" "src\host_logger.cpp" ^
   /link user32.lib "build\aorb_core.lib"
if errorlevel 1 goto build_error

rem ============================================================
rem  Data mirror: data\ -> bin\data\  (mirror, not merge)
rem    data\ now holds runtime save data only; level data is
rem    compiled into the DLL. Skip when there is nothing to copy,
rem    otherwise xcopy complains about an empty directory.
rem ============================================================
if exist "data\*" (
    if exist "bin\data" rmdir /s /q "bin\data"
    xcopy /e /i /y /q "data" "bin\data" >nul
    if errorlevel 1 goto copy_error
)

echo Build succeeded: %CD%\bin\A_or_B.exe
if /i "%~1"=="ref" echo                  %CD%\bin\aorb_core.dll
popd
exit /b 0

:directory_error
echo Failed to create the bin or build directory.
popd
exit /b 1

:compiler_environment_error
echo Failed to initialize the MSVC compiler environment.
echo.
echo No vcvars64.bat was found. Either:
echo   1. Install the "Desktop development with C++" workload for Visual Studio, or
echo   2. Set VCVARS64 to its full path, for example:
echo      set VCVARS64=D:\path\to\BuildTools\VC\Auxiliary\Build\vcvars64.bat
popd
exit /b 1

:build_error
echo Build failed.
popd
exit /b 1

:copy_error
echo Failed to copy the data directory.
popd
exit /b 1
