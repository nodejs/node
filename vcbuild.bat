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
set target_arch=x64
set target_env=
set noprojgen=
set nobuild=
set sign=
set nosnapshot=
set cctest_args=
set test_args=
set package=
set msi=
set upload=
set licensertf=
set jslint=
set cpplint=
set build_testgc_addon=
set noetw=
set noetw_msi_arg=
set noperfctr=
set noperfctr_msi_arg=
set i18n_arg=
set download_arg=
set build_release=
set enable_vtune_arg=
set configure_flags=
set build_addons=
set dll=
set enable_static=
set build_addons_napi=
set test_node_inspect=
set test_check_deopts=
set js_test_suites=async-hooks inspector known_issues message parallel sequential
set v8_test_options=
set v8_build_options=
set "common_test_suites=%js_test_suites% doctool addons addons-napi&set build_addons=1&set build_addons_napi=1"
set http2_debug=
set nghttp2_debug=

:next-arg
if "%1"=="" goto args-done
if /i "%1"=="debug"         set config=Debug&goto arg-ok
if /i "%1"=="release"       set config=Release&goto arg-ok
if /i "%1"=="clean"         set target=Clean&goto arg-ok
if /i "%1"=="ia32"          set target_arch=x86&goto arg-ok
if /i "%1"=="x86"           set target_arch=x86&goto arg-ok
if /i "%1"=="x64"           set target_arch=x64&goto arg-ok
@rem args should be vs2017 and vs2015. keeping vc2015 for backward compatibility (undocumented)
if /i "%1"=="vc2015"        set target_env=vs2015&goto arg-ok
if /i "%1"=="vs2015"        set target_env=vs2015&goto arg-ok
if /i "%1"=="vs2017"        set target_env=vs2017&goto arg-ok
if /i "%1"=="noprojgen"     set noprojgen=1&goto arg-ok
if /i "%1"=="nobuild"       set nobuild=1&goto arg-ok
if /i "%1"=="nosign"        set "sign="&echo Note: vcbuild no longer signs by default. "nosign" is redundant.&goto arg-ok
if /i "%1"=="sign"          set sign=1&goto arg-ok
if /i "%1"=="nosnapshot"    set nosnapshot=1&goto arg-ok
if /i "%1"=="noetw"         set noetw=1&goto arg-ok
if /i "%1"=="noperfctr"     set noperfctr=1&goto arg-ok
if /i "%1"=="licensertf"    set licensertf=1&goto arg-ok
if /i "%1"=="test"          set test_args=%test_args% -J %common_test_suites%&set cpplint=1&set jslint=1&goto arg-ok
if /i "%1"=="test-ci"       set test_args=%test_args% %test_ci_args% -p tap --logfile test.tap %common_test_suites%&set cctest_args=%cctest_args% --gtest_output=tap:cctest.tap&goto arg-ok
if /i "%1"=="test-addons"   set test_args=%test_args% addons&set build_addons=1&goto arg-ok
if /i "%1"=="test-addons-napi"   set test_args=%test_args% addons-napi&set build_addons_napi=1&goto arg-ok
if /i "%1"=="test-simple"   set test_args=%test_args% sequential parallel -J&goto arg-ok
if /i "%1"=="test-message"  set test_args=%test_args% message&goto arg-ok
if /i "%1"=="test-gc"       set test_args=%test_args% gc&set build_testgc_addon=1&goto arg-ok
if /i "%1"=="test-inspector" set test_args=%test_args% inspector&goto arg-ok
if /i "%1"=="test-tick-processor" set test_args=%test_args% tick-processor&goto arg-ok
if /i "%1"=="test-internet" set test_args=%test_args% internet&goto arg-ok
if /i "%1"=="test-pummel"   set test_args=%test_args% pummel&goto arg-ok
if /i "%1"=="test-known-issues" set test_args=%test_args% known_issues&goto arg-ok
if /i "%1"=="test-async-hooks"  set test_args=%test_args% async-hooks&goto arg-ok
if /i "%1"=="test-all"      set test_args=%test_args% gc internet pummel %common_test_suites%&set build_testgc_addon=1&set cpplint=1&set jslint=1&goto arg-ok
if /i "%1"=="test-node-inspect" set test_node_inspect=1&goto arg-ok
if /i "%1"=="test-check-deopts" set test_check_deopts=1&goto arg-ok
if /i "%1"=="test-v8"       set test_v8=1&set custom_v8_test=1&goto arg-ok
if /i "%1"=="test-v8-intl"  set test_v8_intl=1&set custom_v8_test=1&goto arg-ok
if /i "%1"=="test-v8-benchmarks" set test_v8_benchmarks=1&set custom_v8_test=1&goto arg-ok
if /i "%1"=="test-v8-all"       set test_v8=1&set test_v8_intl=1&set test_v8_benchmarks=1&set custom_v8_test=1&goto arg-ok
if /i "%1"=="jslint"        set jslint=1&goto arg-ok
if /i "%1"=="jslint-ci"     set jslint_ci=1&goto arg-ok
if /i "%1"=="lint"          set cpplint=1&set jslint=1&goto arg-ok
if /i "%1"=="lint-ci"       set cpplint=1&set jslint_ci=1&goto arg-ok
if /i "%1"=="package"       set package=1&goto arg-ok
if /i "%1"=="msi"           set msi=1&set licensertf=1&set download_arg="--download=all"&set i18n_arg=small-icu&goto arg-ok
if /i "%1"=="build-release" set build_release=1&set sign=1&goto arg-ok
if /i "%1"=="upload"        set upload=1&goto arg-ok
if /i "%1"=="small-icu"     set i18n_arg=%1&goto arg-ok
if /i "%1"=="full-icu"      set i18n_arg=%1&goto arg-ok
if /i "%1"=="intl-none"     set i18n_arg=%1&goto arg-ok
if /i "%1"=="without-intl"  set i18n_arg=%1&goto arg-ok
if /i "%1"=="download-all"  set download_arg="--download=all"&goto arg-ok
if /i "%1"=="ignore-flaky"  set test_args=%test_args% --flaky-tests=dontcare&goto arg-ok
if /i "%1"=="enable-vtune"  set enable_vtune_arg=1&goto arg-ok
if /i "%1"=="dll"           set dll=1&goto arg-ok
if /i "%1"=="static"        set enable_static=1&goto arg-ok
if /i "%1"=="no-NODE-OPTIONS"	set no_NODE_OPTIONS=1&goto arg-ok
if /i "%1"=="debug-http2"   set debug_http2=1&goto arg-ok
if /i "%1"=="debug-nghttp2" set debug_nghttp2=1&goto arg-ok

