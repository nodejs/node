@echo off

@rem Ensure this Node.js is first in the PATH
set PATH=%~dp0;%PATH%

@rem Figure out architecture and print it.
setlocal
for /F "usebackq delims=" %%v in (`"%~dp0"node.exe -p -e process.arch`) do set arch=%%v
echo Your environment has been set up for using Node.js (%arch%) and NPM
endlocal

@rem Go to the user's home directory
cd /d %HOMEDRIVE%"%HOMEPATH%"
