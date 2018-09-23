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
set target=Build
set target_arch=x86
set debug_arg=
set snapshot_arg=
set noprojgen=
set nobuild=
set nosign=
set nosnapshot=
set test_args=
set msi=
set upload=
set licensertf=
set jslint=
set buildnodeweak=
set noetw=
set noetw_arg=
set noetw_msi_arg=
set noperfctr=
set noperfctr_arg=
set noperfctr_msi_arg=
set i18n_arg=
set download_arg=
set release_urls_arg=
set build_release=

:next-arg
if "%1"=="" goto args-done
if /i "%1"=="debug"         set config=Debug&goto arg-ok
if /i "%1"=="release"       set config=Release&goto arg-ok
if /i "%1"=="clean"         set target=Clean&goto arg-ok
if /i "%1"=="ia32"          set target_arch=x86&goto arg-ok
if /i "%1"=="x86"           set target_arch=x86&goto arg-ok
if /i "%1"=="x64"           set target_arch=x64&goto arg-ok
if /i "%1"=="noprojgen"     set noprojgen=1&goto arg-ok
if /i "%1"=="nobuild"       set nobuild=1&goto arg-ok
if /i "%1"=="nosign"        set nosign=1&goto arg-ok
if /i "%1"=="nosnapshot"    set nosnapshot=1&goto arg-ok
if /i "%1"=="noetw"         set noetw=1&goto arg-ok
if /i "%1"=="noperfctr"     set noperfctr=1&goto arg-ok
if /i "%1"=="licensertf"    set licensertf=1&goto arg-ok
if /i "%1"=="test"          set test_args=%test_args% sequential parallel message -J&set jslint=1&goto arg-ok
if /i "%1"=="test-ci"       set test_args=%test_args% %test_ci_args% -p tap --logfile test.tap message sequential parallel&goto arg-ok
if /i "%1"=="test-simple"   set test_args=%test_args% sequential parallel -J&goto arg-ok
if /i "%1"=="test-message"  set test_args=%test_args% message&goto arg-ok
if /i "%1"=="test-gc"       set test_args=%test_args% gc&set buildnodeweak=1&goto arg-ok
if /i "%1"=="test-internet" set test_args=%test_args% internet&goto arg-ok
if /i "%1"=="test-pummel"   set test_args=%test_args% pummel&goto arg-ok
if /i "%1"=="test-all"      set test_args=%test_args% sequential parallel message gc internet pummel&set buildnodeweak=1&set jslint=1&goto arg-ok
if /i "%1"=="jslint"        set jslint=1&goto arg-ok
@rem Include small-icu support with MSI installer
if /i "%1"=="msi"           set msi=1&set licensertf=1&set download_arg="--download=all"&set i18n_arg=small-icu&goto arg-ok
if /i "%1"=="build-release" set build_release=1&goto arg-ok
if /i "%1"=="upload"        set upload=1&goto arg-ok
if /i "%1"=="small-icu"     set i18n_arg=%1&goto arg-ok
if /i "%1"=="full-icu"      set i18n_arg=%1&goto arg-ok
if /i "%1"=="intl-none"     set i18n_arg=%1&goto arg-ok
if /i "%1"=="download-all"  set download_arg="--download=all"&goto arg-ok
if /i "%1"=="ignore-flaky"  set test_args=%test_args% --flaky-tests=dontcare&goto arg-ok

echo Warning: ignoring invalid command line option `%1`.

:arg-ok
:arg-ok
shift
goto next-arg

:args-done

if defined build_release (
  set config=Release
  set msi=1
  set licensertf=1
  set download_arg="--download=all"
  set i18n_arg=small-icu
)

if "%config%"=="Debug" set debug_arg=--debug
if defined nosnapshot set snapshot_arg=--without-snapshot
if defined noetw set noetw_arg=--without-etw& set noetw_msi_arg=/p:NoETW=1
if defined noperfctr set noperfctr_arg=--without-perfctr& set noperfctr_msi_arg=/p:NoPerfCtr=1
if defined RELEASE_URLBASE set release_urlbase_arg=--release-urlbase=%RELEASE_URLBASE%

if "%i18n_arg%"=="full-icu" set i18n_arg=--with-intl=full-icu
if "%i18n_arg%"=="small-icu" set i18n_arg=--with-intl=small-icu
if "%i18n_arg%"=="intl-none" set i18n_arg=--with-intl=none

if not exist "%~dp0deps\icu" goto no-depsicu
if "%target%"=="Clean" echo deleting %~dp0deps\icu
if "%target%"=="Clean" rmdir /S /Q %~dp0deps\icu
:no-depsicu

