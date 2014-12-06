Evented I/O for V8 javascript.
===

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

The first builds of io.js should be available 2015 January 13.

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
[the wiki](https://github.com/iojs/io.js/wiki).

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
  - [iojs.org](http://iojs.org)
  - irc chatroom, [#io.js on freenode.net](http://webchat.freenode.net?channels=io.js)
  - [gitter chatroom](https://gitter.im/iojs/io.js)
  - [Node Forward](http://nodeforward.org/)
  - [Community](https://github.com/node-forward/discussions)
  - [Contributing](https://github.com/node-forward/help)