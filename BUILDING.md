# Building Node.js

Depending on what platform or features you need, the build process may
differ. After you've built a binary, running the
test suite to confirm that the binary works as intended is a good next step.

If you can reproduce a test failure, search for it in the
[Node.js issue tracker](https://github.com/nodejs/node/issues) or
file a new issue.

## Table of contents

* [Supported platforms](#supported-platforms)
  * [Input](#input)
  * [Strategy](#strategy)
  * [Platform list](#platform-list)
  * [Supported toolchains](#supported-toolchains)
  * [Official binary platforms and toolchains](#official-binary-platforms-and-toolchains)
    * [OpenSSL asm support](#openssl-asm-support)
  * [Previous versions of this document](#previous-versions-of-this-document)
* [Building Node.js on supported platforms](#building-nodejs-on-supported-platforms)
  * [Note about Python](#note-about-python)
  * [Unix and macOS](#unix-and-macos)
    * [Unix prerequisites](#unix-prerequisites)
    * [macOS prerequisites](#macos-prerequisites)
    * [Building Node.js](#building-nodejs-1)
    * [Installing Node.js](#installing-nodejs)
    * [Running Tests](#running-tests)
    * [Running Coverage](#running-coverage)
    * [Building the documentation](#building-the-documentation)
    * [Building a debug build](#building-a-debug-build)
    * [Building an ASAN build](#building-an-asan-build)
    * [Speeding up frequent rebuilds when developing](#speeding-up-frequent-rebuilds-when-developing)
    * [Troubleshooting Unix and macOS builds](#troubleshooting-unix-and-macos-builds)
  * [Windows](#windows)
    * [Prerequisites](#prerequisites)
      * [Option 1: Manual install](#option-1-manual-install)
      * [Option 2: Automated install with Boxstarter](#option-2-automated-install-with-boxstarter)
    * [Building Node.js](#building-nodejs-2)
  * [Android/Android-based devices (e.g. Firefox OS)](#androidandroid-based-devices-eg-firefox-os)
* [`Intl` (ECMA-402) support](#intl-ecma-402-support)
  * [Build with full ICU support (all locales supported by ICU)](#build-with-full-icu-support-all-locales-supported-by-icu)
    * [Unix/macOS](#unixmacos)
    * [Windows](#windows-1)
  * [Trimmed: `small-icu` (English only) support](#trimmed-small-icu-english-only-support)
    * [Unix/macOS](#unixmacos-1)
    * [Windows](#windows-2)
  * [Building without Intl support](#building-without-intl-support)
    * [Unix/macOS](#unixmacos-2)
    * [Windows](#windows-3)
  * [Use existing installed ICU (Unix/macOS only)](#use-existing-installed-icu-unixmacos-only)
  * [Build with a specific ICU](#build-with-a-specific-icu)
    * [Unix/macOS](#unixmacos-3)
    * [Windows](#windows-4)
* [Building Node.js with FIPS-compliant OpenSSL](#building-nodejs-with-fips-compliant-openssl)
* [Building Node.js with external core modules](#building-nodejs-with-external-core-modules)
  * [Unix/macOS](#unixmacos-4)
  * [Windows](#windows-5)
* [Note for downstream distributors of Node.js](#note-for-downstream-distributors-of-nodejs)

## Supported platforms

This list of supported platforms is current as of the branch/release to
which it belongs.

### Input

Node.js relies on V8 and libuv. We adopt a subset of their supported platforms.

### Strategy

There are three support tiers:

* **Tier 1**: These platforms represent the majority of Node.js users. The
  Node.js Build Working Group maintains infrastructure for full test coverage.
  Test failures on tier 1 platforms will block releases.
* **Tier 2**: These platforms represent smaller segments of the Node.js user
  base. The Node.js Build Working Group maintains infrastructure for full test
  coverage. Test failures on tier 2 platforms will block releases.
  Infrastructure issues may delay the release of binaries for these platforms.
* **Experimental**: May not compile or test suite may not pass. The core team
  does not create releases for these platforms. Test failures on experimental
  platforms do not block releases. Contributions to improve support for these
  platforms are welcome.

Platforms may move between tiers between major release lines. The table below
will reflect those changes.

### Platform list

Node.js compilation/execution support depends on operating system, architecture,
and libc version. The table below lists the support tier for each supported
combination. A list of [supported compile toolchains](#supported-toolchains) is
also supplied for tier 1 platforms.

**For production applications, run Node.js on supported platforms only.**

Node.js does not support a platform version if a vendor has expired support
for it. In other words, Node.js does not support running on End-of-Life (EoL)
platforms. This is true regardless of entries in the table below.

| Operating System | Architectures    | Versions                        | Support Type | Notes                             |
| ---------------- | ---------------- | ------------------------------- | ------------ | --------------------------------- |
| GNU/Linux        | x64              | kernel >= 3.10, glibc >= 2.17   | Tier 1       | e.g. Ubuntu 16.04 <sup>[1](#fn1)</sup>, Debian 9, EL 7 <sup>[2](#fn2)</sup> |
| GNU/Linux        | x64              | kernel >= 3.10, musl >= 1.1.19  | Experimental | e.g. Alpine 3.8                   |
| GNU/Linux        | x86              | kernel >= 3.10, glibc >= 2.17   | Experimental | Downgraded as of Node.js 10       |
| GNU/Linux        | arm64            | kernel >= 4.5, glibc >= 2.17    | Tier 1       | e.g. Ubuntu 16.04, Debian 9, EL 7 <sup>[3](#fn3)</sup> |
| GNU/Linux        | armv7            | kernel >= 4.14, glibc >= 2.24   | Tier 1       | e.g. Ubuntu 18.04, Debian 9       |
| GNU/Linux        | armv6            | kernel >= 4.14, glibc >= 2.24   | Experimental | Downgraded as of Node.js 12       |
| GNU/Linux        | ppc64le >=power8 | kernel >= 3.10.0, glibc >= 2.17 | Tier 2       | e.g. Ubuntu 16.04 <sup>[1](#fn1)</sup>, EL 7  <sup>[2](#fn2)</sup> |
| GNU/Linux        | s390x            | kernel >= 3.10.0, glibc >= 2.17 | Tier 2       | e.g. EL 7 <sup>[2](#fn2)</sup>    |
| Windows          | x64, x86 (WoW64) | >= Windows 8.1/2012 R2          | Tier 1       | <sup>[4](#fn4),[5](#fn5)</sup>    |
| Windows          | x86 (native)     | >= Windows 8.1/2012 R2          | Tier 1 (running) / Experimental (compiling) <sup>[6](#fn6)</sup> | |
| Windows          | x64, x86         | Windows Server 2012 (not R2)    | Experimental |                                   |
| Windows          | arm64            | >= Windows 10                   | Tier 2 (compiling) / Experimental (running) |    |
| macOS            | x64              | >= 10.13                        | Tier 1       |                                   |
| macOS            | arm64            | >= 11                           | Experimental |                                   |
| SmartOS          | x64              | >= 18                           | Tier 2       |                                   |
| AIX              | ppc64be >=power7 | >= 7.2 TL04                     | Tier 2       |                                   |
| FreeBSD          | x64              | >= 11                           | Experimental | Downgraded as of Node.js 12  <sup>[7](#fn7)</sup>     |

<em id="fn1">1</em>: GCC 8 is not provided on the base platform. Users will
  need the
  [Toolchain test builds PPA](https://launchpad.net/~ubuntu-toolchain-r/+archive/ubuntu/test?field.series_filter=xenial)
  or similar to source a newer compiler.

<em id="fn2">2</em>: GCC 8 is not provided on the base platform. Users will
  need the
  [devtoolset-8](https://www.softwarecollections.org/en/scls/rhscl/devtoolset-8/)
  or later to source a newer compiler.

<em id="fn3">3</em>: Older kernel versions may work for ARM64. However the
  Node.js test infrastructure only tests >= 4.5.

<em id="fn4">4</em>: On Windows, running Node.js in Windows terminal emulators
  like `mintty` requires the usage of [winpty](https://github.com/rprichard/winpty)
  for the tty channels to work (e.g. `winpty node.exe script.js`).
  In "Git bash" if you call the node shell alias (`node` without the `.exe`
  extension), `winpty` is used automatically.

<em id="fn5">5</em>: The Windows Subsystem for Linux (WSL) is not
  supported, but the GNU/Linux build process and binaries should work. The
  community will only address issues that reproduce on native GNU/Linux
  systems. Issues that only reproduce on WSL should be reported in the
  [WSL issue tracker](https://github.com/Microsoft/WSL/issues). Running the
  Windows binary (`node.exe`) in WSL is not recommended. It will not work
  without workarounds such as stdio redirection.

<em id="fn6">6</em>: Running Node.js on x86 Windows should work and binaries
are provided. However, tests in our infrastructure only run on WoW64.
Furthermore, compiling on x86 Windows is Experimental and
may not be possible.

<em id="fn7">7</em>: The default FreeBSD 12.0 compiler is Clang 6.0.1, but
FreeBSD 12.1 upgrades to 8.0.1. Other Clang/LLVM versions are available
via the system's package manager, including Clang 9.0.

### Supported toolchains

Depending on the host platform, the selection of toolchains may vary.

| Operating System | Compiler Versions                                              |
| ---------------- | -------------------------------------------------------------- |
| Linux            | GCC >= 8.3                                                     |
| Windows          | Visual Studio >= 2019 with the Windows 10 SDK on a 64-bit host |
| macOS            | Xcode >= 11 (Apple LLVM >= 11)                                 |

### Official binary platforms and toolchains

Binaries at <https://nodejs.org/download/release/> are produced on:

| Binary package          | Platform and Toolchain                                                                                        |
| ---------------------   | ------------------------------------------------------------------------------------------------------------- |
| aix-ppc64               | AIX 7.2 TL04 on PPC64BE with GCC 8                                                                            |
| darwin-x64              | macOS 10.15, Xcode Command Line Tools 11 with -mmacosx-version-min=10.13                                      |
| darwin-arm64 (and .pkg) | macOS 11 (arm64), Xcode Command Line Tools 12 with -mmacosx-version-min=10.13                                 |
| linux-arm64             | CentOS 7 with devtoolset-8 / GCC 8 <sup>[8](#fn8)</sup>                                                       |
| linux-armv7l            | Cross-compiled on Ubuntu 18.04 x64 with [custom GCC toolchain](https://github.com/rvagg/rpi-newer-crosstools) |
| linux-ppc64le           | CentOS 7 with devtoolset-8 / GCC 8 <sup>[8](#fn8)</sup>                                                       |
| linux-s390x             | RHEL 7 with devtoolset-8 / GCC 8 <sup>[8](#fn8)</sup>                                                         |
| linux-x64               | CentOS 7 with devtoolset-8 / GCC 8 <sup>[8](#fn8)</sup>                                                       |
| win-x64 and win-x86     | Windows 2012 R2 (x64) with Visual Studio 2019                                                                 |

<em id="fn8">8</em>: The Enterprise Linux devtoolset-8 allows us to compile
binaries with GCC 8 but linked to the glibc and libstdc++ versions of the host
platforms (CentOS 7 / RHEL 7). Therefore, binaries produced on these systems
are compatible with glibc >= 2.17 and libstdc++ >= 6.0.20 (`GLIBCXX_3.4.20`).
These are available on distributions natively supporting GCC 4.9, such as
Ubuntu 14.04 and Debian 8.

#### OpenSSL asm support

OpenSSL-1.1.1 requires the following assembler version for use of asm
support on x86_64 and ia32.

For use of AVX-512,

* gas (GNU assembler) version 2.26 or higher
* nasm version 2.11.8 or higher in Windows

AVX-512 is disabled for Skylake-X by OpenSSL-1.1.1.

For use of AVX2,

* gas (GNU assembler) version 2.23 or higher
* Xcode version 5.0 or higher
* llvm version 3.3 or higher
* nasm version 2.10 or higher in Windows

Please refer to
 <https://www.openssl.org/docs/man1.1.1/man3/OPENSSL_ia32cap.html> for details.

 If compiling without one of the above, use `configure` with the
`--openssl-no-asm` flag. Otherwise, `configure` will fail.

### Previous versions of this document

Supported platforms and toolchains change with each major version of Node.js.
This document is only valid for the current major version of Node.js.
Consult previous versions of this document for older versions of Node.js:

* [Node.js 14](https://github.com/nodejs/node/blob/v14.x/BUILDING.md)
* [Node.js 12](https://github.com/nodejs/node/blob/v12.x/BUILDING.md)
* [Node.js 10](https://github.com/nodejs/node/blob/v10.x/BUILDING.md)

## Building Node.js on supported platforms

### Note about Python

The Node.js project supports Python >= 3 for building and testing.
### Unix and macOS

#### Unix prerequisites

* `gcc` and `g++` >= 8.3 or newer, or
* GNU Make 3.81 or newer
* Python 3.6, 3.7, 3.8, or 3.9 (see note above)

Installation via Linux package manager can be achieved with:

* Ubuntu, Debian: `sudo apt-get install python3 g++ make`
* Fedora: `sudo dnf install python3 gcc-c++ make`
* CentOS and RHEL: `sudo yum install python3 gcc-c++ make`
* OpenSUSE: `sudo zypper install python3 gcc-c++ make`
* Arch Linux, Manjaro: `sudo pacman -S python gcc make`

FreeBSD and OpenBSD users may also need to install `libexecinfo`.

#### macOS prerequisites

* Xcode Command Line Tools >= 11 for macOS
* Python 3.6, 3.7, 3.8, or 3.9 (see note above)

macOS users can install the `Xcode Command Line Tools` by running
`xcode-select --install`. Alternatively, if you already have the full Xcode
installed, you can find them under the menu `Xcode -> Open Developer Tool ->
More Developer Tools...`. This step will install `clang`, `clang++`, and
`make`.

#### Building Node.js

If the path to your build directory contains a space, the build will likely
fail.

To build Node.js:

```console
$ ./configure
$ make -j4
```

The `-j4` option will cause `make` to run 4 simultaneous compilation jobs which
may reduce build time. For more information, see the
[GNU Make Documentation](https://www.gnu.org/software/make/manual/html_node/Parallel.html).

The above requires that `python` resolves to a supported version of
Python. See [Prerequisites](#prerequisites).

After building, setting up [firewall rules](tools/macos-firewall.sh) can avoid
popups asking to accept incoming network connections when running tests.

Running the following script on macOS will add the firewall rules for the
executable `node` in the `out` directory and the symbolic `node` link in the
project's root directory.

```console
$ sudo ./tools/macos-firewall.sh
```

#### Installing Node.js

To install this version of Node.js into a system directory:

```bash
[sudo] make install
```

#### Running tests

To verify the build:

```console
$ make test-only
```

At this point, you are ready to make code changes and re-run the tests.

If you are running tests before submitting a Pull Request, the recommended
command is:

```console
$ make -j4 test
```

`make -j4 test` does a full check on the codebase, including running linters and
documentation tests.

Make sure the linter does not report any issues and that all tests pass. Please
do not submit patches that fail either check.

If you want to run the linter without running tests, use
`make lint`/`vcbuild lint`. It will lint JavaScript, C++, and Markdown files.

If you are updating tests and want to run tests in a single test file
(e.g. `test/parallel/test-stream2-transform.js`):

```text
$ python tools/test.py test/parallel/test-stream2-transform.js
```

You can execute the entire suite of tests for a given subsystem
by providing the name of a subsystem:

```text
$ python tools/test.py -J --mode=release child-process
```

If you want to check the other options, please refer to the help by using
the `--help` option:

```text
$ python tools/test.py --help
```

You can usually run tests directly with node:

```text
$ ./node ./test/parallel/test-stream2-transform.js
```

Remember to recompile with `make -j4` in between test runs if you change code in
the `lib` or `src` directories.

The tests attempt to detect support for IPv6 and exclude IPv6 tests if
appropriate. If your main interface has IPv6 addresses, then your
loopback interface must also have '::1' enabled. For some default installations
on Ubuntu that does not seem to be the case. To enable '::1' on the
loopback interface on Ubuntu:

```bash
sudo sysctl -w net.ipv6.conf.lo.disable_ipv6=0
```

You can use
[node-code-ide-configs](https://github.com/nodejs/node-code-ide-configs)
to run/debug tests, if your IDE configs are present.

#### Running coverage

It's good practice to ensure any code you add or change is covered by tests.
You can do so by running the test suite with coverage enabled:

```console
$ ./configure --coverage
$ make coverage
```

A detailed coverage report will be written to `coverage/index.html` for
JavaScript coverage and to `coverage/cxxcoverage.html` for C++ coverage.

If you only want to run the JavaScript tests then you do not need to run
the first command (`./configure --coverage`). Run `make coverage-run-js`,
to execute JavaScript tests independently of the C++ test suite:

```text
$ make coverage-run-js
```

If you are updating tests and want to collect coverage for a single test file
(e.g. `test/parallel/test-stream2-transform.js`):

```text
$ make coverage-clean
$ NODE_V8_COVERAGE=coverage/tmp python tools/test.py test/parallel/test-stream2-transform.js
$ make coverage-report-js
```

You can collect coverage for the entire suite of tests for a given subsystem
by providing the name of a subsystem:

```text
$ make coverage-clean
$ NODE_V8_COVERAGE=coverage/tmp python tools/test.py -J --mode=release child-process
$ make coverage-report-js
```

The `make coverage` command downloads some tools to the project root directory.
To clean up after generating the coverage reports:

```console
$ make coverage-clean
```

#### Building the documentation

To build the documentation:

This will build Node.js first (if necessary) and then use it to build the docs:

```bash
make doc
```

If you have an existing Node.js build, you can build just the docs with:

```bash
NODE=/path/to/node make doc-only
```

To read the man page:

```bash
man doc/node.1
```

If you prefer to read the full documentation in a browser, run the following.

```bash
make docserve
```

This will spin up a static file server and provide a URL to where you may browse
the documentation locally.

If you're comfortable viewing the documentation using the program your operating
system has associated with the default web browser, run the following.

```bash
make docopen
```

This will open a file URL to a one-page version of all the browsable HTML
documents using the default browser.

To test if Node.js was built correctly:

```bash
./node -e "console.log('Hello from Node.js ' + process.version)"
```

#### Building a debug build

If you run into an issue where the information provided by the JS stack trace
is not enough, or if you suspect the error happens outside of the JS VM, you
can try to build a debug enabled binary:

```console
$ ./configure --debug
$ make -j4
```

`make` with `./configure --debug` generates two binaries, the regular release
one in `out/Release/node` and a debug binary in `out/Debug/node`, only the
release version is actually installed when you run `make install`.

To use the debug build with all the normal dependencies overwrite the release
version in the install directory:

``` console
$ make install PREFIX=/opt/node-debug/
$ cp -a -f out/Debug/node /opt/node-debug/node
```

When using the debug binary, core dumps will be generated in case of crashes.
These core dumps are useful for debugging when provided with the
corresponding original debug binary and system information.

Reading the core dump requires `gdb` built on the same platform the core dump
was captured on (i.e. 64-bit `gdb` for `node` built on a 64-bit system, Linux
`gdb` for `node` built on Linux) otherwise you will get errors like
`not in executable format: File format not recognized`.

Example of generating a backtrace from the core dump:

``` console
$ gdb /opt/node-debug/node core.node.8.1535359906
$ backtrace
```

#### Building an ASAN build

[ASAN](https://github.com/google/sanitizers) can help detect various memory
related bugs. ASAN builds are currently only supported on linux.
If you want to check it on Windows or macOS or you want a consistent toolchain
on Linux, you can try [Docker](https://www.docker.com/products/docker-desktop)
 (using an image like `gengjiawen/node-build:2020-02-14`).

The `--debug` is not necessary and will slow down build and testing, but it can
show clear stacktrace if ASAN hits an issue.

``` console
$  ./configure --debug --enable-asan && make -j4
$ make test-only
```

#### Speeding up frequent rebuilds when developing

If you plan to frequently rebuild Node.js, especially if using several branches,
installing `ccache` can help to greatly reduce build times. Set up with:
```console
$ sudo apt install ccache   # for Debian/Ubuntu, included in most Linux distros
$ ccache -o cache_dir=<tmp_dir>
$ ccache -o max_size=5.0G
$ export CC="ccache gcc"    # add to your .profile
$ export CXX="ccache g++"   # add to your .profile
```
This will allow for near-instantaneous rebuilds even when switching branches.

When modifying only the JS layer in `lib`, it is possible to externally load it
without modifying the executable:
```console
$ ./configure --node-builtin-modules-path $(pwd)
```
The resulting binary won't include any JS files and will try to load them from
the specified directory. The JS debugger of Visual Studio Code supports this
configuration since the November 2020 version and allows for setting
breakpoints.

#### Troubleshooting Unix and macOS builds

Stale builds can sometimes result in `file not found` errors while building.
This and some other problems can be resolved with `make distclean`. The
`distclean` recipe aggressively removes build artifacts. You will need to
build again (`make -j4`). Since all build artifacts have been removed, this
rebuild may take a lot more time than previous builds. Additionally,
`distclean` removes the file that stores the results of `./configure`. If you
ran `./configure` with non-default options (such as `--debug`), you will need
to run it again before invoking `make -j4`.

### Windows

#### Prerequisites

##### Option 1: Manual install

* [Python 3.9](https://www.microsoft.com/en-us/p/python-39/9p7qfqmjrfp7)
* The "Desktop development with C++" workload from
  [Visual Studio 2019](https://visualstudio.microsoft.com/downloads/) or
  the "C++ build tools" workload from the
  [Build Tools](https://visualstudio.microsoft.com/downloads/#build-tools-for-visual-studio-2019),
  with the default optional components
* Basic Unix tools required for some tests,
  [Git for Windows](https://git-scm.com/download/win) includes Git Bash
  and tools which can be included in the global `PATH`.
* The [NetWide Assembler](https://www.nasm.us/), for OpenSSL assembler modules.
  If not installed in the default location, it needs to be manually added
  to `PATH`. A build with the `openssl-no-asm` option does not need this, nor
  does a build targeting ARM64 Windows.

Optional requirements to build the MSI installer package:

* The [WiX Toolset v3.11](https://wixtoolset.org/releases/) and the
  [Wix Toolset Visual Studio 2019 Extension](https://marketplace.visualstudio.com/items?itemName=WixToolset.WixToolsetVisualStudio2019Extension)
* The [WiX Toolset v3.14](https://wixtoolset.org/releases/) if
  building for Windows 10 on ARM (ARM64)

Optional requirements for compiling for Windows 10 on ARM (ARM64):

* Visual Studio 15.9.0 or newer
* Visual Studio optional components
  * Visual C++ compilers and libraries for ARM64
  * Visual C++ ATL for ARM64
* Windows 10 SDK 10.0.17763.0 or newer

##### Option 2: Automated install with Boxstarter

A [Boxstarter](https://boxstarter.org/) script can be used for easy setup of
Windows systems with all the required prerequisites for Node.js development.
This script will install the following [Chocolatey](https://chocolatey.org/)
packages:

* [Git for Windows](https://chocolatey.org/packages/git) with the `git` and
  Unix tools added to the `PATH`
* [Python 3.x](https://chocolatey.org/packages/python)
* [Visual Studio 2019 Build Tools](https://chocolatey.org/packages/visualstudio2019buildtools)
  with [Visual C++ workload](https://chocolatey.org/packages/visualstudio2019-workload-vctools)
* [NetWide Assembler](https://chocolatey.org/packages/nasm)

To install Node.js prerequisites using
[Boxstarter WebLauncher](https://boxstarter.org/WebLauncher), open
<https://boxstarter.org/package/nr/url?https://raw.githubusercontent.com/nodejs/node/HEAD/tools/bootstrap/windows_boxstarter>
with Internet Explorer or Edge browser on the target machine.

Alternatively, you can use PowerShell. Run those commands from an elevated
PowerShell terminal:

```powershell
Set-ExecutionPolicy Unrestricted -Force
iex ((New-Object System.Net.WebClient).DownloadString('https://boxstarter.org/bootstrapper.ps1'))
get-boxstarter -Force
Install-BoxstarterPackage https://raw.githubusercontent.com/nodejs/node/HEAD/tools/bootstrap/windows_boxstarter -DisableReboots
```

The entire installation using Boxstarter will take up approximately 10 GB of
disk space.

#### Building Node.js

If the path to your build directory contains a space or a non-ASCII character,
the build will likely fail.

```console
> .\vcbuild
```

To run the tests:

```console
> .\vcbuild test
```

To test if Node.js was built correctly:

```console
> Release\node -e "console.log('Hello from Node.js', process.version)"
```

### Android/Android-based devices (e.g. Firefox OS)

Android is not a supported platform. Patches to improve the Android build are
welcome. There is no testing on Android in the current continuous integration
environment. The participation of people dedicated and determined to improve
Android building, testing, and support is encouraged.

Be sure you have downloaded and extracted
[Android NDK](https://developer.android.com/tools/sdk/ndk/index.html) before in
a folder. Then run:

```console
$ ./android-configure /path/to/your/android-ndk
$ make
```

## `Intl` (ECMA-402) support

[Intl](https://github.com/nodejs/node/blob/HEAD/doc/api/intl.md) support is
enabled by default.

### Build with full ICU support (all locales supported by ICU)

This is the default option.

#### Unix/macOS

```console
$ ./configure --with-intl=full-icu
```

#### Windows

```console
> .\vcbuild full-icu
```

### Trimmed: `small-icu` (English only) support

 In this configuration, only English data is included, but
the full `Intl` (ECMA-402) APIs.  It does not need to download
any dependencies to function. You can add full data at runtime.

#### Unix/macOS

```console
$ ./configure --with-intl=small-icu
```

#### Windows

```console
> .\vcbuild small-icu
```

### Building without Intl support

The `Intl` object will not be available, nor some other APIs such as
`String.normalize`.

#### Unix/macOS

```console
$ ./configure --without-intl
```

#### Windows

```console
> .\vcbuild without-intl
```

### Use existing installed ICU (Unix/macOS only)

```console
$ pkg-config --modversion icu-i18n && ./configure --with-intl=system-icu
```

If you are cross-compiling, your `pkg-config` must be able to supply a path
that works for both your host and target environments.

### Build with a specific ICU

You can find other ICU releases at
[the ICU homepage](http://site.icu-project.org/download).
Download the file named something like `icu4c-**##.#**-src.tgz` (or
`.zip`).

To check the minimum recommended ICU, run `./configure --help` and see
the help for the `--with-icu-source` option. A warning will be printed
during configuration if the ICU version is too old.

#### Unix/macOS

From an already-unpacked ICU:

```console
$ ./configure --with-intl=[small-icu,full-icu] --with-icu-source=/path/to/icu
```

From a local ICU tarball:

```console
$ ./configure --with-intl=[small-icu,full-icu] --with-icu-source=/path/to/icu.tgz
```

From a tarball URL:

```console
$ ./configure --with-intl=full-icu --with-icu-source=http://url/to/icu.tgz
```

#### Windows

First unpack latest ICU to `deps/icu`
[icu4c-**##.#**-src.tgz](http://site.icu-project.org/download) (or `.zip`)
as `deps/icu` (You'll have: `deps/icu/source/...`)

```console
> .\vcbuild full-icu
```

## Building Node.js with FIPS-compliant OpenSSL

The current version of Node.js does not support FIPS when statically linking
(the default) with OpenSSL 1.1.1 but for dynamically linking it is possible
to enable FIPS using the configuration flag `--openssl-is-fips`.

### Configuring and building quictls/openssl for FIPS

For quictls/openssl 3.0 it is possible to enable FIPS when dynamically linking.
Node.js currently uses openssl-3.0.0+quic which can be configured as
follows:
```console
$ git clone git@github.com:quictls/openssl.git
$ cd openssl
$ ./config --prefix=/path/to/install/dir/ shared enable-fips linux-x86_64
```
This can be compiled and installed using the following commands:
```console
$ make -j8
$ make install_ssldirs
$ make install_fips
```

After the FIPS module and configuration file have been installed by the above
instructions we also need to update `/path/to/install/dir/ssl/openssl.cnf` to
use the generated FIPS configuration file (`fipsmodule.cnf`):
```text
.include fipsmodule.cnf

# List of providers to load
[provider_sect]
default = default_sect
# The fips section name should match the section name inside the
# included /path/to/install/dir/ssl/fipsmodule.cnf.
fips = fips_sect

[default_sect]
activate = 1
```

In the above case OpenSSL is not installed in the default location so two
environment variables need to be set, `OPENSSL_CONF`, and `OPENSSL_MODULES`
which should point to the OpenSSL configuration file and the directory where
OpenSSL modules are located:
```console
$ export OPENSSL_CONF=/path/to/install/dir/ssl/openssl.cnf
$ export OPENSSL_MODULES=/path/to/install/dir/lib/ossl-modules
```

Node.js can then be configured to enable FIPS:
```console
$ ./configure --shared-openssl --shared-openssl-libpath=/path/to/install/dir/lib --shared-openssl-includes=/path/to/install/dir/include --shared-openssl-libname=crypto,ssl --openssl-is-fips
$ export LD_LIBRARY_PATH=/path/to/install/dir/lib
$ make -j8
```

Verify the produced executable:
```console
$ ldd ./node
    linux-vdso.so.1 (0x00007ffd7917b000)
    libcrypto.so.81.3 => /path/to/install/dir/lib/libcrypto.so.81.3 (0x00007fd911321000)
    libssl.so.81.3 => /path/to/install/dir/lib/libssl.so.81.3 (0x00007fd91125e000)
    libdl.so.2 => /usr/lib64/libdl.so.2 (0x00007fd911232000)
    libstdc++.so.6 => /usr/lib64/libstdc++.so.6 (0x00007fd911039000)
    libm.so.6 => /usr/lib64/libm.so.6 (0x00007fd910ef3000)
    libgcc_s.so.1 => /usr/lib64/libgcc_s.so.1 (0x00007fd910ed9000)
    libpthread.so.0 => /usr/lib64/libpthread.so.0 (0x00007fd910eb5000)
    libc.so.6 => /usr/lib64/libc.so.6 (0x00007fd910cec000)
    /lib64/ld-linux-x86-64.so.2 (0x00007fd9117f2000)
```
If the `ldd` command says that `libcrypto` cannot be found one needs to set
`LD_LIBRARY_PATH` to point to the directory used above for
`--shared-openssl-libpath` (see previous step).

Verify the OpenSSL version:
```console
$ ./node -p process.versions.openssl
3.0.0-alpha16+quic
```

Verify that FIPS is available:
```console
$ ./node -p 'process.config.variables.openssl_is_fips'
true
$ ./node --enable-fips -p 'crypto.getFips()'
1
```

FIPS support can then be enable via the OpenSSL configuration file or
using `--enable-fips` or `--force-fips` command line options to the Node.js
executable. See sections
[Enabling FIPS using Node.js options](#enabling-fips-using-node.js-options) and
[Enabling FIPS using OpenSSL config](#enabling-fips-using-openssl-config) below.

### Enabling FIPS using Node.js options
This is done using one of the Node.js options `--enable-fips` or
`--force-fips`, for example:
```console
$ node --enable-fips -p 'crypto.getFips()'
```

### Enabling FIPS using OpenSSL config
This example show that using OpenSSL's configuration file, FIPS can be enabled
without specifying the `--enable-fips` or `--force-fips` options by setting
`default_properties = fips=yes` in the FIPS configuration file. See
[link](https://github.com/openssl/openssl/blob/master/README-FIPS.md#loading-the-fips-module-at-the-same-time-as-other-providers)
for details.

For this to work the OpenSSL configuration file (default openssl.cnf) needs to
be updated. The following shows an example:
```console
openssl_conf = openssl_init

.include /path/to/install/dir/ssl/fipsmodule.cnf

[openssl_init]
providers = prov
alg_section = algorithm_sect

[prov]
fips = fips_sect
default = default_sect

[default_sect]
activate = 1

[algorithm_sect]
default_properties = fips=yes
```
After this change Node.js can be run without the `--enable-fips` or `--force-fips`
options.

## Building Node.js with external core modules

It is possible to specify one or more JavaScript text files to be bundled in
the binary as built-in modules when building Node.js.

### Unix/macOS

This command will make `/root/myModule.js` available via
`require('/root/myModule')` and `./myModule2.js` available via
`require('myModule2')`.

```console
$ ./configure --link-module '/root/myModule.js' --link-module './myModule2.js'
```

### Windows

To make `./myModule.js` available via `require('myModule')` and
`./myModule2.js` available via `require('myModule2')`:

```console
> .\vcbuild link-module './myModule.js' link-module './myModule2.js'
```

## Note for downstream distributors of Node.js

The Node.js ecosystem is reliant on ABI compatibility within a major release.
To maintain ABI compatibility it is required that distributed builds of Node.js
be built against the same version of dependencies, or similar versions that do
not break their ABI compatibility, as those released by Node.js for any given
`NODE_MODULE_VERSION` (located in `src/node_version.h`).

When Node.js is built (with an intention to distribute) with an ABI
incompatible with the official Node.js builds (e.g. using a ABI incompatible
version of a dependency), please reserve and use a custom `NODE_MODULE_VERSION`
by opening a pull request against the registry available at
<https://github.com/nodejs/node/blob/HEAD/doc/abi_version_registry.json>.
