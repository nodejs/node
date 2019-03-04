# Node.js Bootstrapping Guide

## Windows

A [Boxstarter][] script can be used for easy setup of Windows systems with all
the required prerequisites for Node.js development. This script will install
the following [Chocolatey] packages:
  * [Git for Windows][] with the `git` and Unix tools added to the `PATH`
  * [Python 3.x][] and [legacy Python][]
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

Entire installation will take up about 10 GB of disk space.

### Why install two different versions of Python?
Python 2 will reach its _end-of-life_ at the end of 2019. Afterwards, the
interpreter will not get updates — no bugfixes, no security fixes, nothing. In
the interim, the Python ecosystem is abandoning 2.7 support.
https://python3statement.org/  In order to remain safe and current the Node.js
community is transitioning its Python code to Python 3. Having both versions of
Python in this bootstrap will allow developers and end users to test, benchmark,
and debug Node.js running on both versions to ensure a smooth and complete
transition before the yearend deadline.

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
[Python 3.x]: https://chocolatey.org/packages/python
[legacy Python]: https://chocolatey.org/packages/python2
[Visual Studio 2017 Build Tools]: https://chocolatey.org/packages/visualstudio2017buildtools
[Visual C++ workload]: https://chocolatey.org/packages/visualstudio2017-workload-vctools
[NetWide Assembler]: https://chocolatey.org/packages/nasm
