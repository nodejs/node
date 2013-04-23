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
set config=Release
set msiplatform=x86
set target=Build
set target_arch=ia32
set debug_arg=
set nosnapshot_arg=
set noprojgen=
set nobuild=
set nosign=
set nosnapshot=
set test=
set test_args=
set msi=
set licensertf=
set upload=
set jslint=
set buildnodeweak=
set noetw=
set noetw_arg=
set noetw_msi_arg=
set noperfctr=
set noperfctr_arg=
set noperfctr_msi_arg=

:next-arg
if "%1"=="" goto args-done
if /i "%1"=="debug"         set config=Debug&goto arg-ok
if /i "%1"=="release"       set config=Release&goto arg-ok
if /i "%1"=="clean"         set target=Clean&goto arg-ok
if /i "%1"=="ia32"          set target_arch=ia32&goto arg-ok
if /i "%1"=="x86"           set target_arch=ia32&goto arg-ok
if /i "%1"=="x64"           set target_arch=x64&goto arg-ok
if /i "%1"=="noprojgen"     set noprojgen=1&goto arg-ok
if /i "%1"=="nobuild"       set nobuild=1&goto arg-ok
if /i "%1"=="nosign"        set nosign=1&goto arg-ok
if /i "%1"=="nosnapshot"    set nosnapshot=1&goto arg-ok
if /i "%1"=="noetw"         set noetw=1&goto arg-ok
if /i "%1"=="noperfctr"     set noperfctr=1&goto arg-ok
if /i "%1"=="licensertf"    set licensertf=1&goto arg-ok
if /i "%1"=="test-uv"       set test=test-uv&goto arg-ok
if /i "%1"=="test-internet" set test=test-internet&goto arg-ok
if /i "%1"=="test-pummel"   set test=test-pummel&goto arg-ok
if /i "%1"=="test-simple"   set test=test-simple&goto arg-ok
if /i "%1"=="test-message"  set test=test-message&goto arg-ok
if /i "%1"=="test-gc"       set test=test-gc&set buildnodeweak=1&goto arg-ok
if /i "%1"=="test-all"      set test=test-all&set buildnodeweak=1&goto arg-ok
if /i "%1"=="test"          set test=test&goto arg-ok
if /i "%1"=="msi"           set msi=1&set licensertf=1&goto arg-ok
if /i "%1"=="upload"        set upload=1&goto arg-ok
if /i "%1"=="jslint"        set jslint=1&goto arg-ok

echo Warning: ignoring invalid command line option `%1`.

:arg-ok
:arg-ok
shift
goto next-arg

:args-done
if defined upload goto upload
if defined jslint goto jslint

if "%config%"=="Debug" set debug_arg=--debug
if "%target_arch%"=="x64" set msiplatform=x64
if defined nosnapshot set nosnapshot_arg=--without-snapshot
if defined noetw set noetw_arg=--without-etw& set noetw_msi_arg=/p:NoETW=1
if defined noperfctr set noperfctr_arg=--without-perfctr& set noperfctr_msi_arg=/p:NoPerfCtr=1

:project-gen
@rem Skip project generation if requested.
if defined noprojgen goto msbuild

if defined NIGHTLY set TAG=nightly-%NIGHTLY%

@rem Generate the VS project.
SETLOCAL
  if defined VS100COMNTOOLS call "%VS100COMNTOOLS%\VCVarsQueryRegistry.bat"
  python configure %debug_arg% %nosnapshot_arg% %noetw_arg% %noperfctr_arg% --dest-cpu=%target_arch% --tag=%TAG%
  if errorlevel 1 goto create-msvs-files-failed
  if not exist node.sln goto create-msvs-files-failed
  echo Project files generated.
ENDLOCAL

:msbuild
@rem Skip project generation if requested.
if defined nobuild goto sign

@rem Bail out early if not running in VS build env.
if defined VCINSTALLDIR goto msbuild-found

@rem Look for Visual Studio 2012
if not defined VS110COMNTOOLS goto vc-set-2010
if not exist "%VS110COMNTOOLS%\..\..\vc\vcvarsall.bat" goto vc-set-2010
call "%VS110COMNTOOLS%\..\..\vc\vcvarsall.bat"
if not defined VCINSTALLDIR goto msbuild-not-found
set GYP_MSVS_VERSION=2012
goto msbuild-found

:vc-set-2010
if not defined VS100COMNTOOLS goto msbuild-not-found
if not exist "%VS100COMNTOOLS%\..\..\vc\vcvarsall.bat" goto msbuild-not-found
call "%VS100COMNTOOLS%\..\..\vc\vcvarsall.bat"
if not defined VCINSTALLDIR goto msbuild-not-found
goto msbuild-found

