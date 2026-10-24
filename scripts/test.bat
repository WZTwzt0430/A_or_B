@echo off
setlocal EnableDelayedExpansion
pushd "%~dp0.."

call "%~dp0build.bat"
if errorlevel 1 (
    echo [test] build failed, aborting.
    popd & exit /b 1
)

rem build.bat exited, so its setlocal scope is gone and the MSVC
rem environment went with it. Reload it for the smoke-test compile.
call "%~dp0_toolchain.bat"
if errorlevel 1 (
    echo [test] cannot initialize the compiler environment.
    popd & exit /b 1
)

if not exist "bin\" mkdir "bin"
if not exist "build\" mkdir "build"

echo [test] building smoke test ...
if exist "bin\smoke_test.exe" del /q "bin\smoke_test.exe"

cl /nologo /EHsc /std:c++23preview /utf-8 ^
   /I"include" /I"third_party" /I"src" ^
   /Fo"build/" /Fe"bin\smoke_test.exe" ^
   "tests\smoke_test.cpp" "src\host_logger.cpp" ^
   /link "build\aorb_core.lib"
if errorlevel 1 (
    echo [test] compile failed.
    popd & exit /b 1
)

echo [test] running ...
"bin\smoke_test.exe"
set "RC=%ERRORLEVEL%"

if exist "build\smoke_save.sav"  del /q "build\smoke_save.sav"
if exist "build\smoke_save.bak"  del /q "build\smoke_save.bak"
if exist "build\smoke_save.json" del /q "build\smoke_save.json"

echo [test] exit code = %RC%
popd & exit /b %RC%
