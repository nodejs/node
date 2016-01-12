::-------------------------------------------------------------------------------------------------------
:: Copyright (C) Microsoft. All rights reserved.
:: Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
::-------------------------------------------------------------------------------------------------------

@echo off
setlocal
set _FILE=
set _HASERROR=0
:ParseArg
IF "%1" == "" (
    goto :DoneParse
)

IF "%1" == "build" (
    set _BUILD=1
    goto :Parse1
)

IF EXIST "%1"  (
    set _FILE=%_FILE% %1
    goto :Parse1
)

echo ERROR: Invalid parameter %1
exit /B 1

:Parse1
shift /1
goto :ParseArg

:DoneParse
set _RZARG=%_RazzleArguments%
set _RZARG=%_RZARG:amd64=%
set _RZARG=%_RZARG:x86=%
set _RZARG=%_RZARG:chk=%
set _RZARG=%_RZARG:fre=%
IF "%_BUILD%" == "1" (
    echo Building x86chk
    start /WAIT /I %WINDIR%\SysWOW64\cmd.exe /C "%~dp0\BuildValidPointersMap.cmd %RazzleToolPath%\razzle %_RZARG% x86chk"
    IF EXIST "%JSCRIPT_ROOT%\buildchk.err" (
        type %JSCRIPT_ROOT%\buildchk.err
    )
    echo Building amd64chk
    start /WAIT /I %WINDIR%\SysWOW64\cmd.exe /C "%~dp0\BuildValidPointersMap.cmd %RazzleToolPath%\razzle %_RZARG% amd64chk"
    IF EXIST "%JSCRIPT_ROOT%\buildchk.err" (
        type %JSCRIPT_ROOT%\buildchk.err
    )
)

IF "%_FILE%" == "" (
    set "_FILE=vpm"
)

for /D %%i IN (%_FILE%) DO (
    call :GenerateValidPointersMapHeader %%i
)
exit /B %_HASERROR%


:GenerateValidPointersMapHeader
set _BASE_PATH=%_NTTREE%
set _BASE_PATH=%_BASE_PATH:.x86chk=%
set _BASE_PATH=%_BASE_PATH:.amd64chk=%
set _BASE_PATH=%_BASE_PATH:.x86fre=%
set _BASE_PATH=%_BASE_PATH:.amd64fre=%
set _BASE_PATH=%_BASE_PATH:.armfre=%
set _BASE_PATH=%_BASE_PATH:.armfre=%

echo Generating %1.32b.h
call :Generate %1 x86 %1.32b.h
echo Generating %1.64b.h
call :Generate %1 amd64 %1.64b.h
exit /B 0

:Generate
sd edit %3 > nul
echo "// do nothing" > dummy.js
%_BASE_PATH%.%2chk\jscript\jshost -GenerateValidPointersMapHeader:%3 dummy.js
if "%errorlevel%" NEQ "0" (
    Echo %1: Error generating ValidPointersMap header. Ensure %3 writable.
    set _HASERROR=1
) ELSE (
    Echo ValidPointersMap header generated. Please rebuild to incorporate the new definition.
)
del dummy.js
