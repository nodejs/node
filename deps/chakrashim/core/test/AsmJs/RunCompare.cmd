::-------------------------------------------------------------------------------------------------------
:: Copyright (C) Microsoft. All rights reserved.
:: Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
::-------------------------------------------------------------------------------------------------------

@echo off
setlocal ENABLEDELAYEDEXPANSION

set bin=%_nttree%\jscript\jshost.exe
set test=
set args=
set failedTests=
:loop
IF NOT "%1"=="" (
    IF "%1"=="-bin" (
        SET bin=%2
        SHIFT
    )
    IF "%1"=="-test" (
        set test=%2
        SHIFT
    )
    IF "%1"=="-args" (
        set args=%2
        SHIFT
    )
    SHIFT
    GOTO :loop
)

if [%test%] == [] (
    for %%i in (*.*) do (
        set ext=%%~xi
        if !ext! == .js (
            call:RunTestFile %%~ni
        )
    )
) else (
    call:RunTestFile %test%
)
if [!failedTests!] == [] (
    echo.
    echo All test passed !
) else (
    echo.
    echo Failed tests
    for %%i in (!failedTests!) do (
        echo     %%i
        echo         %cd%\%%i.diffCmp
    )
)

goto:eof

:RunTestFile
set filename=%1
echo.
echo Running tests !filename!
%bin% -bvt -off:asmjs -off:globopt !filename!.js > !filename!.interpreter
%bin% -bvt -on:asmjs -bgjit- -lic:1  !filename!.js %args% > !filename!.jit

echo   Checking diff for test !filename!
git diff !filename!.interpreter !filename!.jit > !filename!.diffCmp
set /p diffContent=<!filename!.diffCmp

if [!diffContent!] == [] (
    echo     Success
    del !filename!.diffCmp
    del !filename!.jit
    del !filename!.interpreter
) else (
    set failedTests=!failedTests!,!filename!
    echo     Error check !filename!.diffCmp
)
set diffContent=
goto:eof