echo Error: invalid command line option `%1`.
exit /b 1

:arg-ok
:arg-ok
shift
goto next-arg

:args-done

if defined build_release (
  set config=Release
  set package=1
  set msi=1
  set licensertf=1
  set download_arg="--download=all"
  set i18n_arg=small-icu
)

:: assign path to node_exe
set "node_exe=%config%\node.exe"
set "node_gyp_exe="%node_exe%" deps\npm\node_modules\node-gyp\bin\node-gyp"
if "%target_env%"=="vs2015" set "node_gyp_exe=%node_gyp_exe% --msvs_version=2015"
if "%target_env%"=="vs2017" set "node_gyp_exe=%node_gyp_exe% --msvs_version=2017"

if "%config%"=="Debug" set configure_flags=%configure_flags% --debug
if defined nosnapshot set configure_flags=%configure_flags% --without-snapshot
if defined noetw set configure_flags=%configure_flags% --without-etw& set noetw_msi_arg=/p:NoETW=1
if defined noperfctr set configure_flags=%configure_flags% --without-perfctr& set noperfctr_msi_arg=/p:NoPerfCtr=1
if defined release_urlbase set configure_flags=%configure_flags% --release-urlbase=%release_urlbase%
if defined download_arg set configure_flags=%configure_flags% %download_arg%
if defined enable_vtune_arg set configure_flags=%configure_flags% --enable-vtune-profiling
if defined dll set configure_flags=%configure_flags% --shared
if defined enable_static set configure_flags=%configure_flags% --enable-static
if defined no_NODE_OPTIONS set configure_flags=%configure_flags% --without-node-options

