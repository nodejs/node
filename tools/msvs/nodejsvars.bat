@echo off
@rem Process arguments.
set target_arch=x86

:next-arg
if "%1"=="" goto args-done
if /i "%1"=="x86"           set target_arch=x86&goto arg-ok
if /i "%1"=="x64"           set target_arch=x64&goto arg-ok

echo Warning: ignoring invalid command line option `%1`.

:arg-ok
shift
goto next-arg
:args-done

@rem Ensure this Node.js is first in the PATH
echo Your environment has been set up for using Node.js (%target_arch%) and NPM
set PATH=%~dp0;%PATH%