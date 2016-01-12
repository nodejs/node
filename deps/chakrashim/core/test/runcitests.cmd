::-------------------------------------------------------------------------------------------------------
:: Copyright (C) Microsoft. All rights reserved.
:: Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
::-------------------------------------------------------------------------------------------------------
:: ============================================================================
::
:: runcitests.cmd
::
:: Runs tests for continuous integration.  This script is called from the VSO
:: build and it runs all tests for x86 and x64, debug and test build configs.
:: Logs are copied to build drop.
::
:: Do not use this script to run all tests on your dev box.
::   - It will delete all your existing test logs
::   - It does not run the various flavors of the tests in parallel (though
::     this is not currently possible anyway because rl stages logs in a
::     common directory)
::   - It does nothing to provide useful output when there are failures, e.g.
::     they can be buried under thousands of lines of output from further
::     tests run.
::   - It cannot be cancelled without risk of polluting your command prompt
::     environment with environment variables that will make further calls to
::     runtests.cmd behave unexpectedly.
::   - It will copy the logs to a folder named \testlogs in the directory you
::     started this script from.
::
:: ============================================================================
@echo off
setlocal

set _RootDir=%~dp0..
set _StagingDir=%TF_BUILD_BINARIESDIRECTORY%
set _DropRootDir=%TF_BUILD_DROPLOCATION%
set _HadFailures=0

:: ============================================================================
:: Main script
:: ============================================================================
:main

  call :parseArgs %*

  if not "%fShowUsage%" == "" (
    call :printUsage
    goto :eof
  )

  call :validateArgs

  if not "%fShowGetHelp%" == "" (
    call :printGetHelp
    goto :eof
  )

  if not "%TF_BUILD%" == "True" (
    echo Error: TF_BUILD environment variable is not set to "True".
    echo   This script must be run under a TF Build Agent environment.
    exit /b 2
  )

  :: Cannot run tests for arm on build machine and release builds
  :: do not work with ch.exe so no-op those configurations.
  :: Include _RunAll in the check because if it is specified it
  :: should trump this early out.
  if "%_RunAll%%_BuildArch%" == "arm" goto :noTests
  if "%_RunAll%%_BuildType%" == "release" goto :noTests

  pushd %_RootDir%\test
  set _TestDir=%CD%

  call :doSilent rd /s/q %_TestDir%\logs

  if not "%_RunAll%" == "" (
    call :runTests x86 debug
    call :runTests x86 test
    call :runTests x64 debug
    call :runTests x64 test

    call :summarizeLogs summary.log
  ) else (
    call :runTests %_BuildArch% %_BuildType%
    call :summarizeLogs summary.%_BuildArch%%_BuildType%.log
  )

  call :copyLogsToDrop

  echo.
  if "%_HadFailures%" == "1" (
    echo -- runcitests.cmd ^>^> Tests failed! 1>&2
  ) else (
    echo -- runcitests.cmd ^>^> Tests passed!
  )
  echo -- runcitests.cmd ^>^> Logs at %_DropRootDir%\testlogs

  popd

  exit /b %_HadFailures%

:noTests

  echo -- runcitests.cmd ^>^> The tests are not supported on this build configuration.
  echo -- runcitests.cmd ^>^> No tests were run.  This is expected.
  echo -- runcitests.cmd ^>^> Configuration: %_BuildArch% %_BuildType%

  exit /b 0

:: ============================================================================
:: Run one test suite against one build config and record if there were errors
:: ============================================================================
:runTests

  call :do %_TestDir%\runtests.cmd -%1%2 -quiet -cleanupall -binDir %_StagingDir%\bin

  if ERRORLEVEL 1 set _HadFailures=1

  goto :eof

:: ============================================================================
:: Copy all result logs to the drop share
:: ============================================================================
:copyLogsToDrop

  :: /S Copy all non-empty dirs
  :: /Y Do not prompt for overwriting destination files
  :: /C Continue copying if there are errors
  :: /I Assume destination is a directory if it does not exist

  call :do xcopy %_TestDir%\logs %_StagingDir%\testlogs /S /Y /C /I

  goto :eof

