::-------------------------------------------------------------------------------------------------------
:: Copyright (C) Microsoft. All rights reserved.
:: Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
::-------------------------------------------------------------------------------------------------------

:: ============================================================================
::
:: jenkins.testall.cmd
::
:: Runs tests for Jenkins continuous integration. This script is called from
:: the Jenkins CI build and it runs all tests for x86 and x64, debug and test
:: build configs.
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
::
:: ============================================================================

@echo off
setlocal

set _RootDir=%~dp0..
set _BinDir=%_RootDir%\Build\VcBuild\bin
set _HadFailures=0

:: ============================================================================
:: Main script
:: ============================================================================
:main

  if not "%JENKINS_BUILD%" == "True" (
    echo This script should be run under a Jenkins Build environment
    exit /b 2
  )

  pushd %_RootDir%\test
  set _TestDir=%CD%

  call jenkins.parsetestargs.cmd %*

  call :doSilent rd /s/q %_TestDir%\logs

  call :runTests x86debug
  call :runTests x86test
  call :runTests x64debug
  call :runTests x64test

  call :summarizeLogs

  echo.
  if "%_HadFailures%" == "1" (
    echo -- jenkins.testall.cmd ^>^> Tests failed! 1>&2
  ) else (
    echo -- jenkins.testall.cmd ^>^> Tests passed!
  )

  popd

  exit /b %_HadFailures%

  goto :eof

:: ============================================================================
:: Run one test suite against one build config and record if there were errors
:: ============================================================================
:runTests

  call :do %_TestDir%\runtests.cmd -%1 -quiet -cleanupall -nottags exclude_jenkins %_ExtraTestArgs% -binDir %_BinDir%

  if ERRORLEVEL 1 set _HadFailures=1

  goto :eof

:: ============================================================================
:: Summarize the logs into a listing of only the failures
:: ============================================================================
:summarizeLogs

  pushd %_TestDir%\logs
  findstr /sp failed rl.results.log > summary.log
  rem Echo to stderr so that VSO includes the output in the build summary
  type summary.log 1>&2
  popd

:: ============================================================================
:: Echo a command line before executing it
:: ============================================================================
:do

  echo -- jenkins.testall.cmd ^>^> %*
  cmd /s /c "%*"

  goto :eof

:: ============================================================================
:: Echo a command line before executing it and redirect the command's output
:: to nul
:: ============================================================================
:doSilent

  echo -- jenkins.testall.cmd ^>^> %* ^> nul 2^>^&1
  cmd /s /c "%* > nul 2>&1"

  goto :eof
