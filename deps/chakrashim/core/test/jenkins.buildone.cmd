::-------------------------------------------------------------------------------------------------------
:: Copyright (C) Microsoft. All rights reserved.
:: Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
::-------------------------------------------------------------------------------------------------------

@echo off
setlocal

if not "%JENKINS_BUILD%" == "True" (
    echo This script should be run under a Jenkins Build environment
    exit /b 2
)

if "%_ENTRY_SCRIPT_NAME%"=="" (
    set _ENTRY_SCRIPT_NAME=%0
)

pushd %~dp0

call jenkins.build.init.cmd %*

set _BuildArch=
set _BuildType=

call jenkins.build.cmd %JENKINS_BUILD_ARGS%

popd

endlocal
