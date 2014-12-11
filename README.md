io.js
===
[![Gitter](https://badges.gitter.im/Join Chat.svg)](https://gitter.im/iojs/io.js?utm_source=badge&utm_medium=badge&utm_campaign=pr-badge&utm_content=badge)

This repository began as a GitHub fork of
[joyent/node](https://github.com/joyent/node).
io.js contributions, releases, and contributorship are under an
[open governance model](./CONTRIBUTING.md#governance).
We intend to land, with increasing regularity, releases which are
compatible with the npm ecosystem that has been built to date for node.js.

### Is it io.js or IO.js or iojs or IOjs or iOjS?

The official name is **io.js**, which should never be capitalized,
especially not at the start of a sentence, unless it is being
displayed in a location that is customarily all-caps (such as
the title of man pages.)

### To build:

Prerequisites (Unix only):

* `gcc` and `g++` 4.8 or newer, or
* `clang` and `clang++` 3.3 or newer
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
* Visual Studio 2013 for Windows Desktop, or
* Visual Studio Express 2013 for Windows Desktop

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

### To build `Intl` (ECMA-402) support:

*Note:* more docs, including how to reduce disk footprint, are on
[the wiki](https://github.com/joyent/node/wiki/Intl).

#### Use existing installed ICU (Unix/Macintosh only):

```sh
pkg-config --modversion icu-i18n && ./configure --with-intl=system-icu
```

#### Build ICU from source:

First: Unpack latest ICU
  [icu4c-**##.#**-src.tgz](http://icu-project.org/download) (or `.zip`)
  as `deps/icu` (You'll have: `deps/icu/source/...`)

Unix/Macintosh:

```sh
./configure --with-intl=full-icu
```

Windows:

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
  - irc chatroom, [#io.js on freenode.net](http://webchat.freenode.net?channels=io.js&uio=d4)
  - [community](https://github.com/joyent/node/wiki/Community)
  - [contributing](https://github.com/joyent/node/wiki/Contributing)
  - [big list of all the helpful wiki pages](https://github.com/joyent/node/wiki/_pages)
