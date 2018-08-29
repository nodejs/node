@echo off

echo ====================================================
echo Tools for Node.js Native Modules Installation Script
echo ====================================================
echo.
echo This Boxstarter script will install Python and the Visual Studio Build Tools,
echo necessary to compile Node.js native modules. All necessary Windows updates
echo will also be installed.
echo.
echo This will require about 3 Gb of free disk space, plus any space necessary to
echo install Windows updates.
echo.
echo This will take a while to run. Your computer may reboot during the
echo installation, and will resume automatically.
echo.
echo Please close all open programs for the duration of the installation.
echo.
echo You can close this window to stop now. This script can be invoked from the
echo Start menu. Detailed instructions to install these tools manually are
echo available at https://github.com/nodejs/node-gyp#on-windows
echo.
pause

"%SystemRoot%\System32\WindowsPowerShell\v1.0\powershell.exe" -NoProfile -InputFormat None -ExecutionPolicy Bypass -Command "iex ((New-Object System.Net.WebClient).DownloadString('https://boxstarter.org/bootstrapper.ps1')); get-boxstarter -Force; Install-BoxstarterPackage -PackageName '%~dp0\install_tools.txt'"
