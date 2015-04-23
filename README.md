io.js
=====

[![Gitter](https://badges.gitter.im/Join Chat.svg)](https://gitter.im/iojs/io.js?utm_source=badge&utm_medium=badge&utm_campaign=pr-badge&utm_content=badge)

This repository began as a GitHub fork of
[joyent/node](https://github.com/joyent/node).

io.js contributions, releases, and contributorship are under an
[open governance model](./GOVERNANCE.md).
We intend to land, with increasing regularity, releases which are
compatible with the npm ecosystem that has been built to date for
Node.js.

## Is it io.js or IO.js or iojs or IOjs or iOjS?

The official name is **io.js**, which should never be capitalized,
especially not at the start of a sentence, unless it is being
displayed in a location that is customarily all-caps (such as
the title of man pages).

## Download

Binaries, installers, and source tarballs are available at
<https://iojs.org>.

**Releases** are available at <https://iojs.org/dist/>, listed under
their version string. The <https://iojs.org/dist/latest/> symlink
will point to the latest release directory.

**Nightly** builds are available at
<https://iojs.org/download/nightly/>, listed under their version
string which includes their date (in UTC time) and the commit SHA at
the HEAD of the release.

**API documentation** is available in each release and nightly
directory under _docs_. <https://iojs.org/api/> points to the the
latest version.

### Verifying Binaries

Release and nightly download directories all contain a *SHASUM256.txt*
file that lists the SHA checksums for each file available for
download. To check that a downloaded file matches the checksum, run
it through `sha256sum` with a command such as:

```
$ grep iojs-vx.y.z.tar.gz SHASUMS256.txt | sha256sum -c -
```

_(Where "iojs-vx.y.z.tar.gz" is the name of the file you have
downloaded)_

Additionally, releases (not nightlies) have GPG signed copies of
SHASUM256.txt files available as SHASUM256.txt.asc. You can use `gpg`
to verify that the file has not been tampered with.

To verify a SHASUM256.txt.asc, you will first need to import all of
the GPG keys of individuals authorized to create releases. They are
listed at the bottom of this README. Use a command such as this to
import the keys:

```
$ gpg --keyserver pool.sks-keyservers.net \
  --recv-keys DD8F2338BAE7501E3DD5AC78C273792F7D83545D
```

_(Include each of the key fingerprints at the end of this command.)_

You can then use `gpg --verify SHASUMS256.txt.asc` to verify that the
file has been signed by an authorized member of the io.js team.

Once verified, use the SHASUMS256.txt.asc file to get the checksum for
the binary verification command above.

## Build

### Unix / Macintosh

Prerequisites:

* `gcc` and `g++` 4.8 or newer, or
* `clang` and `clang++` 3.4 or newer
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

### Android / Android based devices, aka. Firefox OS

Be sure you have downloaded and extracted [Android NDK]
(https://developer.android.com/tools/sdk/ndk/index.html)
before in a folder. Then run:

```
$ ./android-configure /path/to/your/android-ndk
$ make
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
  - Release GPG key: 9554F04D7259F04124DE6B476D5A82AC7E37093B
* **Colin Ihrig** ([@cjihrig](https://github.com/cjihrig)) &lt;cjihrig@gmail.com&gt;
* **Mikeal Rogers** ([@mikeal](https://github.com/mikeal)) &lt;mikeal.rogers@gmail.com&gt;
* **Rod Vagg** ([@rvagg](https://github.com/rvagg)) &lt;rod@vagg.org&gt;
  - Release GPG key: DD8F2338BAE7501E3DD5AC78C273792F7D83545D
* **Thorsten Lorenz** ([@thlorenz](https://github.com/thlorenz)) &lt;thlorenz@gmx.de&gt;
* **Stephen Belanger** ([@qard](https://github.com/qard)) &lt;admin@stephenbelanger.com&gt;
* **Jeremiah Senkpiel** ([@fishrock123](https://github.com/fishrock123)) &lt;fishrock123@rocketmail.com&gt; (Technical Committee)
  - Release GPG key: FD3A5288F042B6850C66B31F09FE44734EB7990E
* **Evan Lucas** ([@evanlucas](https://github.com/evanlucas)) &lt;evanlucas@me.com&gt;
* **Brendan Ashworth** ([@brendanashworth](https://github.com/brendanashworth)) &lt;brendan.ashworth@me.com&gt;
* **Vladimir Kurchatkin** ([@vkurchatkin](https://github.com/vkurchatkin)) &lt;vladimir.kurchatkin@gmail.com&gt;
* **Nikolai Vavilov** ([@seishun](https://github.com/seishun)) &lt;vvnicholas@gmail.com&gt;
* **Nicu Micleușanu** ([@micnic](https://github.com/micnic)) &lt;micnic90@gmail.com&gt;
* **Aleksey Smolenchuk** ([@lxe](https://github.com/lxe)) &lt;lxe@lxe.co&gt;
* **Shigeki Ohtsu** ([@shigeki](https://github.com/shigeki)) &lt;ohtsu@iij.ad.jp&gt;
* **Sam Roberts** ([@sam-github](https://github.com/sam-github)) &lt;vieuxtech@gmail.com&gt;
* **Wyatt Preul** ([@geek](https://github.com/geek)) &lt;wpreul@gmail.com&gt;
* **Brian White** ([@mscdex](https://github.com/mscdex)) &lt;mscdex@mscdex.net&gt;
* **Christian Vaagland Tellnes** ([@tellnes](https://github.com/tellnes)) &lt;christian@tellnes.com&gt;
* **Robert Kowalski** ([@robertkowalski](https://github.com/robertkowalski)) &lt;rok@kowalski.gd&gt;
* **Julian Duque** ([@julianduque](https://github.com/julianduque)) &lt;julianduquej@gmail.com&gt;
* **Johan Bergström** ([@jbergstroem](https://github.com/jbergstroem)) &lt;bugs@bergstroem.nu&gt;
* **Roman Reiss** ([@silverwind](https://github.com/silverwind)) &lt;me@silverwind.io&gt;
* **Petka Antonov** ([@petkaantonov](https://github.com/petkaantonov)) &lt;petka_antonov@hotmail.com&gt;
* **Yosuke Furukawa** ([@yosuke-furukawa](https://github.com/yosuke-furukawa)) &lt;yosuke.furukawa@gmail.com&gt;

Collaborators follow the [COLLABORATOR_GUIDE.md](./COLLABORATOR_GUIDE.md) in
maintaining the io.js project.
