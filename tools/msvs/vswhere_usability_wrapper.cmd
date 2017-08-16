:: Copyright 2017 - Refael Ackermann
:: Distributed under MIT style license
:: See accompanying file LICENSE at https://github.com/node4good/windows-autoconf
:: version: 1.14.0

@if not defined DEBUG_HELPER @ECHO OFF
setlocal
set VSWHERE_REQ=-requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64
set VSWHERE_PRP=-property installationPath
set VSWHERE_LMT=-version "[15.0,16.0)"
SET VSWHERE_ARGS=-latest -products * %VSWHERE_REQ% %VSWHERE_PRP% %VSWHERE_LMT%
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer"
if not exist "%VSWHERE%" set "VSWHERE=%ProgramFiles%\Microsoft Visual Studio\Installer"
if not exist "%VSWHERE%" exit /B 1
set Path=%Path%;%VSWHERE%
for /f "usebackq tokens=*" %%i in (`vswhere %VSWHERE_ARGS%`) do (
    endlocal
    set "VCINSTALLDIR=%%i\VC\"
    set "VS150COMNTOOLS=%%i\Common7\Tools\"
    exit /B 0)
