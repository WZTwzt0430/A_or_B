@echo off
rem ============================================================
rem  A_or_B - one-click build
rem
rem  Just double-click this file. It will:
rem    1. locate vcvars64.bat (VCVARS64 env var / vswhere / common paths)
rem    2. build bin\A_or_B.exe + bin\aorb_core.dll
rem
rem  Optional arguments:
rem    /nopause        do not wait for a key press at the end
rem    ref             build the minimal reference host instead
rem ============================================================
setlocal
pushd "%~dp0"

call "scripts\build.bat" %*
set "RC=%ERRORLEVEL%"

echo.
if not "%RC%"=="0" (
    echo  Build FAILED. See the messages above.
) else (
    echo  Build succeeded.
    echo  Run it with:  bin\A_or_B.exe
)

popd

if /i not "%~1"=="/nopause" (
    echo.
    pause
)
exit /b %RC%
