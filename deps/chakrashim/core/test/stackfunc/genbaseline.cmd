::-------------------------------------------------------------------------------------------------------
:: Copyright (C) Microsoft. All rights reserved.
:: Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
::-------------------------------------------------------------------------------------------------------

@echo off
setlocal
IF "%1" == "" (
    set _BIN=%_NTTREE%\jscript\jc.exe
) ELSE (
    set _BIN=%1
)
for /F "usebackq" %%i in (`dir/b *.js`) DO (
    call :Gen %%i
)
del /Q __temp.out
exit /B 0

:Gen
IF /I "%1" == "propname.js" (
    set _VERSION=1
) ELSE IF /I "%1" == "multi_funcname.js" (
    set _VERSION=1
) ELSE (
    set _VERSION=5
)
IF EXIST "%~n1.baseline" (
    call :Gen1 %1 %~n1.baseline
)
IF EXIST "%~n1.deferparse.baseline" (
    call :Gen1 %1 %~n1.deferparse.baseline -forcedeferparse
)

IF EXIST "%~n1.baseline.v2" (
    set _VERSION=2
    call :Gen1 %1 %~n1.baseline.v2
)

IF EXIST "%~n1.deferparse.baseline.v2" (
    set _VERSION=2
    call :Gen1 %1 %~n1.deferparse.baseline.v2 -forcedeferparse
)

exit /B 0

:Gen1
%_BIN% %1 -on:stackfunc -testtrace:stackfunc -bvt -version:%_VERSION% %3 -maxinterpretcount:1 -bgjit- > __temp.out
fc %2 __temp.out > nul
IF NOT "%ERRORLEVEL%" == "0" (
    sd edit %2
    copy __temp.out %2
)