:: ============================================================================
:: Summarize the logs into a listing of only the failures
:: ============================================================================
:summarizeLogs

  pushd %_TestDir%\logs
  findstr /sp failed rl.results.log > %1
  rem Echo to stderr so that VSO includes the output in the build summary
  type %1 1>&2
  popd

  goto :eof

:: ============================================================================
:: Print usage
:: ============================================================================
:printUsage

  echo runcitests.cmd -x86^|-x64^|-arm -debug^|-test^|-release
  echo.
  echo Runs tests post-build for automated VSO TFS Builds.
  echo Depends on TFS Build environment.
  echo.
  echo Required switches:
  echo.
  echo   Specify architecture of build to test:
  echo.
  echo   -x86           Build arch of binaries is x86
  echo   -x64           Build arch of binaries is x64
  echo   -arm           Build arch of binaries is ARM
  echo.
  echo   Specify type of of build to test:
  echo.
  echo   -debug         Build type of binaries is debug
  echo   -test          Build type of binaries is test
  echo   -release       Build type of binaries is release
  echo.
  echo   Shorthand combinations can be used, e.g. -x64debug
  echo.
  echo   Note: No tests are run for ARM or release as they are
  echo   not supported.  The switches are provided for tooling
  echo   convenience.

  goto :eof

:: ============================================================================
:: Print how to get help
:: ============================================================================
:printGetHelp

  echo For help use runcitests.cmd -?

  goto :eof

:: ============================================================================
:: Parse the user arguments into environment variables
:: ============================================================================
:parseArgs

  :NextArgument

  if "%1" == "-?" set fShowUsage=1& goto :ArgOk
  if "%1" == "/?" set fShowUsage=1& goto :ArgOk

  if /i "%1" == "-x86"              set _BuildArch=x86&                                         goto :ArgOk
  if /i "%1" == "-x64"              set _BuildArch=x64&                                         goto :ArgOk
  if /i "%1" == "-arm"              set _BuildArch=arm&                                         goto :ArgOk
  if /i "%1" == "-debug"            set _BuildType=debug&                                       goto :ArgOk
  if /i "%1" == "-test"             set _BuildType=test&                                        goto :ArgOk
  if /i "%1" == "-release"          set _BuildType=release&                                     goto :ArgOk

  if /i "%1" == "-x86debug"         set _BuildArch=x86&set _BuildType=debug&                    goto :ArgOk
  if /i "%1" == "-x64debug"         set _BuildArch=x64&set _BuildType=debug&                    goto :ArgOk
  if /i "%1" == "-armdebug"         set _BuildArch=arm&set _BuildType=debug&                    goto :ArgOk
  if /i "%1" == "-x86test"          set _BuildArch=x86&set _BuildType=test&                     goto :ArgOk
  if /i "%1" == "-x64test"          set _BuildArch=x64&set _BuildType=test&                     goto :ArgOk
  if /i "%1" == "-armtest"          set _BuildArch=arm&set _BuildType=test&                     goto :ArgOk
  if /i "%1" == "-x86release"       set _BuildArch=x86&set _BuildType=release&                  goto :ArgOk
  if /i "%1" == "-x64release"       set _BuildArch=x64&set _BuildType=release&                  goto :ArgOk
  if /i "%1" == "-armrelease"       set _BuildArch=arm&set _BuildType=release&                  goto :ArgOk

  if /i "%1" == "-all"              set _RunAll=1&                                              goto :ArgOk

  if not "%1" == "" echo Unknown argument: %1 & set fShowGetHelp=1

  goto :eof

  :ArgOk
  shift

  goto :NextArgument

:: ============================================================================
:: Validate arguments; if non specified default to -all
:: ============================================================================
:validateArgs

  if "%_BuildArch%" == "" (
    set _RunAll=1
  )

  if "%_BuildType%" == "" (
    set _RunAll=1
  )

  goto :eof

:: ============================================================================
:: Echo a command line before executing it
:: ============================================================================
:do

  echo -- runcitests.cmd ^>^> %*
  cmd /s /c "%*"

  goto :eof

:: ============================================================================
:: Echo a command line before executing it and redirect the command's output
:: to nul
:: ============================================================================
:doSilent

  echo -- runcitests.cmd ^>^> %* ^> nul 2^>^&1
  cmd /s /c "%* > nul 2>&1"

  goto :eof
