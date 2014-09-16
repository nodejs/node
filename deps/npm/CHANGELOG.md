### v1.4.28 (2014-09-12):

* [`f4540b6`](https://github.com/npm/npm/commit/f4540b6537a87e653d7495a9ddcf72949fdd4d14)
  [#6043](https://github.com/npm/npm/issues/6043) defer rollbacks until just
  before the CLI exits ([@isaacs](https://github.com/isaacs))
* [`1eabfd5`](https://github.com/npm/npm/commit/1eabfd5c03f33c2bd28823714ff02059eeee3899)
  [#6043](https://github.com/npm/npm/issues/6043) `slide@1.1.6`: wait until all
  callbacks have finished before proceeding
  ([@othiym23](https://github.com/othiym23))

### v1.4.27 (2014-09-04):

* [`4cf3c8f`](https://github.com/npm/npm/commit/4cf3c8fd78c9e2693a5f899f50c28f4823c88e2e)
  [#6007](https://github.com/npm/npm/issues/6007) `request@2.42.0`: properly set
  headers on proxy requests ([@isaacs](https://github.com/isaacs))
* [`403cb52`](https://github.com/npm/npm/commit/403cb526be1472bb7545fa8e62d4976382cdbbe5)
  [#6055](https://github.com/npm/npm/issues/6055) `npmconf@1.1.8`: restore
  case-insensitivity of environmental config
  ([@iarna](https://github.com/iarna))

### v1.4.26 (2014-08-28):

* [`eceea95`](https://github.com/npm/npm/commit/eceea95c804fa15b18e91c52c0beb08d42a3e77d)
  `github-url-from-git@1.4.0`: add support for git+https and git+ssh
  ([@stefanbuck](https://github.com/stefanbuck))
* [`e561758`](https://github.com/npm/npm/commit/e5617587e7d7ab686192391ce55357dbc7fed0a3)
  `columnify@1.2.1` ([@othiym23](https://github.com/othiym23))
* [`0c4fab3`](https://github.com/npm/npm/commit/0c4fab372ee76eab01dda83b6749429a8564902e)
  `cmd-shim@2.0.0`: upgrade to graceful-fs 3
  ([@ForbesLindesay](https://github.com/ForbesLindesay))
* [`2d69e4d`](https://github.com/npm/npm/commit/2d69e4d95777671958b5e08d3b2f5844109d73e4)
  `github-url-from-username-repo@1.0.0`: accept slashes in branch names
  ([@robertkowalski](https://github.com/robertkowalski))
* [`81f9b2b`](https://github.com/npm/npm/commit/81f9b2bac9d34c223ea093281ba3c495f23f10d1)
  ensure lifecycle spawn errors caught properly
  ([@isaacs](https://github.com/isaacs))
* [`bfaab8c`](https://github.com/npm/npm/commit/bfaab8c6e0942382a96b250634ded22454c36b5a)
  `npm-registry-client@2.0.7`: properly encode % in passwords
  ([@isaacs](https://github.com/isaacs))
* [`91cfb58`](https://github.com/npm/npm/commit/91cfb58dda851377ec604782263519f01fd96ad8)
  doc: Fix 'npm help index' ([@isaacs](https://github.com/isaacs))

### v1.4.25 (2014-08-21):

* [`64c0ec2`](https://github.com/npm/npm/commit/64c0ec241ef5d83761ca8de54acb3c41b079956e)
  `npm-registry-client@2.0.6`: Print the notification header returned by the
  registry, and make sure status codes are printed without gratuitous quotes
  around them.
  ([@othiym23](https://github.com/othiym23))
* [`a8ed12b`](https://github.com/npm/npm/commit/a8ed12b) `tar@1.0.1`:
  Add test for removing an extract target immediately after unpacking.
  ([@isaacs](https://github.com/isaacs))
* [`70fd11d`](https://github.com/npm/npm/commit/70fd11d)
  `lockfile@1.0.0`: Fix incorrect interaction between `wait`, `stale`,
  and `retries` options.  Part 2 of race condition leading to `ENOENT`
  errors.
  ([@isaacs](https://github.com/isaacs))
* [`0072c4d`](https://github.com/npm/npm/commit/0072c4d)
  `fstream@1.0.2`: Fix a double-finish call which can result in excess
  FS operations after the `close` event.  Part 2 of race condition
  leading to `ENOENT` errors.
  ([@isaacs](https://github.com/isaacs))

### v1.4.24 (2014-08-14):

* [`9344bd9`](https://github.com/npm/npm/commit/9344bd9b2929b5c399a0e0e0b34d45bce7bc24bb)
  doc: add new changelog ([@othiym23](https://github.com/othiym23))
* [`4be76fd`](https://github.com/npm/npm/commit/4be76fd65e895883c337a99f275ccc8c801adda3)
  doc: update version doc to include `pre-*` increment args
  ([@isaacs](https://github.com/isaacs))
* [`e4f2620`](https://github.com/npm/npm/commit/e4f262036080a282ad60e236a9aeebd39fde9fe4)
  build: add `make tag` to tag current release as `latest`
  ([@isaacs](https://github.com/isaacs))
* [`ec2596a`](https://github.com/npm/npm/commit/ec2596a7cb626772780b25b0a94a7e547a812bd5)
  build: publish with `--tag=v1.4-next` ([@isaacs](https://github.com/isaacs))
* [`9ee55f8`](https://github.com/npm/npm/commit/9ee55f892b8b473032a43c59912c5684fd1b39e6)
  build: add script to output `v1.4-next` publish tag
  ([@isaacs](https://github.com/isaacs))
* [`aecb56f`](https://github.com/npm/npm/commit/aecb56f95a84687ea46920a0b98aaa587fee1568)
  build: remove outdated `docpublish` make target
  ([@isaacs](https://github.com/isaacs))
* [`b57a9b7`](https://github.com/npm/npm/commit/b57a9b7ccd13e6b38831ed63595c8ea5763da247)
  build: remove unpublish step from `make publish`
  ([@isaacs](https://github.com/isaacs))
* [`2c6acb9`](https://github.com/npm/npm/commit/2c6acb96c71c16106965d5cd829b67195dd673c7)
  install: rename `.gitignore` when unpacking foreign tarballs
  ([@isaacs](https://github.com/isaacs))
* [`22f3681`](https://github.com/npm/npm/commit/22f3681923e993a47fc1769ba735bfa3dd138082)
  cache: detect non-gzipped tar files more reliably
  ([@isaacs](https://github.com/isaacs))

### v2.0.0-alpha-6 (2014-07-31):

* [`d987707`](https://github.com/npm/npm/commit/d987707) move fetch into
  npm-registry-client ([@othiym23](https://github.com/othiym23))
* [`9b318e2`](https://github.com/npm/npm/commit/9b318e2) `read-installed@3.0.0`
  ([@isaacs](https://github.com/isaacs))
* [`9d73de7`](https://github.com/npm/npm/commit/9d73de7) remove unnecessary
  mkdirps ([@isaacs](https://github.com/isaacs))
* [`ea547e2`](https://github.com/npm/npm/commit/ea547e2) Bump semver to version 3
  ([@isaacs](https://github.com/isaacs))
* [`33ccd13`](https://github.com/npm/npm/commit/33ccd13) Don't squash execute
  perms in `_git-remotes/` dir ([@adammeadows](https://github.com/adammeadows))
* [`48fd233`](https://github.com/npm/npm/commit/48fd233) `npm-package-arg@2.0.1`
  ([@isaacs](https://github.com/isaacs))

### v1.4.23 (2014-07-31):

* [`8dd11d1`](https://github.com/npm/npm/commit/8dd11d1) update several
  dependencies to avoid using `semver`s starting with 0.

### v1.4.22 (2014-07-31):

* [`d9a9e84`](https://github.com/npm/npm/commit/d9a9e84) `read-package-json@1.2.4`
  ([@isaacs](https://github.com/isaacs))
* [`86f0340`](https://github.com/npm/npm/commit/86f0340)
  `github-url-from-git@1.2.0` ([@isaacs](https://github.com/isaacs))
* [`a94136a`](https://github.com/npm/npm/commit/a94136a) `fstream@0.1.29`
  ([@isaacs](https://github.com/isaacs))
* [`bb82d18`](https://github.com/npm/npm/commit/bb82d18) `glob@4.0.5`
  ([@isaacs](https://github.com/isaacs))
* [`5b6bcf4`](https://github.com/npm/npm/commit/5b6bcf4) `cmd-shim@1.1.2`
  ([@isaacs](https://github.com/isaacs))
* [`c2aa8b3`](https://github.com/npm/npm/commit/c2aa8b3) license: Cleaned up
  legalese with actual lawyer ([@isaacs](https://github.com/isaacs))
* [`63fe0ee`](https://github.com/npm/npm/commit/63fe0ee) `init-package-json@1.0.0`
  ([@isaacs](https://github.com/isaacs))

### v2.0.0-alpha-5 (2014-07-22):

This release bumps up to 2.0 because of this breaking change, which could
potentially affect how your package's scripts are run:

* [`df4b0e7`](https://github.com/npm/npm/commit/df4b0e7fc1abd9a54f98db75ec9e4d03d37d125b)
  [#5518](https://github.com/npm/npm/issues/5518) BREAKING CHANGE: support
  passing arguments to `run` scripts ([@bcoe](https://github.com/bcoe))

Other changes:

* [`cd422c9`](https://github.com/npm/npm/commit/cd422c9de510766797c65720d70f085000f50543)
  [#5748](https://github.com/npm/npm/issues/5748) link binaries for scoped
  packages ([@othiym23](https://github.com/othiym23))
* [`4c3c778`](https://github.com/npm/npm/commit/4c3c77839920e830991e0c229c3c6a855c914d67)
  [#5758](https://github.com/npm/npm/issues/5758) `npm link` includes scope
  when linking scoped package ([@fengmk2](https://github.com/fengmk2))
* [`f9f58dd`](https://github.com/npm/npm/commit/f9f58dd0f5b715d4efa6619f13901916d8f99c47)
  [#5707](https://github.com/npm/npm/issues/5707) document generic pre- /
  post-commands ([@sudodoki](https://github.com/sudodoki))
* [`ac7a480`](https://github.com/npm/npm/commit/ac7a4801d80361b41dce4a18f22bcdf75e396000)
  [#5406](https://github.com/npm/npm/issues/5406) `npm cache` displays usage
  when called without arguments
  ([@michaelnisi](https://github.com/michaelnisi))
* [`f4554e9`](https://github.com/npm/npm/commit/f4554e99d34f77a8a02884493748f7d49a9a9d8b)
  Test fixes for Windows ([@isaacs](https://github.com/isaacs))
* update dependencies ([@othiym23](https://github.com/othiym23))


### v1.5.0-alpha-4 (2014-07-18):

* fall back to `_auth` config as default auth when using default registry
  ([@isaacs](https://github.com/isaacs))
* support for 'init.version' for those who don't want to deal with semver 0.0.x
  oddities ([@rvagg](https://github.com/rvagg))
* [`be06213`](https://github.com/npm/npm/commit/be06213415f2d51a50d2c792b4cd0d3412a9a7b1)
  remove residual support for `win` log level
  ([@aterris](https://github.com/aterris))

### v1.5.0-alpha-3 (2014-07-17):

* [`a3a85dd`](https://github.com/npm/npm/commit/a3a85dd004c9245a71ad2f0213bd1a9a90d64cd6)
  `--save` scoped packages correctly ([@othiym23](https://github.com/othiym23))
* [`18a3385`](https://github.com/npm/npm/commit/18a3385bcf8bfb8312239216afbffb7eec759150)
  `npm-registry-client@3.0.2` ([@othiym23](https://github.com/othiym23))
* [`375988b`](https://github.com/npm/npm/commit/375988b9bf5aa5170f06a790d624d31b1eb32c6d)
  invalid package names are an early error for optional deps
  ([@othiym23](https://github.com/othiym23))
* consistently use `node-package-arg` instead of arbitrary package spec
  splitting ([@othiym23](https://github.com/othiym23))

### v1.4.21 (2014-07-14):

* [`88f51aa`](https://github.com/npm/npm/commit/88f51aa27eb9a958d1fa7ec50fee5cfdedd05110)
  fix handling for 301s in `npm-registry-client@2.0.3`
  ([@Raynos](https://github.com/Raynos))

### v1.5.0-alpha-2 (2014-07-01):

* [`54cf625`](https://github.com/npm/npm/commit/54cf62534e3331e3f454e609e44f0b944e819283)
  fix handling for 301s in `npm-registry-client@3.0.1`
  ([@Raynos](https://github.com/Raynos))
* [`e410861`](https://github.com/npm/npm/commit/e410861c69a3799c1874614cb5b87af8124ff98d)
  don't crash if no username set on `whoami`
  ([@isaacs](https://github.com/isaacs))
* [`0353dde`](https://github.com/npm/npm/commit/0353ddeaca8171aa7dbdd8102b7e2eb581a86406)
  respect `--json` for output ([@isaacs](https://github.com/isaacs))
* [`b3d112a`](https://github.com/npm/npm/commit/b3d112ae190b984cc1779b9e6de92218f22380c6)
  outdated: Don't show headings if there's nothing to output
  ([@isaacs](https://github.com/isaacs))
* [`bb4b90c`](https://github.com/npm/npm/commit/bb4b90c80dbf906a1cb26d85bc0625dc2758acc3)
  outdated: Default to `latest` rather than `*` for unspecified deps
  ([@isaacs](https://github.com/isaacs))

### v1.4.20 (2014-07-02):

* [`0353dde`](https://github.com/npm/npm/commit/0353ddeaca8171aa7dbdd8102b7e2eb581a86406)
  respect `--json` for output ([@isaacs](https://github.com/isaacs))
* [`b3d112a`](https://github.com/npm/npm/commit/b3d112ae190b984cc1779b9e6de92218f22380c6)
  outdated: Don't show headings if there's nothing to output
  ([@isaacs](https://github.com/isaacs))
* [`bb4b90c`](https://github.com/npm/npm/commit/bb4b90c80dbf906a1cb26d85bc0625dc2758acc3)
  outdated: Default to `latest` rather than `*` for unspecified deps
  ([@isaacs](https://github.com/isaacs))

### v1.5.0-alpha-1 (2014-07-01):

* [`eef4884`](https://github.com/npm/npm/commit/eef4884d6487ee029813e60a5f9c54e67925d9fa)
  use the correct piece of the spec for GitHub shortcuts
  ([@othiym23](https://github.com/othiym23))

### v1.5.0-alpha-0 (2014-07-01):

* [`7f55057`](https://github.com/npm/npm/commit/7f55057807cfdd9ceaf6331968e666424f48116c)
  install scoped packages ([#5239](https://github.com/npm/npm/issues/5239))
  ([@othiym23](https://github.com/othiym23))
* [`0df7e16`](https://github.com/npm/npm/commit/0df7e16c0232d8f4d036ebf4ec3563215517caac)
  publish scoped packages ([#5239](https://github.com/npm/npm/issues/5239))
  ([@othiym23](https://github.com/othiym23))
* [`0689ba2`](https://github.com/npm/npm/commit/0689ba249b92b4c6279a26804c96af6f92b3a501)
  support (and save) --scope=@s config
  ([@othiym23](https://github.com/othiym23))
* [`f34878f`](https://github.com/npm/npm/commit/f34878fc4cee29901e4daf7bace94be01e25cad7)
  scope credentials to registry ([@othiym23](https://github.com/othiym23))
* [`0ac7ca2`](https://github.com/npm/npm/commit/0ac7ca233f7a69751fe4386af6c4daa3ee9fc0da)
  capture and store bearer tokens when sent by registry
  ([@othiym23](https://github.com/othiym23))
* [`63c3277`](https://github.com/npm/npm/commit/63c3277f089b2c4417e922826bdc313ac854cad6)
  only delete files that are created by npm
  ([@othiym23](https://github.com/othiym23))
* [`4f54043`](https://github.com/npm/npm/commit/4f540437091d1cbca3915cd20c2da83c2a88bb8e)
  `npm-package-arg@2.0.0` ([@othiym23](https://github.com/othiym23))
* [`9e1460e`](https://github.com/npm/npm/commit/9e1460e6ac9433019758481ec031358f4af4cd44)
  `read-package-json@1.2.3` ([@othiym23](https://github.com/othiym23))
* [`719d8ad`](https://github.com/npm/npm/commit/719d8adb9082401f905ff4207ede494661f8a554)
  `fs-vacuum@1.2.1` ([@othiym23](https://github.com/othiym23))
* [`9ef8fe4`](https://github.com/npm/npm/commit/9ef8fe4d6ead3acb3e88c712000e2d3a9480ebec)
  `async-some@1.0.0` ([@othiym23](https://github.com/othiym23))
* [`a964f65`](https://github.com/npm/npm/commit/a964f65ab662107b62a4ca58535ce817e8cca331)
  `npmconf@2.0.1` ([@othiym23](https://github.com/othiym23))
* [`113765b`](https://github.com/npm/npm/commit/113765bfb7d3801917c1d9f124b8b3d942bec89a)
  `npm-registry-client@3.0.0` ([@othiym23](https://github.com/othiym23))

### v1.4.19 (2014-07-01):

* [`f687433`](https://github.com/npm/npm/commit/f687433) relative URLS for
  working non-root registry URLS ([@othiym23](https://github.com/othiym23))
* [`bea190c`](https://github.com/npm/npm/commit/bea190c)
  [#5591](https://github.com/npm/npm/issues/5591) bump nopt and npmconf
  ([@isaacs](https://github.com/isaacs))

### v1.4.18 (2014-06-29):

* Bump glob dependency from 4.0.2 to 4.0.3. It now uses graceful-fs when
  available, increasing resilience to [various filesystem
  errors](https://github.com/isaacs/node-graceful-fs#improvements-over-fs-module).
  ([@isaacs](https://github.com/isaacs))

### v1.4.17 (2014-06-27):

* replace escape codes with ansicolors
  ([@othiym23](https://github.com/othiym23))
* Allow to build all the docs OOTB. ([@GeJ](https://github.com/GeJ))
* Use core.longpaths on win32 git - fixes
  [#5525](https://github.com/npm/npm/issues/5525) (Bradley Meck)
* `npmconf@1.1.2` ([@isaacs](https://github.com/isaacs))
* Consolidate color sniffing in config/log loading process
  ([@isaacs](https://github.com/isaacs))
* add verbose log when project config file is ignored
  ([@isaacs](https://github.com/isaacs))
* npmconf: Float patch to remove 'scope' from config defs
  ([@isaacs](https://github.com/isaacs))
* doc: npm-explore can't handle a version
  ([@robertkowalski](https://github.com/robertkowalski))
* Add user-friendly errors for ENOSPC and EROFS.
  ([@voodootikigod](https://github.com/voodootikigod))
* bump tar and fstream deps ([@isaacs](https://github.com/isaacs))
* Run the npm-registry-couchapp tests along with npm tests
  ([@isaacs](https://github.com/isaacs))

### v1.2.8000 (2014-06-17):

* Same as v1.4.16, but with the spinner disabled, and a version number that
  starts with v1.2.

### v1.4.16 (2014-06-17):

* `npm-registry-client@2.0.2` ([@isaacs](https://github.com/isaacs))
* `fstream@0.1.27` ([@isaacs](https://github.com/isaacs))
* `sha@1.2.4` ([@isaacs](https://github.com/isaacs))
* `rimraf@2.2.8` ([@isaacs](https://github.com/isaacs))
* `npmlog@1.0.1` ([@isaacs](https://github.com/isaacs))
* `npm-registry-client@2.0.1` ([@isaacs](https://github.com/isaacs))
* removed redundant dependency ([@othiym23](https://github.com/othiym23))
* `npmconf@1.0.5` ([@isaacs](https://github.com/isaacs))
* Properly handle errors that can occur in the config-loading process
  ([@isaacs](https://github.com/isaacs))

### v1.4.15 (2014-06-10):

* cache: atomic de-race-ified package.json writing
  ([@isaacs](https://github.com/isaacs))
* `fstream@0.1.26` ([@isaacs](https://github.com/isaacs))
* `graceful-fs@3.0.2` ([@isaacs](https://github.com/isaacs))
* `osenv@0.1.0` ([@isaacs](https://github.com/isaacs))
* Only spin the spinner when we're fetching stuff
  ([@isaacs](https://github.com/isaacs))
* Update `osenv@0.1.0` which removes ~/tmp as possible tmp-folder
  ([@robertkowalski](https://github.com/robertkowalski))
* `ini@1.2.1` ([@isaacs](https://github.com/isaacs))
* `graceful-fs@3` ([@isaacs](https://github.com/isaacs))
* Update glob and things depending on glob
  ([@isaacs](https://github.com/isaacs))
* github-url-from-username-repo and read-package-json updates
  ([@isaacs](https://github.com/isaacs))
* `editor@0.1.0` ([@isaacs](https://github.com/isaacs))
* `columnify@1.1.0` ([@isaacs](https://github.com/isaacs))
* bump ansi and associated deps ([@isaacs](https://github.com/isaacs))

### v1.4.14 (2014-06-05):

* char-spinner: update to not bork windows
  ([@isaacs](https://github.com/isaacs))

### v1.4.13 (2014-05-23):

* Fix `npm install` on a tarball.
  ([`ed3abf1`](https://github.com/npm/npm/commit/ed3abf1aa10000f0f687330e976d78d1955557f6),
  [#5330](https://github.com/npm/npm/issues/5330),
  [@othiym23](https://github.com/othiym23))
* Fix an issue with the spinner on Node 0.8.
  ([`9f00306`](https://github.com/npm/npm/commit/9f003067909440390198c0b8f92560d84da37762),
  [@isaacs](https://github.com/isaacs))
* Re-add `npm.commands.cache.clean` and `npm.commands.cache.read` APIs, and
  document `npm.commands.cache.*` as npm-cache(3).
  ([`e06799e`](https://github.com/npm/npm/commit/e06799e77e60c1fc51869619083a25e074d368b3),
  [@isaacs](https://github.com/isaacs))

### v1.4.12 (2014-05-23):

* remove normalize-package-data from top level, de-^-ify inflight dep
  ([@isaacs](https://github.com/isaacs))
* Always sort saved bundleDependencies ([@isaacs](https://github.com/isaacs))
* add inflight to bundledDependencies
  ([@othiym23](https://github.com/othiym23))

### v1.4.11 (2014-05-22):

* fix `npm ls` labeling issue
* `node-gyp@0.13.1`
* default repository to https:// instead of git://
* addLocalTarball: Remove extraneous unpack
  ([@isaacs](https://github.com/isaacs))
* Massive cache folder refactor ([@othiym23](https://github.com/othiym23) and
  [@isaacs](https://github.com/isaacs))
* Busy Spinner, no http noise ([@isaacs](https://github.com/isaacs))
* Per-project .npmrc file support ([@isaacs](https://github.com/isaacs))
* `npmconf@1.0.0`, Refactor config/uid/prefix loading process
  ([@isaacs](https://github.com/isaacs))
* Allow once-disallowed characters in passwords
  ([@isaacs](https://github.com/isaacs))
* Send npm version as 'version' header ([@isaacs](https://github.com/isaacs))
* fix cygwin encoding issue (Karsten Tinnefeld)
* Allow non-github repositories with `npm repo`
  ([@evanlucas](https://github.com/evanlucas))
* Allow peer deps to be satisfied by grandparent
* Stop optional deps moving into deps on `update --save`
  ([@timoxley](https://github.com/timoxley))
* Ensure only matching deps update with `update --save*`
  ([@timoxley](https://github.com/timoxley))
* Add support for `prerelease`, `preminor`, `prepatch` to `npm version`

### v1.4.10 (2014-05-05):

* Don't set referer if already set
* fetch: Send referer and npm-session headers
* `run-script`: Support `--parseable` and `--json`
* list runnable scripts ([@evanlucas](https://github.com/evanlucas))
* Use marked instead of ronn for html docs

### v1.4.9 (2014-05-01):

* Send referer header (with any potentially private stuff redacted)
* Fix critical typo bug in previous npm release

### v1.4.8 (2014-05-01):

* Check SHA before using files from cache
* adduser: allow change of the saved password
* Make `npm install` respect `config.unicode`
* Fix lifecycle to pass `Infinity` for config env value
* Don't return 0 exit code on invalid command
* cache: Handle 404s and other HTTP errors as errors
* Resolve ~ in path configs to env.HOME
* Include npm version in default user-agent conf
* npm init: Use ISC as default license, use save-prefix for deps
* Many test and doc fixes

### v1.4.7 (2014-04-15):

* Add `--save-prefix` option that can be used to override the default of `^`
  when using `npm install --save` and its counterparts.
  ([`64eefdf`](https://github.com/npm/npm/commit/64eefdfe26bb27db8dc90e3ab5d27a5ef18a4470),
  [@thlorenz](https://github.com/thlorenz))
* Allow `--silent` to silence the echoing of commands that occurs with `npm
  run`.
  ([`c95cf08`](https://github.com/npm/npm/commit/c95cf086e5b97dbb48ff95a72517b203a8f29eab),
  [@Raynos](https://github.com/Raynos))
* Some speed improvements to the cache, which should improve install times.
  ([`cb94310`](https://github.com/npm/npm/commit/cb94310a6adb18cb7b881eacb8d67171eda8b744),
  [`3b0870f`](https://github.com/npm/npm/commit/3b0870fb2f40358b3051abdab6be4319d196b99d),
  [`120f5a9`](https://github.com/npm/npm/commit/120f5a93437bbbea9249801574a2f33e44e81c33),
  [@isaacs](https://github.com/isaacs))
* Improve ability to retry registry requests when a subset of the registry
  servers are down.
  ([`4a5257d`](https://github.com/npm/npm/commit/4a5257de3870ac3dafa39667379f19f6dcd6093e),
  https://github.com/npm/npm-registry-client/commit/7686d02cb0b844626d6a401e58c0755ef3bc8432,
  [@isaacs](https://github.com/isaacs))
* Fix marking of peer dependencies as extraneous.
  ([`779b164`](https://github.com/npm/npm/commit/779b1649764607b062c031c7e5c972151b4a1754),
  https://github.com/npm/read-installed/commit/6680ba6ef235b1ca3273a00b70869798ad662ddc,
  [@isaacs](https://github.com/isaacs))
* Fix npm crashing when doing `npm shrinkwrap` in the presence of a
  `package.json` with no dependencies.
  ([`a9d9fa5`](https://github.com/npm/npm/commit/a9d9fa5ad3b8c925a589422b7be28d2735f320b0),
  [@kislyuk](https://github.com/kislyuk))
* Fix error when using `npm view` on packages that have no versions or have
  been unpublished.
  ([`94df2f5`](https://github.com/npm/npm/commit/94df2f56d684b35d1df043660180fc321b743dc8),
  [@juliangruber](https://github.com/juliangruber);
  [`2241a09`](https://github.com/npm/npm/commit/2241a09c843669c70633c399ce698cec3add40b3),
  [@isaacs](https://github.com/isaacs))

### v1.4.6 (2014-03-19):

* Fix extraneous package detection to work in more cases.
  ([`f671286`](https://github.com/npm/npm/commit/f671286), npm/read-installed#20,
  [@LaurentVB](https://github.com/LaurentVB))

### v1.4.5 (2014-03-18):

* Sort dependencies in `package.json` when doing `npm install --save` and all
  its variants.
  ([`6fd6ff7`](https://github.com/npm/npm/commit/6fd6ff7e536ea6acd33037b1878d4eca1f931985),
  [@domenic](https://github.com/domenic))
* Add `--save-exact` option, usable alongside `--save` and its variants, which
  will write the exact version number into `package.json` instead of the
  appropriate semver-compatibility range.
  ([`17f07df`](https://github.com/npm/npm/commit/17f07df8ad8e594304c2445bf7489cb53346f2c5),
  [@timoxley](https://github.com/timoxley))
* Accept gzipped content from the registry to speed up downloads and save
  bandwidth.
  ([`a3762de`](https://github.com/npm/npm/commit/a3762de843b842be8fa0ab57cdcd6b164f145942),
  npm/npm-registry-client#40, [@fengmk2](https://github.com/fengmk2))
* Fix `npm ls`'s `--depth` and `--log` options.
  ([`1d29b17`](https://github.com/npm/npm/commit/1d29b17f5193d52a5c4faa412a95313dcf41ed91),
  npm/read-installed#13, [@zertosh](https://github.com/zertosh))
* Fix "Adding a cache directory to the cache will make the world implode" in
  certain cases.
  ([`9a4b2c4`](https://github.com/npm/npm/commit/9a4b2c4667c2b1e0054e3d5611ab86acb1760834),
  domenic/path-is-inside#1, [@pmarques](https://github.com/pmarques))
* Fix readmes not being uploaded in certain rare cases.
  ([`527b72c`](https://github.com/npm/npm/commit/527b72cca6c55762b51e592c48a9f28cc7e2ff8b),
  [@isaacs](https://github.com/isaacs))

### v1.4.4 (2014-02-20):

* Add `npm t` as an alias for `npm test` (which is itself an alias for `npm run
  test`, or even `npm run-script test`). We like making running your tests
  easy. ([`14e650b`](https://github.com/npm/npm/commit/14e650bce0bfebba10094c961ac104a61417a5de), [@isaacs](https://github.com/isaacs))

### v1.4.3 (2014-02-16):

* Add back `npm prune --production`, which was removed in 1.3.24.
  ([`acc4d02`](https://github.com/npm/npm/commit/acc4d023c57d07704b20a0955e4bf10ee91bdc83),
  [@davglass](https://github.com/davglass))
* Default `npm install --save` and its counterparts to use the `^` version
  specifier, instead of `~`.
  ([`0a3151c`](https://github.com/npm/npm/commit/0a3151c9cbeb50c1c65895685c2eabdc7e2608dc),
  [@mikolalysenko](https://github.com/mikolalysenko))
* Make `npm shrinkwrap` output dependencies in a sorted order, so that diffs
  between shrinkwrap files should be saner now.
  ([`059b2bf`](https://github.com/npm/npm/commit/059b2bfd06ae775205a37257dca80142596a0113),
  [@Raynos](https://github.com/Raynos))
* Fix `npm dedupe` not correctly respecting dependency constraints.
  ([`86028e9`](https://github.com/npm/npm/commit/86028e9fd8524d5e520ce01ba2ebab5a030103fc),
  [@rafeca](https://github.com/rafeca))
* Fix `npm ls` giving spurious warnings when you used `"latest"` as a version
  specifier.
  (https://github.com/npm/read-installed/commit/d2956400e0386931c926e0f30c334840e0938f14,
  [@bajtos](https://github.com/bajtos))
* Fixed a bug where using `npm link` on packages without a `name` value could
  cause npm to delete itself.
  ([`401a642`](https://github.com/npm/npm/commit/401a64286aa6665a94d1d2f13604f7014c5fce87),
  [@isaacs](https://github.com/isaacs))
* Fixed `npm install ./pkg@1.2.3` to actually install the directory at
  `pkg@1.2.3`; before it would try to find version `1.2.3` of the package
  `./pkg` in the npm registry.
  ([`46d8768`](https://github.com/npm/npm/commit/46d876821d1dd94c050d5ebc86444bed12c56739),
  [@rlidwka](https://github.com/rlidwka); see also
  [`f851b79`](https://github.com/npm/npm/commit/f851b79a71d9a5f5125aa85877c94faaf91bea5f))
* Fix `npm outdated` to respect the `color` configuration option.
  ([`d4f6f3f`](https://github.com/npm/npm/commit/d4f6f3ff83bd14fb60d3ac6392cb8eb6b1c55ce1),
  [@timoxley](https://github.com/timoxley))
* Fix `npm outdated --parseable`.
  ([`9575a23`](https://github.com/npm/npm/commit/9575a23f955ce3e75b509c89504ef0bd707c8cf6),
  [@yhpark](https://github.com/yhpark))
* Fix a lockfile-related errors when using certain Git URLs.
  ([`164b97e`](https://github.com/npm/npm/commit/164b97e6089f64e686db7a9a24016f245effc37f),
  [@nigelzor](https://github.com/nigelzor))

### v1.4.2 (2014-02-13):

* Fixed an issue related to mid-publish GET requests made against the registry.
  (https://github.com/npm/npm-registry-client/commit/acbec48372bc1816c67c9e7cbf814cf50437ff93,
  [@isaacs](https://github.com/isaacs))

### v1.4.1 (2014-02-13):

* Fix `npm shrinkwrap` forgetting to shrinkwrap dependencies that were also
  development dependencies.
  ([`9c575c5`](https://github.com/npm/npm/commit/9c575c56efa9b0c8b0d4a17cb9c1de3833004bcd),
  [@diwu1989](https://github.com/diwu1989))
* Fixed publishing of pre-existing packages with uppercase characters in their
  name.
  (https://github.com/npm/npm-registry-client/commit/9345d3b6c3d8510dd5c4418f27ee1fce59acebad,
  [@isaacs](https://github.com/isaacs))

### v1.4.0 (2014-02-12):

* Remove `npm publish --force`. See
  https://github.com/npm/npmjs.org/issues/148.
  ([@isaacs](https://github.com/isaacs),
  npm/npm-registry-client@2c8dba990de6a59af6545b75cc00a6dc12777c2a)
* Other changes to the registry client related to saved configs and couch
  logins. ([@isaacs](https://github.com/isaacs);
  npm/npm-registry-client@25e2b019a1588155e5f87d035c27e79963b75951,
  npm/npm-registry-client@9e41e9101b68036e0f078398785f618575f3cdde,
  npm/npm-registry-client@2c8dba990de6a59af6545b75cc00a6dc12777c2a)
* Show an error to the user when doing `npm update` and the `package.json`
  specifies a version that does not exist.
  ([@evanlucas](https://github.com/evanlucas),
  [`027a33a`](https://github.com/npm/npm/commit/027a33a5c594124cc1d82ddec5aee2c18bc8dc32))
* Fix some issues with cache ownership in certain installation configurations.
  ([@outcoldman](https://github.com/outcoldman),
  [`a132690`](https://github.com/npm/npm/commit/a132690a2876cda5dcd1e4ca751f21dfcb11cb9e))
* Fix issues where GitHub shorthand dependencies `user/repo` were not always
  treated the same as full Git URLs.
  ([@robertkowalski](https://github.com/robertkowalski),
  https://github.com/meryn/normalize-package-data/commit/005d0b637aec1895117fcb4e3b49185eebf9e240)

### v1.3.26 (2014-02-02):

* Fixes and updates to publishing code
  ([`735427a`](https://github.com/npm/npm/commit/735427a69ba4fe92aafa2d88f202aaa42920a9e2)
  and
  [`c0ac832`](https://github.com/npm/npm/commit/c0ac83224d49aa62e55577f8f27d53bbfd640dc5),
  [@isaacs](https://github.com/isaacs))
* Fix `npm bugs` with no arguments.
  ([`b99d465`](https://github.com/npm/npm/commit/b99d465221ac03bca30976cbf4d62ca80ab34091),
  [@Hoops](https://github.com/Hoops))

### v1.3.25 (2014-01-25):

* Remove gubblebum blocky font from documentation headers.
  ([`6940c9a`](https://github.com/npm/npm/commit/6940c9a100160056dc6be8f54a7ad7fa8ceda7e2),
  [@isaacs](https://github.com/isaacs))

### v1.3.24 (2014-01-19):

* Make the search output prettier, with nice truncated columns, and a `--long`
  option to create wrapping columns.
  ([`20439b2`](https://github.com/npm/npm/commit/20439b2) and
  [`3a6942d`](https://github.com/npm/npm/commit/3a6942d),
  [@timoxley](https://github.com/timoxley))
* Support multiple packagenames in `npm docs`.
  ([`823010b`](https://github.com/npm/npm/commit/823010b),
  [@timoxley](https://github.com/timoxley))
* Fix the `npm adduser` bug regarding "Error: default value must be string or
  number" again. ([`b9b4248`](https://github.com/npm/npm/commit/b9b4248),
  [@isaacs](https://github.com/isaacs))
* Fix `scripts` entries containing whitespaces on Windows.
  ([`80282ed`](https://github.com/npm/npm/commit/80282ed),
  [@robertkowalski](https://github.com/robertkowalski))
* Fix `npm update` for Git URLs that have credentials in them
  ([`93fc364`](https://github.com/npm/npm/commit/93fc364),
  [@danielsantiago](https://github.com/danielsantiago))
* Fix `npm install` overwriting `npm link`-ed dependencies when they are tagged
  Git dependencies. ([`af9bbd9`](https://github.com/npm/npm/commit/af9bbd9),
  [@evanlucas](https://github.com/evanlucas))
* Remove `npm prune --production` since it buggily removed some dependencies
  that were necessary for production; see
  [#4509](https://github.com/npm/npm/issues/4509). Hopefully it can make its
  triumphant return, one day.
  ([`1101b6a`](https://github.com/npm/npm/commit/1101b6a),
  [@isaacs](https://github.com/isaacs))

Dependency updates:
* [`909cccf`](https://github.com/npm/npm/commit/909cccf) `read-package-json@1.1.6`
* [`a3891b6`](https://github.com/npm/npm/commit/a3891b6) `rimraf@2.2.6`
* [`ac6efbc`](https://github.com/npm/npm/commit/ac6efbc) `sha@1.2.3`
* [`dd30038`](https://github.com/npm/npm/commit/dd30038) `node-gyp@0.12.2`
* [`c8c3ebe`](https://github.com/npm/npm/commit/c8c3ebe) `npm-registry-client@0.3.3`
* [`4315286`](https://github.com/npm/npm/commit/4315286) `npmconf@0.1.12`

### v1.3.23 (2014-01-03):

* Properly handle installations that contained a certain class of circular
  dependencies.
  ([`5dc93e8`](https://github.com/npm/npm/commit/5dc93e8c82604c45b6067b1acf1c768e0bfce754),
  [@substack](https://github.com/substack))

### v1.3.22 (2013-12-25):

* Fix a critical bug in `npm adduser` that would manifest in the error message
  "Error: default value must be string or number."
  ([`fba4bd2`](https://github.com/npm/npm/commit/fba4bd24bc2ab00ccfeda2043aa53af7d75ef7ce),
  [@isaacs](https://github.com/isaacs))
* Allow `npm bugs` in the current directory to open the current package's bugs
  URL.
  ([`d04cf64`](https://github.com/npm/npm/commit/d04cf6483932c693452f3f778c2fa90f6153a4af),
  [@evanlucas](https://github.com/evanlucas))
* Several fixes to various error messages to include more useful or updated
  information.
  ([`1e6f2a7`](https://github.com/npm/npm/commit/1e6f2a72ca058335f9f5e7ca22d01e1a8bb0f9f7),
  [`ff46366`](https://github.com/npm/npm/commit/ff46366bd40ff0ef33c7bac8400bc912c56201d1),
  [`8b4bb48`](https://github.com/npm/npm/commit/8b4bb4815d80a3612186dc5549d698e7b988eb03);
  [@rlidwka](https://github.com/rlidwka),
  [@evanlucas](https://github.com/evanlucas))

### v1.3.21 (2013-12-17):

* Fix a critical bug that prevented publishing due to incorrect hash
  calculation.
  ([`4ca4a2c`](https://github.com/npm/npm-registry-client/commit/4ca4a2c6333144299428be6b572e2691aa59852e),
  [@dominictarr](https://github.com/dominictarr))

### v1.3.20 (2013-12-17):

* Fixes a critical bug in v1.3.19.  Thankfully, due to that bug, no one could
  install npm v1.3.19 :)

### v1.3.19 (2013-12-16):

* Adds atomic PUTs for publishing packages, which should result in far fewer
  requests and less room for replication errors on the server-side.

### v1.3.18 (2013-12-16):

* Added an `--ignore-scripts` option, which will prevent `package.json` scripts
  from being run. Most notably, this will work on `npm install`, so e.g. `npm
  install --ignore-scripts` will not run preinstall and prepublish scripts.
  ([`d7e67bf`](https://github.com/npm/npm/commit/d7e67bf0d94b085652ec1c87d595afa6f650a8f6),
  [@sqs](https://github.com/sqs))
* Fixed a bug introduced in 1.3.16 that would manifest with certain cache
  configurations, by causing spurious errors saying "Adding a cache directory
  to the cache will make the world implode."
  ([`966373f`](https://github.com/npm/npm/commit/966373fad8d741637f9744882bde9f6e94000865),
  [@domenic](https://github.com/domenic))
* Re-fixed the multiple download of URL dependencies, whose fix was reverted in
  1.3.17.
  ([`a362c3f`](https://github.com/npm/npm/commit/a362c3f1919987419ed8a37c8defa19d2e6697b0),
  [@spmason](https://github.com/spmason))

### v1.3.17 (2013-12-11):

* This release reverts
  [`644c2ff`](https://github.com/npm/npm/commit/644c2ff3e3d9c93764f7045762477f48864d64a7),
  which avoided re-downloading URL and shinkwrap dependencies when doing `npm
  install`. You can see the in-depth reasoning in
  [`d8c907e`](https://github.com/npm/npm/commit/d8c907edc2019b75cff0f53467e34e0ffd7e5fba);
  the problem was, that the patch changed the behavior of `npm install -f` to
  reinstall all dependencies.
* A new version of the no-re-downloading fix has been submitted as
  [#4303](https://github.com/npm/npm/issues/4303) and will hopefully be
  included in the next release.

### v1.3.16 (2013-12-11):

* Git URL dependencies are now updated on `npm install`, fixing a two-year old
  bug
  ([`5829ecf`](https://github.com/npm/npm/commit/5829ecf032b392d2133bd351f53d3c644961396b),
  [@robertkowalski](https://github.com/robertkowalski)). Additional progress on
  reducing the resulting Git-related I/O is tracked as
  [#4191](https://github.com/npm/npm/issues/4191), but for now, this will be a
  big improvement.
* Added a `--json` mode to `npm outdated` to give a parseable output.
  ([`0b6c9b7`](https://github.com/npm/npm/commit/0b6c9b7c8c5579f4d7d37a0c24d9b7a12ccbe5fe),
  [@yyx990803](https://github.com/yyx990803))
* Made `npm outdated` much prettier and more useful. It now outputs a
  color-coded and easy-to-read table.
  ([`fd3017f`](https://github.com/npm/npm/commit/fd3017fc3e9d42acf6394a5285122edb4dc16106),
  [@quimcalpe](https://github.com/quimcalpe))
* Added the `--depth` option to `npm outdated`, so that e.g. you can do `npm
  outdated --depth=0` to show only top-level outdated dependencies.
  ([`1d184ef`](https://github.com/npm/npm/commit/1d184ef3f4b4bc309d38e9128732e3e6fb46d49c),
  [@yyx990803](https://github.com/yyx990803))
* Added a `--no-git-tag-version` option to `npm version`, for doing the usual
  job of `npm version` minus the Git tagging. This could be useful if you need
  to increase the version in other related files before actually adding the
  tag.
  ([`59ca984`](https://github.com/npm/npm/commit/59ca9841ba4f4b2f11b8e72533f385c77ae9f8bd),
  [@evanlucas](https://github.com/evanlucas))
* Made `npm repo` and `npm docs` work without any arguments, adding them to the
  list of npm commands that work on the package in the current directory when
  invoked without arguments.
  ([`bf9048e`](https://github.com/npm/npm/commit/bf9048e2fa16d43fbc4b328d162b0a194ca484e8),
  [@robertkowalski](https://github.com/robertkowalski);
  [`07600d0`](https://github.com/npm/npm/commit/07600d006c652507cb04ac0dae9780e35073dd67),
  [@wilmoore](https://github.com/wilmoore)). There are a few other commands we
  still want to implement this for; see
  [#4204](https://github.com/npm/npm/issues/4204).
* Pass through the `GIT_SSL_NO_VERIFY` environment variable to Git, if it is
  set; we currently do this with a few other environment variables, but we
  missed that one.
  ([`c625de9`](https://github.com/npm/npm/commit/c625de91770df24c189c77d2e4bc821f2265efa8),
  [@arikon](https://github.com/arikon))
* Fixed `npm dedupe` on Windows due to incorrect path separators being used
  ([`7677de4`](https://github.com/npm/npm/commit/7677de4583100bc39407093ecc6bc13715bf8161),
  [@mcolyer](https://github.com/mcolyer)).
* Fixed the `npm help` command when multiple words were searched for; it
  previously gave a `ReferenceError`.
  ([`6a28dd1`](https://github.com/npm/npm/commit/6a28dd147c6957a93db12b1081c6e0da44fe5e3c),
  [@dereckson](https://github.com/dereckson))
* Stopped re-downloading URL and shrinkwrap dependencies, as demonstrated in
  [#3463](https://github.com/npm/npm/issues/3463)
  ([`644c2ff`](https://github.com/isaacs/npm/commit/644c2ff3e3d9c93764f7045762477f48864d64a7),
  [@spmason](https://github.com/spmason)). You can use the `--force` option to
  force re-download and installation of all dependencies.
