@echo off
setlocal EnableDelayedExpansion

set "PF64=%ProgramFiles%"
set "PF86=%ProgramFiles(x86)%"
set "VCVARS="

if defined VCVARS64 (
    if exist "!VCVARS64!" (
        set "VCVARS=!VCVARS64!"
    ) else (
        echo [toolchain] VCVARS64 is set but the file does not exist:
        echo             !VCVARS64!
        endlocal & exit /b 1
    )
)

if not defined VCVARS (
    set "VSWHERE=!PF86!\Microsoft Visual Studio\Installer\vswhere.exe"
    if exist "!VSWHERE!" (
        for /f "usebackq delims=" %%i in (`"!VSWHERE!" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath 2^>nul`) do (
            if exist "%%i\VC\Auxiliary\Build\vcvars64.bat" set "VCVARS=%%i\VC\Auxiliary\Build\vcvars64.bat"
        )
    )
)

if not defined VCVARS (
    for %%r in ("!PF64!" "!PF86!") do (
        if not defined VCVARS (
            for /d %%v in ("%%~r\Microsoft Visual Studio\*") do (
                if not defined VCVARS (
                    for /d %%e in ("%%v\*") do (
                        if not defined VCVARS if exist "%%e\VC\Auxiliary\Build\vcvars64.bat" set "VCVARS=%%e\VC\Auxiliary\Build\vcvars64.bat"
                    )
                )
            )
        )
    )
)

if not defined VCVARS (
    echo [toolchain] no vcvars64.bat found
    endlocal & exit /b 1
)

echo [toolchain] !VCVARS!
endlocal & call "%VCVARS%" >nul
exit /b %errorlevel%
