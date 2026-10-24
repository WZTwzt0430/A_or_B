@echo off
setlocal
pushd "%~dp0.."

rem ============================================================
rem  Build the installer: dist\A_or_B-<version>-setup.exe
rem
rem    bin\A_or_B.exe
rem    bin\aorb_core.dll
rem    installer\Readme.txt  -> payload\readme (game introduction)
rem        -> build\payload.zip -> embedded into the setup exe
rem
rem  Requires: the app already built (scripts\build.bat)
rem ============================================================

set "CSC=%WINDIR%\Microsoft.NET\Framework64\v4.0.30319\csc.exe"
if not exist "%CSC%" set "CSC=%WINDIR%\Microsoft.NET\Framework\v4.0.30319\csc.exe"
if not exist "%CSC%" goto no_csc

if not exist "bin\A_or_B.exe" goto no_app
if not exist "bin\aorb_core.dll" goto no_app

if not exist "build\" mkdir "build"
if not exist "dist\"  mkdir "dist"

echo [package] packing payload ...
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0make_payload.ps1"
if errorlevel 1 goto payload_error

set "OUT=dist\A_or_B-2.0.0-setup.exe"
if exist "%OUT%" del /q "%OUT%"

echo [package] compiling installer ...
"%CSC%" /nologo /target:winexe /platform:anycpu /codepage:65001 ^
    /out:"%OUT%" ^
    /win32manifest:"installer\setup.manifest" ^
    /reference:System.dll ^
    /reference:System.Drawing.dll ^
    /reference:System.IO.Compression.dll ^
    /reference:System.Windows.Forms.dll ^
    /resource:"build\payload.zip",payload.zip ^
    "installer\Setup.cs"
if errorlevel 1 goto compile_error

echo [package] done: %CD%\%OUT%
popd
exit /b 0

:no_csc
echo [package] csc.exe not found. Install the .NET Framework 4.x.
popd
exit /b 1

:no_app
echo [package] bin\A_or_B.exe or bin\aorb_core.dll missing.
echo [package] Run scripts\build.bat first.
popd
exit /b 1

:payload_error
echo [package] failed to build payload.zip
popd
exit /b 1

:compile_error
echo [package] compile failed.
popd
exit /b 1
