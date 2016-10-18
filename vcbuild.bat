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
set noetw_msi_arg=
set noperfctr=
set noperfctr_msi_arg=
set i18n_arg=
set download_arg=
set build_release=
set flaky_tests_arg=
set configure_flags=

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
if /i "%1"=="test-uv"       set test=test-uv&goto arg-ok
if /i "%1"=="test-internet" set test=test-internet&goto arg-ok
if /i "%1"=="test-pummel"   set test=test-pummel&goto arg-ok
if /i "%1"=="test-simple"   set test=test-simple&goto arg-ok
if /i "%1"=="test-message"  set test=test-message&goto arg-ok
if /i "%1"=="test-gc"       set test=test-gc&set buildnodeweak=1&goto arg-ok
if /i "%1"=="test-all"      set test=test-all&set buildnodeweak=1&goto arg-ok
if /i "%1"=="test-ci"       set test=test-ci&set nosnapshot=1&goto arg-ok
if /i "%1"=="test"          set test=test&set jslint=1&goto arg-ok
if /i "%1"=="msi"           set msi=1&set licensertf=1&set download_arg="--download=all"&set i18n_arg=small-icu&goto arg-ok
if /i "%1"=="upload"        set upload=1&goto arg-ok
if /i "%1"=="jslint"        set jslint=1&goto arg-ok
if /i "%1"=="small-icu"     set i18n_arg=%1&goto arg-ok
if /i "%1"=="full-icu"      set i18n_arg=%1&goto arg-ok
if /i "%1"=="intl-none"     set i18n_arg=%1&goto arg-ok
if /i "%1"=="download-all"  set download_arg="--download=all"&goto arg-ok
if /i "%1"=="build-release" set build_release=1&goto arg-ok
if /i "%1"=="ignore-flaky"  set flaky_tests_arg=--flaky-tests=dontcare&goto arg-ok

echo Warning: ignoring invalid command line option `%1`.

:arg-ok
:arg-ok
shift
goto next-arg

:args-done

if defined build_release (
  set nosnapshot=1
  set config=Release
  set msi=1
  set licensertf=1
  set download_arg="--download=all"
  set i18n_arg=small-icu
)

if "%config%"=="Debug" set configure_flags=%configure_flags% --debug
if defined nosnapshot set configure_flags=%configure_flags% --without-snapshot
if defined noetw set configure_flags=%configure_flags% --without-etw& set noetw_msi_arg=/p:NoETW=1
if defined noperfctr set configure_flags=%configure_flags% --without-perfctr& set noperfctr_msi_arg=/p:NoPerfCtr=1

if "%i18n_arg%"=="full-icu" set configure_flags=%configure_flags% --with-intl=full-icu
if "%i18n_arg%"=="small-icu" set configure_flags=%configure_flags% --with-intl=small-icu
if "%i18n_arg%"=="intl-none" set configure_flags=%configure_flags% --with-intl=none

if defined config_flags set configure_flags=%configure_flags% %config_flags%

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
if not defined VS120COMNTOOLS goto vc-set-2012
if not exist "%VS120COMNTOOLS%\..\..\vc\vcvarsall.bat" goto vc-set-2012
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
if not defined VCINSTALLDIR goto vc-set-2012
set GYP_MSVS_VERSION=2013
set PLATFORM_TOOLSET=v120
goto msbuild-found

:vc-set-2012
@rem Look for Visual Studio 2012
echo Looking for Visual Studio 2012
if not defined VS110COMNTOOLS goto vc-set-2010
if not exist "%VS110COMNTOOLS%\..\..\vc\vcvarsall.bat" goto vc-set-2010
echo Found Visual Studio 2012
if defined msi (
  echo Looking for WiX installation for Visual Studio 2012...
  if not exist "%WIX%\SDK\VS2012" (
    echo Failed to find WiX install for Visual Studio 2012
    goto vc-set-2010
  )
)
if "%VCVARS_VER%" NEQ "110" (
  call "%VS110COMNTOOLS%\..\..\vc\vcvarsall.bat"
  SET VCVARS_VER=110
)
if not defined VCINSTALLDIR goto vc-set-2010
set GYP_MSVS_VERSION=2012
set PLATFORM_TOOLSET=v110
goto msbuild-found

