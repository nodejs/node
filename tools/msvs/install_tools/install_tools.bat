@echo off

cls
echo ====================================================
echo Tools for Node.js Native Modules Installation Script
echo ====================================================
echo.
echo This Boxstarter script will install Python and the Visual Studio Build Tools,
echo necessary to compile Node.js native modules. Note that Boxstarter,
echo Chocolatey and required Windows updates will also be installed.
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

cls
REM Adapted from https://github.com/Microsoft/windows-dev-box-setup-scripts/blob/79bbe5bdc4867088b3e074f9610932f8e4e192c2/README.md#legal
echo Using this script downloads third party software
echo ------------------------------------------------
echo This script will direct to Chocolatey to install packages. By using
echo Chocolatey to install a package, you are accepting the license for the
echo application, executable(s), or other artifacts delivered to your machine as a
echo result of a Chocolatey install. This acceptance occurs whether you know the
echo license terms or not. Read and understand the license terms of the packages
echo being installed and their dependencies prior to installation:
echo - https://chocolatey.org/packages/chocolatey
echo - https://chocolatey.org/packages/boxstarter
echo - https://chocolatey.org/packages/python2
echo - https://chocolatey.org/packages/visualstudio2017buildtools
echo - https://chocolatey.org/packages/visualstudio2017-workload-vctools
echo.
echo This script is provided AS-IS without any warranties of any kind
echo ----------------------------------------------------------------
echo Chocolatey has implemented security safeguards in their process to help
echo protect the community from malicious or pirated software, but any use of this
echo script is at your own risk. Please read the Chocolatey's legal terms of use
echo and the Boxstarter project license as well as how the community repository
echo for Chocolatey.org is maintained.
echo.
echo You can close this window to stop now.
pause

"%SystemRoot%\System32\WindowsPowerShell\v1.0\powershell.exe" -NoProfile -InputFormat None -ExecutionPolicy Bypass -Command Start-Process '%SystemRoot%\System32\WindowsPowerShell\v1.0\powershell.exe' -ArgumentList '-NoProfile -InputFormat None -ExecutionPolicy Bypass -Command iex ((New-Object System.Net.WebClient).DownloadString(''https://boxstarter.org/bootstrapper.ps1'')); get-boxstarter -Force; Install-BoxstarterPackage -PackageName ''%~dp0\install_tools.txt''; Read-Host ''Type ENTER to exit'' ' -Verb RunAs
