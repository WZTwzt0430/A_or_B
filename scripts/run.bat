@echo off
setlocal
set "ROOT=%~dp0.."

call "%ROOT%\scripts\build.bat" %*
if errorlevel 1 exit /b 1

pushd "%ROOT%"
"bin\A_or_B.exe"
set "RUN_ERROR=%ERRORLEVEL%"
popd
exit /b %RUN_ERROR%
