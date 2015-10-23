
Node.js
=====

[![Gitter](https://badges.gitter.im/Join Chat.svg)](https://gitter.im/nodejs/node?utm_source=badge&utm_medium=badge&utm_campaign=pr-badge&utm_content=badge)

This repository began as a GitHub fork of
[joyent/node](https://github.com/joyent/node).

Node.js contributions, releases, and contributorship are under an
[open governance model](./GOVERNANCE.md).
We intend to land, with increasing regularity, releases which are
compatible with the npm ecosystem that has been built to date for
Node.js.

## Download

Binaries, installers, and source tarballs are available at
<https://nodejs.org>.

**Releases** are available at <https://nodejs.org/dist/>, listed under
their version string. The <https://nodejs.org/dist/latest/> symlink
will point to the latest release directory.

**Nightly** builds are available at
<https://nodejs.org/download/nightly/>, listed under their version
string which includes their date (in UTC time) and the commit SHA at
the HEAD of the release.

**API documentation** is available in each release and nightly
directory under _docs_. <https://nodejs.org/api/> points to the latest version.

### Verifying Binaries

Release and nightly download directories all contain a *SHASUM256.txt*
file that lists the SHA checksums for each file available for
download. To check that a downloaded file matches the checksum, run
it through `sha256sum` with a command such as:

```
$ grep node-vx.y.z.tar.gz SHASUMS256.txt | sha256sum -c -
```

_(Where "node-vx.y.z.tar.gz" is the name of the file you have
downloaded)_

Additionally, releases (not nightlies) have GPG signed copies of
SHASUM256.txt files available as SHASUM256.txt.asc. You can use `gpg`
to verify that the file has not been tampered with.

To verify a SHASUM256.txt.asc, you will first need to import all of
the GPG keys of individuals authorized to create releases. They are
listed at the bottom of this README under [Release Team](#release-team).
Use a command such as this to import the keys:

```
$ gpg --keyserver pool.sks-keyservers.net \
  --recv-keys DD8F2338BAE7501E3DD5AC78C273792F7D83545D
```

_(See the bottom of this README for a full script to import active
release keys)_

You can then use `gpg --verify SHASUMS256.txt.asc` to verify that the
file has been signed by an authorized member of the Node.js team.

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
$ man doc/node.1
```

To test if Node.js was built correctly:

```
$ node -e "console.log('Hello from node.js ' + process.version)"
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

To test if Node.js was built correctly:

```
$ node -e "console.log('Hello from node.js ' + process.version)"
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

[Intl](https://github.com/nodejs/node/wiki/Intl) support is not
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
[the node wiki](https://github.com/nodejs/node/wiki/Intl).

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

# Building Node.js with FIPS-compliant OpenSSL

NOTE: Windows is not yet supported

It is possible to build Node.js with
[OpenSSL FIPS module](https://www.openssl.org/docs/fips/fipsnotes.html).

**Note** that building in this way does **not** allow you to
claim that the runtime is FIPS 140-2 validated.  Instead you
can indicate that the runtime uses a validated module.  See
the [security policy]
(http://csrc.nist.gov/groups/STM/cmvp/documents/140-1/140sp/140sp1747.pdf)
page 60 for more details.  In addition, the validation for
the underlying module is only valid if it is deployed in
accordance with its [security policy]
(http://csrc.nist.gov/groups/STM/cmvp/documents/140-1/140sp/140sp1747.pdf).
If you need FIPS validated cryptography it is recommended that you
read both the [security policy]
(http://csrc.nist.gov/groups/STM/cmvp/documents/140-1/140sp/140sp1747.pdf)
and [user guide] (https://openssl.org/docs/fips/UserGuide-2.0.pdf).

Instructions:

1. Obtain a copy of openssl-fips-x.x.x.tar.gz.
   To comply with the security policy you must ensure the path
   through which you get the file complies with the requirements
   for a "secure installation" as described in section 6.6 in
   the [user guide] (https://openssl.org/docs/fips/UserGuide-2.0.pdf).
   For evaluation/experimentation you can simply download and verify
   `openssl-fips-x.x.x.tar.gz` from https://www.openssl.org/source/
2. Extract source to `openssl-fips` folder and `cd openssl-fips`
3. `./config`
4. `make`
5. `make install`
   (NOTE: to comply with the security policy you must use the exact
   commands in steps 3-5 without any additional options as per
   Appendix A in the [security policy]
   (http://csrc.nist.gov/groups/STM/cmvp/documents/140-1/140sp/140sp1747.pdf).
   The only exception is that `./config no-asm` can be
   used in place of `./config` )
6. Get into Node.js checkout folder
7. `./configure --openssl-fips=/path/to/openssl-fips/installdir`
   For example on ubuntu 12 the installation directory was
   /usr/local/ssl/fips-2.0
8. Build Node.js with `make -j`
9. Verify with `node -p "process.versions.openssl"` (`1.0.2a-fips`)

## Resources for Newcomers

* [CONTRIBUTING.md](./CONTRIBUTING.md)
* [GOVERNANCE.md](./GOVERNANCE.md)
* IRC:
  [#io.js on Freenode.net](http://webchat.freenode.net?channels=io.js&uio=d4)
* [nodejs/node on Gitter](https://gitter.im/nodejs/node)

## Security

All security bugs in node.js are taken seriously and should be reported by
emailing security@nodejs.org. This will be delivered to a subset of the project
team who handle security issues. Please don't disclose security bugs
public until they have been handled by the security team.

Your email will be acknowledged within 24 hours, and you’ll receive a more
detailed response to your email within 48 hours indicating the next steps in
handling your report.

## Current Project Team Members

The Node.js project team comprises a group of core collaborators and a sub-group
that forms the _Technical Steering Committee_ (TSC) which governs the project. For more
information about the governance of the Node.js project, see
[GOVERNANCE.md](./GOVERNANCE.md).

### TSC (Technical Steering Committee)

* [bnoordhuis](https://github.com/bnoordhuis) - **Ben Noordhuis** &lt;info@bnoordhuis.nl&gt;
* [chrisdickinson](https://github.com/chrisdickinson) - **Chris Dickinson** &lt;christopher.s.dickinson@gmail.com&gt;
* [cjihrig](https://github.com/cjihrig) - **Colin Ihrig** &lt;cjihrig@gmail.com&gt;
* [fishrock123](https://github.com/fishrock123) - **Jeremiah Senkpiel** &lt;fishrock123@rocketmail.com&gt;
* [indutny](https://github.com/indutny) - **Fedor Indutny** &lt;fedor.indutny@gmail.com&gt;
* [jasnell](https://github.com/jasnell) - **James M Snell** &lt;jasnell@gmail.com&gt;
* [misterdjules](https://github.com/misterdjules) - **Julien Gilli** &lt;jgilli@nodejs.org&gt;
* [mscdex](https://github.com/mscdex) - **Brian White** &lt;mscdex@mscdex.net&gt;
* [orangemocha](https://github.com/orangemocha) - **Alexis Campailla** &lt;orangemocha@nodejs.org&gt;
* [piscisaureus](https://github.com/piscisaureus) - **Bert Belder** &lt;bertbelder@gmail.com&gt;
* [rvagg](https://github.com/rvagg) - **Rod Vagg** &lt;rod@vagg.org&gt;
* [shigeki](https://github.com/shigeki) - **Shigeki Ohtsu** &lt;ohtsu@iij.ad.jp&gt;
* [trevnorris](https://github.com/trevnorris) - **Trevor Norris** &lt;trev.norris@gmail.com&gt;

### Collaborators

* [brendanashworth](https://github.com/brendanashworth) - **Brendan Ashworth** &lt;brendan.ashworth@me.com&gt;
* [ChALkeR](https://github.com/ChALkeR) - **Сковорода Никита Андреевич** &lt;chalkerx@gmail.com&gt;
* [domenic](https://github.com/domenic) - **Domenic Denicola** &lt;d@domenic.me&gt;
* [evanlucas](https://github.com/evanlucas) - **Evan Lucas** &lt;evanlucas@me.com&gt;
* [geek](https://github.com/geek) - **Wyatt Preul** &lt;wpreul@gmail.com&gt;
* [isaacs](https://github.com/isaacs) - **Isaac Z. Schlueter** &lt;i@izs.me&gt;
* [jbergstroem](https://github.com/jbergstroem) - **Johan Bergström** &lt;bugs@bergstroem.nu&gt;
* [joaocgreis](https://github.com/joaocgreis) - **João Reis** &lt;reis@janeasystems.com&gt;
* [julianduque](https://github.com/julianduque) - **Julian Duque** &lt;julianduquej@gmail.com&gt;
* [lxe](https://github.com/lxe) - **Aleksey Smolenchuk** &lt;lxe@lxe.co&gt;
* [mhdawson](https://github.com/mhdawson) - **Michael Dawson** &lt;michael_dawson@ca.ibm.com&gt;
* [micnic](https://github.com/micnic) - **Nicu Micleușanu** &lt;micnic90@gmail.com&gt;
* [mikeal](https://github.com/mikeal) - **Mikeal Rogers** &lt;mikeal.rogers@gmail.com&gt;
* [monsanto](https://github.com/monsanto) - **Christopher Monsanto** &lt;chris@monsan.to&gt;
* [ofrobots](https://github.com/ofrobots) - **Ali Ijaz Sheikh** &lt;ofrobots@google.com&gt;
* [Olegas](https://github.com/Olegas) - **Oleg Elifantiev** &lt;oleg@elifantiev.ru&gt;
* [petkaantonov](https://github.com/petkaantonov) - **Petka Antonov** &lt;petka_antonov@hotmail.com&gt;
* [qard](https://github.com/qard) - **Stephen Belanger** &lt;admin@stephenbelanger.com&gt;
* [rlidwka](https://github.com/rlidwka) - **Alex Kocharin** &lt;alex@kocharin.ru&gt;
* [robertkowalski](https://github.com/robertkowalski) - **Robert Kowalski** &lt;rok@kowalski.gd&gt;
* [sam-github](https://github.com/sam-github) - **Sam Roberts** &lt;vieuxtech@gmail.com&gt;
* [seishun](https://github.com/seishun) - **Nikolai Vavilov** &lt;vvnicholas@gmail.com&gt;
* [silverwind](https://github.com/silverwind) - **Roman Reiss** &lt;me@silverwind.io&gt;
* [srl295](https://github.com/srl295) - **Steven R Loomis** &lt;srloomis@us.ibm.com&gt;
* [targos](https://github.com/targos) - **Michaël Zasso** &lt;mic.besace@gmail.com&gt;
* [tellnes](https://github.com/tellnes) - **Christian Tellnes** &lt;christian@tellnes.no&gt;
* [thefourtheye](https://github.com/thefourtheye) - **Sakthipriyan Vairamani** &lt;thechargingvolcano@gmail.com&gt;
* [thlorenz](https://github.com/thlorenz) - **Thorsten Lorenz** &lt;thlorenz@gmx.de&gt;
* [Trott](https://github.com/Trott) - **Rich Trott** &lt;rtrott@gmail.com&gt;
* [tunniclm](https://github.com/tunniclm) - **Mike Tunnicliffe** &lt;m.j.tunnicliffe@gmail.com&gt;
* [vkurchatkin](https://github.com/vkurchatkin) - **Vladimir Kurchatkin** &lt;vladimir.kurchatkin@gmail.com&gt;
* [yosuke-furukawa](https://github.com/yosuke-furukawa) - **Yosuke Furukawa** &lt;yosuke.furukawa@gmail.com&gt;

Collaborators & TSC members follow the [COLLABORATOR_GUIDE.md](./COLLABORATOR_GUIDE.md) in
maintaining the Node.js project.

### Release Team

Releases of Node.js and io.js will be signed with one of the following GPG keys:

* **Chris Dickinson** &lt;christopher.s.dickinson@gmail.com&gt;: `9554F04D7259F04124DE6B476D5A82AC7E37093B`
* **Colin Ihrig** &lt;cjihrig@gmail.com&gt; `94AE36675C464D64BAFA68DD7434390BDBE9B9C5`
* **Sam Roberts** &lt;octetcloud@keybase.io&gt; `0034A06D9D9B0064CE8ADF6BF1747F4AD2306D93`
* **Jeremiah Senkpiel** &lt;fishrock@keybase.io&gt; `FD3A5288F042B6850C66B31F09FE44734EB7990E`
* **James M Snell** &lt;jasnell@keybase.io&gt; `71DCFD284A79C3B38668286BC97EC7A07EDE3FC1`
* **Rod Vagg** &lt;rod@vagg.org&gt; `DD8F2338BAE7501E3DD5AC78C273792F7D83545D`

The full set of trusted release keys can be imported by running:

```
gpg --keyserver pool.sks-keyservers.net --recv-keys 9554F04D7259F04124DE6B476D5A82AC7E37093B
gpg --keyserver pool.sks-keyservers.net --recv-keys 94AE36675C464D64BAFA68DD7434390BDBE9B9C5
gpg --keyserver pool.sks-keyservers.net --recv-keys 0034A06D9D9B0064CE8ADF6BF1747F4AD2306D93
gpg --keyserver pool.sks-keyservers.net --recv-keys FD3A5288F042B6850C66B31F09FE44734EB7990E
gpg --keyserver pool.sks-keyservers.net --recv-keys 71DCFD284A79C3B38668286BC97EC7A07EDE3FC1
gpg --keyserver pool.sks-keyservers.net --recv-keys DD8F2338BAE7501E3DD5AC78C273792F7D83545D
```

See the section above on [Verifying Binaries](#verifying-binaries) for
details on what to do with these keys to verify that a downloaded file is official.

Previous releases of Node.js have been signed with one of the following GPG
keys:

* Julien Gilli &lt;jgilli@fastmail.fm&gt; `114F43EE0176B71C7BC219DD50A3051F888C628D`
* Timothy J Fontaine &lt;tjfontaine@gmail.com&gt; `7937DFD2AB06298B2293C3187D33FF9D0246406D`
* Isaac Z. Schlueter &lt;i@izs.me&gt; `93C7E9E91B49E432C2F75674B0A78B0A6C481CF6`
