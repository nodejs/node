# Node.js Bootstrapping Guide

## Windows

A [Boxstarter][] script can be used for easy setup of Windows systems with all
the required prerequisites for Node.js development. This script will install
the following [Chocolatey] packages:
  * [Git for Windows][] with the `git` and Unix tools added to the `PATH`
  * [Python 2.x][]
  * [Visual Studio 2017 Build Tools][] with [Visual C++ workload][]
  * [NetWide Assembler][]

To install Node.js prerequisites using [Boxstarter WebLauncher][], just open
[this link](http://boxstarter.org/package/nr/url?https://raw.githubusercontent.com/nodejs/node/master/tools/bootstrap/windows_boxstarter)
with Internet Explorer or Edge browser on the target machine.

Alternatively, you can use PowerShell. Run those commands from an elevated
PowerShell terminal:
```console
Set-ExecutionPolicy Unrestricted -Force
iex ((New-Object System.Net.WebClient).DownloadString('http://boxstarter.org/bootstrapper.ps1'))
get-boxstarter -Force
Install-BoxstarterPackage https://raw.githubusercontent.com/nodejs/node/master/tools/bootstrap/windows_boxstarter -DisableReboots
```

Entire installation will take up about 8 GB of disk space.

## Linux

For building Node.js on Linux, following packages are required (note, that this
can vary from distribution to distribution):
  * `git`
  * `python`
  * `gcc-c++` or `g++`
  * `make`

To bootstrap Node.js on Linux, run in terminal:
  * OpenSUSE: `sudo zypper install git python gcc-c++ make`
  * Fedora: `sudo dnf install git python gcc-c++ make`
  * Ubuntu, Debian: `sudo apt-get install git python g++ make`

## macOS

To install required tools on macOS, run in terminal:
```console
xcode-select --install
```

[Boxstarter]: http://boxstarter.org/
[Boxstarter WebLauncher]: http://boxstarter.org/WebLauncher
[Chocolatey]: https://chocolatey.org/
[Git for Windows]: https://chocolatey.org/packages/git
[Python 2.x]: https://chocolatey.org/packages/python2
[Visual Studio 2017 Build Tools]: https://chocolatey.org/packages/visualstudio2017buildtools
[Visual C++ workload]: https://chocolatey.org/packages/visualstudio2017-workload-vctools
[NetWide Assembler]: https://chocolatey.org/packages/nasm