REM if defined debug_http2 set configure_flags=%configure_flags% --debug-http2
REM if defined debug_nghttp2 set configure_flags=%configure_flags% --debug-nghttp2

if "%i18n_arg%"=="full-icu" set configure_flags=%configure_flags% --with-intl=full-icu
if "%i18n_arg%"=="small-icu" set configure_flags=%configure_flags% --with-intl=small-icu
if "%i18n_arg%"=="intl-none" set configure_flags=%configure_flags% --with-intl=none
if "%i18n_arg%"=="without-intl" set configure_flags=%configure_flags% --without-intl

if defined config_flags set configure_flags=%configure_flags% %config_flags%

if not exist "%~dp0deps\icu" goto no-depsicu
if "%target%"=="Clean" echo deleting %~dp0deps\icu
if "%target%"=="Clean" rmdir /S /Q %~dp0deps\icu
:no-depsicu

call :getnodeversion || exit /b 1

if "%target%"=="Clean" rmdir /Q /S "%~dp0%config%\node-v%FULLVERSION%-win-%target_arch%" > nul 2> nul

if defined noprojgen if defined nobuild if not defined sign if not defined msi goto licensertf

@rem Set environment for msbuild

set msvs_host_arch=x86
if _%PROCESSOR_ARCHITECTURE%_==_AMD64_ set msvs_host_arch=amd64
if _%PROCESSOR_ARCHITEW6432%_==_AMD64_ set msvs_host_arch=amd64
@rem usually vcvarsall takes an argument: host + '_' + target
set vcvarsall_arg=%msvs_host_arch%_%target_arch%
@rem unless both host and target are x64
if %target_arch%==x64 if %msvs_host_arch%==amd64 set vcvarsall_arg=amd64

@rem Look for Visual Studio 2017
:vs-set-2017
if defined target_env if "%target_env%" NEQ "vs2017" goto vs-set-2015
echo Looking for Visual Studio 2017
@rem check if VS2017 is already setup, and for the requested arch
if "_%VisualStudioVersion%_" == "_15.0_" if "_%VSCMD_ARG_TGT_ARCH%_"=="_%target_arch%_" goto found_vs2017
@rem need to clear VSINSTALLDIR for vcvarsall to work as expected
set "VSINSTALLDIR="
call tools\msvs\vswhere_usability_wrapper.cmd
if "_%VCINSTALLDIR%_" == "__" goto vs-set-2015
@rem prevent VsDevCmd.bat from changing the current working directory
set "VSCMD_START_DIR=%CD%"
set vcvars_call="%VCINSTALLDIR%\Auxiliary\Build\vcvarsall.bat" %vcvarsall_arg%
echo calling: %vcvars_call%
call %vcvars_call%
if errorlevel 1 goto vs-set-2015
:found_vs2017
echo Found MSVS version %VisualStudioVersion%
set GYP_MSVS_VERSION=2017
set PLATFORM_TOOLSET=v141
goto msbuild-found

@rem Look for Visual Studio 2015
:vs-set-2015
if defined target_env if "%target_env%" NEQ "vs2015" goto msbuild-not-found
echo Looking for Visual Studio 2015
if not defined VS140COMNTOOLS goto msbuild-not-found
if not exist "%VS140COMNTOOLS%\..\..\vc\vcvarsall.bat" goto msbuild-not-found
if defined msi (
  echo Looking for WiX installation for Visual Studio 2015...
  if not exist "%WIX%\SDK\VS2015" (
    echo Failed to find WiX install for Visual Studio 2015
    echo VS2015 support for WiX is only present starting at version 3.10
    goto wix-not-found
  )
)

