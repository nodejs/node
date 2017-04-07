<p align="center">
  <img alt="Node.js" src="https://nodejs.org/static/images/logo-light.svg" width="400"/>
</p>
<p align="center">
  <a title="Gitter" href="https://gitter.im/nodejs/node?utm_source=badge&utm_medium=badge&utm_campaign=pr-badge&utm_content=badge"><img src="https://badges.gitter.im/Join%20Chat.svg"></a>
  <a title="CII Best Practices" href="https://bestpractices.coreinfrastructure.org/projects/29"><img src="https://bestpractices.coreinfrastructure.org/projects/29/badge"></a>
</p>

Node.js is a JavaScript runtime built on Chrome's V8 JavaScript engine. Node.js
uses an event-driven, non-blocking I/O model that makes it lightweight and
efficient. The Node.js package ecosystem, [npm][], is the largest ecosystem of
open source libraries in the world.

The Node.js project is supported by the
[Node.js Foundation](https://nodejs.org/en/foundation/). Contributions,
policies, and releases are managed under an
[open governance model](./GOVERNANCE.md). We are also bound by a
[Code of Conduct](./CODE_OF_CONDUCT.md).

If you need help using or installing Node.js, please use the
[nodejs/help](https://github.com/nodejs/help) issue tracker.


# Table of Contents

* [Resources for Newcomers](#resources-for-newcomers)
* [Release Types](#release-types)
  * [Download](#download)
    * [Current and LTS Releases](#current-and-lts-releases)
    * [Nightly Releases](#nightly-releases)
    * [API Documentation](#api-documentation)
  * [Verifying Binaries](#verifying-binaries)
* [Building Node.js](#building-nodejs)
  * [Security](#security)
  * [Current Project Team Members](#current-project-team-members)
    * [CTC (Core Technical Committee)](#ctc-core-technical-committee)
    * [Collaborators](#collaborators)
    * [Release Team](#release-team)

## Resources for Newcomers

### Official Resources

* [Website][]
* [Node.js Help][]
* [Contributing to the project][]
* IRC (node core development): [#node-dev on chat.freenode.net][]

### Unofficial Resources

* IRC (general questions): [#node.js on chat.freenode.net][]. Please see
<http://nodeirc.info/> for more information regarding the `#node.js` IRC
channel.

_Please note that unofficial resources are neither managed by (nor necessarily
endorsed by) the Node.js TSC/CTC. Specifically, such resources are not
currently covered by the [Node.js Moderation Policy][] and the selection and
actions of resource operators/moderators are not subject to TSC/CTC oversight._

## Release Types

The Node.js project maintains multiple types of releases:

* **Current**: Released from active development branches of this repository,
  versioned by [SemVer](http://semver.org/) and signed by a member of the
  [Release Team](#release-team).
  Code for Current releases is organized in this repository by major version
  number. For example: [v4.x](https://github.com/nodejs/node/tree/v4.x).
  The major version number of Current releases will increment every 6 months
  allowing for breaking changes to be introduced. This happens in April and
  October every year. Current release lines beginning in October each year have
  a maximum support life of 8 months. Current release lines beginning in April
  each year will convert to LTS (see below) after 6 months and receive further
  support for 30 months.
* **LTS**: Releases that receive Long-term Support, with a focus on stability
  and security. Every second Current release line (major version) will become an
  LTS line and receive 18 months of _Active LTS_ support and a further 12
  months of _Maintenance_. LTS release lines are given alphabetically
  ordered codenames, beginning with v4 Argon. LTS releases are less frequent
  and will attempt to maintain consistent major and minor version numbers,
  only incrementing patch version numbers. There are no breaking changes or
  feature additions, except in some special circumstances. More information
  can be found in the [LTS README](https://github.com/nodejs/LTS/).
* **Nightly**: Versions of code in this repository on the current Current
  branch, automatically built every 24-hours where changes exist. Use with
  caution.

## Download

Binaries, installers, and source tarballs are available at
<https://nodejs.org>.

#### Current and LTS Releases
**Current** and **LTS** releases are available at
<https://nodejs.org/download/release/>, listed under their version strings.
The [latest](https://nodejs.org/download/release/latest/) directory is an
alias for the latest Current release. The latest LTS release from an LTS
line is available in the form: latest-_codename_. For example:
<https://nodejs.org/download/release/latest-argon>

#### Nightly Releases
**Nightly** builds are available at
<https://nodejs.org/download/nightly/>, listed under their version
string which includes their date (in UTC time) and the commit SHA at
the HEAD of the release.

#### API Documentation
**API documentation** is available in each release and nightly
directory under _docs_. <https://nodejs.org/api/> points to the API
documentation of the latest stable version.

### Verifying Binaries

Current, LTS and Nightly download directories all contain a _SHASUM256.txt_
file that lists the SHA checksums for each file available for
download.

The _SHASUM256.txt_ can be downloaded using curl.

```console
$ curl -O https://nodejs.org/dist/vx.y.z/SHASUMS256.txt
```

To check that a downloaded file matches the checksum, run
it through `sha256sum` with a command such as:

```console
$ grep node-vx.y.z.tar.gz SHASUMS256.txt | sha256sum -c -
```

_(Where "node-vx.y.z.tar.gz" is the name of the file you have
downloaded)_

Additionally, Current and LTS releases (not Nightlies) have GPG signed
copies of SHASUM256.txt files available as SHASUM256.txt.asc. You can use
`gpg` to verify that the file has not been tampered with.

To verify a SHASUM256.txt.asc, you will first need to import all of
the GPG keys of individuals authorized to create releases. They are
listed at the bottom of this README under [Release Team](#release-team).
Use a command such as this to import the keys:

```console
$ gpg --keyserver pool.sks-keyservers.net --recv-keys DD8F2338BAE7501E3DD5AC78C273792F7D83545D
```

_(See the bottom of this README for a full script to import active
release keys)_

You can then use `gpg --verify SHASUMS256.txt.asc` to verify that the
file has been signed by an authorized member of the Node.js team.

Once verified, use the SHASUMS256.txt.asc file to get the checksum for
the binary verification command above.

## Building Node.js

See [BUILDING.md](BUILDING.md) for instructions on how to build
Node.js from source along with a list of officially supported platforms.

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
that forms the _Core Technical Committee_ (CTC) which governs the project. For
more information about the governance of the Node.js project, see
[GOVERNANCE.md](./GOVERNANCE.md).

### CTC (Core Technical Committee)

* [addaleax](https://github.com/addaleax) -
**Anna Henningsen** &lt;anna@addaleax.net&gt; (she/her)
* [bnoordhuis](https://github.com/bnoordhuis) -
**Ben Noordhuis** &lt;info@bnoordhuis.nl&gt;
* [ChALkeR](https://github.com/ChALkeR) -
**Сковорода Никита Андреевич** &lt;chalkerx@gmail.com&gt; (he/him)
* [cjihrig](https://github.com/cjihrig) -
**Colin Ihrig** &lt;cjihrig@gmail.com&gt;
* [evanlucas](https://github.com/evanlucas) -
**Evan Lucas** &lt;evanlucas@me.com&gt; (he/him)
* [fhinkel](https://github.com/fhinkel) -
**Franziska Hinkelmann** &lt;franziska.hinkelmann@gmail.com&gt;
* [fishrock123](https://github.com/fishrock123) -
**Jeremiah Senkpiel** &lt;fishrock123@rocketmail.com&gt;
* [indutny](https://github.com/indutny) -
**Fedor Indutny** &lt;fedor.indutny@gmail.com&gt;
* [jasnell](https://github.com/jasnell) -
**James M Snell** &lt;jasnell@gmail.com&gt; (he/him)
* [mhdawson](https://github.com/mhdawson) -
**Michael Dawson** &lt;michael_dawson@ca.ibm.com&gt; (he/him)
* [misterdjules](https://github.com/misterdjules) -
**Julien Gilli** &lt;jgilli@nodejs.org&gt;
* [mscdex](https://github.com/mscdex) -
**Brian White** &lt;mscdex@mscdex.net&gt;
* [MylesBorins](https://github.com/MylesBorins) -
**Myles Borins** &lt;myles.borins@gmail.com&gt; (he/him)
* [ofrobots](https://github.com/ofrobots) -
**Ali Ijaz Sheikh** &lt;ofrobots@google.com&gt;
* [rvagg](https://github.com/rvagg) -
**Rod Vagg** &lt;rod@vagg.org&gt;
* [shigeki](https://github.com/shigeki) -
**Shigeki Ohtsu** &lt;ohtsu@ohtsu.org&gt; (he/him)
* [targos](https://github.com/targos) -
**Michaël Zasso** &lt;targos@protonmail.com&gt; (he/him)
* [thefourtheye](https://github.com/thefourtheye) -
**Sakthipriyan Vairamani** &lt;thechargingvolcano@gmail.com&gt; (he/him)
* [trevnorris](https://github.com/trevnorris) -
**Trevor Norris** &lt;trev.norris@gmail.com&gt;
* [Trott](https://github.com/Trott) -
**Rich Trott** &lt;rtrott@gmail.com&gt; (he/him)

### Collaborators

* [abouthiroppy](https://github.com/abouthiroppy) -
**Yuta Hiroto** &lt;hello@about-hiroppy.com&gt; (he/him)
* [ak239](https://github.com/ak239) -
**Aleksei Koziatinskii** &lt;ak239spb@gmail.com&gt;
* [andrasq](https://github.com/andrasq) -
**Andras** &lt;andras@kinvey.com&gt;
* [AndreasMadsen](https://github.com/AndreasMadsen) -
**Andreas Madsen** &lt;amwebdk@gmail.com&gt; (he/him)
* [aqrln](https://github.com/aqrln) -
**Alexey Orlenko** &lt;eaglexrlnk@gmail.com&gt;
* [bengl](https://github.com/bengl) -
**Bryan English** &lt;bryan@bryanenglish.com&gt; (he/him)
* [benjamingr](https://github.com/benjamingr) -
**Benjamin Gruenbaum** &lt;benjamingr@gmail.com&gt;
* [bmeck](https://github.com/bmeck) -
**Bradley Farias** &lt;bradley.meck@gmail.com&gt;
* [brendanashworth](https://github.com/brendanashworth) -
**Brendan Ashworth** &lt;brendan.ashworth@me.com&gt;
* [bzoz](https://github.com/bzoz) -
**Bartosz Sosnowski** &lt;bartosz@janeasystems.com&gt;
* [calvinmetcalf](https://github.com/calvinmetcalf) -
**Calvin Metcalf** &lt;calvin.metcalf@gmail.com&gt;
* [chrisdickinson](https://github.com/chrisdickinson) -
**Chris Dickinson** &lt;christopher.s.dickinson@gmail.com&gt;
* [claudiorodriguez](https://github.com/claudiorodriguez) -
**Claudio Rodriguez** &lt;cjrodr@yahoo.com&gt;
* [danbev](https://github.com/danbev) -
**Daniel Bevenius** &lt;daniel.bevenius@gmail.com&gt;
* [edsadr](https://github.com/edsadr) -
**Adrian Estrada** &lt;edsadr@gmail.com&gt; (he/him)
* [eljefedelrodeodeljefe](https://github.com/eljefedelrodeodeljefe) -
**Robert Jefe Lindstaedt** &lt;robert.lindstaedt@gmail.com&gt;
* [estliberitas](https://github.com/estliberitas) -
**Alexander Makarenko** &lt;estliberitas@gmail.com&gt;
* [eugeneo](https://github.com/eugeneo) -
**Eugene Ostroukhov** &lt;eostroukhov@google.com&gt;
* [firedfox](https://github.com/firedfox) -
**Daniel Wang** &lt;wangyang0123@gmail.com&gt;
* [geek](https://github.com/geek) -
**Wyatt Preul** &lt;wpreul@gmail.com&gt;
* [gibfahn](https://github.com/gibfahn) -
**Gibson Fahnestock** &lt;gibfahn@gmail.com&gt; (he/him)
* [iarna](https://github.com/iarna) -
**Rebecca Turner** &lt;me@re-becca.org&gt;
* [imyller](https://github.com/imyller) -
**Ilkka Myller** &lt;ilkka.myller@nodefield.com&gt;
* [isaacs](https://github.com/isaacs) -
**Isaac Z. Schlueter** &lt;i@izs.me&gt;
* [italoacasas](https://github.com/italoacasas) -
**Italo A. Casas** &lt;me@italoacasas.com&gt; (he/him)
* [iWuzHere](https://github.com/iWuzHere) -
**Imran Iqbal** &lt;imran@imraniqbal.org&gt;
* [JacksonTian](https://github.com/JacksonTian) -
**Jackson Tian** &lt;shyvo1987@gmail.com&gt;
* [jbergstroem](https://github.com/jbergstroem) -
**Johan Bergström** &lt;bugs@bergstroem.nu&gt;
* [jhamhader](https://github.com/jhamhader) -
**Yuval Brik** &lt;yuval@brik.org.il&gt;
* [joaocgreis](https://github.com/joaocgreis) -
**João Reis** &lt;reis@janeasystems.com&gt;
* [joshgav](https://github.com/joshgav) -
**Josh Gavant** &lt;josh.gavant@outlook.com&gt;
* [joyeecheung](https://github.com/joyeecheung) -
**Joyee Cheung** &lt;joyeec9h3@gmail.com&gt; (she/her)
* [julianduque](https://github.com/julianduque) -
**Julian Duque** &lt;julianduquej@gmail.com&gt; (he/him)
* [JungMinu](https://github.com/JungMinu) -
**Minwoo Jung** &lt;minwoo@nodesource.com&gt; (he/him)
* [lance](https://github.com/lance) -
**Lance Ball** &lt;lball@redhat.com&gt;
* [lpinca](https://github.com/lpinca) -
**Luigi Pinca** &lt;luigipinca@gmail.com&gt; (he/him)
* [lxe](https://github.com/lxe) -
**Aleksey Smolenchuk** &lt;lxe@lxe.co&gt;
* [matthewloring](https://github.com/matthewloring) -
**Matthew Loring** &lt;mattloring@google.com&gt;
* [mcollina](https://github.com/mcollina) -
**Matteo Collina** &lt;matteo.collina@gmail.com&gt; (he/him)
* [micnic](https://github.com/micnic) -
**Nicu Micleușanu** &lt;micnic90@gmail.com&gt; (he/him)
* [mikeal](https://github.com/mikeal) -
**Mikeal Rogers** &lt;mikeal.rogers@gmail.com&gt;
* [monsanto](https://github.com/monsanto) -
**Christopher Monsanto** &lt;chris@monsan.to&gt;
* [not-an-aardvark](https://github.com/not-an-aardvark) -
**Teddy Katz** &lt;teddy.katz@gmail.com&gt;
* [Olegas](https://github.com/Olegas) -
**Oleg Elifantiev** &lt;oleg@elifantiev.ru&gt;
* [orangemocha](https://github.com/orangemocha) -
**Alexis Campailla** &lt;orangemocha@nodejs.org&gt;
* [othiym23](https://github.com/othiym23) -
**Forrest L Norvell** &lt;ogd@aoaioxxysz.net&gt; (he/him)
* [petkaantonov](https://github.com/petkaantonov) -
**Petka Antonov** &lt;petka_antonov@hotmail.com&gt;
* [phillipj](https://github.com/phillipj) -
**Phillip Johnsen** &lt;johphi@gmail.com&gt;
* [piscisaureus](https://github.com/piscisaureus) -
**Bert Belder** &lt;bertbelder@gmail.com&gt;
* [pmq20](https://github.com/pmq20) -
**Minqi Pan** &lt;pmq2001@gmail.com&gt;
* [princejwesley](https://github.com/princejwesley) -
**Prince John Wesley** &lt;princejohnwesley@gmail.com&gt;
* [qard](https://github.com/qard) -
**Stephen Belanger** &lt;admin@stephenbelanger.com&gt; (he/him)
* [refack](https://github.com/refack) -
**Refael Ackermann** &lt;refack@gmail.com&gt; (he/him)
* [richardlau](https://github.com/richardlau) -
**Richard Lau** &lt;riclau@uk.ibm.com&gt;
* [rlidwka](https://github.com/rlidwka) -
**Alex Kocharin** &lt;alex@kocharin.ru&gt;
* [rmg](https://github.com/rmg) -
**Ryan Graham** &lt;r.m.graham@gmail.com&gt;
* [robertkowalski](https://github.com/robertkowalski) -
**Robert Kowalski** &lt;rok@kowalski.gd&gt;
* [romankl](https://github.com/romankl) -
**Roman Klauke** &lt;romaaan.git@gmail.com&gt;
* [ronkorving](https://github.com/ronkorving) -
**Ron Korving** &lt;ron@ronkorving.nl&gt;
* [RReverser](https://github.com/RReverser) -
**Ingvar Stepanyan** &lt;me@rreverser.com&gt;
* [saghul](https://github.com/saghul) -
**Saúl Ibarra Corretgé** &lt;saghul@gmail.com&gt;
* [sam-github](https://github.com/sam-github) -
**Sam Roberts** &lt;vieuxtech@gmail.com&gt;
* [santigimeno](https://github.com/santigimeno) -
**Santiago Gimeno** &lt;santiago.gimeno@gmail.com&gt;
* [seishun](https://github.com/seishun) -
**Nikolai Vavilov** &lt;vvnicholas@gmail.com&gt;
* [silverwind](https://github.com/silverwind) -
**Roman Reiss** &lt;me@silverwind.io&gt;
* [srl295](https://github.com/srl295) -
**Steven R Loomis** &lt;srloomis@us.ibm.com&gt;
* [stefanmb](https://github.com/stefanmb) -
**Stefan Budeanu** &lt;stefan@budeanu.com&gt;
* [tellnes](https://github.com/tellnes) -
**Christian Tellnes** &lt;christian@tellnes.no&gt;
* [thekemkid](https://github.com/thekemkid) -
**Glen Keane** &lt;glenkeane.94@gmail.com&gt; (he/him)
* [thlorenz](https://github.com/thlorenz) -
**Thorsten Lorenz** &lt;thlorenz@gmx.de&gt;
* [TimothyGu](https://github.com/TimothyGu) -
**Timothy Gu** &lt;timothygu99@gmail.com&gt; (he/him)
* [tunniclm](https://github.com/tunniclm) -
**Mike Tunnicliffe** &lt;m.j.tunnicliffe@gmail.com&gt;
* [vkurchatkin](https://github.com/vkurchatkin) -
**Vladimir Kurchatkin** &lt;vladimir.kurchatkin@gmail.com&gt;
* [vsemozhetbyt](https://github.com/vsemozhetbyt) -
**Vse Mozhet Byt** &lt;vsemozhetbyt@gmail.com&gt; (he/him)
* [watilde](https://github.com/watilde) -
**Daijiro Wachi** &lt;daijiro.wachi@gmail.com&gt; (he/him)
* [whitlockjc](https://github.com/whitlockjc) -
**Jeremy Whitlock** &lt;jwhitlock@apache.org&gt;
* [yorkie](https://github.com/yorkie) -
**Yorkie Liu** &lt;yorkiefixer@gmail.com&gt;
* [yosuke-furukawa](https://github.com/yosuke-furukawa) -
**Yosuke Furukawa** &lt;yosuke.furukawa@gmail.com&gt;

Collaborators (which includes CTC members) follow the
[COLLABORATOR_GUIDE.md](./COLLABORATOR_GUIDE.md) in maintaining the Node.js
project.

### Release Team

Node.js releases are signed with one of the following GPG keys:

* **Colin Ihrig** &lt;cjihrig@gmail.com&gt;
`94AE36675C464D64BAFA68DD7434390BDBE9B9C5`
* **Evan Lucas** &lt;evanlucas@me.com&gt;
`B9AE9905FFD7803F25714661B63B535A4C206CA9`
* **Italo A. Casas** &lt;me@italoacasas.com&gt;
`56730D5401028683275BD23C23EFEFE93C4CFFFE`
* **James M Snell** &lt;jasnell@keybase.io&gt;
`71DCFD284A79C3B38668286BC97EC7A07EDE3FC1`
* **Jeremiah Senkpiel** &lt;fishrock@keybase.io&gt;
`FD3A5288F042B6850C66B31F09FE44734EB7990E`
* **Myles Borins** &lt;myles.borins@gmail.com&gt;
`C4F0DFFF4E8C1A8236409D08E73BC641CC11F4C8`
* **Rod Vagg** &lt;rod@vagg.org&gt;
`DD8F2338BAE7501E3DD5AC78C273792F7D83545D`

The full set of trusted release keys can be imported by running:

```shell
gpg --keyserver pool.sks-keyservers.net --recv-keys 94AE36675C464D64BAFA68DD7434390BDBE9B9C5
gpg --keyserver pool.sks-keyservers.net --recv-keys FD3A5288F042B6850C66B31F09FE44734EB7990E
gpg --keyserver pool.sks-keyservers.net --recv-keys 71DCFD284A79C3B38668286BC97EC7A07EDE3FC1
gpg --keyserver pool.sks-keyservers.net --recv-keys DD8F2338BAE7501E3DD5AC78C273792F7D83545D
gpg --keyserver pool.sks-keyservers.net --recv-keys C4F0DFFF4E8C1A8236409D08E73BC641CC11F4C8
gpg --keyserver pool.sks-keyservers.net --recv-keys B9AE9905FFD7803F25714661B63B535A4C206CA9
gpg --keyserver pool.sks-keyservers.net --recv-keys 56730D5401028683275BD23C23EFEFE93C4CFFFE
```

See the section above on [Verifying Binaries](#verifying-binaries) for details
on what to do with these keys to verify that a downloaded file is official.

Previous releases may also have been signed with one of the following GPG keys:

* **Chris Dickinson** &lt;christopher.s.dickinson@gmail.com&gt;
`9554F04D7259F04124DE6B476D5A82AC7E37093B`
* **Isaac Z. Schlueter** &lt;i@izs.me&gt;
`93C7E9E91B49E432C2F75674B0A78B0A6C481CF6`
* **Julien Gilli** &lt;jgilli@fastmail.fm&gt;
`114F43EE0176B71C7BC219DD50A3051F888C628D`
* **Timothy J Fontaine** &lt;tjfontaine@gmail.com&gt;
`7937DFD2AB06298B2293C3187D33FF9D0246406D`

### Working Groups

Information on the current Node.js Working Groups can be found in the
[CTC repository](https://github.com/nodejs/CTC/blob/master/WORKING_GROUPS.md).

[npm]: https://www.npmjs.com
[Website]: https://nodejs.org/en/
[Contributing to the project]: CONTRIBUTING.md
[Node.js Help]: https://github.com/nodejs/help
[Node.js Moderation Policy]: https://github.com/nodejs/TSC/blob/master/Moderation-Policy.md
[#node.js on chat.freenode.net]: https://webchat.freenode.net?channels=node.js&uio=d4
[#node-dev on chat.freenode.net]: https://webchat.freenode.net?channels=node-dev&uio=d4
