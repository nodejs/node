## Building Node.js

Depending on what platform or features you require the build process may
differ slightly. After you've successfully built a binary, running the
test suite to validate that the binary works as intended is a good next step.

If you consistently can reproduce a test failure, search for it in the
[Node.js issue tracker](https://github.com/nodejs/node/issues) or
file a new issue.


### Unix / OS X

Prerequisites:

* `gcc` and `g++` 4.8 or newer, or
* `clang` and `clang++` 3.4 or newer
* Python 2.6 or 2.7
* GNU Make 3.81 or newer

On OS X, you will also need:
* [Xcode](https://developer.apple.com/xcode/download/)
  * You also need to install the `Command Line Tools` via Xcode. You can find
    this under the menu `Xcode -> Preferences -> Downloads`
  * This step will install `gcc` and the related toolchain containing `make`

On FreeBSD and OpenBSD, you may also need:
* libexecinfo (FreeBSD and OpenBSD only)


```text
$ ./configure
$ make
$ [sudo] make install
```

If your Python binary is in a non-standard location or has a
non-standard name, run the following instead:

```text
$ export PYTHON=/path/to/python
$ $PYTHON ./configure
$ make
$ [sudo] make install
```

To run the tests:

```text
$ make test
```

To run the native module tests:

```text
$ make test-addons
```

To run the npm test suite:

*note: to run the suite on node v4 or earlier you must first*
*run `make install`*

```
$ make test-npm
```

To build the documentation:

This will build Node.js first (if necessary) and then use it to build the docs:

```text
$ make doc
```

If you have an existing Node.js you can build just the docs with:

```text
$ NODE=node make doc-only
```

(Where `node` is the path to your executable.)

To read the documentation:

```text
$ man doc/node.1
```

To test if Node.js was built correctly:

```
$ node -e "console.log('Hello from Node.js ' + process.version)"
```


### Windows

Prerequisites:

* [Python 2.6 or 2.7](https://www.python.org/downloads/)
* One of:
  * [Visual C++ Build Tools](http://landinghub.visualstudio.com/visual-cpp-build-tools)
  * [Visual Studio](https://www.visualstudio.com/) 2013 / 2015, all editions including the Community edition
  * [Visual Studio](https://www.visualstudio.com/) Express 2013 / 2015 for Desktop
* Basic Unix tools required for some tests,
  [Git for Windows](http://git-scm.com/download/win) includes Git Bash
  and tools which can be included in the global `PATH`.

```text
> vcbuild nosign
```

To run the tests:

```text
> vcbuild test
```

To test if Node.js was built correctly:

```text
> Release\node -e "console.log('Hello from Node.js', process.version)"
```

### Android / Android-based devices (e.g., Firefox OS)

Although these instructions for building on Android are provided, please note
that Android is not an officially supported platform at this time. Patches to
improve the Android build are accepted. However, there is no testing on Android
in the current continuous integration environment. The participation of people
dedicated and determined to improve Android building, testing, and support is
encouraged.

Be sure you have downloaded and extracted [Android NDK]
(https://developer.android.com/tools/sdk/ndk/index.html)
before in a folder. Then run:

```
$ ./android-configure /path/to/your/android-ndk
$ make
```


### `Intl` (ECMA-402) support:

[Intl](https://github.com/nodejs/node/wiki/Intl) support is not
enabled by default.


#### "small" (English only) support

This option will build with "small" (English only) support, but
the full `Intl` (ECMA-402) APIs.  With `--download=all` it will
download the ICU library as needed.

##### Unix / OS X:

```text
$ ./configure --with-intl=small-icu --download=all
```

##### Windows:

```text
> vcbuild small-icu download-all
```

The `small-icu` mode builds with English-only data. You can add full
data at runtime.

*Note:* more docs are on
[the node wiki](https://github.com/nodejs/node/wiki/Intl).

#### Build with full ICU support (all locales supported by ICU):

With the `--download=all`, this may download ICU if you don't have an
ICU in `deps/icu`.

##### Unix / OS X:

```text
$ ./configure --with-intl=full-icu --download=all
```

##### Windows:

```text
> vcbuild full-icu download-all
```

#### Building without Intl support

The `Intl` object will not be available. This is the default at
present, so this option is not normally needed.

##### Unix / OS X:

```text
$ ./configure --with-intl=none
```

##### Windows:

```text
> vcbuild intl-none
```

#### Use existing installed ICU (Unix / OS X only):

```text
$ pkg-config --modversion icu-i18n && ./configure --with-intl=system-icu
```

If you are cross compiling, your `pkg-config` must be able to supply a path
that works for both your host and target environments.

#### Build with a specific ICU:

You can find other ICU releases at
[the ICU homepage](http://icu-project.org/download).
Download the file named something like `icu4c-**##.#**-src.tgz` (or
`.zip`).

##### Unix / OS X

```text
# from an already-unpacked ICU:
$ ./configure --with-intl=[small-icu,full-icu] --with-icu-source=/path/to/icu

# from a local ICU tarball
$ ./configure --with-intl=[small-icu,full-icu] --with-icu-source=/path/to/icu.tgz

# from a tarball URL
$ ./configure --with-intl=full-icu --with-icu-source=http://url/to/icu.tgz
```

##### Windows

First unpack latest ICU to `deps/icu`
[icu4c-**##.#**-src.tgz](http://icu-project.org/download) (or `.zip`)
as `deps/icu` (You'll have: `deps/icu/source/...`)

```text
> vcbuild full-icu
```

## Building Node.js with FIPS-compliant OpenSSL

NOTE: Windows is not yet supported

It is possible to build Node.js with
[OpenSSL FIPS module](https://www.openssl.org/docs/fips/fipsnotes.html).

**Note**: building in this way does **not** allow you to claim that the
runtime is FIPS 140-2 validated. Instead you can indicate that the runtime
uses a validated module. See the [security policy](http://csrc.nist.gov/groups/STM/cmvp/documents/140-1/140sp/140sp1747.pdf)
page 60 for more details. In addition, the validation for the underlying module
is only valid if it is deployed in accordance with its [security policy](http://csrc.nist.gov/groups/STM/cmvp/documents/140-1/140sp/140sp1747.pdf).
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
   /usr/local/ssl/fips-2.0
8. Build Node.js with `make -j`
9. Verify with `node -p "process.versions.openssl"` (`1.0.2a-fips`)
