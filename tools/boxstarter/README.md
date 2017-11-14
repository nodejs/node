# Boxstarter setup for Node.js

A [Boxstarter][] script can be used for an easy setup of Windows systems with all
the required prerequisites for Node.js development. This script will install
the following [Chocolatey] packages:
  * [Git for Windows][] with the `git` and Unix tools added to the `PATH`
  * [Python 2.x][]
  * [Visual Studio 2017 Build Tools][] with [Visual C++ workload][]
  * Optionally: [Visual Studio Code][] e.g. when setting up Code and Learn boxes

## WebLauncher Installation

To install Node.js prerequisites using [Boxstarter WebLauncher][], just open
**one** of  the following links with Internet Explorer or Edge browser on the
target machine:
  * [Node.js prerequisites][]
  * [Node.js prerequisites with VS Code][]

## PowerShell Installation

To install the Node.js prerequisites using PowerShell, first install Boxstarter:

 * From PowerShell v2 (Windows 7, Windows Server 2008):  
```console
iex ((New-Object System.Net.WebClient).DownloadString('http://boxstarter.org/bootstrapper.ps1')); get-boxstarter -Force
```

 * From PowerShell v3 (newer Windows systems):  
```console
. { iwr -useb http://boxstarter.org/bootstrapper.ps1 } | iex; get-boxstarter -Force
```

After Boxstarter has been successfully installed, run:

 * For Node.js prerequisites:  
```console
Install-BoxstarterPackage https://raw.githubusercontent.com/nodejs/node/master/tools/boxstarter/node_boxstarter -DisableReboots
```

 * For Node.js prerequisites with VS Code:  
```console
Install-BoxstarterPackage https://raw.githubusercontent.com/nodejs/node/master/tools/boxstarter/node_boxstarter_vscode -DisableReboots
```

[Boxstarter]: http://boxstarter.org/
[Boxstarter WebLauncher]: http://boxstarter.org/WebLauncher
[Chocolatey]: https://chocolatey.org/
[Git for Windows]: https://chocolatey.org/packages/git
[Python 2.x]: https://chocolatey.org/packages/python2
[Visual Studio 2017 Build Tools]: https://chocolatey.org/packages/visualstudio2017buildtools
[Visual C++ workload]: https://chocolatey.org/packages/visualstudio2017-workload-vctools
[Visual Studio Code]: https://chocolatey.org/packages/visualstudiocode
[Node.js prerequisites]: http://boxstarter.org/package/nr/url?https://raw.githubusercontent.com/nodejs/node/master/tools/boxstarter/node_boxstarter
[Node.js prerequisites with VS Code]: http://boxstarter.org/package/nr/url?https://raw.githubusercontent.com/nodejs/node/master/tools/boxstarter/node_boxstarter_vscode