@rem check if VS2015 is already setup
if "_%VisualStudioVersion%_" == "_14.0_" if "_%VCVARS_VER%_" == "_140_" goto found_vs2015
call "%VS140COMNTOOLS%\..\..\vc\vcvarsall.bat"
SET VCVARS_VER=140
:found_vs2015
if not defined VCINSTALLDIR goto msbuild-not-found
@rem Visual C++ Build Tools 2015 does not define VisualStudioVersion
echo Found MSVS version 14.0
set GYP_MSVS_VERSION=2015
set PLATFORM_TOOLSET=v140
goto msbuild-found

:msbuild-not-found
echo Failed to find a suitable Visual Studio installation.
echo Try to run in a "Developer Command Prompt" or consult
echo https://github.com/nodejs/node/blob/master/BUILDING.md#windows-1
goto exit

:wix-not-found
echo Build skipped. To generate installer, you need to install Wix.
goto run

:msbuild-found

:project-gen
@rem Skip project generation if requested.
if defined noprojgen goto msbuild

@rem Generate the VS project.
call :run-python configure %configure_flags% --dest-cpu=%target_arch% --tag=%TAG%
if errorlevel 1 goto create-msvs-files-failed
if not exist node.sln goto create-msvs-files-failed
echo Project files generated.

:msbuild
@rem Skip build if requested.
if defined nobuild goto sign

@rem Build the sln with msbuild.
set "msbcpu=/m:2"
if "%NUMBER_OF_PROCESSORS%"=="1" set "msbcpu=/m:1"
set "msbplatform=Win32"
if "%target_arch%"=="x64" set "msbplatform=x64"
msbuild node.sln %msbcpu% /t:%target% /p:Configuration=%config% /p:Platform=%msbplatform% /clp:NoSummary;NoItemAndPropertyList;Verbosity=minimal /nologo
if errorlevel 1 goto exit
if "%target%" == "Clean" goto exit

:sign
@rem Skip signing unless the `sign` option was specified.
if not defined sign goto licensertf

call tools\sign.bat Release\node.exe
if errorlevel 1 echo Failed to sign exe&goto exit

:licensertf
@rem Skip license.rtf generation if not requested.
if not defined licensertf goto package

%config%\node tools\license2rtf.js < LICENSE > %config%\license.rtf
if errorlevel 1 echo Failed to generate license.rtf&goto exit

:package
if not defined package goto msi
echo Creating package...
cd Release
mkdir node-v%FULLVERSION%-win-%target_arch% > nul 2> nul
mkdir node-v%FULLVERSION%-win-%target_arch%\node_modules > nul 2>nul

copy /Y node.exe node-v%FULLVERSION%-win-%target_arch%\ > nul
if errorlevel 1 echo Cannot copy node.exe && goto package_error
copy /Y ..\LICENSE node-v%FULLVERSION%-win-%target_arch%\ > nul
if errorlevel 1 echo Cannot copy LICENSE && goto package_error
copy /Y ..\README.md node-v%FULLVERSION%-win-%target_arch%\ > nul
if errorlevel 1 echo Cannot copy README.md && goto package_error
copy /Y ..\CHANGELOG.md node-v%FULLVERSION%-win-%target_arch%\ > nul
if errorlevel 1 echo Cannot copy CHANGELOG.md && goto package_error
robocopy /e ..\deps\npm node-v%FULLVERSION%-win-%target_arch%\node_modules\npm > nul
if errorlevel 8 echo Cannot copy npm package && goto package_error
copy /Y ..\deps\npm\bin\npm node-v%FULLVERSION%-win-%target_arch%\ > nul
if errorlevel 1 echo Cannot copy npm && goto package_error
copy /Y ..\deps\npm\bin\npm.cmd node-v%FULLVERSION%-win-%target_arch%\ > nul
if errorlevel 1 echo Cannot copy npm.cmd && goto package_error
copy /Y ..\deps\npm\bin\npx node-v%FULLVERSION%-win-%target_arch%\ > nul
if errorlevel 1 echo Cannot copy npx && goto package_error
copy /Y ..\deps\npm\bin\npx.cmd node-v%FULLVERSION%-win-%target_arch%\ > nul
if errorlevel 1 echo Cannot copy npx.cmd && goto package_error
copy /Y ..\tools\msvs\nodevars.bat node-v%FULLVERSION%-win-%target_arch%\ > nul
if errorlevel 1 echo Cannot copy nodevars.bat && goto package_error
if not defined noetw (
    copy /Y ..\src\res\node_etw_provider.man node-v%FULLVERSION%-win-%target_arch%\ > nul
    if errorlevel 1 echo Cannot copy node_etw_provider.man && goto package_error
)
if not defined noperfctr (
    copy /Y ..\src\res\node_perfctr_provider.man node-v%FULLVERSION%-win-%target_arch%\ > nul
    if errorlevel 1 echo Cannot copy node_perfctr_provider.man && goto package_error
)

