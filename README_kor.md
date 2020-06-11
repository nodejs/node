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

Node.js는 오픈 소스이며, 크로스 플랫폼, 자바스크립트 런타임 환경입니다.
이것은 브라우저 밖에서 자바스크립트 코드로 실행됩니다.
node.js를 사용하면서 더 많은 정보가 필요하다면, [Node.js Website][]을 확인하세요.

Node.js 프로젝트는 [open governance model](./GOVERNANCE.md)을 사용합니다. 
[OpenJS Foundation][]에서 이 프로젝트를 지원제공합니다.

**이 프로젝트는 [Code of Conduct][]와 묶여있습니다.**

# Table of Contents

* [지원](#support)
* [릴리즈 타입](#release-types)
  * [다운로드](#download)
    * [Current 및 LTS 릴리즈](#current-and-lts-releases)
    * [야간 릴리즈](#nightly-releases)
    * [API 문서](#api-documentation)
  * [바이너리 확인](#verifying-binaries)
* [Node.js 빌드하기](#building-nodejs)
* [보안](#security)
* [Node.js에 기여하기](#contributing-to-nodejs)
* [현재 프로젝트 팀 멤버들](#current-project-team-members)
  * [TSC (Technical Steering Committee)](#tsc-technical-steering-committee)
  * [Collaborators](#collaborators)
  * [Release Keys](#release-keys)

## 지원

도움을 원한다면
[instructions for getting support](.github/SUPPORT.md)을 확인하기.

## 릴리즈 타입

* **Current**: 진행되고 있는 개발에서 이뤄집니다. 
현재 배포판의 코드는 주요 버전 넘버의 브랜치에 있습니다. 
( 예시 :  [v10.x](https://github.com/nodejs/node/tree/v10.x))
Node.js는 주요 변화에 따라 6개월마다 새 버전을 배포합니다. 매년 10월과 4월에 발생합니다. 
10월에 나타나는 배포의  지원 수명은 8달입니다. 
4월에 나타나는 배포판은 각 10월에 LTS로 변환됩니다.

* **LTS**: 안정성과 보안에 중점을 둔 오랜 기간동안 지원을 받는 배포판입니다. 모든 주요 버전은 LTS 배포판이 됩니다. LTS 배포판은 12개월의 활성 LTS 지원과 18개월의 유지보수를 받습니다. LTS 배포판 라인은 v4 Argon에서 시작하는 알파벳 순서로 된 코드네임이 있습니다. 거기에는 몇몇 특별한 순환을 제외하고, 주요한 변화나 특징적인 추가사항은 없습니다.

* **Nightly**: 변경사항이 있을 때 24시간마다 현재 브랜치의 코드가 작성되므로 주의해서 사용해야합니다.

Current 및 LTS 릴리즈는 [Semantic Versioning](https://semver.org)을 따릅니다. 
릴리즈 팀의 구성원은 각 Current와 LTS 배포에 [signs](#release-keys)합니다.
자세한 내용은 [Release README](https://github.com/nodejs/Release#readme)을 참조하십시오.

### 다운로드

바이너리, 설치 프로그램 및 소스 tarballs은
<https://nodejs.org/en/download/>에서 제공됩니다.

#### Current 및 LTS Releases
<https://nodejs.org/download/release/>

[latest](https://nodejs.org/download/release/latest/) 디렉토리는 최신 현재 릴리즈를 다르게 부르는 말입니다. 
latest- codename 디렉토리는 LTS 최신 릴리스에 대한 별명입니다. 
예를 들어,
[latest-carbon](https://nodejs.org/download/release/latest-carbon/) 디렉토리에는
최신 Carbon(Node.js 8) 릴리즈가 포함됩니다.

#### 야간 릴리즈
<https://nodejs.org/download/nightly/>

각 디렉토리 이름과 파일 이름에는 날짜(UTC)와 릴리즈 HEAD의 커밑 SHA가 포함됩니다.

#### API 문서

최신 Current 릴리즈에 대한 설명서는 <https://nodejs.org/api/>에 있습니다.
버전 별 설명서는 _docs_ 하위 디렉토리의 각 릴리즈 디렉토리에서 사용할 수 있습니다.
버전별 설명서는 <https://nodejs.org/download/docs/> 에도 있습니다.

### 바이너리 확인

다운로드 디렉토리에는 파일에 대한 SHA 체크합계가 있는`SHASUMS256.txt` 파일이 있습니다. 

`curl`을 사용하여 `SHASUMS256.txt`를 다운로드 하려면 :

```console
$ curl -O https://nodejs.org/dist/vx.y.z/SHASUMS256.txt
```

다운로드 한 파일이 checksum과 일치하는지 확인하려면 `sha256sum`을 통해
다음과 같은 명령으로 실행하십니오. :

```console
$ grep node-vx.y.z.tar.gz SHASUMS256.txt | sha256sum -c -
```

Current 및 LTS의 경우`SHASUMS256.txt`의 GPG 분리 서명은`SHASUMS256.txt.sig `에 있습니다. 
`gpg`와 함께 사용하여`SHASUM256.txt`의 무결성을 확인할 수 있습니다. 
먼저 [the GPG keys of individuals authorized to create releases](#release-keys)를 가져와야합니다.
키를 가져 오려면 :

```console
$ gpg --keyserver pool.sks-keyservers.net --recv-keys DD8F2338BAE7501E3DD5AC78C273792F7D83545D
```

활성 릴리즈키를 가져오는 전체 스크립트는 이 README의 맨 아래를 참조하십시오.

다음으로 `SHASUMS256.txt.sig` 을 다운로드 하십시오.

```console
$ curl -O https://nodejs.org/dist/vx.y.z/SHASUMS256.txt.sig
```

그리고 파일의 서명을 확인하기 위해 `gpg --verify SHASUMS256.txt.sig SHASUMS256.txt` 을 사용해야합니다.

## Node.js 빌드

소스 및 지원되는 플랫폼 목록에서 Node.js를 빌드하는 방법에 대한 지시 사항은 
[BUILDING.md](BUILDING.md) 을 참조하십시오. 

## 보안

Node.js의 보안 취약성 보고에 대한정보는
[SECURITY.md](./SECURITY.md)을 참조하십시오.

## Node.js에 기여

* [프로젝트에 기여][]
* [실무 그룹][]
* [전략적 이니셔티브][]

## 현재 프로젝트 팀원

Node.js 프로젝트의 통제에 대한 정보는
[GOVERNANCE.md](./GOVERNANCE.md) 을 참조하십시오.

### TSC (Technical Steering Committee)

<!--lint disable prohibited-strings-->
* [addaleax](https://github.com/addaleax) -
**Anna Henningsen** &lt;anna@addaleax.net&gt; (she/her)
* [apapirovski](https://github.com/apapirovski) -
**Anatoli Papirovski** &lt;apapirovski@mac.com&gt; (he/him)
* [BethGriggs](https://github.com/BethGriggs) -
**Beth Griggs** &lt;Bethany.Griggs@uk.ibm.com&gt; (she/her)
* [BridgeAR](https://github.com/BridgeAR) -
**Ruben Bridgewater** &lt;ruben@bridgewater.de&gt; (he/him)
* [ChALkeR](https://github.com/ChALkeR) -
**Сковорода Никита Андреевич** &lt;chalkerx@gmail.com&gt; (he/him)
* [cjihrig](https://github.com/cjihrig) -
**Colin Ihrig** &lt;cjihrig@gmail.com&gt; (he/him)
* [codebytere](https://github.com/codebytere) -
**Shelley Vohr** &lt;codebytere@gmail.com&gt; (she/her)
* [danbev](https://github.com/danbev) -
**Daniel Bevenius** &lt;daniel.bevenius@gmail.com&gt; (he/him)
* [fhinkel](https://github.com/fhinkel) -
**Franziska Hinkelmann** &lt;franziska.hinkelmann@gmail.com&gt; (she/her)
* [gabrielschulhof](https://github.com/gabrielschulhof) -
**Gabriel Schulhof** &lt;gabriel.schulhof@intel.com&gt;
* [gireeshpunathil](https://github.com/gireeshpunathil) -
**Gireesh Punathil** &lt;gpunathi@in.ibm.com&gt; (he/him)
* [jasnell](https://github.com/jasnell) -
**James M Snell** &lt;jasnell@gmail.com&gt; (he/him)
* [joyeecheung](https://github.com/joyeecheung) -
**Joyee Cheung** &lt;joyeec9h3@gmail.com&gt; (she/her)
* [mcollina](https://github.com/mcollina) -
**Matteo Collina** &lt;matteo.collina@gmail.com&gt; (he/him)
* [mhdawson](https://github.com/mhdawson) -
**Michael Dawson** &lt;michael_dawson@ca.ibm.com&gt; (he/him)
* [mmarchini](https://github.com/mmarchini) -
**Matheus Marchini** &lt;mat@mmarchini.me&gt;
* [MylesBorins](https://github.com/MylesBorins) -
**Myles Borins** &lt;myles.borins@gmail.com&gt; (he/him)
* [sam-github](https://github.com/sam-github) -
**Sam Roberts** &lt;vieuxtech@gmail.com&gt;
* [targos](https://github.com/targos) -
**Michaël Zasso** &lt;targos@protonmail.com&gt; (he/him)
* [tniessen](https://github.com/tniessen) -
**Tobias Nießen** &lt;tniessen@tnie.de&gt;
* [Trott](https://github.com/Trott) -
**Rich Trott** &lt;rtrott@gmail.com&gt; (he/him)

### TSC Emeriti

* [bnoordhuis](https://github.com/bnoordhuis) -
**Ben Noordhuis** &lt;info@bnoordhuis.nl&gt;
* [chrisdickinson](https://github.com/chrisdickinson) -
**Chris Dickinson** &lt;christopher.s.dickinson@gmail.com&gt;
* [evanlucas](https://github.com/evanlucas) -
**Evan Lucas** &lt;evanlucas@me.com&gt; (he/him)
* [Fishrock123](https://github.com/Fishrock123) -
**Jeremiah Senkpiel** &lt;fishrock123@rocketmail.com&gt; (he/they)
* [gibfahn](https://github.com/gibfahn) -
**Gibson Fahnestock** &lt;gibfahn@gmail.com&gt; (he/him)
* [indutny](https://github.com/indutny) -
**Fedor Indutny** &lt;fedor.indutny@gmail.com&gt;
* [isaacs](https://github.com/isaacs) -
**Isaac Z. Schlueter** &lt;i@izs.me&gt;
* [joshgav](https://github.com/joshgav) -
**Josh Gavant** &lt;josh.gavant@outlook.com&gt;
* [mscdex](https://github.com/mscdex) -
**Brian White** &lt;mscdex@mscdex.net&gt;
* [nebrius](https://github.com/nebrius) -
**Bryan Hughes** &lt;bryan@nebri.us&gt;
* [ofrobots](https://github.com/ofrobots) -
**Ali Ijaz Sheikh** &lt;ofrobots@google.com&gt; (he/him)
* [orangemocha](https://github.com/orangemocha) -
**Alexis Campailla** &lt;orangemocha@nodejs.org&gt;
* [piscisaureus](https://github.com/piscisaureus) -
**Bert Belder** &lt;bertbelder@gmail.com&gt;
* [rvagg](https://github.com/rvagg) -
**Rod Vagg** &lt;r@va.gg&gt;
* [shigeki](https://github.com/shigeki) -
**Shigeki Ohtsu** &lt;ohtsu@ohtsu.org&gt; (he/him)
* [thefourtheye](https://github.com/thefourtheye) -
**Sakthipriyan Vairamani** &lt;thechargingvolcano@gmail.com&gt; (he/him)
* [TimothyGu](https://github.com/TimothyGu) -
**Tiancheng "Timothy" Gu** &lt;timothygu99@gmail.com&gt; (he/him)
* [trevnorris](https://github.com/trevnorris) -
**Trevor Norris** &lt;trev.norris@gmail.com&gt;

### Collaborators

* [addaleax](https://github.com/addaleax) -
**Anna Henningsen** &lt;anna@addaleax.net&gt; (she/her)
* [ak239](https://github.com/ak239) -
**Aleksei Koziatinskii** &lt;ak239spb@gmail.com&gt;
* [AndreasMadsen](https://github.com/AndreasMadsen) -
**Andreas Madsen** &lt;amwebdk@gmail.com&gt; (he/him)
* [antsmartian](https://github.com/antsmartian) -
**Anto Aravinth** &lt;anto.aravinth.cse@gmail.com&gt; (he/him)
* [apapirovski](https://github.com/apapirovski) -
**Anatoli Papirovski** &lt;apapirovski@mac.com&gt; (he/him)
* [bcoe](https://github.com/bcoe) -
**Ben Coe** &lt;bencoe@gmail.com&gt; (he/him)
* [bengl](https://github.com/bengl) -
**Bryan English** &lt;bryan@bryanenglish.com&gt; (he/him)
* [benjamingr](https://github.com/benjamingr) -
**Benjamin Gruenbaum** &lt;benjamingr@gmail.com&gt;
* [BethGriggs](https://github.com/BethGriggs) -
**Beth Griggs** &lt;Bethany.Griggs@uk.ibm.com&gt; (she/her)
* [bmeck](https://github.com/bmeck) -
**Bradley Farias** &lt;bradley.meck@gmail.com&gt;
* [bmeurer](https://github.com/bmeurer) -
**Benedikt Meurer** &lt;benedikt.meurer@gmail.com&gt;
* [bnoordhuis](https://github.com/bnoordhuis) -
**Ben Noordhuis** &lt;info@bnoordhuis.nl&gt;
* [boneskull](https://github.com/boneskull) -
**Christopher Hiller** &lt;boneskull@boneskull.com&gt; (he/him)
* [BridgeAR](https://github.com/BridgeAR) -
**Ruben Bridgewater** &lt;ruben@bridgewater.de&gt; (he/him)
* [bzoz](https://github.com/bzoz) -
**Bartosz Sosnowski** &lt;bartosz@janeasystems.com&gt;
* [cclauss](https://github.com/cclauss) -
**Christian Clauss** &lt;cclauss@me.com&gt; (he/him)
* [ChALkeR](https://github.com/ChALkeR) -
**Сковорода Никита Андреевич** &lt;chalkerx@gmail.com&gt; (he/him)
* [cjihrig](https://github.com/cjihrig) -
**Colin Ihrig** &lt;cjihrig@gmail.com&gt; (he/him)
* [codebytere](https://github.com/codebytere) -
**Shelley Vohr** &lt;codebytere@gmail.com&gt; (she/her)
* [danbev](https://github.com/danbev) -
**Daniel Bevenius** &lt;daniel.bevenius@gmail.com&gt; (he/him)
* [davisjam](https://github.com/davisjam) -
**Jamie Davis** &lt;davisjam@vt.edu&gt; (he/him)
* [devnexen](https://github.com/devnexen) -
**David Carlier** &lt;devnexen@gmail.com&gt;
* [devsnek](https://github.com/devsnek) -
**Gus Caplan** &lt;me@gus.host&gt; (he/him)
* [digitalinfinity](https://github.com/digitalinfinity) -
**Hitesh Kanwathirtha** &lt;digitalinfinity@gmail.com&gt; (he/him)
* [edsadr](https://github.com/edsadr) -
**Adrian Estrada** &lt;edsadr@gmail.com&gt; (he/him)
* [eugeneo](https://github.com/eugeneo) -
**Eugene Ostroukhov** &lt;eostroukhov@google.com&gt;
* [evanlucas](https://github.com/evanlucas) -
**Evan Lucas** &lt;evanlucas@me.com&gt; (he/him)
* [fhinkel](https://github.com/fhinkel) -
**Franziska Hinkelmann** &lt;franziska.hinkelmann@gmail.com&gt; (she/her)
* [Fishrock123](https://github.com/Fishrock123) -
**Jeremiah Senkpiel** &lt;fishrock123@rocketmail.com&gt;  (he/they)
* [Flarna](https://github.com/Flarna) -
**Gerhard Stöbich** &lt;deb2001-github@yahoo.de&gt;  (he/they)
* [gabrielschulhof](https://github.com/gabrielschulhof) -
**Gabriel Schulhof** &lt;gabriel.schulhof@intel.com&gt;
* [gdams](https://github.com/gdams) -
**George Adams** &lt;george.adams@uk.ibm.com&gt; (he/him)
* [geek](https://github.com/geek) -
**Wyatt Preul** &lt;wpreul@gmail.com&gt;
* [gengjiawen](https://github.com/gengjiawen) -
**Jiawen Geng** &lt;technicalcute@gmail.com&gt;
* [GeoffreyBooth](https://github.com/geoffreybooth) -
**Geoffrey Booth** &lt;webmaster@geoffreybooth.com&gt; (he/him)
* [gibfahn](https://github.com/gibfahn) -
**Gibson Fahnestock** &lt;gibfahn@gmail.com&gt; (he/him)
* [gireeshpunathil](https://github.com/gireeshpunathil) -
**Gireesh Punathil** &lt;gpunathi@in.ibm.com&gt; (he/him)
* [guybedford](https://github.com/guybedford) -
**Guy Bedford** &lt;guybedford@gmail.com&gt; (he/him)
* [hashseed](https://github.com/hashseed) -
**Yang Guo** &lt;yangguo@chromium.org&gt; (he/him)
* [himself65](https://github.com/himself65) -
**Zeyu Yang** &lt;himself65@outlook.com&gt; (he/him)
* [hiroppy](https://github.com/hiroppy) -
**Yuta Hiroto** &lt;hello@hiroppy.me&gt; (he/him)
* [indutny](https://github.com/indutny) -
**Fedor Indutny** &lt;fedor.indutny@gmail.com&gt;
* [JacksonTian](https://github.com/JacksonTian) -
**Jackson Tian** &lt;shyvo1987@gmail.com&gt;
* [jasnell](https://github.com/jasnell) -
**James M Snell** &lt;jasnell@gmail.com&gt; (he/him)
* [jdalton](https://github.com/jdalton) -
**John-David Dalton** &lt;john.david.dalton@gmail.com&gt;
* [jkrems](https://github.com/jkrems) -
**Jan Krems** &lt;jan.krems@gmail.com&gt; (he/him)
* [joaocgreis](https://github.com/joaocgreis) -
**João Reis** &lt;reis@janeasystems.com&gt;
* [joyeecheung](https://github.com/joyeecheung) -
**Joyee Cheung** &lt;joyeec9h3@gmail.com&gt; (she/her)
* [juanarbol](https://github.com/juanarbol) -
**Juan José Arboleda** &lt;soyjuanarbol@gmail.com&gt; (he/him)
* [JungMinu](https://github.com/JungMinu) -
**Minwoo Jung** &lt;nodecorelab@gmail.com&gt; (he/him)
* [kfarnung](https://github.com/kfarnung) -
**Kyle Farnung** &lt;kfarnung@microsoft.com&gt; (he/him)
* [lance](https://github.com/lance) -
**Lance Ball** &lt;lball@redhat.com&gt; (he/him)
* [legendecas](https://github.com/legendecas) -
**Chengzhong Wu** &lt;legendecas@gmail.com&gt; (he/him)
* [Leko](https://github.com/Leko) -
**Shingo Inoue** &lt;leko.noor@gmail.com&gt; (he/him)
* [lpinca](https://github.com/lpinca) -
**Luigi Pinca** &lt;luigipinca@gmail.com&gt; (he/him)
* [lundibundi](https://github.com/lundibundi) -
**Denys Otrishko** &lt;shishugi@gmail.com&gt; (he/him)
* [mafintosh](https://github.com/mafintosh) -
**Mathias Buus** &lt;mathiasbuus@gmail.com&gt; (he/him)
* [mcollina](https://github.com/mcollina) -
**Matteo Collina** &lt;matteo.collina@gmail.com&gt; (he/him)
* [mhdawson](https://github.com/mhdawson) -
**Michael Dawson** &lt;michael_dawson@ca.ibm.com&gt; (he/him)
* [mildsunrise](https://github.com/mildsunrise) -
**Alba Mendez** &lt;me@alba.sh&gt; (she/her)
* [misterdjules](https://github.com/misterdjules) -
**Julien Gilli** &lt;jgilli@nodejs.org&gt;
* [mmarchini](https://github.com/mmarchini) -
**Matheus Marchini** &lt;mat@mmarchini.me&gt;
* [mscdex](https://github.com/mscdex) -
**Brian White** &lt;mscdex@mscdex.net&gt;
* [MylesBorins](https://github.com/MylesBorins) -
**Myles Borins** &lt;myles.borins@gmail.com&gt; (he/him)
* [ofrobots](https://github.com/ofrobots) -
**Ali Ijaz Sheikh** &lt;ofrobots@google.com&gt; (he/him)
* [oyyd](https://github.com/oyyd) -
**Ouyang Yadong** &lt;oyydoibh@gmail.com&gt; (he/him)
* [psmarshall](https://github.com/psmarshall) -
**Peter Marshall** &lt;petermarshall@chromium.org&gt; (he/him)
* [puzpuzpuz](https://github.com/puzpuzpuz) -
**Andrey Pechkurov** &lt;apechkurov@gmail.com&gt; (he/him)
* [Qard](https://github.com/Qard) -
**Stephen Belanger** &lt;admin@stephenbelanger.com&gt; (he/him)
* [refack](https://github.com/refack) -
**Refael Ackermann (רפאל פלחי)** &lt;refack@gmail.com&gt; (he/him/הוא/אתה)
* [richardlau](https://github.com/richardlau) -
**Richard Lau** &lt;riclau@uk.ibm.com&gt;
* [ronag](https://github.com/ronag) -
**Robert Nagy** &lt;ronagy@icloud.com&gt;
* [ronkorving](https://github.com/ronkorving) -
**Ron Korving** &lt;ron@ronkorving.nl&gt;
* [rubys](https://github.com/rubys) -
**Sam Ruby** &lt;rubys@intertwingly.net&gt;
* [rvagg](https://github.com/rvagg) -
**Rod Vagg** &lt;rod@vagg.org&gt;
* [ryzokuken](https://github.com/ryzokuken) -
**Ujjwal Sharma** &lt;ryzokuken@disroot.org&gt; (he/him)
* [saghul](https://github.com/saghul) -
**Saúl Ibarra Corretgé** &lt;saghul@gmail.com&gt;
* [sam-github](https://github.com/sam-github) -
**Sam Roberts** &lt;vieuxtech@gmail.com&gt;
* [santigimeno](https://github.com/santigimeno) -
**Santiago Gimeno** &lt;santiago.gimeno@gmail.com&gt;
* [sebdeckers](https://github.com/sebdeckers) -
**Sebastiaan Deckers** &lt;sebdeckers83@gmail.com&gt;
* [seishun](https://github.com/seishun) -
**Nikolai Vavilov** &lt;vvnicholas@gmail.com&gt;
* [shigeki](https://github.com/shigeki) -
**Shigeki Ohtsu** &lt;ohtsu@ohtsu.org&gt; (he/him)
* [shisama](https://github.com/shisama) -
**Masashi Hirano** &lt;shisama07@gmail.com&gt; (he/him)
* [silverwind](https://github.com/silverwind) -
**Roman Reiss** &lt;me@silverwind.io&gt;
* [srl295](https://github.com/srl295) -
**Steven R Loomis** &lt;srloomis@us.ibm.com&gt;
* [starkwang](https://github.com/starkwang) -
**Weijia Wang** &lt;starkwang@126.com&gt;
* [targos](https://github.com/targos) -
**Michaël Zasso** &lt;targos@protonmail.com&gt; (he/him)
* [thefourtheye](https://github.com/thefourtheye) -
**Sakthipriyan Vairamani** &lt;thechargingvolcano@gmail.com&gt; (he/him)
* [TimothyGu](https://github.com/TimothyGu) -
**Tiancheng "Timothy" Gu** &lt;timothygu99@gmail.com&gt; (he/him)
* [tniessen](https://github.com/tniessen) -
**Tobias Nießen** &lt;tniessen@tnie.de&gt;
* [trivikr](https://github.com/trivikr) -
**Trivikram Kamat** &lt;trivikr.dev@gmail.com&gt;
* [Trott](https://github.com/Trott) -
**Rich Trott** &lt;rtrott@gmail.com&gt; (he/him)
* [vdeturckheim](https://github.com/vdeturckheim) -
**Vladimir de Turckheim** &lt;vlad2t@hotmail.com&gt; (he/him)
* [watilde](https://github.com/watilde) -
**Daijiro Wachi** &lt;daijiro.wachi@gmail.com&gt; (he/him)
* [watson](https://github.com/watson) -
**Thomas Watson** &lt;w@tson.dk&gt;
* [XadillaX](https://github.com/XadillaX) -
**Khaidi Chu** &lt;i@2333.moe&gt; (he/him)
* [yhwang](https://github.com/yhwang) -
**Yihong Wang** &lt;yh.wang@ibm.com&gt;
* [yorkie](https://github.com/yorkie) -
**Yorkie Liu** &lt;yorkiefixer@gmail.com&gt;
* [yosuke-furukawa](https://github.com/yosuke-furukawa) -
**Yosuke Furukawa** &lt;yosuke.furukawa@gmail.com&gt;
* [ZYSzys](https://github.com/ZYSzys) -
**Yongsheng Zhang** &lt;zyszys98@gmail.com&gt; (he/him)

### Collaborator Emeriti

* [andrasq](https://github.com/andrasq) -
**Andras** &lt;andras@kinvey.com&gt;
* [AnnaMag](https://github.com/AnnaMag) -
**Anna M. Kedzierska** &lt;anna.m.kedzierska@gmail.com&gt;
* [aqrln](https://github.com/aqrln) -
**Alexey Orlenko** &lt;eaglexrlnk@gmail.com&gt; (he/him)
* [brendanashworth](https://github.com/brendanashworth) -
**Brendan Ashworth** &lt;brendan.ashworth@me.com&gt;
* [calvinmetcalf](https://github.com/calvinmetcalf) -
**Calvin Metcalf** &lt;calvin.metcalf@gmail.com&gt;
* [chrisdickinson](https://github.com/chrisdickinson) -
**Chris Dickinson** &lt;christopher.s.dickinson@gmail.com&gt;
* [claudiorodriguez](https://github.com/claudiorodriguez) -
**Claudio Rodriguez** &lt;cjrodr@yahoo.com&gt;
* [DavidCai1993](https://github.com/DavidCai1993) -
**David Cai** &lt;davidcai1993@yahoo.com&gt; (he/him)
* [eljefedelrodeodeljefe](https://github.com/eljefedelrodeodeljefe) -
**Robert Jefe Lindstaedt** &lt;robert.lindstaedt@gmail.com&gt;
* [estliberitas](https://github.com/estliberitas) -
**Alexander Makarenko** &lt;estliberitas@gmail.com&gt;
* [firedfox](https://github.com/firedfox) -
**Daniel Wang** &lt;wangyang0123@gmail.com&gt;
* [glentiki](https://github.com/glentiki) -
**Glen Keane** &lt;glenkeane.94@gmail.com&gt; (he/him)
* [iarna](https://github.com/iarna) -
**Rebecca Turner** &lt;me@re-becca.org&gt;
* [imran-iq](https://github.com/imran-iq) -
**Imran Iqbal** &lt;imran@imraniqbal.org&gt;
* [imyller](https://github.com/imyller) -
**Ilkka Myller** &lt;ilkka.myller@nodefield.com&gt;
* [isaacs](https://github.com/isaacs) -
**Isaac Z. Schlueter** &lt;i@izs.me&gt;
* [italoacasas](https://github.com/italoacasas) -
**Italo A. Casas** &lt;me@italoacasas.com&gt; (he/him)
* [jasongin](https://github.com/jasongin) -
**Jason Ginchereau** &lt;jasongin@microsoft.com&gt;
* [jbergstroem](https://github.com/jbergstroem) -
**Johan Bergström** &lt;bugs@bergstroem.nu&gt;
* [jhamhader](https://github.com/jhamhader) -
**Yuval Brik** &lt;yuval@brik.org.il&gt;
* [joshgav](https://github.com/joshgav) -
**Josh Gavant** &lt;josh.gavant@outlook.com&gt;
* [julianduque](https://github.com/julianduque) -
**Julian Duque** &lt;julianduquej@gmail.com&gt; (he/him)
* [kunalspathak](https://github.com/kunalspathak) -
**Kunal Pathak** &lt;kunal.pathak@microsoft.com&gt;
* [lucamaraschi](https://github.com/lucamaraschi) -
**Luca Maraschi** &lt;luca.maraschi@gmail.com&gt; (he/him)
* [lxe](https://github.com/lxe) -
**Aleksey Smolenchuk** &lt;lxe@lxe.co&gt;
* [maclover7](https://github.com/maclover7) -
**Jon Moss** &lt;me@jonathanmoss.me&gt; (he/him)
* [matthewloring](https://github.com/matthewloring) -
**Matthew Loring** &lt;mattloring@google.com&gt;
* [micnic](https://github.com/micnic) -
**Nicu Micleușanu** &lt;micnic90@gmail.com&gt; (he/him)
* [mikeal](https://github.com/mikeal) -
**Mikeal Rogers** &lt;mikeal.rogers@gmail.com&gt;
* [monsanto](https://github.com/monsanto) -
**Christopher Monsanto** &lt;chris@monsan.to&gt;
* [MoonBall](https://github.com/MoonBall) -
**Chen Gang** &lt;gangc.cxy@foxmail.com&gt;
* [not-an-aardvark](https://github.com/not-an-aardvark) -
**Teddy Katz** &lt;teddy.katz@gmail.com&gt; (he/him)
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
* [rlidwka](https://github.com/rlidwka) -
**Alex Kocharin** &lt;alex@kocharin.ru&gt;
* [rmg](https://github.com/rmg) -
**Ryan Graham** &lt;r.m.graham@gmail.com&gt;
* [robertkowalski](https://github.com/robertkowalski) -
**Robert Kowalski** &lt;rok@kowalski.gd&gt;
* [romankl](https://github.com/romankl) -
**Roman Klauke** &lt;romaaan.git@gmail.com&gt;
* [RReverser](https://github.com/RReverser) -
**Ingvar Stepanyan** &lt;me@rreverser.com&gt;
* [stefanmb](https://github.com/stefanmb) -
**Stefan Budeanu** &lt;stefan@budeanu.com&gt;
* [tellnes](https://github.com/tellnes) -
**Christian Tellnes** &lt;christian@tellnes.no&gt;
* [thlorenz](https://github.com/thlorenz) -
**Thorsten Lorenz** &lt;thlorenz@gmx.de&gt;
* [trevnorris](https://github.com/trevnorris) -
**Trevor Norris** &lt;trev.norris@gmail.com&gt;
* [tunniclm](https://github.com/tunniclm) -
**Mike Tunnicliffe** &lt;m.j.tunnicliffe@gmail.com&gt;
* [vkurchatkin](https://github.com/vkurchatkin) -
**Vladimir Kurchatkin** &lt;vladimir.kurchatkin@gmail.com&gt;
* [vsemozhetbyt](https://github.com/vsemozhetbyt) -
**Vse Mozhet Byt** &lt;vsemozhetbyt@gmail.com&gt; (he/him)
* [whitlockjc](https://github.com/whitlockjc) -
**Jeremy Whitlock** &lt;jwhitlock@apache.org&gt;
<!--lint enable prohibited-strings-->

콜라보레이터들은 Node.js 프로젝트를 유지하는데에 [Collaborator Guide](./doc/guides/collaborator-guide.md) 을 따릅니다.
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
* **Rod Vagg** &lt;rod@vagg.org&gt;
`DD8F2338BAE7501E3DD5AC78C273792F7D83545D`
* **Ruben Bridgewater** &lt;ruben@bridgewater.de&gt;
`A48C2BEE680E841632CD4E44F07496B3EB3C1762`
* **Shelley Vohr** &lt;shelley.vohr@gmail.com&gt;
`B9E2F5981AA6E0CD28160D9FF13993A75599653C`

신뢰할 수있는 릴리즈 키의 전체 세트를 가져 오려면 :

```bash
gpg --keyserver pool.sks-keyservers.net --recv-keys 4ED778F539E3634C779C87C6D7062848A1AB005C
gpg --keyserver pool.sks-keyservers.net --recv-keys 94AE36675C464D64BAFA68DD7434390BDBE9B9C5
gpg --keyserver pool.sks-keyservers.net --recv-keys 71DCFD284A79C3B38668286BC97EC7A07EDE3FC1
gpg --keyserver pool.sks-keyservers.net --recv-keys 8FCCA13FEF1D0C2E91008E09770F7A9A5AE15600
gpg --keyserver pool.sks-keyservers.net --recv-keys C4F0DFFF4E8C1A8236409D08E73BC641CC11F4C8
gpg --keyserver pool.sks-keyservers.net --recv-keys DD8F2338BAE7501E3DD5AC78C273792F7D83545D
gpg --keyserver pool.sks-keyservers.net --recv-keys A48C2BEE680E841632CD4E44F07496B3EB3C1762
gpg --keyserver pool.sks-keyservers.net --recv-keys B9E2F5981AA6E0CD28160D9FF13993A75599653C
```

이 키를 사용하여 다운로드 한 파일을 확인하기 위한 방법은 
위의 [Verifying Binaries](#verifying-binaries) 섹션을 참조하십시오.

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
