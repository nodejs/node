
io.js
=====

[![Gitter](https://badges.gitter.im/Join Chat.svg)](https://gitter.im/nodejs/io.js?utm_source=badge&utm_medium=badge&utm_campaign=pr-badge&utm_content=badge)

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
directory under _docs_. <https://iojs.org/api/> points to the latest version.

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

To test if io.js was built correctly:

```
$ iojs -e "console.log('Hello from io.js ' + process.version)"
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

To test if io.js was built correctly:

```
$ iojs -e "console.log('Hello from io.js ' + process.version)"
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

# Building io.js with FIPS-compliant OpenSSL

NOTE: Windows is not yet supported

It is possible to build io.js with
[OpenSSL FIPS module](https://www.openssl.org/docs/fips/fipsnotes.html).

Instructions:

1. Download and verify `openssl-fips-x.x.x.tar.gz` from
   https://www.openssl.org/source/
2. Extract source to `openssl-fips` folder
3. ``cd openssl-fips && ./config fipscanisterbuild --prefix=`pwd`/out``
   (NOTE: On OS X, you may want to run
    ``./Configure darwin64-x86_64-cc --prefix=`pwd`/out`` if you are going to
    build x64-mode io.js)
4. `make -j && make install`
5. Get into io.js checkout folder
6. `./configure --openssl-fips=/path/to/openssl-fips/out`
7. Build io.js with `make -j`
8. Verify with `node -p "process.versions.openssl"` (`1.0.2a-fips`)

## Resources for Newcomers

* [CONTRIBUTING.md](./CONTRIBUTING.md)
* [GOVERNANCE.md](./GOVERNANCE.md)
* IRC:
  [#io.js on Freenode.net](http://webchat.freenode.net?channels=io.js&uio=d4)
* [iojs/io.js on Gitter](https://gitter.im/nodejs/io.js)

## Security

All security bugs in io.js are taken seriously and should be reported by
emailing security@iojs.org. This will be delivered to a subset of the project
team who handle security issues. Please don't disclose security bugs
public until they have been handled by the security team.

Your email will be acknowledged within 24 hours, and you’ll receive a more
detailed response to your email within 48 hours indicating the next steps in
handling your report.

## Current Project Team Members

The io.js project team comprises a group of core collaborators and a sub-group
that forms the _Technical Committee_ (TC) which governs the project. For more
information about the governance of the io.js project, see
[GOVERNANCE.md](./GOVERNANCE.md).

* **Isaac Z. Schlueter** &lt;i@izs.me&gt; ([@isaacs](https://github.com/isaacs)) (Technical Committee)
* **Ben Noordhuis** &lt;info@bnoordhuis.nl&gt; ([@bnoordhuis](https://github.com/bnoordhuis)) (Technical Committee)
* **Bert Belder** &lt;bertbelder@gmail.com&gt; ([@piscisaureus](https://github.com/piscisaureus)) (Technical Committee)
* **Fedor Indutny** &lt;fedor.indutny@gmail.com&gt; ([@indutny](https://github.com/indutny)) (Technical Committee)
* **Trevor Norris** &lt;trev.norris@gmail.com&gt; ([@trevnorris](https://github.com/trevnorris)) (Technical Committee)
* **Chris Dickinson** &lt;christopher.s.dickinson@gmail.com&gt; ([@chrisdickinson](https://github.com/chrisdickinson)) (Technical Committee)
  - Release GPG key: 9554F04D7259F04124DE6B476D5A82AC7E37093B
* **Colin Ihrig** &lt;cjihrig@gmail.com&gt; ([@cjihrig](https://github.com/cjihrig))
* **Mikeal Rogers** &lt;mikeal.rogers@gmail.com&gt; ([@mikeal](https://github.com/mikeal))
* **Rod Vagg** &lt;rod@vagg.org&gt; ([@rvagg](https://github.com/rvagg)) (Technical Committee)
  - Release GPG key: DD8F2338BAE7501E3DD5AC78C273792F7D83545D
* **Thorsten Lorenz** &lt;thlorenz@gmx.de&gt; ([@thlorenz](https://github.com/thlorenz))
* **Stephen Belanger** &lt;admin@stephenbelanger.com&gt; ([@qard](https://github.com/qard))
* **Jeremiah Senkpiel** &lt;fishrock123@rocketmail.com&gt; ([@fishrock123](https://github.com/fishrock123)) (Technical Committee)
  - Release GPG key: FD3A5288F042B6850C66B31F09FE44734EB7990E
* **Evan Lucas** &lt;evanlucas@me.com&gt; ([@evanlucas](https://github.com/evanlucas))
* **Brendan Ashworth** &lt;brendan.ashworth@me.com&gt; ([@brendanashworth](https://github.com/brendanashworth))
* **Vladimir Kurchatkin** &lt;vladimir.kurchatkin@gmail.com&gt; ([@vkurchatkin](https://github.com/vkurchatkin))
* **Nikolai Vavilov** &lt;vvnicholas@gmail.com&gt; ([@seishun](https://github.com/seishun))
* **Nicu Micleușanu** &lt;micnic90@gmail.com&gt; ([@micnic](https://github.com/micnic))
* **Aleksey Smolenchuk** &lt;lxe@lxe.co&gt; ([@lxe](https://github.com/lxe))
* **Shigeki Ohtsu** &lt;ohtsu@iij.ad.jp&gt; ([@shigeki](https://github.com/shigeki)) (Technical Committee)
* **Sam Roberts** &lt;vieuxtech@gmail.com&gt; ([@sam-github](https://github.com/sam-github))
* **Wyatt Preul** &lt;wpreul@gmail.com&gt; ([@geek](https://github.com/geek))
* **Brian White** &lt;mscdex@mscdex.net&gt; ([@mscdex](https://github.com/mscdex)) (Technical Committee)
* **Christian Tellnes** &lt;christian@tellnes.no&gt; ([@tellnes](https://github.com/tellnes))
* **Robert Kowalski** &lt;rok@kowalski.gd&gt; ([@robertkowalski](https://github.com/robertkowalski))
* **Julian Duque** &lt;julianduquej@gmail.com&gt; ([@julianduque](https://github.com/julianduque))
* **Johan Bergström** &lt;bugs@bergstroem.nu&gt; ([@jbergstroem](https://github.com/jbergstroem))
* **Roman Reiss** &lt;me@silverwind.io&gt; ([@silverwind](https://github.com/silverwind))
* **Petka Antonov** &lt;petka_antonov@hotmail.com&gt; ([@petkaantonov](https://github.com/petkaantonov))
* **Yosuke Furukawa** &lt;yosuke.furukawa@gmail.com&gt; ([@yosuke-furukawa](https://github.com/yosuke-furukawa))
* **Alex Kocharin** &lt;alex@kocharin.ru&gt; ([@rlidwka](https://github.com/rlidwka))
* **Christopher Monsanto** &lt;chris@monsan.to&gt; ([@monsanto](https://github.com/monsanto))
* **Ali Ijaz Sheikh** &lt;ofrobots@google.com&gt; ([@ofrobots](https://github.com/ofrobots))
* **Oleg Elifantiev** &lt;oleg@elifantiev.ru&gt; ([@Olegas](https://github.com/Olegas))
* **Domenic Denicola** &lt;d@domenic.me&gt; ([@domenic](https://github.com/domenic))
* **Rich Trott** &lt;rtrott@gmail.com&gt; ([@Trott](https://github.com/Trott))
* **Сковорода Никита Андреевич** &lt;chalkerx@gmail.com&gt; ([@ChALkeR](https://github.com/ChALkeR))

Collaborators follow the [COLLABORATOR_GUIDE.md](./COLLABORATOR_GUIDE.md) in
maintaining the io.js project.