echo Creating node-v%FULLVERSION%-win-%target_arch%.7z
del node-v%FULLVERSION%-win-%target_arch%.7z > nul 2> nul
7z a -r -mx9 -t7z node-v%FULLVERSION%-win-%target_arch%.7z node-v%FULLVERSION%-win-%target_arch% > nul
if errorlevel 1 echo Cannot create node-v%FULLVERSION%-win-%target_arch%.7z && goto package_error

echo Creating node-v%FULLVERSION%-win-%target_arch%.zip
del node-v%FULLVERSION%-win-%target_arch%.zip > nul 2> nul
7z a -r -mx9 -tzip node-v%FULLVERSION%-win-%target_arch%.zip node-v%FULLVERSION%-win-%target_arch% > nul
if errorlevel 1 echo Cannot create node-v%FULLVERSION%-win-%target_arch%.zip && goto package_error

echo Creating node_pdb.7z
del node_pdb.7z > nul 2> nul
7z a -mx9 -t7z node_pdb.7z node.pdb > nul

echo Creating node_pdb.zip
del node_pdb.zip  > nul 2> nul
7z a -mx9 -tzip node_pdb.zip node.pdb > nul

cd ..
echo Package created!
goto package_done
:package_error
cd ..
exit /b 1
:package_done

:msi
@rem Skip msi generation if not requested
if not defined msi goto run

:msibuild
echo Building node-v%FULLVERSION%-%target_arch%.msi
msbuild "%~dp0tools\msvs\msi\nodemsi.sln" /m /t:Clean,Build /p:PlatformToolset=%PLATFORM_TOOLSET% /p:GypMsvsVersion=%GYP_MSVS_VERSION% /p:Configuration=%config% /p:Platform=%target_arch% /p:NodeVersion=%NODE_VERSION% /p:FullVersion=%FULLVERSION% /p:DistTypeDir=%DISTTYPEDIR% %noetw_msi_arg% %noperfctr_msi_arg% /clp:NoSummary;NoItemAndPropertyList;Verbosity=minimal /nologo
if errorlevel 1 goto exit

if not defined sign goto upload
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
ssh -F %SSHCONFIG% %STAGINGSERVER% "mkdir -p nodejs/%DISTTYPEDIR%/v%FULLVERSION%/win-%target_arch%"
scp -F %SSHCONFIG% Release\node.exe %STAGINGSERVER%:nodejs/%DISTTYPEDIR%/v%FULLVERSION%/win-%target_arch%/node.exe
scp -F %SSHCONFIG% Release\node.lib %STAGINGSERVER%:nodejs/%DISTTYPEDIR%/v%FULLVERSION%/win-%target_arch%/node.lib
scp -F %SSHCONFIG% Release\node_pdb.zip %STAGINGSERVER%:nodejs/%DISTTYPEDIR%/v%FULLVERSION%/win-%target_arch%/node_pdb.zip
scp -F %SSHCONFIG% Release\node_pdb.7z %STAGINGSERVER%:nodejs/%DISTTYPEDIR%/v%FULLVERSION%/win-%target_arch%/node_pdb.7z
scp -F %SSHCONFIG% Release\node-v%FULLVERSION%-win-%target_arch%.7z %STAGINGSERVER%:nodejs/%DISTTYPEDIR%/v%FULLVERSION%/node-v%FULLVERSION%-win-%target_arch%.7z
scp -F %SSHCONFIG% Release\node-v%FULLVERSION%-win-%target_arch%.zip %STAGINGSERVER%:nodejs/%DISTTYPEDIR%/v%FULLVERSION%/node-v%FULLVERSION%-win-%target_arch%.zip
scp -F %SSHCONFIG% node-v%FULLVERSION%-%target_arch%.msi %STAGINGSERVER%:nodejs/%DISTTYPEDIR%/v%FULLVERSION%/
ssh -F %SSHCONFIG% %STAGINGSERVER% "touch nodejs/%DISTTYPEDIR%/v%FULLVERSION%/node-v%FULLVERSION%-%target_arch%.msi.done nodejs/%DISTTYPEDIR%/v%FULLVERSION%/node-v%FULLVERSION%-win-%target_arch%.zip.done nodejs/%DISTTYPEDIR%/v%FULLVERSION%/node-v%FULLVERSION%-win-%target_arch%.7z.done nodejs/%DISTTYPEDIR%/v%FULLVERSION%/win-%target_arch%.done && chmod -R ug=rw-x+X,o=r+X nodejs/%DISTTYPEDIR%/v%FULLVERSION%/node-v%FULLVERSION%-%target_arch%.* nodejs/%DISTTYPEDIR%/v%FULLVERSION%/win-%target_arch%*"

