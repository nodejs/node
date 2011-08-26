@echo off

cd %~dp0

if /i "%1"=="help" goto help
if /i "%1"=="--help" goto help
if /i "%1"=="-help" goto help
if /i "%1"=="/help" goto help
if /i "%1"=="?" goto help
if /i "%1"=="-?" goto help
if /i "%1"=="--?" goto help
if /i "%1"=="/?" goto help

@rem Process arguments.
set config=
set target=Build
set noprojgen=
set nobuild=
set run=

:next-arg
if "%1"=="" goto args-done
if /i "%1"=="debug"        set config=Debug&goto arg-ok
if /i "%1"=="release"      set config=Release&goto arg-ok
if /i "%1"=="test"         set run=run-tests.exe&goto arg-ok
if /i "%1"=="bench"        set run=run-benchmarks.exe&goto arg-ok
if /i "%1"=="clean"        set target=Clean&goto arg-ok
if /i "%1"=="noprojgen"    set noprojgen=1&goto arg-ok
if /i "%1"=="nobuild"      set nobuild=1&goto arg-ok
:arg-ok
shift
goto next-arg
:args-done

if not "%config%"=="" goto project-gen
if "%run%"=="run-tests.exe" set config=Debug& goto project-gen
if "%run%"=="run-benchmarks.exe" set config=Release& goto project-gen
set config=Debug

:project-gen
@rem Skip project generation if requested.
if defined noprojgen goto msbuild

@rem Generate the VS project.

if exist build\gyp goto have_gyp
echo svn co http://gyp.googlecode.com/svn/trunk@983 build/gyp
svn co http://gyp.googlecode.com/svn/trunk@983 build/gyp
if errorlevel 1 goto gyp_install_failed
goto have_gyp

:gyp_install_failed
echo Failed to download gyp. Make sure you have subversion installed, or 
echo manually install gyp into %~dp0build\gyp.
goto exit

:have_gyp
python gyp_uv
if errorlevel 1 goto create-msvs-files-failed
if not exist uv.sln goto create-msvs-files-failed
echo Project files generated.

:msbuild
@rem Skip project generation if requested.
if defined nobuild goto run

if not defined VCINSTALLDIR echo Build skipped. To build, this file needs to run from VS cmd prompt.& goto run

@rem Build the sln with msbuild.
msbuild uv.sln /t:%target% /p:Configuration=%config% /clp:NoSummary;NoItemAndPropertyList;Verbosity=minimal /nologo
if errorlevel 1 goto exit

:run
@rem Run tests if requested.
if "%run%"=="" goto exit
if not exist %config%\%run% goto exit
echo running '%config%\%run%'
%config%\%run%
goto exit

:create-msvs-files-failed
echo Failed to create vc project files. 
goto exit

:help
echo vcbuild.bat [debug/release] [test/bench] [clean] [noprojgen] [nobuild]
echo Examples:
echo   vcbuild.bat              : builds debug build
echo   vcbuild.bat test         : builds debug build and runs tests
echo   vcbuild.bat release bench: builds release build and runs benchmarks
goto exit

:exit
