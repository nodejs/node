::-------------------------------------------------------------------------------------------------------
:: Copyright (C) Microsoft. All rights reserved.
:: Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
::-------------------------------------------------------------------------------------------------------

@echo off
setlocal
set _HASERROR=0

if "%1"=="-nojit" (
    set _suffix=.nojit
    shift
)

:: This script will expect ch.exe to be built for x86_debug and x64_debug
set _BinLocation=%~dp0..\..\..\..\Build\VcBuild%_suffix%\bin
if "%OutBaseDir%" NEQ "" (
  set _BinLocation=%OutBaseDir%\Chakra.Core%_suffix%\bin
)

if not exist %_BinLocation%\x86_debug\ch.exe (
    echo Error: %_BinLocation%\x86_debug\ch.exe not found, please build sources. Exiting ...
    exit /b 1
)

if not exist %_BinLocation%\x64_debug\ch.exe (
    echo Error: %_BinLocation%\x64_debug\ch.exe not found, please build sources. Exiting ...
    exit /b 1
)

if "%_FILE%" == "" (
    set "_FILE=%~dp0*.js"
)

for %%i in (%_FILE%) do (
    call :GenerateLibraryByteCodeHeader %%i
)
exit /B %_HASERROR%

:GenerateLibraryBytecodeHeader

echo Generating %1%_suffix%.bc.32b.h
call :Generate %1 %_BinLocation%\x86_debug %1%_suffix%.bc.32b.h
echo Generating %1%_suffix%.bc.64b.h
call :Generate %1 %_BinLocation%\x64_debug %1%_suffix%.bc.64b.h
exit /B 0

:Generate
%2\ch.exe -GenerateLibraryByteCodeHeader:%3 -Intl %1
if "%errorlevel%" NEQ "0" (
    echo %1: Error generating bytecode file. Ensure %3 writable.
    set _HASERROR=1
) else (
    echo Bytecode generated. Please rebuild to incorporate the new bytecode.
)