:run
@rem Run tests if requested.

@rem Build test/gc add-on if required.
if "%build_testgc_addon%"=="" goto build-addons
%node_gyp_exe% rebuild --directory="%~dp0test\gc" --nodedir="%~dp0."
if errorlevel 1 goto build-testgc-addon-failed
goto build-addons

:build-testgc-addon-failed
echo Failed to build test/gc add-on."
goto exit

:build-addons
if not defined build_addons goto build-addons-napi
if not exist "%node_exe%" (
  echo Failed to find node.exe
  goto build-addons-napi
)
echo Building addons
:: clear
for /d %%F in (test\addons\??_*) do (
  rd /s /q %%F
)
:: generate
"%node_exe%" tools\doc\addon-verify.js
if %errorlevel% neq 0 exit /b %errorlevel%
:: building addons
setlocal EnableDelayedExpansion
for /d %%F in (test\addons\*) do (
  %node_gyp_exe% rebuild ^
    --directory="%%F" ^
    --nodedir="%cd%"
  if !errorlevel! neq 0 exit /b !errorlevel!
)

:build-addons-napi
if not defined build_addons_napi goto run-tests
if not exist "%node_exe%" (
  echo Failed to find node.exe
  goto run-tests
)
echo Building addons-napi
:: clear
for /d %%F in (test\addons-napi\??_*) do (
  rd /s /q %%F
)
:: building addons-napi
for /d %%F in (test\addons-napi\*) do (
  %node_gyp_exe% rebuild ^
    --directory="%%F" ^
    --nodedir="%cd%"
)
endlocal
goto run-tests

:run-tests
if defined test_check_deopts goto node-check-deopts
if defined test_node_inspect goto node-test-inspect
goto node-tests

:node-check-deopts
call :run-python tools\test.py --mode=release --check-deopts parallel sequential -J
if defined test_node_inspect goto node-test-inspect
goto node-tests

:node-test-inspect
set USE_EMBEDDED_NODE_INSPECT=1
%config%\node tools\test-npm-package.js --install deps\node-inspect test
goto node-tests

:node-tests
if "%test_args%"=="" goto test-v8
if "%config%"=="Debug" set test_args=--mode=debug %test_args%
if "%config%"=="Release" set test_args=--mode=release %test_args%
echo running 'cctest %cctest_args%'
"%config%\cctest" %cctest_args%
REM when building a static library there's no binary to run tests
if defined enable_static goto test-v8
call :run-python tools\test.py %test_args%

:test-v8
if not defined custom_v8_test goto cpplint
call tools/test-v8.bat
if errorlevel 1 goto exit
goto cpplint

:cpplint
if not defined cpplint goto jslint
call :run-cpplint src\*.c src\*.cc src\*.h test\addons\*.cc test\addons\*.h test\cctest\*.cc test\cctest\*.h test\gc\binding.cc tools\icu\*.cc tools\icu\*.h
call :run-python tools/check-imports.py
goto jslint

