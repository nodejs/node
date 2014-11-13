Evented I/O for V8 javascript.
===

### To build:

Prerequisites (Unix only):

    * GCC 4.2 or newer
    * G++ 4.2 or newer
    * Python 2.6 or 2.7
    * GNU Make 3.81 or newer
    * libexecinfo (FreeBSD and OpenBSD only)

Unix/Macintosh:

```sh
./configure
make
make install
```

If your python binary is in a non-standard location or has a
non-standard name, run the following instead:

```sh
export PYTHON=/path/to/python
$PYTHON ./configure
make
make install
```

Prerequisites (Windows only):

    * Python 2.6 or 2.7
    * Visual Studio 2010 or 2012

Windows:

```sh
vcbuild nosign
```

You can download pre-built binaries for various operating systems from
[http://nodejs.org/download/](http://nodejs.org/download/).  The Windows
and OS X installers will prompt you for the location in which to install.
The tarballs are self-contained; you can extract them to a local directory
with:

```sh
tar xzf /path/to/node-<version>-<platform>-<arch>.tar.gz
```

Or system-wide with:

```sh
cd /usr/local && tar --strip-components 1 -xzf \
                    /path/to/node-<version>-<platform>-<arch>.tar.gz
```

### To run the tests:

Unix/Macintosh:

```sh
make test
```

Windows:

```sh
vcbuild test
```

### To build the documentation:

```sh
make doc
```

### To read the documentation:

```sh
man doc/node.1
```

### `Intl` (ECMA-402) support:

[Intl](https://github.com/joyent/node/wiki/Intl) support is not
enabled by default.

#### "small" (English only) support

This option will build with "small" (English only) support, but
the full `Intl` (ECMA-402) APIs.  With `--download=all` it will
download the ICU library as needed.

Unix/Macintosh:

```sh
./configure --with-intl=small-icu --download=all
```

Windows:

```sh
vcbuild small-icu download-all
```

The `small-icu` mode builds
with English-only data. You can add full data at runtime.

*Note:* more docs are on
[the wiki](https://github.com/joyent/node/wiki/Intl).

#### Build with full ICU support (all locales supported by ICU):

With the `--download=all`, this may download ICU if you don't
have an ICU in `deps/icu`.

Unix/Macintosh:

```sh
./configure --with-intl=full-icu --download=all
```

Windows:

```sh
vcbuild full-icu download-all
```

#### Build with no Intl support `:-(`

The `Intl` object will not be available.
This is the default at present, so this option is not normally needed.

Unix/Macintosh:

```sh
./configure --with-intl=none
```

Windows:

```sh
vcbuild intl-none
```

#### Use existing installed ICU (Unix/Macintosh only):

```sh
pkg-config --modversion icu-i18n && ./configure --with-intl=system-icu
```

#### Build with a specific ICU:

You can find other ICU releases at
[the ICU homepage](http://icu-project.org/download).
Download the file named something like `icu4c-**##.#**-src.tgz` (or
`.zip`).

Unix/Macintosh: from an already-unpacked ICU

```sh
./configure --with-intl=[small-icu,full-icu] --with-icu-source=/path/to/icu
```

Unix/Macintosh: from a local ICU tarball

```sh
./configure --with-intl=[small-icu,full-icu] --with-icu-source=/path/to/icu.tgz
```

Unix/Macintosh: from a tarball URL

```sh
./configure --with-intl=full-icu --with-icu-source=http://url/to/icu.tgz
```

Windows: first unpack latest ICU to `deps/icu`
  [icu4c-**##.#**-src.tgz](http://icu-project.org/download) (or `.zip`)
  as `deps/icu` (You'll have: `deps/icu/source/...`)

```sh
vcbuild full-icu
```

Resources for Newcomers
---
  - [The Wiki](https://github.com/joyent/node/wiki)
  - [nodejs.org](http://nodejs.org/)
  - [how to install node.js and npm (node package manager)](http://www.joyent.com/blog/installing-node-and-npm/)
  - [list of modules](https://github.com/joyent/node/wiki/modules)
  - [searching the npm registry](http://npmjs.org/)
  - [list of companies and projects using node](https://github.com/joyent/node/wiki/Projects,-Applications,-and-Companies-Using-Node)
  - [node.js mailing list](http://groups.google.com/group/nodejs)
  - irc chatroom, [#node.js on freenode.net](http://webchat.freenode.net?channels=node.js&uio=d4)
  - [community](https://github.com/joyent/node/wiki/Community)
  - [contributing](https://github.com/joyent/node/wiki/Contributing)
  - [big list of all the helpful wiki pages](https://github.com/joyent/node/wiki/_pages)