:vc-set-2010
echo Looking for Visual Studio 2010
if not defined VS100COMNTOOLS goto msbuild-not-found
if not exist "%VS100COMNTOOLS%\..\..\vc\vcvarsall.bat" goto msbuild-not-found
echo Found Visual Studio 2010
if defined msi (
  echo Looking for WiX installation for Visual Studio 2010...
  if not exist "%WIX%\SDK\VS2010" (
    echo Failed to find WiX install for Visual Studio 2010
    goto wix-not-found
  )
)
if "%VCVARS_VER%" NEQ "100" (
  call "%VS100COMNTOOLS%\..\..\vc\vcvarsall.bat"
  SET VCVARS_VER=100
)
if not defined VCINSTALLDIR goto msbuild-not-found
set GYP_MSVS_VERSION=2010
set PLATFORM_TOOLSET=v100
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
echo configure %configure_flags% --dest-cpu=%target_arch% --tag=%TAG%
python configure %configure_flags% --dest-cpu=%target_arch% --tag=%TAG%
if errorlevel 1 goto create-msvs-files-failed
if not exist node.sln goto create-msvs-files-failed
echo Project files generated.

:msbuild
@rem Skip project generation if requested.
if defined nobuild goto sign

@rem Build the sln with msbuild.
set "msbplatform=Win32"
if "%target_arch%"=="x64" set "msbplatform=x64"
msbuild node.sln /m /t:%target% /p:Configuration=%config% /p:Platform=%msbplatform% /clp:NoSummary;NoItemAndPropertyList;Verbosity=minimal /nologo
if errorlevel 1 goto exit

:sign
@rem Skip signing if the `nosign` option was specified.
if defined nosign goto licensertf

call tools\sign.bat Release\node.exe
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
call tools\sign.bat node-v%FULLVERSION%-%target_arch%.msi
if errorlevel 1 echo Failed to sign msi&goto exit

:upload
@rem Skip upload if not requested
if not defined upload goto run
if not defined SSHCONFIG (
  echo SSHCONFIG is not set for upload
  exit /b 1
)
if not defined STAGINGSERVER set STAGINGSERVER=node-www
if "%target_arch%"=="x64" set staging_dir=nodejs/%DISTTYPEDIR%/v%FULLVERSION%/x64
if "%target_arch%"=="x86" set staging_dir=nodejs/%DISTTYPEDIR%/v%FULLVERSION%/
echo Uploading to %STAGINGSERVER%:%staging_dir%
ssh -F %SSHCONFIG% %STAGINGSERVER% "mkdir -p %staging_dir%"
scp -F %SSHCONFIG% Release\node.exe %STAGINGSERVER%:%staging_dir%/node.exe
scp -F %SSHCONFIG% Release\node.lib %STAGINGSERVER%:%staging_dir%/node.lib
scp -F %SSHCONFIG% Release\node.pdb %STAGINGSERVER%:%staging_dir%/node.pdb
scp -F %SSHCONFIG% Release\node.exp %STAGINGSERVER%:%staging_dir%/node.exp
scp -F %SSHCONFIG% Release\openssl-cli.exe %STAGINGSERVER%:%staging_dir%/openssl-cli.exe
scp -F %SSHCONFIG% Release\openssl-cli.pdb %STAGINGSERVER%:%staging_dir%/openssl-cli.pdb
scp -F %SSHCONFIG% node-v%FULLVERSION%-%target_arch%.msi %STAGINGSERVER%:%staging_dir%/
if "%target_arch%"=="x64" (
  ssh -F %SSHCONFIG% %STAGINGSERVER% "touch %staging_dir%.done && chmod -R ug=rw-x+X,o=r+X %staging_dir%*"
)
if "%target_arch%"=="x86" (
  ssh -F %SSHCONFIG% %STAGINGSERVER% "touch %staging_dir%/node-v%FULLVERSION%-%target_arch%.msi.done %staging_dir%/node.exe.done %staging_dir%/node.lib.done %staging_dir%/node.pdb.done %staging_dir%/node.exp.done %staging_dir%/openssl-cli.exe.done %staging_dir%/openssl-cli.pdb.done && chmod -R ug=rw-x+X,o=r+X %staging_dir%/node.* %staging_dir%/openssl-cli.* %staging_dir%/node-v%FULLVERSION%-%target_arch%.msi* && chmod -R ug=rw-x+X,o=r+X %staging_dir%"
)

:run
@rem Run tests if requested.
if "%test%"=="" goto jslint

if "%config%"=="Debug" set test_args=--mode=debug
if "%config%"=="Release" set test_args=--mode=release

set test_args=%test_args% --arch=%target_arch% 

if "%test%"=="test" set test_args=%test_args% simple message
if "%test%"=="test-ci" set test_args=%test_args% -p tap --logfile test.tap %flaky_tests_arg% simple message internet 
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
goto jslint

:create-msvs-files-failed
echo Failed to create vc project files. 
goto exit

:jslint
if not defined jslint goto exit
echo running jslint
set PYTHONPATH=tools/closure_linter/
python tools/closure_linter/closure_linter/gjslint.py --unix_mode --strict --nojsdoc -r lib/ -r src/ --exclude_files lib/punycode.js
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
