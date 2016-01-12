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

pushd %~dp0

call jenkins.build.init.cmd

set _BuildArch=
set _BuildType=

call jenkins.build.cmd x86 debug
call jenkins.build.cmd x86 test
call jenkins.build.cmd x86 release

call jenkins.build.cmd x64 debug
call jenkins.build.cmd x64 test
call jenkins.build.cmd x64 release

call jenkins.build.cmd arm debug
call jenkins.build.cmd arm test
call jenkins.build.cmd arm release

popd

endlocal