:msbuild-not-found
echo Build skipped. To build, this file needs to run from VS cmd prompt.
goto run

:msbuild-found
@rem Build the sln with msbuild.
msbuild node.sln /m /t:%target% /p:Configuration=%config% /clp:NoSummary;NoItemAndPropertyList;Verbosity=minimal /nologo
if errorlevel 1 goto exit

:sign
@rem Skip signing if the `nosign` option was specified.
if defined nosign goto licensertf

signtool sign /a Release\node.exe

:licensertf
@rem Skip license.rtf generation if not requested.
if not defined licensertf goto msi

%config%\node tools\license2rtf.js < LICENSE > %config%\license.rtf
if errorlevel 1 echo Failed to generate license.rtf&goto exit

:msi
@rem Skip msi generation if not requested
if not defined msi goto run
call :getnodeversion

if not defined NIGHTLY goto msibuild
set NODE_VERSION=%NODE_VERSION%.%NIGHTLY%

:msibuild
echo Building node-%NODE_VERSION%
msbuild "%~dp0tools\msvs\msi\nodemsi.sln" /m /t:Clean,Build /p:Configuration=%config% /p:Platform=%msiplatform% /p:NodeVersion=%NODE_VERSION% %noetw_msi_arg% %noperfctr_msi_arg% /clp:NoSummary;NoItemAndPropertyList;Verbosity=minimal /nologo
if errorlevel 1 goto exit

if defined nosign goto run
signtool sign /a Release\node-v%NODE_VERSION%-%msiplatform%.msi

:run
@rem Run tests if requested.
if "%test%"=="" goto exit

if "%config%"=="Debug" set test_args=--mode=debug
if "%config%"=="Release" set test_args=--mode=release

if "%test%"=="test" set test_args=%test_args% simple message
if "%test%"=="test-internet" set test_args=%test_args% internet
if "%test%"=="test-pummel" set test_args=%test_args% pummel
if "%test%"=="test-simple" set test_args=%test_args% simple
if "%test%"=="test-message" set test_args=%test_args% message
if "%test%"=="test-gc" set test_args=%test_args% gc
if "%test%"=="test-all" set test_args=%test_args%

:build-node-weak
@rem Build node-weak if required
if "%buildnodeweak%"=="" goto run-tests
"%config%\node" deps\npm\node_modules\node-gyp\bin\node-gyp rebuild --directory="%~dp0test\gc\node_modules\weak" --nodedir="%~dp0."
if errorlevel 1 goto build-node-weak-failed
goto run-tests

:build-node-weak-failed
echo Failed to build node-weak.
goto exit

:run-tests
echo running 'python tools/test.py %test_args%'
python tools/test.py %test_args%
if "%test%"=="test" goto jslint
goto exit

:create-msvs-files-failed
echo Failed to create vc project files. 
goto exit

:upload
echo uploading .exe .msi .pdb to nodejs.org
call :getnodeversion
@echo on
ssh node@nodejs.org mkdir -p web/nodejs.org/dist/v%NODE_VERSION%
scp Release\node.msi node@nodejs.org:~/web/nodejs.org/dist/v%NODE_VERSION%/node-v%NODE_VERSION%.msi
scp Release\node.exe node@nodejs.org:~/web/nodejs.org/dist/v%NODE_VERSION%/node.exe
scp Release\node.pdb node@nodejs.org:~/web/nodejs.org/dist/v%NODE_VERSION%/node.pdb
@echo off
goto exit

:jslint
echo running jslint
set PYTHONPATH=tools/closure_linter/
python tools/closure_linter/closure_linter/gjslint.py --unix_mode --strict --nojsdoc -r lib/ -r src/ --exclude_files lib/punycode.js
goto exit

:help
echo vcbuild.bat [debug/release] [msi] [test-all/test-uv/test-internet/test-pummel/test-simple/test-message] [clean] [noprojgen] [nobuild] [nosign] [x86/x64]
echo Examples:
echo   vcbuild.bat                : builds release build
echo   vcbuild.bat debug          : builds debug build
echo   vcbuild.bat release msi    : builds release build and MSI installer package
echo   vcbuild.bat test           : builds debug build and runs tests
goto exit

:exit
goto :EOF

rem ***************
rem   Subroutines
rem ***************

:getnodeversion
set NODE_VERSION=
for /F "usebackq tokens=*" %%i in (`python "%~dp0tools\getnodeversion.py"`) do set NODE_VERSION=%%i
if not defined NODE_VERSION echo Cannot determine current version of node.js & exit /b 1
goto :EOF
