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

set "_msbuildArgs="

:ContinueArgParse
if not [%1]==[] (
    :: _BuildArch
    if [%1]==[x86] (
        set _BuildArch=x86
        goto :ContinueArgParseEnd
    ) else if [%1]==[x64] (
        set _BuildArch=x64
        goto :ContinueArgParseEnd
    ) else if [%1]==[arm] (
        set _BuildArch=arm
        goto :ContinueArgParseEnd
    )

    :: _BuildArch (deprecated name)
    if [%1]==[amd64] (
        set _BuildArch=x64
        goto :ContinueArgParseEnd
    )

    :: _BuildType (new names)
    if [%1]==[debug] (
        set _BuildType=chk
        goto :ContinueArgParseEnd
    ) else if [%1]==[test] (
        set _BuildType=test
        goto :ContinueArgParseEnd
    ) else if [%1]==[release] (
        set _BuildType=fre
        goto :ContinueArgParseEnd
    )

    :: _BuildType (old names)
    if [%1]==[chk] (
        set _BuildType=chk
        goto :ContinueArgParseEnd
    ) else if [%1]==[fre] (
        set _BuildType=fre
        goto :ContinueArgParseEnd
    )

    :: _targets
    if /i [%1] EQU [/c] (
        set _targets=/t:Clean,Build
        goto :ContinueArgParseEnd
    )

    :: DEFAULT - add any other params to %_msBuildArgs%
    :: _msbuildArgs
    if [%1] NEQ [] (
        set _msbuildArgs=%_msbuildArgs% %1
        goto :ContinueArgParseEnd
    )

    :ContinueArgParseEnd
    shift
    goto :ContinueArgParse
)

if "%_BuildArch%"=="" (
    goto :invalidBuildArch
)
if "%_BuildType%"=="" (
    goto :invalidBuildType
)

if "%_LoggingParams%" EQU "" (
    set _LoggingParams=/fl1 /flp1:logfile=build_%_BuildArch%%_BuildType%.log;verbosity=normal /fl2 /flp2:logfile=build_%_BuildArch%%_BuildType%.err;errorsonly /fl3 /flp3:logfile=build_%_BuildArch%%_BuildType%.wrn;warningsonly
)

set _ChakraBuildConfig=
if "%_BuildType%" EQU "chk" (
    set _ChakraBuildConfig=Debug
) else if "%_BuildType%" EQU "fre" (
    set _ChakraBuildConfig=Release
) else if "%_BuildType%" EQU "test" (
    set _ChakraBuildConfig=Test
) else (
    echo WARNING: Unknown build type '%_BuildType%'
)

set "_msbuildProj="
set _ChakraSolution=%REPO_ROOT%\Build\Chakra.Core.sln
set _ChakraConfiguration=all
set _CoreBuild=1

if "%_CoreBuild%" EQU "0" (
    set _ChakraBuildConfig=%_ChakraConfiguration%-%_ChakraBuildConfig%
)

echo MSBuildArgs are %_msBuildArgs%

echo msbuild %_msBuildArgs% /m /p:Configuration=%_ChakraBuildConfig% /p:Platform=%_BuildArch% %_ChakraSolution% %_msbuildProj% %_LoggingParams% %_targets% /verbosity:minimal /nr:false

msbuild %_msBuildArgs% /m /p:Configuration=%_ChakraBuildConfig% /p:Platform=%_BuildArch% %_ChakraSolution% %_msbuildProj% %_LoggingParams% %_targets% /verbosity:minimal /nr:false

goto :end

:: ============================================================================
:: Invald Build Arch
:: ============================================================================
:invalidBuildArch

    echo None of the parameters were a valid build architecture. Please specify both architecture and type.
    goto :examples

:: ============================================================================
:: Invald Build Type
:: ============================================================================
:invalidBuildType

    echo None of the parameters were a valid build type. Please specify both architecture and type.
    goto :examples

:: ============================================================================
:: Examples
:: ============================================================================
:examples

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