call :getnodeversion || exit /b 1

@rem Set environment for msbuild

@rem Look for Visual Studio 2015
echo Looking for Visual Studio 2015
if not defined VS140COMNTOOLS goto vc-set-2013
if not exist "%VS140COMNTOOLS%\..\..\vc\vcvarsall.bat" goto vc-set-2013
echo Found Visual Studio 2015
if defined msi (
  echo Looking for WiX installation for Visual Studio 2015...
  if not exist "%WIX%\SDK\VS2015" (
    echo Failed to find WiX install for Visual Studio 2015
    echo VS2015 support for WiX is only present starting at version 3.10
    goto vc-set-2013
  )
)
if "%VCVARS_VER%" NEQ "140" (
  call "%VS140COMNTOOLS%\..\..\vc\vcvarsall.bat"
  SET VCVARS_VER=140
)
if not defined VCINSTALLDIR goto vc-set-2013
set GYP_MSVS_VERSION=2015
set PLATFORM_TOOLSET=v140
goto msbuild-found

:vc-set-2013
@rem Look for Visual Studio 2013
echo Looking for Visual Studio 2013
if not defined VS120COMNTOOLS goto msbuild-not-found
if not exist "%VS120COMNTOOLS%\..\..\vc\vcvarsall.bat" goto msbuild-not-found
echo Found Visual Studio 2013
if defined msi (
  echo Looking for WiX installation for Visual Studio 2013...
  if not exist "%WIX%\SDK\VS2013" (
    echo Failed to find WiX install for Visual Studio 2013
    echo VS2013 support for WiX is only present starting at version 3.8
    goto vc-set-2012
  )
)
if "%VCVARS_VER%" NEQ "120" (
  call "%VS120COMNTOOLS%\..\..\vc\vcvarsall.bat"
  SET VCVARS_VER=120
)
if not defined VCINSTALLDIR goto msbuild-not-found
set GYP_MSVS_VERSION=2013
set PLATFORM_TOOLSET=v120
goto msbuild-found

:msbuild-not-found
echo Failed to find Visual Studio installation.
goto exit

:wix-not-found
echo Build skipped. To generate installer, you need to install Wix.
goto run

:msbuild-found

:project-gen
@rem Skip project generation if requested.
if defined noprojgen goto msbuild

@rem Generate the VS project.
python configure %download_arg% %i18n_arg% %debug_arg% %snapshot_arg% %noetw_arg% %noperfctr_arg% --dest-cpu=%target_arch% --tag=%TAG%
if errorlevel 1 goto create-msvs-files-failed
if not exist node.sln goto create-msvs-files-failed
echo Project files generated.

:msbuild
@rem Skip build if requested.
if defined nobuild goto sign

@rem Build the sln with msbuild.
msbuild node.sln /m /t:%target% /p:Configuration=%config% /clp:NoSummary;NoItemAndPropertyList;Verbosity=minimal /nologo
if errorlevel 1 goto exit
if "%target%" == "Clean" goto exit

:sign
@rem Skip signing if the `nosign` option was specified.
if defined nosign goto licensertf

signtool sign /a /d "node" /t http://timestamp.globalsign.com/scripts/timestamp.dll Release\node.exe
if errorlevel 1 echo Failed to sign exe&goto exit

:licensertf
@rem Skip license.rtf generation if not requested.
if not defined licensertf goto msi

%config%\node tools\license2rtf.js < LICENSE > %config%\license.rtf
if errorlevel 1 echo Failed to generate license.rtf&goto exit

:msi
@rem Skip msi generation if not requested
if not defined msi goto run

:msibuild
echo Building node-v%FULLVERSION%-%target_arch%.msi
msbuild "%~dp0tools\msvs\msi\nodemsi.sln" /m /t:Clean,Build /p:PlatformToolset=%PLATFORM_TOOLSET% /p:GypMsvsVersion=%GYP_MSVS_VERSION% /p:Configuration=%config% /p:Platform=%target_arch% /p:NodeVersion=%NODE_VERSION% /p:FullVersion=%FULLVERSION% /p:DistTypeDir=%DISTTYPEDIR% %noetw_msi_arg% %noperfctr_msi_arg% /clp:NoSummary;NoItemAndPropertyList;Verbosity=minimal /nologo
if errorlevel 1 goto exit

if defined nosign goto upload
signtool sign /a /d "node" /t http://timestamp.globalsign.com/scripts/timestamp.dll node-v%FULLVERSION%-%target_arch%.msi
if errorlevel 1 echo Failed to sign msi&goto exit