:run-cpplint
if "%*"=="" goto exit
echo running cpplint '%*'
set cppfilelist=
setlocal enabledelayedexpansion
for /f "tokens=*" %%G in ('dir /b /s /a %*') do (
  set relpath=%%G
  set relpath=!relpath:*%~dp0=!
  call :add-to-list !relpath!
)
( endlocal
  set cppfilelist=%localcppfilelist%
)
call :run-python tools/cpplint.py %cppfilelist%
goto exit

:add-to-list
echo %1 | findstr /c:"src\node_root_certs.h"
if %errorlevel% equ 0 goto exit

@rem skip subfolders under /src
echo %1 | findstr /r /c:"src\\.*\\.*"
if %errorlevel% equ 0 goto exit

echo %1 | findstr /r /c:"test\\addons\\[0-9].*_.*\.h"
if %errorlevel% equ 0 goto exit

echo %1 | findstr /r /c:"test\\addons\\[0-9].*_.*\.cc"
if %errorlevel% equ 0 goto exit

set "localcppfilelist=%localcppfilelist% %1"
goto exit

:jslint
if defined enable_static goto exit
if defined jslint_ci goto jslint-ci
if not defined jslint goto exit
if not exist tools\eslint\bin\eslint.js goto no-lint
echo running jslint
%config%\node tools\eslint\bin\eslint.js --cache --rule "linebreak-style: 0" --rulesdir=tools\eslint-rules --ext=.js,.md benchmark doc lib test tools
goto exit

:jslint-ci
echo running jslint-ci
%config%\node tools\jslint.js -J -f tap -o test-eslint.tap benchmark doc lib test tools
goto exit

:no-lint
echo Linting is not available through the source tarball.
echo Use the git repo instead: $ git clone https://github.com/nodejs/node.git
exit /b 1

:create-msvs-files-failed
echo Failed to create vc project files.
goto exit

:help
echo vcbuild.bat [debug/release] [msi] [test/test-ci/test-all/test-uv/test-inspector/test-internet/test-pummel/test-simple/test-message/test-async-hooks/test-v8/test-v8-intl/test-v8-benchmarks/test-v8-all] [clean] [noprojgen] [small-icu/full-icu/without-intl] [nobuild] [sign] [x86/x64] [vs2015/vs2017] [download-all] [enable-vtune] [lint/lint-ci] [no-NODE-OPTIONS]
echo Examples:
echo   vcbuild.bat                : builds release build
echo   vcbuild.bat debug          : builds debug build
echo   vcbuild.bat release msi    : builds release build and MSI installer package
echo   vcbuild.bat test           : builds debug build and runs tests
echo   vcbuild.bat build-release  : builds the release distribution as used by nodejs.org
echo   vcbuild.bat enable-vtune   : builds nodejs with Intel VTune profiling support to profile JavaScript
goto exit

:run-python
call tools\msvs\find_python.cmd
if errorlevel 1 echo Could not find python2 & goto :exit
set cmd1="%VCBUILD_PYTHON_LOCATION%" %*
echo %cmd1%
%cmd1%
exit /b %ERRORLEVEL%

:exit
goto :EOF


rem ***************
rem   Subroutines
rem ***************

:getnodeversion
set NODE_VERSION=
set TAG=
set FULLVERSION=
:: Call as subroutine for validation of python
call :run-python tools\getnodeversion.py > nul
for /F "tokens=*" %%i in ('"%VCBUILD_PYTHON_LOCATION%" tools\getnodeversion.py') do set NODE_VERSION=%%i
if not defined NODE_VERSION (
  echo Cannot determine current version of Node.js
  exit /b 1
)

if not defined DISTTYPE set DISTTYPE=release
if "%DISTTYPE%"=="release" (
  set FULLVERSION=%NODE_VERSION%
  goto distexit
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

:distexit
if not defined DISTTYPEDIR set DISTTYPEDIR=%DISTTYPE%
goto :EOF
