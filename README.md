<!--lint disable no-literal-urls-->
<p align="center">
  <a href="https://nodejs.org/">
    <img
      alt="Node.js"
      src="https://nodejs.org/static/images/logo-light.svg"
      width="400"
    />
  </a>
</p>

Node.js is an open-source, cross-platform, JavaScript runtime environment. It
executes JavaScript code outside of a browser. For more information on using
Node.js, see the [Node.js Website][].

The Node.js project uses an [open governance model](./GOVERNANCE.md). The
[OpenJS Foundation][] provides support for the project.

**This project is bound by a [Code of Conduct][].**

# Table of Contents

* [Support](#support)
* [Release Types](#release-types)
  * [Download](#download)
    * [Current and LTS Releases](#current-and-lts-releases)
    * [Nightly Releases](#nightly-releases)
    * [API Documentation](#api-documentation)
  * [Verifying Binaries](#verifying-binaries)
* [Building Node.js](#building-nodejs)
* [Security](#security)
* [Contributing to Node.js](#contributing-to-nodejs)
* [Current Project Team Members](#current-project-team-members)
  * [TSC (Technical Steering Committee)](#tsc-technical-steering-committee)
  * [Collaborators](#collaborators)
  * [Release Keys](#release-keys)

## Support

Looking for help? Check out the
[instructions for getting support](.github/SUPPORT.md).

## Release Types

* **Current**: Under active development. Code for the Current release is in the
  branch for its major version number (for example,
  [v10.x](https://github.com/nodejs/node/tree/v10.x)). Node.js releases a new
  major version every 6 months, allowing for breaking changes. This happens in
  April and October every year. Releases appearing each October have a support
  life of 8 months. Releases appearing each April convert to LTS (see below)
  each October.
* **LTS**: Releases that receive Long-term Support, with a focus on stability
  and security. Every even-numbered major version will become an LTS release.
  LTS releases receive 12 months of _Active LTS_ support and a further 18 months
  of _Maintenance_. LTS release lines have alphabetically-ordered codenames,
  beginning with v4 Argon. There are no breaking changes or feature additions,
  except in some special circumstances.
* **Nightly**: Code from the Current branch built every 24-hours when there are
  changes. Use with caution.

Current and LTS releases follow [Semantic Versioning](https://semver.org). A
member of the Release Team [signs](#release-keys) each Current and LTS release.
For more information, see the
[Release README](https://github.com/nodejs/Release#readme).

### Download

Binaries, installers, and source tarballs are available at
<https://nodejs.org/en/download/>.

#### Current and LTS Releases
<https://nodejs.org/download/release/>

The [latest](https://nodejs.org/download/release/latest/) directory is an
alias for the latest Current release. The latest-_codename_ directory is an
alias for the latest release from an LTS line. For example, the
[latest-carbon](https://nodejs.org/download/release/latest-carbon/) directory
contains the latest Carbon (Node.js 8) release.

#### Nightly Releases
<https://nodejs.org/download/nightly/>

Each directory name and filename contains a date (in UTC) and the commit
SHA at the HEAD of the release.

#### API Documentation

Documentation for the latest Current release is at <https://nodejs.org/api/>.
Version-specific documentation is available in each release directory in the
_docs_ subdirectory. Version-specific documentation is also at
<https://nodejs.org/download/docs/>.

### Verifying Binaries

Download directories contain a `SHASUMS256.txt` file with SHA checksums for the
files.

To download `SHASUMS256.txt` using `curl`:

```console
$ curl -O https://nodejs.org/dist/vx.y.z/SHASUMS256.txt
```

To check that a downloaded file matches the checksum, run
it through `sha256sum` with a command such as:

```console
$ grep node-vx.y.z.tar.gz SHASUMS256.txt | sha256sum -c -
```

For Current and LTS, the GPG detached signature of `SHASUMS256.txt` is in
`SHASUMS256.txt.sig`. You can use it with `gpg` to verify the integrity of
`SHASUM256.txt`. You will first need to import
[the GPG keys of individuals authorized to create releases](#release-keys). To
import the keys:

```console
$ gpg --keyserver pool.sks-keyservers.net --recv-keys DD8F2338BAE7501E3DD5AC78C273792F7D83545D
```

See the bottom of this README for a full script to import active release keys.

Next, download the `SHASUMS256.txt.sig` for the release:

```console
$ curl -O https://nodejs.org/dist/vx.y.z/SHASUMS256.txt.sig
```

Then use `gpg --verify SHASUMS256.txt.sig SHASUMS256.txt` to verify
the file's signature.

## Building Node.js

See [BUILDING.md](BUILDING.md) for instructions on how to build Node.js from
source and a list of supported platforms.

## Security

For information on reporting security vulnerabilities in Node.js, see
[SECURITY.md](./SECURITY.md).

## Contributing to Node.js

* [Contributing to the project][]
* [Working Groups][]
* [Strategic Initiatives][]

## Current Project Team Members

For information about the governance of the Node.js project, see
[GOVERNANCE.md](./GOVERNANCE.md).

### TSC (Technical Steering Committee)

<!--lint disable prohibited-strings maximum-line-length-->
* @addaleax - **Anna Henningsen** &lt;anna@addaleax.net&gt; (she/her)
* @apapirovski - **Anatoli Papirovski** &lt;apapirovski@mac.com&gt; (he/him)
* @BethGriggs - **Beth Griggs** &lt;Bethany.Griggs@uk.ibm.com&gt; (she/her)
* @BridgeAR - **Ruben Bridgewater** &lt;ruben@bridgewater.de&gt; (he/him)
* @ChALkeR - **Сковорода Никита Андреевич** &lt;chalkerx@gmail.com&gt; (he/him)
* @cjihrig - **Colin Ihrig** &lt;cjihrig@gmail.com&gt; (he/him)
* @codebytere - **Shelley Vohr** &lt;codebytere@gmail.com&gt; (she/her)
* @danbev - **Daniel Bevenius** &lt;daniel.bevenius@gmail.com&gt; (he/him)
* @fhinkel - **Franziska Hinkelmann** &lt;franziska.hinkelmann@gmail.com&gt; (she/her)
* @gabrielschulhof - **Gabriel Schulhof** &lt;gabriel.schulhof@intel.com&gt;
* @gireeshpunathil - **Gireesh Punathil** &lt;gpunathi@in.ibm.com&gt; (he/him)
* @jasnell - **James M Snell** &lt;jasnell@gmail.com&gt; (he/him)
* @joyeecheung - **Joyee Cheung** &lt;joyeec9h3@gmail.com&gt; (she/her)
* @mcollina - **Matteo Collina** &lt;matteo.collina@gmail.com&gt; (he/him)
* @mhdawson - **Michael Dawson** &lt;michael_dawson@ca.ibm.com&gt; (he/him)
* @mmarchini - **Mary Marchini** &lt;oss@mmarchini.me&gt; (she/her)
* @MylesBorins - **Myles Borins** &lt;myles.borins@gmail.com&gt; (he/him)
* @targos - **Michaël Zasso** &lt;targos@protonmail.com&gt; (he/him)
* @tniessen - **Tobias Nießen** &lt;tniessen@tnie.de&gt;
* @Trott - **Rich Trott** &lt;rtrott@gmail.com&gt; (he/him)

### TSC Emeriti

* @bnoordhuis - **Ben Noordhuis** &lt;info@bnoordhuis.nl&gt;
* @chrisdickinson - **Chris Dickinson** &lt;christopher.s.dickinson@gmail.com&gt;
* @evanlucas - **Evan Lucas** &lt;evanlucas@me.com&gt; (he/him)
* @Fishrock123 - **Jeremiah Senkpiel** &lt;fishrock123@rocketmail.com&gt; (he/they)
* @gibfahn - **Gibson Fahnestock** &lt;gibfahn@gmail.com&gt; (he/him)
* @indutny - **Fedor Indutny** &lt;fedor.indutny@gmail.com&gt;
* @isaacs - **Isaac Z. Schlueter** &lt;i@izs.me&gt;
* @joshgav - **Josh Gavant** &lt;josh.gavant@outlook.com&gt;
* @mscdex - **Brian White** &lt;mscdex@mscdex.net&gt;
* @nebrius - **Bryan Hughes** &lt;bryan@nebri.us&gt;
* @ofrobots - **Ali Ijaz Sheikh** &lt;ofrobots@google.com&gt; (he/him)
* @orangemocha - **Alexis Campailla** &lt;orangemocha@nodejs.org&gt;
* @piscisaureus - **Bert Belder** &lt;bertbelder@gmail.com&gt;
* @rvagg - **Rod Vagg** &lt;r@va.gg&gt;
* @sam-github - **Sam Roberts** &lt;vieuxtech@gmail.com&gt;
* @shigeki - **Shigeki Ohtsu** &lt;ohtsu@ohtsu.org&gt; (he/him)
* @thefourtheye - **Sakthipriyan Vairamani** &lt;thechargingvolcano@gmail.com&gt; (he/him)
* @TimothyGu - **Tiancheng "Timothy" Gu** &lt;timothygu99@gmail.com&gt; (he/him)
* @trevnorris - **Trevor Norris** &lt;trev.norris@gmail.com&gt;

### Collaborators

* @addaleax - **Anna Henningsen** &lt;anna@addaleax.net&gt; (she/her)
* @ak239 - **Aleksei Koziatinskii** &lt;ak239spb@gmail.com&gt;
* @AndreasMadsen - **Andreas Madsen** &lt;amwebdk@gmail.com&gt; (he/him)
* @antsmartian - **Anto Aravinth** &lt;anto.aravinth.cse@gmail.com&gt; (he/him)
* @apapirovski - **Anatoli Papirovski** &lt;apapirovski@mac.com&gt; (he/him)
* @AshCripps - **Ash Cripps** &lt;ashley.cripps@ibm.com&gt;
* @bcoe - **Ben Coe** &lt;bencoe@gmail.com&gt; (he/him)
* @bengl - **Bryan English** &lt;bryan@bryanenglish.com&gt; (he/him)
* @benjamingr - **Benjamin Gruenbaum** &lt;benjamingr@gmail.com&gt;
* @BethGriggs - **Beth Griggs** &lt;Bethany.Griggs@uk.ibm.com&gt; (she/her)
* @bmeck - **Bradley Farias** &lt;bradley.meck@gmail.com&gt;
* @bmeurer - **Benedikt Meurer** &lt;benedikt.meurer@gmail.com&gt;
* @bnoordhuis - **Ben Noordhuis** &lt;info@bnoordhuis.nl&gt;
* @boneskull - **Christopher Hiller** &lt;boneskull@boneskull.com&gt; (he/him)
* @BridgeAR - **Ruben Bridgewater** &lt;ruben@bridgewater.de&gt; (he/him)
* @bzoz - **Bartosz Sosnowski** &lt;bartosz@janeasystems.com&gt;
* @cclauss - **Christian Clauss** &lt;cclauss@me.com&gt; (he/him)
* @ChALkeR - **Сковорода Никита Андреевич** &lt;chalkerx@gmail.com&gt; (he/him)
* @cjihrig - **Colin Ihrig** &lt;cjihrig@gmail.com&gt; (he/him)
* @codebytere - **Shelley Vohr** &lt;codebytere@gmail.com&gt; (she/her)
* @danbev - **Daniel Bevenius** &lt;daniel.bevenius@gmail.com&gt; (he/him)
* @danielleadams - **Danielle Adams** &lt;adamzdanielle@gmail.com&gt; (she/her)
* @davisjam - **Jamie Davis** &lt;davisjam@vt.edu&gt; (he/him)
* @devnexen - **David Carlier** &lt;devnexen@gmail.com&gt;
* @devsnek - **Gus Caplan** &lt;me@gus.host&gt; (he/him)
* @edsadr - **Adrian Estrada** &lt;edsadr@gmail.com&gt; (he/him)
* @eugeneo - **Eugene Ostroukhov** &lt;eostroukhov@google.com&gt;
* @evanlucas - **Evan Lucas** &lt;evanlucas@me.com&gt; (he/him)
* @fhinkel - **Franziska Hinkelmann** &lt;franziska.hinkelmann@gmail.com&gt; (she/her)
* @Fishrock123 - **Jeremiah Senkpiel** &lt;fishrock123@rocketmail.com&gt;  (he/they)
* @Flarna - **Gerhard Stöbich** &lt;deb2001-github@yahoo.de&gt;  (he/they)
* @gabrielschulhof - **Gabriel Schulhof** &lt;gabriel.schulhof@intel.com&gt;
* @gdams - **George Adams** &lt;george.adams@uk.ibm.com&gt; (he/him)
* @geek - **Wyatt Preul** &lt;wpreul@gmail.com&gt;
* @gengjiawen - **Jiawen Geng** &lt;technicalcute@gmail.com&gt;
* @GeoffreyBooth - **Geoffrey Booth** &lt;webmaster@geoffreybooth.com&gt; (he/him)
* @gireeshpunathil - **Gireesh Punathil** &lt;gpunathi@in.ibm.com&gt; (he/him)
* @guybedford - **Guy Bedford** &lt;guybedford@gmail.com&gt; (he/him)
* @HarshithaKP - **Harshitha K P** &lt;harshitha014@gmail.com&gt; (she/her)
* @hashseed - **Yang Guo** &lt;yangguo@chromium.org&gt; (he/him)
* @himself65 - **Zeyu Yang** &lt;himself65@outlook.com&gt; (he/him)
* @hiroppy - **Yuta Hiroto** &lt;hello@hiroppy.me&gt; (he/him)
* @indutny - **Fedor Indutny** &lt;fedor.indutny@gmail.com&gt;
* @JacksonTian - **Jackson Tian** &lt;shyvo1987@gmail.com&gt;
* @jasnell - **James M Snell** &lt;jasnell@gmail.com&gt; (he/him)
* @jdalton - **John-David Dalton** &lt;john.david.dalton@gmail.com&gt;
* @jkrems - **Jan Krems** &lt;jan.krems@gmail.com&gt; (he/him)
* @joaocgreis - **João Reis** &lt;reis@janeasystems.com&gt;
* @joyeecheung - **Joyee Cheung** &lt;joyeec9h3@gmail.com&gt; (she/her)
* @juanarbol - **Juan José Arboleda** &lt;soyjuanarbol@gmail.com&gt; (he/him)
* @JungMinu - **Minwoo Jung** &lt;nodecorelab@gmail.com&gt; (he/him)
* @lance - **Lance Ball** &lt;lball@redhat.com&gt; (he/him)
* @legendecas - **Chengzhong Wu** &lt;legendecas@gmail.com&gt; (he/him)
* @Leko - **Shingo Inoue** &lt;leko.noor@gmail.com&gt; (he/him)
* @lpinca - **Luigi Pinca** &lt;luigipinca@gmail.com&gt; (he/him)
* @lundibundi - **Denys Otrishko** &lt;shishugi@gmail.com&gt; (he/him)
* @mafintosh - **Mathias Buus** &lt;mathiasbuus@gmail.com&gt; (he/him)
* @mcollina - **Matteo Collina** &lt;matteo.collina@gmail.com&gt; (he/him)
* @mhdawson - **Michael Dawson** &lt;michael_dawson@ca.ibm.com&gt; (he/him)
* @mildsunrise - **Alba Mendez** &lt;me@alba.sh&gt; (she/her)
* @misterdjules - **Julien Gilli** &lt;jgilli@nodejs.org&gt;
* @mmarchini - **Mary Marchini** &lt;oss@mmarchini.me&gt; (she/her)
* @mscdex - **Brian White** &lt;mscdex@mscdex.net&gt;
* @MylesBorins - **Myles Borins** &lt;myles.borins@gmail.com&gt; (he/him)
* @ofrobots - **Ali Ijaz Sheikh** &lt;ofrobots@google.com&gt; (he/him)
* @oyyd - **Ouyang Yadong** &lt;oyydoibh@gmail.com&gt; (he/him)
* @psmarshall - **Peter Marshall** &lt;petermarshall@chromium.org&gt; (he/him)
* @puzpuzpuz - **Andrey Pechkurov** &lt;apechkurov@gmail.com&gt; (he/him)
* @Qard - **Stephen Belanger** &lt;admin@stephenbelanger.com&gt; (he/him)
* @refack - **Refael Ackermann (רפאל פלחי)** &lt;refack@gmail.com&gt; (he/him/הוא/אתה)
* @rexagod - **Pranshu Srivastava** &lt;rexagod@gmail.com&gt; (he/him)
* @richardlau - **Richard Lau** &lt;riclau@uk.ibm.com&gt;
* @ronag - **Robert Nagy** &lt;ronagy@icloud.com&gt;
* @ronkorving - **Ron Korving** &lt;ron@ronkorving.nl&gt;
* @rubys - **Sam Ruby** &lt;rubys@intertwingly.net&gt;
* @ruyadorno - **Ruy Adorno** &lt;ruyadorno@github.com&gt; (he/him)
* @rvagg - **Rod Vagg** &lt;rod@vagg.org&gt;
* @ryzokuken - **Ujjwal Sharma** &lt;ryzokuken@disroot.org&gt; (he/him)
* @saghul - **Saúl Ibarra Corretgé** &lt;saghul@gmail.com&gt;
* @santigimeno - **Santiago Gimeno** &lt;santiago.gimeno@gmail.com&gt;
* @seishun - **Nikolai Vavilov** &lt;vvnicholas@gmail.com&gt;
* @shigeki - **Shigeki Ohtsu** &lt;ohtsu@ohtsu.org&gt; (he/him)
* @shisama - **Masashi Hirano** &lt;shisama07@gmail.com&gt; (he/him)
* @silverwind - **Roman Reiss** &lt;me@silverwind.io&gt;
* @srl295 - **Steven R Loomis** &lt;srloomis@us.ibm.com&gt;
* @starkwang - **Weijia Wang** &lt;starkwang@126.com&gt;
* @sxa - **Stewart X Addison** &lt;sxa@uk.ibm.com&gt;
* @targos - **Michaël Zasso** &lt;targos@protonmail.com&gt; (he/him)
* @TimothyGu - **Tiancheng "Timothy" Gu** &lt;timothygu99@gmail.com&gt; (he/him)
* @tniessen - **Tobias Nießen** &lt;tniessen@tnie.de&gt;
* @trivikr - **Trivikram Kamat** &lt;trivikr.dev@gmail.com&gt;
* @Trott - **Rich Trott** &lt;rtrott@gmail.com&gt; (he/him)
* @vdeturckheim - **Vladimir de Turckheim** &lt;vlad2t@hotmail.com&gt; (he/him)
* @watilde - **Daijiro Wachi** &lt;daijiro.wachi@gmail.com&gt; (he/him)
* @watson - **Thomas Watson** &lt;w@tson.dk&gt;
* @XadillaX - **Khaidi Chu** &lt;i@2333.moe&gt; (he/him)
* @yhwang - **Yihong Wang** &lt;yh.wang@ibm.com&gt;
* @yorkie - **Yorkie Liu** &lt;yorkiefixer@gmail.com&gt;
* @yosuke-furukawa - **Yosuke Furukawa** &lt;yosuke.furukawa@gmail.com&gt;
* @ZYSzys - **Yongsheng Zhang** &lt;zyszys98@gmail.com&gt; (he/him)

### Collaborator Emeriti

* @andrasq - **Andras** &lt;andras@kinvey.com&gt;
* @AnnaMag - **Anna M. Kedzierska** &lt;anna.m.kedzierska@gmail.com&gt;
* @aqrln - **Alexey Orlenko** &lt;eaglexrlnk@gmail.com&gt; (he/him)
* @brendanashworth - **Brendan Ashworth** &lt;brendan.ashworth@me.com&gt;
* @calvinmetcalf - **Calvin Metcalf** &lt;calvin.metcalf@gmail.com&gt;
* @chrisdickinson - **Chris Dickinson** &lt;christopher.s.dickinson@gmail.com&gt;
* @claudiorodriguez - **Claudio Rodriguez** &lt;cjrodr@yahoo.com&gt;
* @DavidCai1993 - **David Cai** &lt;davidcai1993@yahoo.com&gt; (he/him)
* @digitalinfinity - **Hitesh Kanwathirtha** &lt;digitalinfinity@gmail.com&gt; (he/him)
* @eljefedelrodeodeljefe - **Robert Jefe Lindstaedt** &lt;robert.lindstaedt@gmail.com&gt;
* @estliberitas - **Alexander Makarenko** &lt;estliberitas@gmail.com&gt;
* @firedfox - **Daniel Wang** &lt;wangyang0123@gmail.com&gt;
* @gibfahn - **Gibson Fahnestock** &lt;gibfahn@gmail.com&gt; (he/him)
* @glentiki - **Glen Keane** &lt;glenkeane.94@gmail.com&gt; (he/him)
* @iarna - **Rebecca Turner** &lt;me@re-becca.org&gt;
* @imran-iq - **Imran Iqbal** &lt;imran@imraniqbal.org&gt;
* @imyller - **Ilkka Myller** &lt;ilkka.myller@nodefield.com&gt;
* @isaacs - **Isaac Z. Schlueter** &lt;i@izs.me&gt;
* @italoacasas - **Italo A. Casas** &lt;me@italoacasas.com&gt; (he/him)
* @jasongin - **Jason Ginchereau** &lt;jasongin@microsoft.com&gt;
* @jbergstroem - **Johan Bergström** &lt;bugs@bergstroem.nu&gt;
* @jhamhader - **Yuval Brik** &lt;yuval@brik.org.il&gt;
* @joshgav - **Josh Gavant** &lt;josh.gavant@outlook.com&gt;
* @julianduque - **Julian Duque** &lt;julianduquej@gmail.com&gt; (he/him)
* @kfarnung - **Kyle Farnung** &lt;kfarnung@microsoft.com&gt; (he/him)
* @kunalspathak - **Kunal Pathak** &lt;kunal.pathak@microsoft.com&gt;
* @lucamaraschi - **Luca Maraschi** &lt;luca.maraschi@gmail.com&gt; (he/him)
* @lxe - **Aleksey Smolenchuk** &lt;lxe@lxe.co&gt;
* @maclover7 - **Jon Moss** &lt;me@jonathanmoss.me&gt; (he/him)
* @matthewloring - **Matthew Loring** &lt;mattloring@google.com&gt;
* @micnic - **Nicu Micleușanu** &lt;micnic90@gmail.com&gt; (he/him)
* @mikeal - **Mikeal Rogers** &lt;mikeal.rogers@gmail.com&gt;
* @monsanto - **Christopher Monsanto** &lt;chris@monsan.to&gt;
* @MoonBall - **Chen Gang** &lt;gangc.cxy@foxmail.com&gt;
* @not-an-aardvark - **Teddy Katz** &lt;teddy.katz@gmail.com&gt; (he/him)
* @Olegas - **Oleg Elifantiev** &lt;oleg@elifantiev.ru&gt;
* @orangemocha - **Alexis Campailla** &lt;orangemocha@nodejs.org&gt;
* @othiym23 - **Forrest L Norvell** &lt;ogd@aoaioxxysz.net&gt; (he/him)
* @petkaantonov - **Petka Antonov** &lt;petka_antonov@hotmail.com&gt;
* @phillipj - **Phillip Johnsen** &lt;johphi@gmail.com&gt;
* @piscisaureus - **Bert Belder** &lt;bertbelder@gmail.com&gt;
* @pmq20 - **Minqi Pan** &lt;pmq2001@gmail.com&gt;
* @princejwesley - **Prince John Wesley** &lt;princejohnwesley@gmail.com&gt;
* @rlidwka - **Alex Kocharin** &lt;alex@kocharin.ru&gt;
* @rmg - **Ryan Graham** &lt;r.m.graham@gmail.com&gt;
* @robertkowalski - **Robert Kowalski** &lt;rok@kowalski.gd&gt;
* @romankl - **Roman Klauke** &lt;romaaan.git@gmail.com&gt;
* @RReverser - **Ingvar Stepanyan** &lt;me@rreverser.com&gt;
* @sam-github - **Sam Roberts** &lt;vieuxtech@gmail.com&gt;
* @sebdeckers - **Sebastiaan Deckers** &lt;sebdeckers83@gmail.com&gt;
* @stefanmb - **Stefan Budeanu** &lt;stefan@budeanu.com&gt;
* @tellnes - **Christian Tellnes** &lt;christian@tellnes.no&gt;
* @thefourtheye - **Sakthipriyan Vairamani** &lt;thechargingvolcano@gmail.com&gt; (he/him)
* @thlorenz - **Thorsten Lorenz** &lt;thlorenz@gmx.de&gt;
* @trevnorris - **Trevor Norris** &lt;trev.norris@gmail.com&gt;
* @tunniclm - **Mike Tunnicliffe** &lt;m.j.tunnicliffe@gmail.com&gt;
* @vkurchatkin - **Vladimir Kurchatkin** &lt;vladimir.kurchatkin@gmail.com&gt;
* @vsemozhetbyt - **Vse Mozhet Byt** &lt;vsemozhetbyt@gmail.com&gt; (he/him)
* @whitlockjc - **Jeremy Whitlock** &lt;jwhitlock@apache.org&gt;
<!--lint enable prohibited-strings maximum-line-length-->

Collaborators follow the [Collaborator Guide](./doc/guides/collaborator-guide.md) in
maintaining the Node.js project.

### Release Keys

Primary GPG keys for Node.js Releasers (some Releasers sign with subkeys):

* **Beth Griggs** &lt;bethany.griggs@uk.ibm.com&gt;
`4ED778F539E3634C779C87C6D7062848A1AB005C`
* **Colin Ihrig** &lt;cjihrig@gmail.com&gt;
`94AE36675C464D64BAFA68DD7434390BDBE9B9C5`
* **James M Snell** &lt;jasnell@keybase.io&gt;
`71DCFD284A79C3B38668286BC97EC7A07EDE3FC1`
* **Michaël Zasso** &lt;targos@protonmail.com&gt;
`8FCCA13FEF1D0C2E91008E09770F7A9A5AE15600`
* **Myles Borins** &lt;myles.borins@gmail.com&gt;
`C4F0DFFF4E8C1A8236409D08E73BC641CC11F4C8`
* **Richard Lau** &lt;riclau@uk.ibm.com&gt;
`C82FA3AE1CBEDC6BE46B9360C43CEC45C17AB93C`
* **Rod Vagg** &lt;rod@vagg.org&gt;
`DD8F2338BAE7501E3DD5AC78C273792F7D83545D`
* **Ruben Bridgewater** &lt;ruben@bridgewater.de&gt;
`A48C2BEE680E841632CD4E44F07496B3EB3C1762`
* **Shelley Vohr** &lt;shelley.vohr@gmail.com&gt;
`B9E2F5981AA6E0CD28160D9FF13993A75599653C`

To import the full set of trusted release keys:

```bash
gpg --keyserver pool.sks-keyservers.net --recv-keys 4ED778F539E3634C779C87C6D7062848A1AB005C
gpg --keyserver pool.sks-keyservers.net --recv-keys 94AE36675C464D64BAFA68DD7434390BDBE9B9C5
gpg --keyserver pool.sks-keyservers.net --recv-keys 71DCFD284A79C3B38668286BC97EC7A07EDE3FC1
gpg --keyserver pool.sks-keyservers.net --recv-keys 8FCCA13FEF1D0C2E91008E09770F7A9A5AE15600
gpg --keyserver pool.sks-keyservers.net --recv-keys C4F0DFFF4E8C1A8236409D08E73BC641CC11F4C8
gpg --keyserver pool.sks-keyservers.net --recv-keys C82FA3AE1CBEDC6BE46B9360C43CEC45C17AB93C
gpg --keyserver pool.sks-keyservers.net --recv-keys DD8F2338BAE7501E3DD5AC78C273792F7D83545D
gpg --keyserver pool.sks-keyservers.net --recv-keys A48C2BEE680E841632CD4E44F07496B3EB3C1762
gpg --keyserver pool.sks-keyservers.net --recv-keys B9E2F5981AA6E0CD28160D9FF13993A75599653C
```

See the section above on [Verifying Binaries](#verifying-binaries) for how to
use these keys to verify a downloaded file.

Other keys used to sign some previous releases:

* **Chris Dickinson** &lt;christopher.s.dickinson@gmail.com&gt;
`9554F04D7259F04124DE6B476D5A82AC7E37093B`
* **Evan Lucas** &lt;evanlucas@me.com&gt;
`B9AE9905FFD7803F25714661B63B535A4C206CA9`
* **Gibson Fahnestock** &lt;gibfahn@gmail.com&gt;
`77984A986EBC2AA786BC0F66B01FBB92821C587A`
* **Isaac Z. Schlueter** &lt;i@izs.me&gt;
`93C7E9E91B49E432C2F75674B0A78B0A6C481CF6`
* **Italo A. Casas** &lt;me@italoacasas.com&gt;
`56730D5401028683275BD23C23EFEFE93C4CFFFE`
* **Jeremiah Senkpiel** &lt;fishrock@keybase.io&gt;
`FD3A5288F042B6850C66B31F09FE44734EB7990E`
* **Julien Gilli** &lt;jgilli@fastmail.fm&gt;
`114F43EE0176B71C7BC219DD50A3051F888C628D`
* **Timothy J Fontaine** &lt;tjfontaine@gmail.com&gt;
`7937DFD2AB06298B2293C3187D33FF9D0246406D`

[Code of Conduct]: https://github.com/nodejs/admin/blob/master/CODE_OF_CONDUCT.md
[Contributing to the project]: CONTRIBUTING.md
[Node.js Website]: https://nodejs.org/
[OpenJS Foundation]: https://openjsf.org/
[Working Groups]: https://github.com/nodejs/TSC/blob/master/WORKING_GROUPS.md
[Strategic Initiatives]: https://github.com/nodejs/TSC/blob/master/Strategic-Initiatives.md