:upload
@rem Skip upload if not requested
if not defined upload goto run

if not defined SSHCONFIG (
  echo SSHCONFIG is not set for upload
  exit /b 1
)
if not defined STAGINGSERVER set STAGINGSERVER=node-www
ssh -F %SSHCONFIG% %STAGINGSERVER% "mkdir -p nodejs/%DISTTYPEDIR%/v%FULLVERSION%/win-%target_arch%"
scp -F %SSHCONFIG% Release\node.exe %STAGINGSERVER%:nodejs/%DISTTYPEDIR%/v%FULLVERSION%/win-%target_arch%/node.exe
scp -F %SSHCONFIG% Release\node.lib %STAGINGSERVER%:nodejs/%DISTTYPEDIR%/v%FULLVERSION%/win-%target_arch%/node.lib
scp -F %SSHCONFIG% node-v%FULLVERSION%-%target_arch%.msi %STAGINGSERVER%:nodejs/%DISTTYPEDIR%/v%FULLVERSION%/
ssh -F %SSHCONFIG% %STAGINGSERVER% "touch nodejs/%DISTTYPEDIR%/v%FULLVERSION%/node-v%FULLVERSION%-%target_arch%.msi.done nodejs/%DISTTYPEDIR%/v%FULLVERSION%/win-%target_arch%.done && chmod -R ug=rw-x+X,o=r+X nodejs/%DISTTYPEDIR%/v%FULLVERSION%/node-v%FULLVERSION%-%target_arch%.msi* nodejs/%DISTTYPEDIR%/v%FULLVERSION%/win-%target_arch%*"

:run
@rem Run tests if requested.

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
if "%test_args%"=="" goto jslint
if "%config%"=="Debug" set test_args=--mode=debug %test_args%
if "%config%"=="Release" set test_args=--mode=release %test_args%
echo running 'cctest'
"%config%\cctest"
echo running 'python tools\test.py %test_args%'
python tools\test.py %test_args%
goto jslint

:jslint
if not defined jslint goto exit
echo running jslint
%config%\node tools\eslint\bin\eslint.js src lib test tools\eslint-rules --rulesdir tools\eslint-rules --reset --quiet
goto exit

:create-msvs-files-failed
echo Failed to create vc project files.
goto exit

:help
echo vcbuild.bat [debug/release] [msi] [test-all/test-uv/test-internet/test-pummel/test-simple/test-message] [clean] [noprojgen] [small-icu/full-icu/intl-none] [nobuild] [nosign] [x86/x64] [download-all]
echo Examples:
echo   vcbuild.bat                : builds release build
echo   vcbuild.bat debug          : builds debug build
echo   vcbuild.bat release msi    : builds release build and MSI installer package
echo   vcbuild.bat test           : builds debug build and runs tests
echo   vcbuild.bat build-release  : builds the release distribution as used by nodejs.org
goto exit

:exit
goto :EOF

rem ***************
rem   Subroutines
rem ***************

:getnodeversion
set NODE_VERSION=
set TAG=
set FULLVERSION=

for /F "usebackq tokens=*" %%i in (`python "%~dp0tools\getnodeversion.py"`) do set NODE_VERSION=%%i
if not defined NODE_VERSION (
  echo Cannot determine current version of Node.js
  exit /b 1
)

if not defined DISTTYPE set DISTTYPE=release
if "%DISTTYPE%"=="release" (
  set FULLVERSION=%NODE_VERSION%
  goto exit
)
if "%DISTTYPE%"=="custom" (
  if not defined CUSTOMTAG (
    echo "CUSTOMTAG is not set for DISTTYPE=custom"
    exit /b 1
  )
  set TAG=%CUSTOMTAG%
)
if not "%DISTTYPE%"=="custom" (
  if not defined DATESTRING (
    echo "DATESTRING is not set for nightly"
    exit /b 1
  )
  if not defined COMMIT (
    echo "COMMIT is not set for nightly"
    exit /b 1
  )
  if not "%DISTTYPE%"=="nightly" (
    if not "%DISTTYPE%"=="next-nightly" (
      echo "DISTTYPE is not release, custom, nightly or next-nightly"
      exit /b 1
    )
  )
  set TAG=%DISTTYPE%%DATESTRING%%COMMIT%
)
set FULLVERSION=%NODE_VERSION%-%TAG%

:exit
if not defined DISTTYPEDIR set DISTTYPEDIR=%DISTTYPE%
goto :EOF
