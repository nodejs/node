@echo off

REM -- Script requirements:
REM --
REM --  * git               This program must be in the Path to check out
REM --                      build-gyp.  If that directory already exists, then
REM --                      git isn't necessary, but if it is missing, no
REM --                      commit hash will be embedded into binaries.
REM --
REM --  * python            A non-Cygwin Python 2 python.exe must be in the
REM --                      Path to run gyp.
REM --
REM --  * msbuild           msbuild must be in the Path.  It is probably
REM --                      important to have msbuild from the correct MSVC
REM --                      release.
REM --
REM -- The script's output binaries are in the src/Release/{Win32,x64}
REM -- directory.

REM -------------------------------------------------------------------------
REM -- Parse arguments

setlocal
cd %~dp0
set GYP_ARGS=
set MSVC_PLATFORM=x64

:ParamLoop
if "%1" == "" goto :ParamDone
if "%1" == "--msvc-platform" (
    REM -- One of Win32 or x64.
    set MSVC_PLATFORM=%2
    shift && shift
    goto :ParamLoop
)
if "%1" == "--gyp-msvs-version" (
    set GYP_ARGS=%GYP_ARGS% -G msvs_version=%2
    shift && shift
    goto :ParamLoop
)
if "%1" == "--toolset" (
    set GYP_ARGS=%GYP_ARGS% -D WINPTY_MSBUILD_TOOLSET=%2
    shift && shift
    goto :ParamLoop
)
if "%1" == "--commit-hash" (
    set GYP_ARGS=%GYP_ARGS% -D WINPTY_COMMIT_HASH=%2
    shift && shift
    goto :ParamLoop
)
echo error: Unrecognized argument: %1
exit /b 1
:ParamDone

REM -------------------------------------------------------------------------
REM -- Check out GYP.  GYP doesn't seem to have releases, so just use the
REM -- current master commit.

if not exist build-gyp (
    git clone https://chromium.googlesource.com/external/gyp build-gyp || (
        echo error: GYP clone failed
        exit /b 1
    )
)

REM -------------------------------------------------------------------------
REM -- Run gyp to generate MSVC project files.

cd src

call ..\build-gyp\gyp.bat winpty.gyp -I configurations.gypi %GYP_ARGS%
if errorlevel 1 (
    echo error: GYP failed
    exit /b 1
)

REM -------------------------------------------------------------------------
REM -- Compile the project.

msbuild winpty.sln /m /p:Platform=%MSVC_PLATFORM% || (
    echo error: msbuild failed
    exit /b 1
)
