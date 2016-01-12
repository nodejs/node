::-------------------------------------------------------------------------------------------------------
:: Copyright (C) Microsoft. All rights reserved.
:: Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
::-------------------------------------------------------------------------------------------------------

@echo off
setlocal

REM set TEMP and TMP to a new temp folder under the WORKSPACE and create it
set TEMP=%WORKSPACE%\TEMP
set TMP=%TEMP%
REM create the TMP folder if it doesn't exist
if not exist %TEMP% (
    mkdir %TEMP%
)

if "%_ENTRY_SCRIPT_NAME%"=="" (
    set _ENTRY_SCRIPT_NAME=%0
)

REM check that we have enough parameters
if "%1"=="" (
    goto :usage
)
if "%2"=="" (
    goto :usage
)

:: ============================================================================
:: Main script
:: ============================================================================
:main

    set JENKINS_BUILD=True
    call test\jenkins.buildone.cmd %*

    goto :end

:: ============================================================================
:: Not enough params
:: ============================================================================
:usage

    echo Not enough parameters. Please specify architecture and type.
    echo Examples:
    echo.
    echo     %_ENTRY_SCRIPT_NAME% x86 debug
    echo     %_ENTRY_SCRIPT_NAME% x86 test
    echo     %_ENTRY_SCRIPT_NAME% x86 release
    echo.
    echo     %_ENTRY_SCRIPT_NAME% x64 debug
    echo     %_ENTRY_SCRIPT_NAME% x64 test
    echo     %_ENTRY_SCRIPT_NAME% x64 release

    goto :end

:: ============================================================================
:: Epilogue of script (cleanup)
:: ============================================================================
:end
endlocal
