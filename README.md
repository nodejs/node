Node.js
=======

[![Gitter](https://badges.gitter.im/Join Chat.svg)](https://gitter.im/nodejs/node?utm_source=badge&utm_medium=badge&utm_campaign=pr-badge&utm_content=badge)

Node.js is a JavaScript runtime built on Chrome's V8 JavaScript engine. Node.js
uses an event-driven, non-blocking I/O model that makes it lightweight and
efficient. The Node.js package ecosystem, npm, is the largest ecosystem of open
source libraries in the world.

The Node.js project is supported by the
[Node.js Foundation](https://nodejs.org/en/foundation/). Contributions,
policies and releases are managed under an
[open governance model](./GOVERNANCE.md). We are also bound by a
[Code of Conduct](./CODE_OF_CONDUCT.md).

If you need help using or installing Node.js, please use the
[nodejs/help](https://github.com/nodejs/help) issue tracker.

## Release Types

The Node.js project maintains multiple types of releases:

* **Stable**: Released from active development branches of this repository,
  versioned by [SemVer](http://semver.org/) and signed by a member of the
  [Release Team](#release-team).
  Code for Stable releases is organized in this repository by major version
  number, For example: [v4.x](https://github.com/nodejs/node/tree/v4.x).
  The major version number of Stable releases will increment every 6 months
  allowing for breaking changes to be introduced. This happens in April and
  October every year. Stable release lines beginning in October each year have
  a maximum support life of 8 months. Stable release lines beginning in April
  each year will convert to LTS (see below) after 6 months and receive further
  support for 30 months.
* **LTS**: Releases that receive Long-term Support, with a focus on stability
  and security. Every second Stable release line (major version) will become an
  LTS line and receive 18 months of _Active LTS_ support and a further 12
  months of _Maintenance_. LTS release lines are given alphabetically
  ordered codenames, beginning with v4 Argon. LTS releases are less frequent
  and will attempt to maintain consistent major and minor version numbers,
  only incrementing patch version numbers. There are no breaking changes or
  feature additions, except in some special circumstances. More information
  can be found in the [LTS README](https://github.com/nodejs/LTS/).
* **Nightly**: Versions of code in this repository on the current Stable
  branch, automatically built every 24-hours where changes exist. Use with
  caution.

## Download

Binaries, installers, and source tarballs are available at
<https://nodejs.org>.

**Stable** and **LTS** releases are available at
<https://nodejs.org/download/release/>, listed under their version strings.
The [latest](https://nodejs.org/download/release/latest/) directory is an
alias for the latest Stable release. The latest LTS release from an LTS
line is available in the form: latest-_codename_. For example:
<https://nodejs.org/download/release/latest-argon>

**Nightly** builds are available at
<https://nodejs.org/download/nightly/>, listed under their version
string which includes their date (in UTC time) and the commit SHA at
the HEAD of the release.

**API documentation** is available in each release and nightly
directory under _docs_. <https://nodejs.org/api/> points to the API
documentation of the latest stable version.

### Verifying Binaries

Stable, LTS and Nightly download directories all contain a *SHASUM256.txt*
file that lists the SHA checksums for each file available for
download.

The *SHASUM256.txt* can be downloaded using curl.

```
$ curl -O https://nodejs.org/dist/vx.y.z/SHASUMS256.txt
```

To check that a downloaded file matches the checksum, run
it through `sha256sum` with a command such as:

```
$ grep node-vx.y.z.tar.gz SHASUMS256.txt | sha256sum -c -
```

_(Where "node-vx.y.z.tar.gz" is the name of the file you have
downloaded)_

Additionally, Stable and LTS releases (not Nightlies) have GPG signed
copies of SHASUM256.txt files available as SHASUM256.txt.asc. You can use
`gpg` to verify that the file has not been tampered with.

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

## Building Node.js

See [BUILDING.md](BUILDING.md) for instructions on how to build
Node.js from source.


## Resources for Newcomers

* [CODE_OF_CONDUCT.md](./CODE_OF_CONDUCT.md)
* [CONTRIBUTING.md](./CONTRIBUTING.md)
* [GOVERNANCE.md](./GOVERNANCE.md)
* IRC (general questions): [#node.js on Freenode.net](https://webchat.freenode.net?channels=node.js&uio=d4)
* IRC (node core development): [#node-dev on Freenode.net](https://webchat.freenode.net?channels=node-dev&uio=d4)
* [nodejs/node on Gitter](https://gitter.im/nodejs/node)

## Security

All security bugs in Node.js are taken seriously and should be reported by
emailing security@nodejs.org. This will be delivered to a subset of the project
team who handle security issues. Please don't disclose security bugs
publicly until they have been handled by the security team.

Your email will be acknowledged within 24 hours, and you’ll receive a more
detailed response to your email within 48 hours indicating the next steps in
handling your report.

## Current Project Team Members

The Node.js project team comprises a group of core collaborators and a sub-group
that forms the _Core Technical Committee_ (CTC) which governs the project. For more
information about the governance of the Node.js project, see
[GOVERNANCE.md](./GOVERNANCE.md).

### CTC (Core Technical Committee)

* [bnoordhuis](https://github.com/bnoordhuis) - **Ben Noordhuis** &lt;info@bnoordhuis.nl&gt;
* [ChALkeR](https://github.com/ChALkeR) - **Сковорода Никита Андреевич** &lt;chalkerx@gmail.com&gt;
* [chrisdickinson](https://github.com/chrisdickinson) - **Chris Dickinson** &lt;christopher.s.dickinson@gmail.com&gt;
* [cjihrig](https://github.com/cjihrig) - **Colin Ihrig** &lt;cjihrig@gmail.com&gt;
* [evanlucas](https://github.com/evanlucas) - **Evan Lucas** &lt;evanlucas@me.com&gt;
* [fishrock123](https://github.com/fishrock123) - **Jeremiah Senkpiel** &lt;fishrock123@rocketmail.com&gt;
* [indutny](https://github.com/indutny) - **Fedor Indutny** &lt;fedor.indutny@gmail.com&gt;
* [jasnell](https://github.com/jasnell) - **James M Snell** &lt;jasnell@gmail.com&gt;
* [mhdawson](https://github.com/mhdawson) - **Michael Dawson** &lt;michael_dawson@ca.ibm.com&gt;
* [misterdjules](https://github.com/misterdjules) - **Julien Gilli** &lt;jgilli@nodejs.org&gt;
* [mscdex](https://github.com/mscdex) - **Brian White** &lt;mscdex@mscdex.net&gt;
* [ofrobots](https://github.com/ofrobots) - **Ali Ijaz Sheikh** &lt;ofrobots@google.com&gt;
* [orangemocha](https://github.com/orangemocha) - **Alexis Campailla** &lt;orangemocha@nodejs.org&gt;
* [piscisaureus](https://github.com/piscisaureus) - **Bert Belder** &lt;bertbelder@gmail.com&gt;
* [rvagg](https://github.com/rvagg) - **Rod Vagg** &lt;rod@vagg.org&gt;
* [shigeki](https://github.com/shigeki) - **Shigeki Ohtsu** &lt;ohtsu@iij.ad.jp&gt;
* [trevnorris](https://github.com/trevnorris) - **Trevor Norris** &lt;trev.norris@gmail.com&gt;
* [Trott](https://github.com/Trott) - **Rich Trott** &lt;rtrott@gmail.com&gt;

### Collaborators

* [AndreasMadsen](https://github.com/AndreasMadsen) - **Andreas Madsen** &lt;amwebdk@gmail.com&gt;
* [benjamingr](https://github.com/benjamingr) - **Benjamin Gruenbaum** &lt;benjamingr@gmail.com&gt;
* [brendanashworth](https://github.com/brendanashworth) - **Brendan Ashworth** &lt;brendan.ashworth@me.com&gt;
* [calvinmetcalf](https://github.com/calvinmetcalf) - **Calvin Metcalf** &lt;calvin.metcalf@gmail.com&gt;
* [claudiorodriguez](https://github.com/claudiorodriguez) - **Claudio Rodriguez** &lt;cjrodr@yahoo.com&gt;
* [domenic](https://github.com/domenic) - **Domenic Denicola** &lt;d@domenic.me&gt;
* [geek](https://github.com/geek) - **Wyatt Preul** &lt;wpreul@gmail.com&gt;
* [iarna](https://github.com/iarna) - **Rebecca Turner** &lt;me@re-becca.org&gt;
* [isaacs](https://github.com/isaacs) - **Isaac Z. Schlueter** &lt;i@izs.me&gt;
* [jbergstroem](https://github.com/jbergstroem) - **Johan Bergström** &lt;bugs@bergstroem.nu&gt;
* [joaocgreis](https://github.com/joaocgreis) - **João Reis** &lt;reis@janeasystems.com&gt;
* [julianduque](https://github.com/julianduque) - **Julian Duque** &lt;julianduquej@gmail.com&gt;
* [JungMinu](https://github.com/JungMinu) - **Minwoo Jung** &lt;jmwsoft@gmail.com&gt;
* [lxe](https://github.com/lxe) - **Aleksey Smolenchuk** &lt;lxe@lxe.co&gt;
* [matthewloring](https://github.com/matthewloring) - **Matthew Loring** &lt;mattloring@google.com&gt;
* [mcollina](https://github.com/mcollina) - **Matteo Collina** &lt;matteo.collina@gmail.com&gt;
* [micnic](https://github.com/micnic) - **Nicu Micleușanu** &lt;micnic90@gmail.com&gt;
* [mikeal](https://github.com/mikeal) - **Mikeal Rogers** &lt;mikeal.rogers@gmail.com&gt;
* [monsanto](https://github.com/monsanto) - **Christopher Monsanto** &lt;chris@monsan.to&gt;
* [Olegas](https://github.com/Olegas) - **Oleg Elifantiev** &lt;oleg@elifantiev.ru&gt;
* [petkaantonov](https://github.com/petkaantonov) - **Petka Antonov** &lt;petka_antonov@hotmail.com&gt;
* [phillipj](https://github.com/phillipj) - **Phillip Johnsen** &lt;johphi@gmail.com&gt;
* [qard](https://github.com/qard) - **Stephen Belanger** &lt;admin@stephenbelanger.com&gt;
* [rlidwka](https://github.com/rlidwka) - **Alex Kocharin** &lt;alex@kocharin.ru&gt;
* [rmg](https://github.com/rmg) - **Ryan Graham** &lt;r.m.graham@gmail.com&gt;
* [robertkowalski](https://github.com/robertkowalski) - **Robert Kowalski** &lt;rok@kowalski.gd&gt;
* [romankl](https://github.com/romankl) - **Roman Klauke** &lt;romaaan.git@gmail.com&gt;
* [saghul](https://github.com/saghul) - **Saúl Ibarra Corretgé** &lt;saghul@gmail.com&gt;
* [sam-github](https://github.com/sam-github) - **Sam Roberts** &lt;vieuxtech@gmail.com&gt;
* [seishun](https://github.com/seishun) - **Nikolai Vavilov** &lt;vvnicholas@gmail.com&gt;
* [silverwind](https://github.com/silverwind) - **Roman Reiss** &lt;me@silverwind.io&gt;
* [srl295](https://github.com/srl295) - **Steven R Loomis** &lt;srloomis@us.ibm.com&gt;
* [targos](https://github.com/targos) - **Michaël Zasso** &lt;mic.besace@gmail.com&gt;
* [tellnes](https://github.com/tellnes) - **Christian Tellnes** &lt;christian@tellnes.no&gt;
* [thealphanerd](https://github.com/thealphanerd) - **Myles Borins** &lt;myles.borins@gmail.com&gt;
* [thefourtheye](https://github.com/thefourtheye) - **Sakthipriyan Vairamani** &lt;thechargingvolcano@gmail.com&gt;
* [thekemkid](https://github.com/thekemkid) - **Glen Keane** &lt;glenkeane.94@gmail.com&gt;
* [thlorenz](https://github.com/thlorenz) - **Thorsten Lorenz** &lt;thlorenz@gmx.de&gt;
* [tunniclm](https://github.com/tunniclm) - **Mike Tunnicliffe** &lt;m.j.tunnicliffe@gmail.com&gt;
* [vkurchatkin](https://github.com/vkurchatkin) - **Vladimir Kurchatkin** &lt;vladimir.kurchatkin@gmail.com&gt;
* [whitlockjc](https://github.com/whitlockjc) - **Jeremy Whitlock** &lt;jwhitlock@apache.org&gt;
* [yosuke-furukawa](https://github.com/yosuke-furukawa) - **Yosuke Furukawa** &lt;yosuke.furukawa@gmail.com&gt;
* [zkat](https://github.com/zkat) - **Kat Marchán** &lt;kzm@sykosomatic.org&gt;

Collaborators & CTC members follow the [COLLABORATOR_GUIDE.md](./COLLABORATOR_GUIDE.md) in
maintaining the Node.js project.

### Release Team

Releases of Node.js and io.js will be signed with one of the following GPG keys:

* **Chris Dickinson** &lt;christopher.s.dickinson@gmail.com&gt; `9554F04D7259F04124DE6B476D5A82AC7E37093B`
* **Colin Ihrig** &lt;cjihrig@gmail.com&gt; `94AE36675C464D64BAFA68DD7434390BDBE9B9C5`
* **Evan Lucas** &lt;evanlucas@me.com&gt; `B9AE9905FFD7803F25714661B63B535A4C206CA9`
* **James M Snell** &lt;jasnell@keybase.io&gt; `71DCFD284A79C3B38668286BC97EC7A07EDE3FC1`
* **Jeremiah Senkpiel** &lt;fishrock@keybase.io&gt; `FD3A5288F042B6850C66B31F09FE44734EB7990E`
* **Myles Borins** &lt;myles.borins@gmail.com&gt; `C4F0DFFF4E8C1A8236409D08E73BC641CC11F4C8`
* **Rod Vagg** &lt;rod@vagg.org&gt; `DD8F2338BAE7501E3DD5AC78C273792F7D83545D`
* **Sam Roberts** &lt;octetcloud@keybase.io&gt; `0034A06D9D9B0064CE8ADF6BF1747F4AD2306D93`

The full set of trusted release keys can be imported by running:

```
gpg --keyserver pool.sks-keyservers.net --recv-keys 9554F04D7259F04124DE6B476D5A82AC7E37093B
gpg --keyserver pool.sks-keyservers.net --recv-keys 94AE36675C464D64BAFA68DD7434390BDBE9B9C5
gpg --keyserver pool.sks-keyservers.net --recv-keys 0034A06D9D9B0064CE8ADF6BF1747F4AD2306D93
gpg --keyserver pool.sks-keyservers.net --recv-keys FD3A5288F042B6850C66B31F09FE44734EB7990E
gpg --keyserver pool.sks-keyservers.net --recv-keys 71DCFD284A79C3B38668286BC97EC7A07EDE3FC1
gpg --keyserver pool.sks-keyservers.net --recv-keys DD8F2338BAE7501E3DD5AC78C273792F7D83545D
gpg --keyserver pool.sks-keyservers.net --recv-keys C4F0DFFF4E8C1A8236409D08E73BC641CC11F4C8
gpg --keyserver pool.sks-keyservers.net --recv-keys B9AE9905FFD7803F25714661B63B535A4C206CA9
```

See the section above on [Verifying Binaries](#verifying-binaries) for
details on what to do with these keys to verify that a downloaded file is official.

Previous releases of Node.js have been signed with one of the following GPG
keys:

* **Isaac Z. Schlueter** &lt;i@izs.me&gt; `93C7E9E91B49E432C2F75674B0A78B0A6C481CF6`
* **Julien Gilli** &lt;jgilli@fastmail.fm&gt; `114F43EE0176B71C7BC219DD50A3051F888C628D`
* **Timothy J Fontaine** &lt;tjfontaine@gmail.com&gt; `7937DFD2AB06298B2293C3187D33FF9D0246406D`
