io.js
===
[![Gitter](https://badges.gitter.im/Join Chat.svg)](https://gitter.im/iojs/io.js?utm_source=badge&utm_medium=badge&utm_campaign=pr-badge&utm_content=badge)

This repository began as a GitHub fork of
[joyent/node](https://github.com/joyent/node).

io.js contributions, releases, and contributorship are under an
[open governance model](./GOVERNANCE.md).
We intend to land, with increasing regularity, releases which are
compatible with the npm ecosystem that has been built to date for Node.js.

## Is it io.js or IO.js or iojs or IOjs or iOjS?

The official name is **io.js**, which should never be capitalized,
especially not at the start of a sentence, unless it is being
displayed in a location that is customarily all-caps (such as
the title of man pages).

## To build:

### Unix / Macintosh

Prerequisites:

* `gcc` and `g++` 4.8 or newer, or
* `clang` and `clang++` 3.3 or newer
* Python 2.6 or 2.7
* GNU Make 3.81 or newer
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

To build the documentation:

```text
$ make doc
```

To read the documentation:

```text
$ man doc/iojs.1
```

### Windows

Prerequisites:

* [Python 2.6 or 2.7](https://www.python.org/downloads/)
* Visual Studio 2013 for Windows Desktop, or
* Visual Studio Express 2013 for Windows Desktop
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

### `Intl` (ECMA-402) support:

[Intl](https://github.com/joyent/node/wiki/Intl) support is not
enabled by default.

#### "small" (English only) support

This option will build with "small" (English only) support, but
the full `Intl` (ECMA-402) APIs.  With `--download=all` it will
download the ICU library as needed.

Unix / Macintosh:

```text
$ ./configure --with-intl=small-icu --download=all
```

Windows:

```text
> vcbuild small-icu download-all
```

The `small-icu` mode builds with English-only data. You can add full
data at runtime.

*Note:* more docs are on
[the joyent/node wiki](https://github.com/joyent/node/wiki/Intl).

#### Build with full ICU support (all locales supported by ICU):

With the `--download=all`, this may download ICU if you don't have an
ICU in `deps/icu`.

Unix / Macintosh:

```text
$ ./configure --with-intl=full-icu --download=all
```

Windows:

```text
> vcbuild full-icu download-all
```

#### Build with no Intl support `:-(`

The `Intl` object will not be available. This is the default at
present, so this option is not normally needed.

Unix / Macintosh:

```text
$ ./configure --with-intl=none
```

Windows:

```text
> vcbuild intl-none
```

#### Use existing installed ICU (Unix / Macintosh only):

```text
$ pkg-config --modversion icu-i18n && ./configure --with-intl=system-icu
```

#### Build with a specific ICU:

You can find other ICU releases at
[the ICU homepage](http://icu-project.org/download).
Download the file named something like `icu4c-**##.#**-src.tgz` (or
`.zip`).

Unix / Macintosh

```text
# from an already-unpacked ICU:
$ ./configure --with-intl=[small-icu,full-icu] --with-icu-source=/path/to/icu

# from a local ICU tarball
$ ./configure --with-intl=[small-icu,full-icu] --with-icu-source=/path/to/icu.tgz

# from a tarball URL
$ ./configure --with-intl=full-icu --with-icu-source=http://url/to/icu.tgz
```

Windows

First unpack latest ICU to `deps/icu`
[icu4c-**##.#**-src.tgz](http://icu-project.org/download) (or `.zip`)
as `deps/icu` (You'll have: `deps/icu/source/...`)

```text
> vcbuild full-icu
```

## Resources for Newcomers

* [CONTRIBUTING.md](./CONTRIBUTING.md)
* [GOVERNANCE.md](./GOVERNANCE.md)
* IRC:
  [#io.js on Freenode.net](http://webchat.freenode.net?channels=io.js&uio=d4)
* [iojs/io.js on Gitter](https://gitter.im/iojs/io.js)



## Current Project Team Members

The io.js project team comprises a group of core collaborators and a sub-group
that forms the _Technical Committee_ (TC) which governs the project. For more
information about the governance of the io.js project, see
[GOVERNANCE.md](./GOVERNANCE.md).

* **Isaac Z. Schlueter** ([@isaacs](https://github.com/isaacs)) &lt;i@izs.me&gt; (Technical Committee)
* **Ben Noordhuis** ([@bnoordhuis](https://github.com/bnoordhuis)) &lt;info@bnoordhuis.nl&gt; (Technical Committee)
* **Bert Belder** ([@piscisaureus](https://github.com/piscisaureus)) &lt;bertbelder@gmail.com&gt; (Technical Committee)
* **Fedor Indutny** ([@indutny](https://github.com/indutny)) &lt;fedor.indutny@gmail.com&gt; (Technical Committee)
* **Trevor Norris** ([@trevnorris](https://github.com/trevnorris)) &lt;trev.norris@gmail.com&gt; (Technical Committee)
* **Chris Dickinson** ([@chrisdickinson](https://github.com/chrisdickinson)) &lt;christopher.s.dickinson@gmail.com&gt; (Technical Committee)
* **Colin Ihrig** ([@cjihrig](https://github.com/cjihrig)) &lt;cjihrig@gmail.com&gt; (Technical Committee)
* **Mikeal Rogers** ([@mikeal](https://github.com/mikeal)) &lt;mikeal.rogers@gmail.com&gt;
* **Rod Vagg** ([@rvagg](https://github.com/rvagg)) &lt;rod@vagg.org&gt;
* **Thorsten Lorenz** ([@thlorenz](https://github.com/thlorenz)) &lt;thlorenz@gmx.de&gt;
* **Stephen Belanger** ([@qard](https://github.com/qard)) &lt;admin@stephenbelanger.com&gt;
* **Jeremiah Senkpiel** ([@fishrock123](https://github.com/fishrock123)) &lt;fishrock123@rocketmail.com&gt;
* **Evan Lucas** ([@evanlucas](https://github.com/evanlucas)) &lt;evanlucas@me.com&gt;
* **Brendan Ashworth** ([@brendanashworth](https://github.com/brendanashworth)) &lt;brendan.ashworth@me.com&gt;

Collaborators follow the [COLLABORATOR_GUIDE.md](./COLLABORATOR_GUIDE.md) in
maintaining the io.js project.
