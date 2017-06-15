# Building Node.js

Depending on what platform or features you require, the build process may
differ slightly. After you've successfully built a binary, running the
test suite to validate that the binary works as intended is a good next step.

If you consistently can reproduce a test failure, search for it in the
[Node.js issue tracker](https://github.com/nodejs/node/issues) or
file a new issue.

## Supported platforms

This list of supported platforms is current as of the branch / release to
which it is attached.

### Input

Node.js relies on V8 and libuv. Therefore, we adopt a subset of their
supported platforms.

### Strategy

Support is divided into three tiers:

* **Tier 1**: Full test coverage and maintenance by the Node.js core team and
  the broader community.
* **Tier 2**: Full test coverage but more limited maintenance,
  often provided by the vendor of the platform.
* **Experimental**: May not compile reliably or test suite may not pass.
  These are often working to be promoted to Tier 2 but are not quite ready.
  There is at least one individual actively providing maintenance and the team
  is striving to broaden quality and reliability of support.

### Supported platforms

The community does not build or test against end of life distributions (EoL).
Thus we do not recommend that you use Node on end of life or unsupported platforms
in production.

|  System      | Support type | Version                          | Architectures        | Notes            |
|--------------|--------------|----------------------------------|----------------------|------------------|
| GNU/Linux    | Tier 1       | kernel >= 2.6.32, glibc >= 2.12  | x86, x64, arm, arm64 |                  |
| macOS        | Tier 1       | >= 10.10                         | x64                  |                  |
| Windows      | Tier 1       | >= Windows 7 / 2008 R2           | x86, x64             | vs2015 or vs2017 |
| SmartOS      | Tier 2       | >= 15 < 16.4                     | x86, x64             | see note1        |
| FreeBSD      | Tier 2       | >= 10                            | x64                  |                  |
| GNU/Linux    | Tier 2       | kernel >= 3.13.0, glibc >= 2.19  | ppc64le >=power8     |                  |
| AIX          | Tier 2       | >= 7.1 TL04                      | ppc64be >=power7     |                  |
| GNU/Linux    | Tier 2       | kernel >= 3.10, glibc >= 2.17    | s390x                |                  |
| macOS        | Experimental | >= 10.8 < 10.10                  | x64                  | no test coverage |
| Linux (musl) | Experimental | musl >= 1.0                      | x64                  |                  |

note1 - The gcc4.8-libs package needs to be installed, because node
  binaries have been built with GCC 4.8, for which runtime libraries are not
  installed by default. For these node versions, the recommended binaries
  are the ones available in pkgsrc, not the one available from nodejs.org.
  Note that the binaries downloaded from the pkgsrc repositories are not
  officially supported by the Node.js project, and instead are supported
  by Joyent. SmartOS images >= 16.4 are not supported because
  GCC 4.8 runtime libraries are not available in their pkgsrc repository

### Supported toolchains

Depending on host platform, the selection of toolchains may vary.

#### Unix

* GCC 4.9.4 or newer
* Clang 3.4.2 or newer

#### Windows

* Building Node: Visual Studio 2015 or Visual C++ Build Tools 2015 or newer
* Building native add-ons: Visual Studio 2013 or Visual C++ Build Tools 2015
  or newer

## Building Node.js on supported platforms

### Unix / macOS

Prerequisites:

* `gcc` and `g++` 4.9.4 or newer, or
* `clang` and `clang++` 3.4.2 or newer (macOS: latest Xcode Command Line Tools)
* Python 2.6 or 2.7
* GNU Make 3.81 or newer

On macOS you will need to install the `Xcode Command Line Tools` by running
`xcode-select --install`. Alternatively, if you already have the full Xcode
installed, you can find them under the menu `Xcode -> Open Developer Tool ->
More Developer Tools...`. This step will install `clang`, `clang++`, and
`make`.
* You may want to setup [firewall rules](tools/macosx-firewall.sh)
to avoid popups asking to accept incoming network connections when running tests:

```console
$ sudo ./tools/macosx-firewall.sh
```
Running this script will add rules for the executable `node` in the out
directory and the symbolic `node` link in the project's root directory.

On FreeBSD and OpenBSD, you may also need:
* libexecinfo

To build Node.js:

```console
$ ./configure
$ make -j4
```

Running `make` with the `-j4` flag will cause it to run 4 compilation jobs
concurrently which may significantly reduce build time. The number after `-j`
can be changed to best suit the number of processor cores on your machine. If
you run into problems running `make` with concurrency, try running it without
the `-j4` flag. See the
[GNU Make Documentation](https://www.gnu.org/software/make/manual/html_node/Parallel.html)
for more information.

Note that the above requires that `python` resolve to Python 2.6 or 2.7
and not a newer version.

To run the tests:

```console
$ make test
```

To build the documentation:

This will build Node.js first (if necessary) and then use it to build the docs:

```console
$ make doc
```

If you have an existing Node.js you can build just the docs with:

```console
$ NODE=/path/to/node make doc-only
```

To read the documentation:

```console
$ man doc/node.1
```

To test if Node.js was built correctly:

```console
$ ./node -e "console.log('Hello from Node.js ' + process.version)"
```

To install this version of Node.js into a system directory:

```console
$ [sudo] make install
```


### Windows

Prerequisites:

* [Python 2.6 or 2.7](https://www.python.org/downloads/)
* One of:
  * [Visual C++ Build Tools](http://landinghub.visualstudio.com/visual-cpp-build-tools)
  * [Visual Studio 2015 Update 3](https://www.visualstudio.com/), all editions
    including the Community edition (remember to select
    "Common Tools for Visual C++ 2015" feature during installation).
  * [Visual Studio 2017](https://www.visualstudio.com/downloads/), any edition (including the Build Tools SKU).
    **Required Components:** "MSbuild", "VC++ 2017 v141 toolset" and one of the Windows SDKs (10 or 8.1).
* Basic Unix tools required for some tests,
  [Git for Windows](http://git-scm.com/download/win) includes Git Bash
  and tools which can be included in the global `PATH`.

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

### Android / Android-based devices (e.g. Firefox OS)

Although these instructions for building on Android are provided, please note
that Android is not an officially supported platform at this time. Patches to
improve the Android build are accepted. However, there is no testing on Android
in the current continuous integration environment. The participation of people
dedicated and determined to improve Android building, testing, and support is
encouraged.

Be sure you have downloaded and extracted
[Android NDK](https://developer.android.com/tools/sdk/ndk/index.html) before in
a folder. Then run:

```console
$ ./android-configure /path/to/your/android-ndk
$ make
```


### `Intl` (ECMA-402) support:

[Intl](https://github.com/nodejs/node/wiki/Intl) support is
enabled by default, with English data only.

#### Default: `small-icu` (English only) support

By default, only English data is included, but
the full `Intl` (ECMA-402) APIs.  It does not need to download
any dependencies to function. You can add full
data at runtime.

*Note:* more docs are on
[the node wiki](https://github.com/nodejs/node/wiki/Intl).

#### Build with full ICU support (all locales supported by ICU):

With the `--download=all`, this may download ICU if you don't have an
ICU in `deps/icu`. (The embedded `small-icu` included in the default
Node.js source does not include all locales.)

##### Unix / macOS:

```console
$ ./configure --with-intl=full-icu --download=all
```

##### Windows:

```console
> .\vcbuild full-icu download-all
```

#### Building without Intl support

The `Intl` object will not be available, nor some other APIs such as
`String.normalize`.

##### Unix / macOS:

```console
$ ./configure --without-intl
```

##### Windows:

```console
> .\vcbuild without-intl
```

#### Use existing installed ICU (Unix / macOS only):

```console
$ pkg-config --modversion icu-i18n && ./configure --with-intl=system-icu
```

If you are cross compiling, your `pkg-config` must be able to supply a path
that works for both your host and target environments.

#### Build with a specific ICU:

You can find other ICU releases at
[the ICU homepage](http://icu-project.org/download).
Download the file named something like `icu4c-**##.#**-src.tgz` (or
`.zip`).

##### Unix / macOS

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

##### Windows

First unpack latest ICU to `deps/icu`
[icu4c-**##.#**-src.tgz](http://icu-project.org/download) (or `.zip`)
as `deps/icu` (You'll have: `deps/icu/source/...`)

```console
> .\vcbuild full-icu
```

## Building Node.js with FIPS-compliant OpenSSL

NOTE: Windows is not yet supported

It is possible to build Node.js with
[OpenSSL FIPS module](https://www.openssl.org/docs/fipsnotes.html).

**Note**: building in this way does **not** allow you to claim that the
runtime is FIPS 140-2 validated. Instead you can indicate that the runtime
uses a validated module. See the
[security policy](http://csrc.nist.gov/groups/STM/cmvp/documents/140-1/140sp/140sp1747.pdf)
page 60 for more details. In addition, the validation for the underlying module
is only valid if it is deployed in accordance with its
[security policy](http://csrc.nist.gov/groups/STM/cmvp/documents/140-1/140sp/140sp1747.pdf).
If you need FIPS validated cryptography it is recommended that you read both
the [security policy](http://csrc.nist.gov/groups/STM/cmvp/documents/140-1/140sp/140sp1747.pdf)
and [user guide](https://openssl.org/docs/fips/UserGuide-2.0.pdf).

### Instructions

1. Obtain a copy of openssl-fips-x.x.x.tar.gz.
   To comply with the security policy you must ensure the path
   through which you get the file complies with the requirements
   for a "secure installation" as described in section 6.6 in
   the [user guide](https://openssl.org/docs/fips/UserGuide-2.0.pdf).
   For evaluation/experimentation you can simply download and verify
   `openssl-fips-x.x.x.tar.gz` from https://www.openssl.org/source/
2. Extract source to `openssl-fips` folder and `cd openssl-fips`
3. `./config`
4. `make`
5. `make install`
   (NOTE: to comply with the security policy you must use the exact
   commands in steps 3-5 without any additional options as per
   Appendix A in the [security policy](http://csrc.nist.gov/groups/STM/cmvp/documents/140-1/140sp/140sp1747.pdf).
   The only exception is that `./config no-asm` can be
   used in place of `./config`, and the FIPSDIR environment variable
   may be used to specify a non-standard install folder for the
   validated module, as per User Guide sections 4.2.1, 4.2.2, and 4.2.3.
6. Get into Node.js checkout folder
7. `./configure --openssl-fips=/path/to/openssl-fips/installdir`
   For example on ubuntu 12 the installation directory was
   `/usr/local/ssl/fips-2.0`
8. Build Node.js with `make -j`
9. Verify with `node -p "process.versions.openssl"` (for example `1.0.2a-fips`)
