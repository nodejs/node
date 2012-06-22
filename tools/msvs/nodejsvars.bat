@echo off

rem Ensure this Node.js and NPM are first in the PATH
set PATH=%APPDATA%\npm;%~dp0;%PATH%

rem Figure out node version and architecture and print it.
setlocal
pushd "%~dp0"
set print_version=.\node.exe -p -e "process.versions.node + ' (' + process.arch + ')'"
for /F "usebackq delims=" %%v in (`%print_version%`) do set version=%%v
echo Your environment has been set up for using Node.js %version% and NPM
popd
endlocal

rem If we're in the node.js directory, change to the user's home dir.
if "%CD%\"=="%~dp0" cd /d "%HOMEDRIVE%%HOMEPATH%"
