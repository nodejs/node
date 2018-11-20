## v4.6.1 (2017-04-21)

A little release to tide you over while we hammer out the last bits for npm@5.

### FEATURES

* [`d13c9b2f2`](https://github.com/npm/npm/commit/d13c9b2f24b6380427f359b6e430b149ac8aaa79)
  `init-package-json@1.10.0`:
  The `name:` prompt is now `package name:` to make this less ambiguous for new users.

  The default package name is now a valid package name. For example: If your package directory
  has mixed case, the default package name will be all lower case.
* [`f08c66323`](https://github.com/npm/npm/commit/f08c663231099f7036eb82b92770806a3a79cdf1)
  [#16213](https://github.com/npm/npm/pull/16213)
  Add `--allow-same-version` option to `npm version` so that you can use `npm version` to run
  your version lifecycles and tag your git repo without actually changing the version number in
  your `package.json`.
  ([@lucastheisen](https://github.com/lucastheisen))
* [`f5e8becd0`](https://github.com/npm/npm/commit/f5e8becd05e0426379eb0c999abdbc8e87a7f6f2)
  Timing has been added throughout the install implementation. You can see it by running
  a command with `--loglevel=timing`. You can also run commands with `--timing` which will write
  an `npm-debug.log` even on success and add an entry to `_timing.json` in your cache with
  the timing information from that run.
  ([@iarna](https://github.com/iarna))

### BUG FIXES

* [`9c860f2ed`](https://github.com/npm/npm/commit/9c860f2ed3bdea1417ed059b019371cd253db2ad)
  [#16021](https://github.com/npm/npm/pull/16021)
  Fix a crash in `npm doctor` when used with a registry that does not support
  the `ping` API endpoint.
  ([@watilde](https://github.com/watilde))
* [`65b9943e9`](https://github.com/npm/npm/commit/65b9943e9424c67547b0029f02b0258e35ba7d26)
  [#16364](https://github.com/npm/npm/pull/16364)
  Shorten the ELIFECYCLE error message. The shorter error message should make it much
  easier to discern the actual cause of the error.
  ([@j-f1](https://github.com/j-f1))
* [`a87a4a835`](https://github.com/npm/npm/commit/a87a4a8359693518ee41dfeb13c5a8929136772a)
  `npmlog@4.0.2`:
  Fix flashing of the progress bar when your terminal is very narrow.
  ([@iarna](https://github.com/iarna))
* [`41c10974f`](https://github.com/npm/npm/commit/41c10974fe95a2e520e33e37725570c75f6126ea)
  `write-file-atomic@1.3.2`:
  Wait for `fsync` to complete before considering our file written to disk.
  This will improve certain sorts of Windows diagnostic problems.
* [`2afa9240c`](https://github.com/npm/npm/commit/2afa9240ce5b391671ed5416464f2882d18a94bc)
  [#16336](https://github.com/npm/npm/pull/16336)
  Don't ham-it-up when expecting JSON.
  ([@bdukes](https://github.com/bdukes))

### DOCUMENTATION FIXES

* [`566f3eebe`](https://github.com/npm/npm/commit/566f3eebe741f935b7c1e004bebf19b8625a1413)
  [#16296](https://github.com/npm/npm/pull/16296)
  Use a single convention when referring to the `<command>` you're running.
  ([@desfero](https://github.com/desfero))
* [`ccbb94934`](https://github.com/npm/npm/commit/ccbb94934d4f677f680c3e2284df3d0ae0e65758)
  [#16267](https://github.com/npm/npm/pull/16267)
  Fix a missing space in the example package.json.
  ([@famousgarkin](https://github.com/famousgarkin))

### DEPENDENCY UPDATES

* [`ebde4ea33`](https://github.com/npm/npm/commit/ebde4ea3363dfc154c53bd537189503863c9b3a4)
  `hosted-git-info@2.4.2`
* [`c46ad71bb`](https://github.com/npm/npm/commit/c46ad71bbe27aaa9ee10e107d8bcd665d98544d7)
  `init-package-json@1.9.6`
* [`d856d570d`](https://github.com/npm/npm/commit/d856d570d2df602767c039cf03439d647bba2e3d)
  `npm-registry-client@8.1.1`
* [`4a2e14436`](https://github.com/npm/npm/commit/4a2e1443613a199665e7adbda034d5b9d10391a2)
  `readable-stream@2.2.9`
* [`f0399138e`](https://github.com/npm/npm/commit/f0399138e6d6f1cd7f807d523787a3b129996301)
  `normalize-package-data@2.3.8`

### v4.5.0 (2017-03-24)

Welcome a wrinkle on npm's registry API!

Codename: Corgi

![corgi-meme](https://cloud.githubusercontent.com/assets/757502/24126107/64c14268-0d89-11e7-871b-d457e6d0082b.jpg)

This release has some bug fixes, but it's mostly about bringing support for
MUCH smaller package metadata.  How much smaller?  Well, for npm itself it
reduces 416K of gzip compressed JSON to 24K.

As a user, all you have to do is update to get to use the new API.  If
you're interested in the details we've [documented the
changes](https://github.com/npm/registry/blob/master/docs/responses/package-metadata.md)
in detail.

#### CORGUMENTS

Package metadata: now smaller. This means a smaller cache and less to download.

* [`86dad0d74`](https://github.com/npm/npm/commit/86dad0d747f288eab467d49c9635644d3d44d6f0)
  Add support for filtered package metadata.
  ([@iarna](https://github.com/iarna))
* [`41789cffa`](https://github.com/npm/npm/commit/41789cffac9845603f4bdf3f5b03f412144a0e9f)
  `npm-registry-client@8.1.0`
  ([@iarna](https://github.com/iarna))

#### NO SHRINKWRAP, NO PROBLEM

Previously we needed to extract every package's tarball to look for an
`npm-shrinkwrap.json` before we could begin working through what its
dependencies were.  This was one of the things stopping npm's network
accesses from happening more concurrently.  The new filtered package
metadata provides a new key, `_hasShrinkwrap`.  When that's set to `false`
then we know we don't have to look for one.

* [`4f5060eb3`](https://github.com/npm/npm/commit/4f5060eb31b9091013e1d6a34050973613a294a3)
  [#15969](https://github.com/npm/npm/pull/15969)
  Add support for skipping `npm-shrinkwrap.json` extraction when the
  registry can affirm that one doesn't exist.
  ([@iarna](https://github.com/iarna))

#### INTERRUPTING SCRIPTS

* [`878aceb25`](https://github.com/npm/npm/commit/878aceb25e6d6052dac15da74639ce274c8e62c5)
  [#16129](https://github.com/npm/npm/pull/16129)
  Better handle Ctrl-C while running scripts.  `npm` will now no longer exit
  until the script it is running has exited.  If you press Ctrl-C a second
  time it kill the script rather than just forwarding the Ctrl-C.
  ([@jaridmargolin](https://github.com/jaridmargolin))

#### DEPENDENCY UPDATES:

* [`def75eebf`](https://github.com/npm/npm/commit/def75eebf1ad437bf4fd3f5e103cc2d963bd2a73)
  `hosted-git-info@2.4.1`:
  Preserve case of the user name part of shortcut specifiers, previously they were lowercased.
  ([@iarna](https://github.com/iarna))
* [`eb3789fd1`](https://github.com/npm/npm/commit/eb3789fd18cfb063de9e6f80c3049e314993d235)
  `node-gyp@3.6.0`: Add support for VS2017 and Chakracore improvements.
  ([@refack](https://github.com/refack))
  ([@kunalspathak](https://github.com/kunalspathak))
* [`245e25315`](https://github.com/npm/npm/commit/245e25315524b95c0a71c980223a27719392ba75)
  `readable-stream@2.2.6` ([@mcollina](https://github.com/mcollina))
* [`30357ebc5`](https://github.com/npm/npm/commit/30357ebc5691d7c9e9cdc6e0fe7dc6253220c9c2)
  `which@1.2.14` ([@isaacs](https://github.com/isaacs))

### v4.4.4 (2017-03-16)

😩😤😅 Okay!  We have another `next`
release for ya today.  So, yes!  With v4.4.3 we fixed the bug that made
bundled scoped modules uninstallable.  But somehow I overlooked the fact
that we: A) were using these and B) that made upgrading to v4.4.3 impossible. 😭

So I've renamed those two scoped modules to no longer use scopes and we now
have a shiny new test to ensure that scoped modules don't creep into our
transitive deps and make it impossible to upgrade to `npm`.

(None of our woes applies to most of you all because most of you all don't
use bundled dependencies. `npm` does because we want the published artifact to be
installable without having to already have `npm`.)

* [`2a7409fcb`](https://github.com/npm/npm/commit/2a7409fcba6a8fab716c80f56987b255983e048e)
  [#16066](https://github.com/npm/npm/pull/16066)
  Ensure we aren't using any scoped modules
  Because `npm`s prior 4.4.3 can't install dependencies that have bundled scoped
  modules.  This didn't show up sooner because they ALSO had a bug that caused
  bundled scoped modules to not be included in the bundle.
  ([@iarna](https://github.com/iarna))
* [`eb4c70796`](https://github.com/npm/npm/commit/eb4c70796c38f24ee9357f5d4a0116db582cc7a9)
  [#16066](https://github.com/npm/npm/pull/16066)
  Switch to move-concurrently to remove scoped dependency
  ([@iarna](https://github.com/iarna))

### v4.4.3 (2017-03-15)

This is a small patch release, mostly because the published tarball for
v4.4.2 was missing a couple of modules, due to a bug involving scoped
modules, bundled dependencies and legacy tree layouts.

There are a couple of other things here that happened to be ready to go.  So
without further ado…

#### BUG FIXES

* [`3d80f8f70`](https://github.com/npm/npm/commit/3d80f8f70679ad2b8ce7227d20e8dbce257a47b9)
  [npm/fs-vacuum#6](https://github.com/npm/fs-vacuum/pull/6)
  `fs-vacuum@1.2.1`: Make sure we never, ever remove home directories. Previously if your
  home directory was entirely empty then we might `rmdir` it.
  ([@helio-frota](https://github.com/helio-frota))
* [`1af85ca9f`](https://github.com/npm/npm/commit/1af85ca9f4d625f948e85961372de7df3f3774e2)
  [#16040](https://github.com/npm/npm/pull/16040)
  Fix bug where bundled transitive dependencies that happened to be
  installed under bundled scoped dependencies wouldn't be included in the
  tarball when building a package.
  ([@iarna](https://github.com/iarna))
* [`13c7fdc2e`](https://github.com/npm/npm/commit/13c7fdc2e87456a87b1c9385a3daeae228ed7c95)
  [#16040](https://github.com/npm/npm/pull/16040)
  Fix a bug where bundled scoped dependencies couldn't be extracted.
  ([@iarna](https://github.com/iarna))
* [`d6cde98c2`](https://github.com/npm/npm/commit/d6cde98c2513fe160eab41e31c3198dfde993207)
  [#16040](https://github.com/npm/npm/pull/16040)
  Stop printing `ENOENT` errors more than once.
  ([@iarna](https://github.com/iarna))
* [`722fbf0f6`](https://github.com/npm/npm/commit/722fbf0f6cf4413cdc24b610bbd60a7dbaf2adfe)
  [#16040](https://github.com/npm/npm/pull/16040)
  Rewrite the `extract` action for greater clarity.
  Specifically, this involves moving things around structurally to do the same
  thing [`d0c6d194`](https://github.com/npm/npm/commit/d0c6d194) did, but in a more comprehensive manner.
  This also fixes a long standing bug where errors from the move step would be
  eaten during this phase and as a result we would get mysterious crashes in
  the finalize phase when finalize tried to act on them.
  ([@iarna](https://github.com/iarna))
* [`6754dabb6`](https://github.com/npm/npm/commit/6754dabb6bd3301504efb3b62f36d3fe70958c19)
  [#16040](https://github.com/npm/npm/pull/16040)
  Flatten out `@npmcorp/move`'s deps for backwards compatibility reasons. Versions prior to this
  one will fail to install any package that bundles a scoped dependency. This was responsible
  for `ENOENT` errors during the `finalize` phase.
  ([@iarna](https://github.com/iarna))

#### DOC UPDATES

* [`fba51c582`](https://github.com/npm/npm/commit/fba51c582d1d08dd4aa6eb27f9044dddba91bb18)
  [#15960](https://github.com/npm/npm/pull/15960)
  Update troubleshooting and contribution guide links.
  ([@watilde](https://github.com/watilde))


### v4.4.2 (2017-03-09):

This week, the focus on the release was mainly going through [all of npm's deps
that we manage
ourselves](https://github.com/npm/npm/wiki/npm-maintained-dependencies), and
making sure all their PRs and versions were up to date. That means there's a few
fixes here and there. Nothing too big codewise, though.

The most exciting part of this release is probably our [shiny new
Contributing](https://github.com/npm/npm/blob/latest/CONTRIBUTING.md) and
[Troubleshooting](https://github.com/npm/npm/blob/latest/TROUBLESHOOTING.md)
docs! [@snopeks](https://github.com/snopeks) did some ✨fantastic✨ work hashing it
out, and we're really hoping this is a nice big step towards making contributing
to npm easier. The troubleshooting doc will also hopefully solve common issues
for people! Do you think something is missing from it? File a PR and we'll add
it! The current document is just a baseline for further editing and additions.

Also there's maybe a bit of an easter egg in this release. 'Cause those are fun and I'm a huge nerd. 😉

#### DOCUMENTATION AHOY

* [`07e997a`](https://github.com/npm/npm/commit/07e997a7ecedba7b29ad76ffb2ce990d5c0200fc)
  [#15756](https://github.com/npm/npm/pull/15756)
  Overhaul `CONTRIBUTING.md` and add new `TROUBLESHOOTING.md` files. 🙌🏼
  ([@snopeks](https://github.com/snopeks))
* [`2f3e4b6`](https://github.com/npm/npm/commit/2f3e4b645cdc268889cf95ba24b2aae572d722ad)
  [#15833](https://github.com/npm/npm/pull/15833)
  Mention the [24-hour unpublish
  policy](http://blog.npmjs.org/post/141905368000/changes-to-npms-unpublish-policy)
  on the main registry.
  ([@carols10cents](https://github.com/carols10cents))

#### NOT REALLY FEATURES, NOT REALLY BUGFIXES. MORE LIKE TWEAKS? 🤔

* [`84be534`](https://github.com/npm/npm/commit/84be534aedb78c65cd8012427fc04871ceeccf90)
  [#15888](https://github.com/npm/npm/pull/15888)
  Stop flattening `ls`-tree output. From now on, deduped deps will be marked as
  such in the place where they would've been before getting hoisted by the
  installer.
  ([@iarna](https://github.com/iarna))
* [`e9a5dca`](https://github.com/npm/npm/commit/e9a5dca369ead646ab5922326cede1406c62bd3b)
  [#15967](https://github.com/npm/npm/pull/15967)
  Limit metadata fetches to 10 concurrent requests.
  ([@iarna](https://github.com/iarna))
* [`46aa9bc`](https://github.com/npm/npm/commit/46aa9bcae088740df86234fc199f7aef53b116df)
  [#15967](https://github.com/npm/npm/pull/15967)
  Limit concurrent installer actions to 10.
  ([@iarna](https://github.com/iarna))

#### BUGFIXES

* [`c3b994b`](https://github.com/npm/npm/commit/c3b994b71565eb4f943cce890bb887d810e6e2d4)
  [#15901](https://github.com/npm/npm/pull/15901)
  Use EXDEV aware move instead of rename. This will allow moving across devices
  and moving when filesystems don't support renaming directories full of files. It might make folks using Docker a bit happier.
  ([@iarna](https://github.com/iarna))
* [`0de1a9c`](https://github.com/npm/npm/commit/0de1a9c1db90e6705c65c068df1fe82899e60d68)
  [#15735](https://github.com/npm/npm/pull/15735)
  Autocomplete support for npm scripts with `:` colons in the name.
  ([@beyondcompute](https://github.com/beyondcompute))
* [`84b0b92`](https://github.com/npm/npm/commit/84b0b92e7f78ec4add42e8161c555325c99b7f98)
  [#15874](https://github.com/npm/npm/pull/15874)
  Stop using [undocumented](https://github.com/nodejs/node/pull/11355)
  `res.writeHeader` alias for `res.writeHead`.
  ([@ChALkeR](https://github.com/ChALkeR))
* [`895ffe4`](https://github.com/npm/npm/commit/895ffe4f3eecd674796395f91c30eda88aca6b36)
  [#15824](https://github.com/npm/npm/pull/15824)
  Fix empty versions column in `npm search` output.
  ([@bcoe](https://github.com/bcoe))
* [`38c8d7a`](https://github.com/npm/npm/commit/38c8d7adc1f43ab357d1e729ae7cd5d801a26e68)
  `init-package-json@1.9.5`: [npm/init-package-json#61](https://github.com/npm/init-package-json/pull/61) Exclude existing `devDependencies` from being added to `dependencies`. Fixes [#12260](https://github.com/npm/npm/issues/12260).
  ([@addaleax](https://github.com/addaleax))

### v4.4.1 (2017-03-06):

This is a quick little patch release to forgo the update notification
checker if you're on an unsuported (but not otherwise broken) version of
Node.js.  Right now that means 0.10 or 0.12.

* [`56ac249`](https://github.com/npm/npm/commit/56ac249ef8ede1021f1bc62a0e4fe1e9ba556af2)
  [#15864](https://github.com/npm/npm/pull/15864)
  Only use `update-notifier` on supported versions.
  ([@legodude17](https://github.com/legodude17))

### v4.4.0 (2017-02-23):

Aaaah, [@iarna](https://github.com/iarna) here, it's been a little while
since I did one of these! This is a nice little release, we've got an
update notifier, vastly less verbose error messages, new warnings on package
metadata that will probably give you a bad day, and a sprinkling of bug
fixes.

#### UPDATE NOTIFICATIONS

We now have a little nudge to update your `npm`, courtesy of
[update-notifier](https://www.npmjs.com/package/update-notifier).

* [`148ee66`](https://github.com/npm/npm/commit/148ee663740aa05877c64f16cdf18eba33fbc371)
  [#15774](https://github.com/npm/npm/pull/15774)
  `npm` will now check at start up to see if a newer version is available.
  It will check once a day. If you want to disable this, set `optOut` to `true` in
  `~/.config/configstore/update-notifier-npm.json`.
  ([@ceejbot](https://github.com/ceejbot))

#### LESS VERBOSE ERROR MESSAGES

`npm` has, for a long time, had very verbose error messages.  There was a
lot of info in there, including the cause of the error you were seeing but
without a lot of experience reading them pulling that out was time consuming
and difficult.

With this change the output is cut down substantially, centering the error
message.  So, for example if you try to `npm run sdlkfj` then the entire
error you'll get will be:

```
npm ERR! missing script: sldkfj

npm ERR! A complete log of this run can be found in:
npm ERR!     /Users/rebecca/.npm/_logs/2017-02-24T00_41_36_988Z-debug.log
```

The CLI team has discussed cutting this down even further and stripping the
`npm ERR!` prefix off those lines too.  We'd appreciate your feedback on
this!

* [`e544124`](https://github.com/npm/npm/commit/e544124592583654f2970ec332003cfd00d04f2b)
  [#15716](https://github.com/npm/npm/pull/15716)
  Make error output less verbose.
  ([@iarna](https://github.com/iarna))
* [`166bda9`](https://github.com/npm/npm/commit/166bda97410d0518b42ed361020ade1887e684af)
  [#15716](https://github.com/npm/npm/pull/15716)
  Stop encouraging users to visit the issue tracker unless we know for
  certain that it's an npm bug.
  ([@iarna](https://github.com/iarna))

#### OTHER NEW FEATURES

* [`53412eb`](https://github.com/npm/npm/commit/53412eb22c1c75d768e30f96d69ed620dfedabde)
  [#15772](https://github.com/npm/npm/pull/15772)
  We now warn if you have a module listed in both dependencies and
  devDependencies.
  ([@TedYav](https://github.com/TedYav))
* [`426b180`](https://github.com/npm/npm/commit/426b1805904a13bdc5c0dd504105ba037270cbee)
  [#15757](https://github.com/npm/npm/pull/15757)
  Default reporting metrics to default registry. Previously it defaulted to using
  `https://registry.npmjs.org`, now it will default to the result of
  `npm config get registry`. For most folks this won't actually change anything, but it
  means that folks who use a private registry will have metrics routed there by default.
  This has the potential to be interesting because it means that in the
  future private registry products ([npme](https://npme.npmjs.com/docs/)!)
  will be able to report on these metrics.
  ([@iarna](https://github.com/iarna))

#### BUG FIXES

* [`8ea0de9`](https://github.com/npm/npm/commit/8ea0de98563648ba0db032acd4d23d27c4a50a66)
  [#15716](https://github.com/npm/npm/pull/15716)
  Write logs for `cb() never called` errors.
* [`c4e83dc`](https://github.com/npm/npm/commit/c4e83dca830b24305e3cb3201a42452d56d2d864)
  Make it so that errors while reading the existing node_modules tree can't
  result in installer crashes.
  ([@iarna](https://github.com/iarna))
* [`2690dc2`](https://github.com/npm/npm/commit/2690dc2684a975109ef44953c2cf0746dbe343bb)
  Update `npm doctor` to not treat broken symlinks in your global modules as
  a permission failure. This is particularly important if you link modules and your text
  editor uses the convention of creating symlinks from `.#filename.js` to a
  machine name and pid to lock files (eg emacs and compatible things).
  ([@iarna](https://github.com/iarna))
* [`f4c3f48`](https://github.com/npm/npm/commit/f4c3f489aa5787cf0d60e8436be2190e4b0d0ff7)
  [#15777](https://github.com/npm/npm/pull/15777)
  Not exactly a bug, but change a parameterless `.apply` to `.call`.
  ([@notarseniy](https://github.com/notarseniy))

#### DEPENDENCY UPDATES

* [`549dcff`](https://github.com/npm/npm/commit/549dcff58c7aaa1e7ba71abaa14008fdf2697297)
  `rimraf@2.6.0`:
  Retry EBUSY, ENOTEMPTY and EPERM on non-Windows platforms too.
  More reliable `rimraf.sync` on Windows.
  ([@isaacs](https://github.com/isaacs))
* [`052dfb6`](https://github.com/npm/npm/commit/052dfb623da508f2b5f681da0258125552a18a4a)
  `validate-npm-package-name@3.0.0`:
  Remove ableist language in README.
  Stop allowing ~'!()* in package names.
  ([@tomdale](https://github.com/tomdale))
  ([@chrisdickinson](https://github.com/chrisdickinson))
* [`6663ea6`](https://github.com/npm/npm/commit/6663ea6ac0f0ecec5a3f04a3c01a71499632f4dc)
  `abbrev@1.1.0` ([@isaacs](https://github.com/isaacs))
* [`be6de9a`](https://github.com/npm/npm/commit/be6de9aab9e20b6eac70884e8626161eebf8721a)
  `opener@1.4.3` ([@dominic](https://github.com/dominic))
* [`900a5e3`](https://github.com/npm/npm/commit/900a5e3e3411ec221306455f99b24b9ce35757c0)
  `readable-stream@2.2.3` ([@RangerMauve](https://github.com/RangerMauve)) ([@mcollina](https://github.com/mcollina))
* [`c972a8b`](https://github.com/npm/npm/commit/c972a8b0f20a61a79c45b6642f870bea8c55c7e4)
  `tacks@1.2.6`
  ([@iarna](https://github.com/iarna))
* [`85a36ef`](https://github.com/npm/npm/commit/85a36efdac0c24501876875cb9ad40292024e0b0)
  [`7ac9265`](https://github.com/npm/npm/commit/7ac9265c56b4d9eeaca6fcfb29513f301713e7bb)
  `tap@10.2.0`
  ([@isaacs](https://github.com/saacs))

### v4.3.0 (2017-02-09):

Yay! Release time! It's a rainy day, and we have another smallish release for
y'all. These things are not necessarily related. Or are they 🌧🤔

As far as news go, you may have noticed that the CLI team dropped support for
`node@0.12` when that version went out of maintenance. Still, we've avoided
explicitly breaking it and `node@0.10` so far -- but not much longer.

Sometime soon, the CLI team plans on switching over to language features only
available as of `node@4 LTS`, and will likely start dropping old versions of node
as they go out of maintenance. The new features are exciting! We're really
looking forward to using them in the core CLI (and its dependencies) as we keep up
with our current feature work.

And speaking of features, this release is a minor bump due to a small change in
how `npm login` works for the sake of supporting OAuth-based login for npm
Enterprise users. But we won't leave the rest of y'all out -- we're working on a
larger version of this feature. Soon enough, you'll be able to log in to npm
with, say, GitHub -- and use some shiny features that come from the integration.
Or turn on 2FA and other such security features. Keep your eyes peeled for new
on this in the next few releases and our weekly newsletter!

#### NEW AUTH TYPES

There's a new command line option: `--auth-type`, which can be used to log in to
a supporting registry with OAuth2 or SAML. The current implementation is mainly
meant to support npmE customers, so if you're one of those: ask us about using
it! If not, just hold off cause we'll have a much more complete version of this
feature out soon.

* [`ac8595e`](https://github.com/npm/npm/commit/ac8595e3c9b615ff95abc3301fac1262c434792c) [`bcf2dd8`](https://github.com/npm/npm/commit/bcf2dd8a165843255c06515fa044c6e4d3b71ca4) [`9298d20`](https://github.com/npm/npm/commit/9298d20af58b92572515bfa9cf7377bd4221dc7d) [`66b61bc`](https://github.com/npm/npm/commit/66b61bc42e81ee8a1ee00fc63517f62284140688) [`dc85de7`](https://github.com/npm/npm/commit/dc85de7df6bb61f7788611813ee82ae695a18f1f)
  [#13389](https://github.com/npm/npm/pull/13389)
  Implement single-sign-on support with `--auth-type` option.
  ([@zkat](https://github.com/zkat))

#### FASTER STARTUP. SOMETIMES!

`request` is pretty heavy. And it loads a bunch of things. It's actually a
pretty big chunk of npm's load time. This small patch by Rebecca will make it so
npm only loads that module when we're actually intending to make network
requests. Those of you who use npm commands that run offline might see a small
speedup in startup time.

* [`ac73568`](https://github.com/npm/npm/commit/ac735682e666e8724549d56146821f3b8b018e25)
  [#15631](https://github.com/npm/npm/pull/15631)
  Lazy load `caching-registry-client`.
  ([@iarna](https://github.com/iarna))

#### DOCUMENTATION

* [`4ad9247`](https://github.com/npm/npm/commit/4ad9247aa82f7553c9667ee93c74ec7399d6ceec)
  [#15630](https://github.com/npm/npm/pull/15630)
  Fix formatting/rendering for root npm README.
  ([@ungoldman](https://github.com/ungoldman))

#### DEPENDENCY UPDATES

* [`8cc1112`](https://github.com/npm/npm/commit/8cc1112958638ff88ac2c24c4a065acacb93d64b)
  [npm/hosted-git-info#21](https://github.com/npm/hosted-git-info/pull/21)
  `hosted-git-info@2.2.0`:
  Add support for `.tarball()` URLs.
  ([@zkat](https://github.com/zkat))
* [`6eacc1b`](https://github.com/npm/npm/commit/6eacc1bc1925fe3cc79fc97bdc3194d944fce55e)
  `npm-registry-mock@1.1.0`
  ([@addaleax](https://github.com/addaleax))
* [`a9b6d77`](https://github.com/npm/npm/commit/a9b6d775e61cf090df0e13514c624f99bf31d1e7)
  `aproba@1.1.1`
  ([@iarna](https://github.com/iarna))

### v4.2.0 (2017-01-26):

Hi all! I'm Kat, and I'm currently sitting in a train traveling at ~300km/h
through Spain. So clearly, this release should have *something* to do with
speed. And it does! Heck, with this release, you could say we're really
_blazing_, even. 🌲🔥😏

#### IMPROVED CLI SEARCH~

You might recall if you've been keeping up that one of the reasons for a
semver-major bump to `npm@4` was an improved CLI search (read: no longer blowing
up Node). The work done for that new search system, while still relying on a
full metadata download and local search, was also meant to act as groundwork for
then-ongoing work on a brand-new, smarter search system for npm. Shortly after
`npm@4` came out, the bulk of the server-side work was done, and with this
release, the npm CLI has integrated use of the new endpoint for high-quality,
fast-turnaround searches.

No, seriously, it's *fast*. And *relevant*:

[![GOTTA GO FAST! This is a gif of the new npm search returning results in around a second for `npm search web framework`.](https://cloud.githubusercontent.com/assets/17535/21954136/f007e8be-d9fd-11e6-9231-f899c12790e0.gif)](https://github.com/npm/npm/pull/15481)

Give it a shot! And remember to check out the new website version of the search,
too, which uses the same backend as the CLI now. 🎉

Incidentally, the backend is a public service, so you can write your own search
tools, be they web-based, CLI, or GUI-based. You can read up on the [full
documentation for the search
endpoint](https://github.com/npm/registry/blob/master/docs/REGISTRY-API.md#get-v1search),
and let us know about the cool things you come up with!

* [`ce3ca51`](https://github.com/npm/npm/commit/ce3ca51ca2d60e15e901c8bf6256338e53e1eca2)
  [#15481](https://github.com/npm/npm/pull/15481)
  Add an internal `gunzip-maybe` utility for optional gunzipping.
  ([@zkat](https://github.com/zkat))
* [`e322932`](https://github.com/npm/npm/commit/e3229324d507fda10ea9e94fd4de8a4ae5025c75) [`a53055e`](https://github.com/npm/npm/commit/a53055e423f1fe168f05047aa0dfec6d963cb211) [`a1f4365`](https://github.com/npm/npm/commit/a1f436570730c6e4a173ca92d1967a87c29b7f2d) [`c56618c`](https://github.com/npm/npm/commit/c56618c62854ea61f6f716dffe7bcac80b5f4144)
  [#15481](https://github.com/npm/npm/pull/15481)
  Add support for using the new npm search endpoint for fast, quality search
  results. Includes a fallback to "classic" search.
  ([@zkat](https://github.com/zkat))

#### WHERE DID THE DEBUG LOGS GO

This is another pretty significant change: Usually, when the npm process
crashed, you would get an `npm-debug.log` in your current working directory.
This debug log would get cleared out as soon as you ran npm again. This was a
bit annoying because 1) you would get a random file in your `git status` that
you might accidentally commit, and 2) if you hit a hard-to-reproduce bug and
instinctually tried again, you would no longer have access to the repro
`npm-debug.log`.

So now, any time a crash happens, we'll save your debug logs to your cache
folder, under `_logs` (`~/.npm` on *nix, by default -- use `npm config get
cache` to see what your current value is). The cache will now hold a
(configurable) number of `npm-debug.log` files, which you can access in the
future. Hopefully this will help clean stuff up and reduce frustration from
missed repros! In the future, this will also be used by `npm report` to make it
super easy to put up issues about crashes you run into with npm. 💃🕺🏿👯‍♂️

* [`04fca22`](https://github.com/npm/npm/commit/04fca223a0f704b69340c5f81b26907238fad878)
  [#11439](https://github.com/npm/npm/pull/11439)
  Put debug logs in `$(npm get cache)/_logs` and store multiple log files.
  ([@KenanY](https://github.com/KenanY))
  ([@othiym23](https://github.com/othiym23))
  ([@isaacs](https://github.com/isaacs))
  ([@iarna](https://github.com/iarna))

#### DOCS

* [`ae8e71c`](https://github.com/npm/npm/commit/ae8e71c2b7d64d782af287a21e146d7cea6e5273)
  [#15402](https://github.com/npm/npm/pull/15402)
  Add missing backtick in one of the `npm doctor` messages.
  ([@watilde](https://github.com/watilde), [@charlotteis](https://github.com/charlotteis))
* [`821fee6`](https://github.com/npm/npm/commit/821fee6d0b12a324e035c397ae73904db97d07d2)
  [#15480](https://github.com/npm/npm/pull/15480)
  Clarify that unscoped packages can depend on scoped packages and vice-versa.
  ([@chocolateboy](https://github.com/chocolateboy))
* [`2ee45a8`](https://github.com/npm/npm/commit/2ee45a884137ae0706b7c741c671fef2cb3bac96)
  [#15515](https://github.com/npm/npm/pull/15515)
  Update minimum supported Node version number in the README to `node@>=4`.
  ([@watilde](https://github.com/watilde))
* [`af06aa9`](https://github.com/npm/npm/commit/af06aa9a357578a8fd58c575f3dbe55bc65fc376)
  [#15520](https://github.com/npm/npm/pull/15520)
  Add section to `npm-scope` docs to explain that scope owners will own scoped
  packages with that scope. That is, user `@alice` is not allowed to publish to
  `@bob/my-package` unless explicitly made an owner by user (or org) `@bob`.
  ([@hzoo](https://github.com/hzoo))
* [`bc892e6`](https://github.com/npm/npm/commit/bc892e6d07a4c6646480703641a4d71129c38b6d)
  [#15539](https://github.com/npm/npm/pull/15539)
  Replace `http` with `https` and fix typos in some docs.
  ([@watilde](https://github.com/watilde))
* [`1dfe875`](https://github.com/npm/npm/commit/1dfe875b9ac61a0ab9f61a2eab02bacf6cce583c)
  [#15545](https://github.com/npm/npm/pull/15545)
  Update Node.js download link to point to the right place.
  ([@watilde](https://github.com/watilde))

#### DEPENDENCIES

  * [`b824bfb`](https://github.com/npm/npm/commit/b824bfbeb2d89c92762e9170b026af98b5a3668a)
    `ansi-regex@2.1.1`
  * [`81ea3e8`](https://github.com/npm/npm/commit/81ea3e8e4ea34cd9c2b418512dcb508abcee1380)
    `mississippi@1.3.0`

#### MISC

* [`98df212`](https://github.com/npm/npm/commit/98df212a91fd6ff4a02b9cd247f4166f93d3977a)
  [#15492](https://github.com/npm/npm/pull/15492)
  Update the "master" node version used for AppVeyor to `node@7`.
  ([@watilde](https://github.com/watilde))
* [`d75fc03`](https://github.com/npm/npm/commit/d75fc03eda5364f12ac266fa4f66e31c2e44e864)
  [#15413](https://github.com/npm/npm/pull/15413)
  `npm run-script` now exits with the child process' exit code on exit.
  ([@kapals](https://github.com/kapals))

### v4.1.2 (2017-01-12)

We have a twee little release this week as we come back from the holidays.

#### 0.12 IS UNSUPPORTED NOW (really)

After [jumping the gun a
little](https://github.com/npm/npm/releases/tag/v4.0.2), we can now
officially remove 0.12 from our supported versions list.  The Node.js
project has now officially ended even maintenance support for 0.12 and thus,
so will we.  To reiterate from the last time we did this:

What this means:

* Your contributions will no longer block on the tests passing on 0.12.
* We will no longer block dependency upgrades on working with 0.12.
* Bugs filed on the npm CLI that are due to incompatibilities with 0.12
  (and older versions) will be closed with a strong urging to upgrade to a
  supported version of Node.
* On the flip side, we'll continue to (happily!) accept patches that
  address regressions seen when running the CLI with Node.js 0.12.

What this doesn't mean:

* The CLI is going to start depending on ES2015+ features. npm continues
  to work, in almost all cases, all the way back to Node.js 0.8, and our
  long history of backwards compatibility is a source of pride for the
  team.
* We aren't concerned about the problems of users who, for whatever
  reason, can't update to newer versions of npm. As mentioned above, we're
  happy to take community patches intended to address regressions.

We're not super interested in taking sides on what version of Node.js
you "should" be running. We're a workflow tool, and we understand that
you all have a diverse set of operational environments you need to be
able to support. At the same time, we _are_ a small team, and we need
to put some limits on what we support. Tracking what's supported by our
runtime's own team seems most practical, so that's what we're doing.

* [`c7bbba8`](https://github.com/npm/npm/commit/c7bbba8744b62448103a1510c65d9751288abb5d)
  Remove 0.12 from our supported versions list.
  ([@iarna](https://github.com/iarna))

#### WRITING TO SYMLINKED `package.json` (AND OTHER FILES)

If your `package.json`, `npm-shrinkwrap.json` or `.npmrc` were a symlink and
you used an `npm` command that modified one of these (eg `npm config set` or
`npm install --save`) then previously we would have removed your symlink and
replaced it with an ordinary file. While making these files symlinks is pretty
uncommon, this was still surprising behavior. With this fix we now overwrite
the _destination_ of the symlink and preserve the symlink itself.

* [`a583983`](https://github.com/npm/npm/commit/a5839833d3de7072be06884b91902c093aff1aed)
  [write-file-atomic/#5](https://github.com/npm/write-file-atomic/issues/5)
  [#10223](https://github.com/npm/npm/10223)
  `write-file-atomic@1.3.1`:
  When the target is a symlink, write-file-atomic now overwrites the
  _destination_ of the symlink, instead of replacing the symlink itself.  This
  makes it's behavior match `fs.writeFile`.

  Fixed a bug where it would ALWAYS fs.stat to look up default mode and chown
  values even if you'd passed them in.  (It still used the values you passed
  in, but did a needless stat.)
  ([@iarna](https://github.com/iarna))

#### DEPENDENCY UPDATES

* [`521f230`](https://github.com/npm/npm/commit/521f230dd57261e64ac9613b3db62f5312971dca)
  `node-gyp@3.5.0`:
  Improvements to how Python is located. New `--devdir` flag.
  ([@bnoordhuis](https://github.com/bnoordhuis))
  ([@mhart](https://github.com/mhart))
* [`ccd83e8`](https://github.com/npm/npm/commit/ccd83e8a70d35fb0904f8a9adb2ff7ac8a6b2706)
  `JSONStream@1.3.0`:
  Add new emitPath option.
  ([@nathanwills](https://github.com/nathanwills))

#### TEST IMPROVEMENTS

* [`d76e084`](https://github.com/npm/npm/commit/d76e08463fd65705217624b861a1443811692f34)
  Disable metric reporting for test suite even if the user has it enabled.
  ([@iarna](https://github.com/iarna))

### v4.1.1 (2016-12-16)

This fixes a bug in the metrics reporting where, if you had enabled it then
installs would create a metrics reporting process, that would create a
metrics reporting process, that would… well, you get the idea.  The only
way to actually kill these processes is to turn off your networking, then
on MacOS/Linux kill them with `kill -9`. Alternatively you can just reboot.

Anyway, this is a quick release to fix that bug:

* [`51c393f`](https://github.com/npm/npm/commit/51c393feff5f4908c8a9fb02baef505b1f2259be)
  [#15237](https://github.com/npm/npm/pull/15237)
  Don't launch a metrics sender process if we're running from a metrics
  sender process.
  ([@iarna](https://github.com/iarna))

### v4.1.0 (2016-12-15)

I'm really excited about `npm@4.1.0`. I know, I know, I'm kinda overexcited
in my changelogs, but this one is GREAT. We've got a WHOLE NEW subcommand, I
mean, when was the last time you saw that? YEARS! And we have the beginnings
of usage metrics reporting. Then there's a fix for a really subtle bug that
resulted in `shasum` errors. And then we also have a few more bug fixes and
other improvements.

#### ANONYMOUS METRIC REPORTING

We're adding the ability for you all to help us track the quality of your
experiences using `npm`. Metrics will be sent if you run:

```
npm config set send-metrics true
```

Then `npm` will report to `registry.npmjs.org` the number of successful and
failed installations you've had. The data contains no identifying
information and npm will not attempt to correlate things like IP address
with the metrics being submitted.

Currently we only track number of successful and failed installations. In
the future we would like to find additional metrics to help us better
quantify the quality of the `npm` experience.

* [`190a658`](https://github.com/npm/npm/commit/190a658c4222f6aa904cbc640fc394a5c875e4db)
  [#15084](https://github.com/npm/npm/pull/15084)
  Add facility for recording and reporting success metrics.
  ([@iarna](https://github.com/iarna))
* [`87afc8b`](https://github.com/npm/npm/commit/87afc8b466f553fb49746c932c259173de48d0a4)
  [npm/npm-registry-client#147](https://github.com/npm/npm-registry-client/pull/148)
  `npm-registry-client@7.4.5`:
  Add support for sending anonymous CLI metrics.
  ([@iarna](https://github.com/iarna),
  [@sisidovski](https://github.com/sisidovski))

### NPM DOCTOR

<pre>
<u>Check</u>                               <u>Value</u>                        <u>Recommendation</u>
npm ping                            ok
npm -v                              v4.0.5
node -v                             v4.6.1                       Use node v6.9.2
npm config get registry             https://registry.npmjs.org/
which git                           /Users/rebecca/bin/git
Perms check on cached files         ok
Perms check on global node_modules  ok
Perms check on local node_modules   ok
Checksum cached files               ok
</pre>

It's a rare day that we add a new command to `npm`, so I'm excited to
present to you `npm doctor`. It checks for a number of common problems and
provides some recommended solutions. It was put together through the hard
work of [@watilde](https://github.com/watilde).

* [`2359505`](https://github.com/npm/npm/commit/23595055669f76c9fe8f5f1cf4a705c2e794f0dc)
  [`0209ee5`](https://github.com/npm/npm/commit/0209ee50448441695fbf9699019d34178b69ba73)
  [#14582](https://github.com/npm/npm/pull/14582)
  Add new `npm doctor` to give your project environment a health check.
  ([@watilde](https://github.com/watilde))

#### FIX MAJOR SOURCE OF SHASUM ERRORS

If you've been getting intermittent shasum errors then you'll be pleased to
know that we've tracked down at least one source of them, if not THE source
of them.

* [`87afc8b`](https://github.com/npm/npm/commit/87afc8b466f553fb49746c932c259173de48d0a4)
  [#14626](https://github.com/npm/npm/issues/14626)
  [npm/npm-registry-client#148](https://github.com/npm/npm-registry-client/pull/148)
  `npm-registry-client@7.4.5`:
  Fix a bug where an `ECONNRESET` while fetching a package file would result
  in a partial download that would be reported as a "shasum mismatch". It
  now throws away the partial download and retries it.
  ([@iarna](https://github.com/iarna))

#### FILE URLS AND NODE.JS 7

When `npm` was formatting `file` URLs we took advantage of `url.format` to
construct them. Node.js 7 changed the behavior in such a way that our use of
`url.format` stopped producing URLs that we could make use of.

The reasons for this have to do with the `file` URL specification and how
invalid (according to the specification) URLs are handled. How this changed
is most easily explained with a table:

<table>
<tr><th></th><th>URL</th><th>Node.js &lt;= 6</th><th><tt>npm</tt>'s understanding</th><th>Node.js 7</th><th><tt>npm</tt>'s understanding</th></tr>
<tr><td>VALID</td><td><tt>file:///abc/def</tt></td><td><tt>file:///abc/def</tt></td><td><tt>/abc/def</tt></td><td><tt>file:///abc/def</tt></td><td><tt>/abc/def</tt></td></tr>
<tr><td>invalid</td><td><tt>file:/abc/def</tt></td><td><tt>file:/abc/def</tt></td><td><tt>/abc/def</tt></td><td><tt>file:///abc/def</tt></td><td><tt>/abc/def</tt></td></tr>
<tr><td>invalid</td><td><tt>file:abc/def</tt></td><td><tt>file:abc/def</tt></td><td><tt>$CWD/abc/def</tt></td><td><tt>file://abc/def</tt></td><td><tt>/def</tt> on the <tt>abc</tt> host</td></tr>
<tr><td>invalid</td><td><tt>file:../abc/def</tt></td><td><tt>file:../abc/def</tt></td><td><tt>$CWD/../abc/def</tt></td><td><tt>file://../abc/def</tt></td><td><tt>/abc/def</tt> on the <tt>..</tt> host</td></tr>
</table>

So the result was that passing a `file` URL that npm had received that used
through Node.js 7's `url.format` changed its meaning as far as `npm` was
concerned. As those kinds of URLs are, per the specification, invalid, how
they should be handled is undefined and so the change in Node.js wasn't a
bug per se.

Our solution is to stop using `url.format` when constructing this kind of
URL.

* [`173935b`](https://github.com/npm/npm/commit/173935b4298e09c4fdcb8f3a44b06134d5aff181)
  [#15114](https://github.com/npm/npm/issues/15114)
  Stop using `url.format` for relative local dep paths.
  ([@zkat](https://github.com/zkat))

#### EXTRANEOUS LIFECYCLE SCRIPT EXECUTION WHEN REMOVING

* [`afb1dfd`](https://github.com/npm/npm/commit/afb1dfd944e57add25a05770c0d52d983dc4e96c)
  [#15090](https://github.com/npm/npm/pull/15090)
  Skip top level lifecycles when uninstalling.
  ([@iarna](https://github.com/iarna))

#### REFACTORING AND INTERNALS

* [`c9b279a`](https://github.com/npm/npm/commit/c9b279aca0fcb8d0e483e534c7f9a7250e2a9392)
  [#15205](https://github.com/npm/npm/pull/15205)
  [#15196](https://github.com/npm/npm/pull/15196)
  Only have one function that determines which version of a package to use
  given a specifier and a list of versions.
  ([@iarna](https://github.com/iarna),
  [@zkat](https://github.com/zkat))

* [`981ce63`](https://github.com/npm/npm/commit/981ce6395e7892dde2591b44e484e191f8625431)
  [#15090](https://github.com/npm/npm/pull/15090)
  Rewrite prune to use modern npm plumbing.
  ([@iarna](https://github.com/iarna))

* [`bc4b739`](https://github.com/npm/npm/commit/bc4b73911f58a11b4a2d28b49e24b4dd7365f95b)
  [#15089](https://github.com/npm/npm/pull/15089)
  Rename functions and variables in the module that computes what changes to
  make to your installation.
  ([@iarna](https://github.com/iarna))

* [`2449f74`](https://github.com/npm/npm/commit/2449f74a202b3efdb1b2f5a83356a78ea9ecbe35)
  [#15089](https://github.com/npm/npm/pull/15089)
  When computing changes to make to your installation, use a function to add
  new actions to take instead of just pushing on a list.
  ([@iarna](https://github.com/iarna))

#### IMPROVED LOGGING

* [`335933a`](https://github.com/npm/npm/commit/335933a05396258eead139d27eea3f7668ccdfab)
  [#15089](https://github.com/npm/npm/pull/15089)
  Log when we remove obsolete dependencies in the tree.
  ([@iarna](https://github.com/iarna))

#### DOCUMENTATION

* [`33ca4e6`](https://github.com/npm/npm/commit/33ca4e6db3c1878cbc40d5e862ab49bb0e82cfb2)
  [#15157](https://github.com/npm/npm/pull/15157)
  Update `npm cache` docs to use more consistent language
  ([@JonahMoses](https://github.com/JonahMoses))

#### DEPENDENCY UPDATES

* [`c2d22fa`](https://github.com/npm/npm/commit/c2d22faf916e8260136a1cc95913ca474421c0d3)
  [#15215](https://github.com/npm/npm/pull/15215)
  `nopt@4.0.1`:
  The breaking change is a small tweak to how empty string values are
  handled. See the brand-new
  [CHANGELOG.md for nopt](https://github.com/npm/nopt/blob/v4.0.1/CHANGELOG.md) for further
  details about what's changed in this release!
  ([@adius](https://github.com/adius),
  [@samjonester](https://github.com/samjonester),
  [@elidoran](https://github.com/elidoran),
  [@helio](https://github.com/helio),
  [@silkentrance](https://github.com/silkentrance),
  [@othiym23](https://github.com/othiym23))
* [`54d949b`](https://github.com/npm/npm/commit/54d949b05adefffeb7b5b10229c5fe0ccb929ac3)
  [npm/lockfile#24](https://github.com/npm/lockfile/pull/24)
  `lockfile@1.0.3`:
  Handled case where callback was not passed in by the user.
  ([@ORESoftware](https://github.com/ORESoftware))
* [`54acc03`](https://github.com/npm/npm/commit/54acc0389b39850c0725d0868cb5e61317b57503)
  `npmlog@4.0.2`:
  Documentation update.
  ([@helio-frota](https://github.com/helio-frota))
* [`57f4bc1`](https://github.com/npm/npm/commit/57f4bc1150322294c1ea0a287ad0a8e457c151e6)
  `osenv@0.1.4`:
  Test changes.
  ([@isaacs](https://github.com/isaacs))
* [`bea1a2d`](https://github.com/npm/npm/commit/bea1a2d0db566560e13ecc1d5f42e55811269c88)
  `retry@0.10.1`:
  No changes.
  ([@tim-kos](https://github.com/tim-kos))
* [`6749e39`](https://github.com/npm/npm/commit/6749e395f868109afd97f79d36507e6567dd48fb)
  [kapouer/marked-man#9](https://github.com/kapouer/marked-man/pull/9)
  `marked-man@0.2.0`:
  Add table support.
  ([@gholk](https://github.com/gholk))

### v4.0.5 (2016-12-01)

It's that time of year! December is upon us, which means y'all are just going to
be doing a lot less, in general, for the next month or so. The "Xmas Chasm", as
we like to call it, has already begun. So for those of you reading it from the
other side: Hi! Welcome back!

This week's release is a relatively small one, involving just a few bugfixes and
dependency upgrades. The CLI team has been busy recently with scoping out
`npm@5`, and starting to do initial spec work for in-scope stuff.

#### BUGFIXES

On to the actual changes!

* [`9776d8f`](https://github.com/npm/npm/commit/9776d8f70a0ea8d921cbbcab7a54e52c15fc455f)
  [#15081](https://github.com/npm/npm/pull/15081)
  `bundledDependencies` are intended to be left untouched by the installer, as
  much as possible -- if they're bundled, we assume that you want to be
  particular about the contents of your bundle.

  The installer used to have a corner case where existing dependencies that had
  bundledDependencies would get clobbered by as the installer moved stuff
  around, even though the installer already avoided moving deps that were
  themselves bundled. This is now fixed, along with the connected crasher, and
  your bundledDeps should be left even more intact than before!
  ([@iarna](https://github.com/iarna))
* [`fc61c08`](https://github.com/npm/npm/commit/fc61c082122104031ccfb2a888432c9f809a0e8b)
  [#15082](https://github.com/npm/npm/pull/15082)
  Initialize nodes from bundled dependencies. This should address
  [#14427](https://github.com/npm/npm/issues/14427) and related issues, but it's
  turned out to be a tremendously difficult issue to reproduce in a test. We
  decided to include it even pending tests, because we found the root cause of
  the errors.
  ([@iarna](https://github.com/iarna))
* [`d8471a2`](https://github.com/npm/npm/commit/d8471a294ef848fc893f60e17d6ec6695b975d16)
  [#12811](https://github.com/npm/npm/pull/12811)
  Consider `devDependencies` when deciding whether to hoist a package. This
  should resolve a variety of missing dependency issues some folks were seeing
  when `devDependencies` happened to also be dependencies of your
  `dependencies`. This often manifested as modules going missing, or only being
  installed, after `npm install` was called twice.
  ([@schmod](https://github.com/schmod))

#### DEPENDENCY UPDATES

* [`5978703`](https://github.com/npm/npm/commit/5978703da8669adae464789b1b15ee71d7f8d55d)
  `graceful-fs@4.1.11`:
  `EPERM` errors are Windows are now handled more gracefully. Windows users that
  tended to see these errors due to, say, an antivirus-induced race condition,
  should see them much more rarely, if at all.
  ([@zkatr](https://github.com/zkat))
* [`85b0174`](https://github.com/npm/npm/commit/85b0174ba9842e8e89f3c33d009e4b4a9e877c7d)
  `request@2.79.0`
  ([@zkat](https://github.com/zkat))
* [`9664d36`](https://github.com/npm/npm/commit/9664d36653503247737630440bc2ff657de965c3)
  `tap@8.0.1`
  ([@zkat](https://github.com/zkat))

#### MISCELLANEOUS

* [`f0f7b0f`](https://github.com/npm/npm/commit/f0f7b0fd025daa2b69994130345e6e8fdaaa0304)
  [#15083](https://github.com/npm/npm/pull/15083)
  Removed dead code.
  ([@iarna](https://github.com/iarna))* [`bc32afe`](https://github.com/npm/npm/commit/bc32afe4d12e3760fb5a26466dc9c26a5a2981d5) [`c8a22fe`](https://github.com/npm/npm/commit/c8a22fe5320550e09c978abe560b62ce732686f4) [`db2666d`](https://github.com/npm/npm/commit/db2666d8c078fc69d0c02c6a3de9b31be1e995e9)
  [#15085](https://github.com/npm/npm/pull/15085)
  Change some network tests so they can run offline.
  ([@iarna](https://github.com/iarna))
* [`744a39b`](https://github.com/npm/npm/commit/744a39b836821b388ad8c848bd898c1d006689a9)
  [#15085](https://github.com/npm/npm/pull/15085)
  Make Node.js tests compatible with Windows.
  ([@iarna](https://github.com/iarna))

### v4.0.3 (2016-11-17)

Hey you all, we've got a couple of bug fixes for you, a slew of
documentation improvements and some improvements to our CI environment.  I
know we just got v4 out the door, but the CLI team is already busy planning
v5.  We'll have more for you in early December.

#### BUG FIXES

* [`45d40d9`](https://github.com/npm/npm/commit/45d40d96d2cd145f1e36702d6ade8cd033f7f332)
  [`ba2adc2`](https://github.com/npm/npm/commit/ba2adc2e822d5e75021c12f13e3f74ea2edbde32)
  [`1dc8908`](https://github.com/npm/npm/commit/1dc890807bd78a1794063688af31287ed25a2f06)
  [`2ba19ee`](https://github.com/npm/npm/commit/2ba19ee643d612d103cdd8f288d313b00d05ee87)
  [#14403](https://github.com/npm/npm/pull/14403)
  Fix a bug where a scoped module could produce crashes when incorrectly
  computing the paths related to their location. This patch reorganizes how path information
  is passed in to eliminate the possibility of this sort of bug.
  ([@iarna](https://github.com/iarna))
  ([@NatalieWolfe](https://github.com/NatalieWolfe))
* [`1011ec6`](https://github.com/npm/npm/commit/1011ec61230288c827a1c256735c55cf03d6228f)
  [npm/npmlog#46](https://github.com/npm/npmlog/pull/46)
  `npmlog@4.0.1`: Fix a bug where the progress bar would still display even if
  you passed in `--no-progress`.
  ([@iarna](https://github.com/iarna))

#### DOCUMENTATION UPDATES

* [`c3ac177`](https://github.com/npm/npm/commit/c3ac177236124c80524c5f252ba8f6670f05dcd8)
  [#14406](https://github.com/npm/npm/pull/14406)
  Sync up the dispute policy included with the CLI with the [current official text](https://www.npmjs.com/policies/disputes).
  ([@mike-engel](https://github.com/mike-engel))
* [`9c663b2`](https://github.com/npm/npm/commit/9c663b2dd8552f892dc0205330bbc73a484ecd81)
  [#14627](https://github.com/npm/npm/pull/14627)
  Update build status branch in README.
  ([@cameronroe](https://github.com/cameronroe))
* [`8a8a0a3`](https://github.com/npm/npm/commit/8a8a0a3d490fc767def208f925cdff57e16e565b)
  [#14609](https://github.com/npm/npm/pull/14609)
  Update examples URLs of GitHub repos where those repos have moved to new URLs.
  ([@dougwilson](https://github.com/dougwilson))
* [`7a6425b`](https://github.com/npm/npm/commit/7a6425bcd4decde5d4b0af8b507e98723a07c680)
  [#14472](https://github.com/npm/npm/pull/14472)
  Document `sign-git-tag` in
  [npm-version(1)](https://github.com/npm/npm/blob/release-next/doc/cli/npm-version.md)'s
  configuration section.
  ([@strugee](https://github.com/strugee))
* [`f3087cc`](https://github.com/npm/npm/commit/f3087cc58c903d9a70275be805ebaf0eadbcbe1b)
  [#14546](https://github.com/npm/npm/pull/14546)
  Add a note about the dangers of configuring npm via uppercase env vars.
  ([@tuhoojabotti](https://github.com/tuhoojabotti))
* [`50e51b0`](https://github.com/npm/npm/commit/50e51b04a143959048cf9e1e4c8fe15094f480b0)
  [#14559](https://github.com/npm/npm/pull/14559)
  Remove documentation that incorrectly stated that we check `.npmrc` permissions.
  ([@iarna](https://github.com/iarna))

##### OH UH, HELLO AGAIN NODE.JS 0.12

* [`6f0c353`](https://github.com/npm/npm/commit/6f0c353e4e89b0378a4c88c829ccf9a1c5ae829d)
  [`f78bde6`](https://github.com/npm/npm/commit/f78bde6983bdca63d5fcb9c220c87e8f75ffb70e)
  [#14591](https://github.com/npm/npm/pull/14591)
  Reintroduce Node.js 0.12 to our support matrix.  We jumped the gun when
  removing it.  We won't drop support for it till the Node.js project does
  so at the end of December 2016.
  ([@othiym23](https://github.com/othiym23))

#### TEST/CI UPDATES

* [`aa73d1c`](https://github.com/npm/npm/commit/aa73d1c1cc22608f95382a35b33da252addff38e)
  [`c914e80`](https://github.com/npm/npm/commit/c914e80f5abcb16c572fe756c89cf0bcef4ff991)
* [`58fe064`](https://github.com/npm/npm/commit/58fe064dcc80bc08c677647832f2adb4a56b538a)
  [#14602](https://github.com/npm/npm/pull/14602)
  When running tests with coverage, use nyc's cache. This provides an 8x speedup!
  ([@bcoe](https://github.com/bcoe))
* [`ba091ce`](https://github.com/npm/npm/commit/ba091ce843af5d694f4540e825b095435b3558d8)
  [#14435](https://github.com/npm/npm/pull/14435)
  Remove an unused zero byte `package.json` found in the test fixtures.
  ([@baderbuddy](https://github.com/baderbuddy))

#### DEPENDENCY UPDATES

* [`442e01e`](https://github.com/npm/npm/commit/442e01e42d8a439809f6726032e3b73ac0d2b2f8)
  `readable-stream@2.2.2`:
  Bring in latest changes from Node.js 7.x.
  ([@calvinmetcalf](https://github.com/calvinmetcalf))
* [`bfc4a1c`](https://github.com/npm/npm/commit/bfc4a1c0c17ef0a00dfaa09beba3389598a46535)
  `which@1.2.12`:
  Remove unused require.
  ([@isaacs](https://github.com/isaacs))

#### DEV DEPENDENCY UPDATES

* [`7075b05`](https://github.com/npm/npm/commit/7075b054d8d2452bb53bee9b170498a48a0dc4e9)
  `marked-man@0.1.6`
  ([@kapouer](https://github.com/kapouer))
* [`3e13fea`](https://github.com/npm/npm/commit/3e13fea907ee1141506a6de7d26cbc91c28fdb80)
  `tap@8.0.0`
  ([@isaacs](https://github.com/isaacs))

### v4.0.2 (2016-11-03)

Hola, amigxs. I know it's been a long time since I rapped at ya, but I
been spending a lotta time quietly reflecting on all the things going on
in my life. I was, like, [in Japan for a while](https://gist.github.com/othiym23/c98bd4ef5d9fb3f496835bd481ef40ae),
and before that my swell colleagues [@zkat](https://github.com/zkat) and
[@iarna](https://github.com/iarna) have been very capably managing the release
process for quite a while. But I returned from Japan somewhat refreshed, very
jetlagged, and filled with a burning urge to get `npm@4` as stable as possible
before we push it out to the user community at large, so I decided to do this
release myself. (Also, huge thanks to Kat and Rebecca for putting out `npm@4`
so capably while I was on vacation! So cool to return to a major release having
gone so well without my involvement!)

That said...

#### NEVER TRUST AN X.0.0 RELEASE

Even though 4.0.1 came out hard on the heels of 4.0.0 with a couple
critical fixes, we've found a couple other major issues that we want to
see fixed before making `npm@4` into `npm@latest`. Some of these are
arguably breaking changes on their own, so now is the time to get them
out if we're going to do so before `npm@5`, and all of them are pretty
significant blockers for a substantial number of users, so now is the
best time to fix them.

##### PREPUBLISHONLY WHOOPS

The code running the `publish*` lifecycle events was very confusingly written.
In fact, we didn't really figure out what it was doing until we added the new
`prepublishOnly` event and it was running people's scripts from the wrong
directory. We made it simpler. See the [commit
message](https://github.com/npm/npm/commit/8b32d67aa277fd7e62edbed886387a855f58387f)
for details.

Because the change is no longer running publish events when publishing prebuilt
artifacts, it's technically a breaking / semver-major change. In the off chance
that the new behavior breaks any of y'all's workflows, let us know, and we can
roll some or all of this change back until `npm@5` (or forever, if that works
better for you).

* [`8b32d67`](https://github.com/npm/npm/commit/8b32d67aa277fd7e62edbed886387a855f58387f)
  [#14502](https://github.com/npm/npm/pull/14502)
  Simplify lifecycle invocation and fix `prepublishOnly`.
  ([@othiym23](https://github.com/othiym23))

##### G'BYE NODE.JS 0.10, 0.12, and 5.X; HI THERE, NODE 7

With the advent of the second official Node.js LTS release, Node 6.x
'Boron', the Node.js project has now officially dropped versions 0.10
and 0.12 out of the maintenance phase of LTS. (Also, Node 5 was never
part of LTS, and will see no further support now that Node 7 has been
released.) As a small team with limited resources, the npm CLI team is
following suit and dropping those versions of Node from its CI test
matrix.

What this means:

* Your contributions will no longer block on the tests passing on 0.10 and 0.12.
* We will no longer block dependency upgrades on working with 0.10 and 0.12.
* Bugs filed on the npm CLI that are due to incompatibilities with 0.10
  or 0.12 (and older versions) will be closed with a strong urging to
  upgrade to a supported version of Node.
* On the flip side, we'll continue to (happily!) accept patches that
  address regressions seen when running the CLI with Node.js 0.10 and
  0.12.

What this doesn't mean:

* The CLI is going to start depending on ES2015+ features. npm continues
  to work, in almost all cases, all the way back to Node.js 0.8, and our
  long history of backwards compatibility is a source of pride for the
  team.
* We aren't concerned about the problems of users who, for whatever
  reason, can't update to newer versions of npm. As mentioned above, we're
  happy to take community patches intended to address regressions.

We're not super interested in taking sides on what version of Node.js
you "should" be running. We're a workflow tool, and we understand that
you all have a diverse set of operational environments you need to be
able to support. At the same time, we _are_ a small team, and we need
to put some limits on what we support. Tracking what's supported by our
runtime's own team seems most practical, so that's what we're doing.

* [`ab630c9`](https://github.com/npm/npm/commit/ab630c9a7a1b40cdd4f1244be976c25ab1525907)
  [#14503](https://github.com/npm/npm/pull/14503)
  Node 6 is LTS; 5.x, 0.10, and 0.12 are unsupported.
  ([@othiym23](https://github.com/othiym23))
* [`731ae52`](https://github.com/npm/npm/commit/731ae526fb6e9951c43d82a26ccd357b63cc56c2)
  [#14503](https://github.com/npm/npm/pull/14503)
  Update supported version expression.
  ([@othiym23](https://github.com/othiym23))

##### DISENTANGLING SCOPE

The new `Npm-Scope` header was previously reusing the `scope`
configuration option to pass the current scope back to your current
registry (which, as [described
previously](https://github.com/npm/npm/blob/release-next/CHANGELOG.md#send-extra-headers-to-registry), is meant to set up some upcoming
registry features). It turns out that had some [seriously weird
consequences](https://github.com/npm/npm/issues/14412) in the case where
you were already configuring `scope` in your own environment. The CLI
now uses separate configuration for this.

* [`39358f7`](https://github.com/npm/npm/commit/39358f732ded4aa46d86d593393a0d6bca5dc12a)
  [#14477](https://github.com/npm/npm/pull/14477)
  Differentiate registry scope from project scope in configuration.
  ([@zkat](https://github.com/zkat))

#### SMALLER CHANGES

* [`7f41295`](https://github.com/npm/npm/commit/7f41295775f28b958a926f9cb371cb37b05771dd)
  [#14519](https://github.com/npm/npm/pull/14519)
  Document that as of `npm@4.0.1`, `npm shrinkwrap` now includes `devDependencies` unless
  instructed otherwise.
  ([@iarna](https://github.com/iarna))
* [`bdc2f9e`](https://github.com/npm/npm/commit/bdc2f9e255ddf1a47fd13ec8749d17ed41638b2c)
  [#14501](https://github.com/npm/npm/pull/14501)
  The `ENOSELF` error message is tricky to word. It's also an error that
  normally bites new users. Clean it up in an effort to make it easier
  to understand what's going on.
  ([@snopeks](https://github.com/snopeks), [@zkat](https://github.com/zkat))

#### DEPENDENCY UPGRADES

* [`a52d0f0`](https://github.com/npm/npm/commit/a52d0f0c9cf2de5caef77e12eabd7dca9e89b49c)
  `glob@7.1.1`:
  - Handle files without associated perms on Windows.
  - Fix failing case with `absolute` option.
  ([@isaacs](https://github.com/isaacs), [@phated](https://github.com/phated))
* [`afda66d`](https://github.com/npm/npm/commit/afda66d9afcdcbae1d148f589287583c4182d124)
  [isaacs/node-graceful-fs#97](https://github.com/isaacs/node-graceful-fs/pull/97)
  `graceful-fs@4.1.10`: Better backoff for EPERM on Windows.
  ([@sam-github](https://github.com/sam-github))
* [`e0023c0`](https://github.com/npm/npm/commit/e0023c089ded9161fbcbe544f12b07e12e3e5729)
  [npm/inflight#3](https://github.com/npm/inflight/pull/3)
  `inflight@1.0.6`: Clean up even if / when a callback throws.
  ([@phated](https://github.com/phated))
* [`1d91594`](https://github.com/npm/npm/commit/1d9159440364d2fe21e8bc15e08e284aaa118347)
  `request@2.78.0`
  ([@othiym23](https://github.com/othiym23))

### v4.0.1 (2016-10-24)

Ayyyy~ 🌊

So thanks to folks who were running on `npm@next`, we managed to find a few
issues of notes in that preview version, and we're rolling out a small patch
change to fix them. Most notably, anyone who was using a symlinked `node` binary
(for example, if they installed Node.js through `homebrew`), was getting a very
loud warning every time they ran scripts. Y'all should get warnings in a more
useful way, now that we're resolving those path symlinks.

Another fairly big change that we decided to slap into this version, since
`npm@4.0.0` is never going to be `latest`, is to make it so `devDependencies`
are included in `npm-shrinkwrap.json` by default -- if you do not want this, use
`--production` with `npm shrinkwrap`.

#### BIG FIXES/CHANGES

* [`eff46dd`](https://github.com/npm/npm/commit/eff46dd498ed007bfa77ab7782040a3a828b852d)
  [#14374](https://github.com/npm/npm/pull/14374)
  Fully resolve the path for `node` executables in both `$PATH` and
  `process.execPath` to avoid issues with symlinked `node`.
  ([@addaleax](https://github.com/addaleax))
* [`964f2d3`](https://github.com/npm/npm/commit/964f2d3a0675584267e6ece95b0115a53c6ca6a9)
  [#14375](https://github.com/npm/npm/pull/14375)
  Make including `devDependencies` in `npm-shrinkwrap.json` the default. This
  should help make the transition to `npm@5` smoother in the future.
  ([@iarna](https://github.com/iarna))

#### BUGFIXES

* [`a5b0a8d`](https://github.com/npm/npm/commit/a5b0a8db561916086fc7dbd6eb2836c952a42a7e)
  [#14400](https://github.com/npm/npm/pull/14400)
  Recently, we've had some consistent timeout failures while running the test
  suite under Travis. This tweak to tests should take care of those issues and
  Travis should go back to being reliably green.
  ([@iarna](https://github.com/iarna))

#### DOC PATCHES

* [`c5907b2`](https://github.com/npm/npm/commit/c5907b2fc1a82ec919afe3b370ecd34d8895c7a2)
  [#14251](https://github.com/npm/npm/pull/14251)
  Update links to Node.js downloads. They previously pointed to 404 pages.😬
  ([@ArtskydJ](https://github.com/ArtskydJ))
* [`0c122f2`](https://github.com/npm/npm/commit/0c122f24ff1d4d400975edda2b7262aaaf6f7d69)
  [#14380](https://github.com/npm/npm/pull/14380)
  Add note and clarification on when `prepare` script is run. Make it more
  consistent with surrounding descriptions.
  ([@SimenB](https://github.com/SimenB))
* [`51a62ab`](https://github.com/npm/npm/commit/51a62abd88324ba3dad18e18ca5e741f1d60883c)
  [#14359](https://github.com/npm/npm/pull/14359)
  Fixes typo in `npm@4` changelog.
  ([@kimroen](https://github.com/kimroen))

### v4.0.0 (2016-10-20)

Welcome to `npm@4`, friends!

This is our first semver major release since the release of `npm@3` just over a
year ago. Back then, `@3` turned out to be a bit of a ground-shaking release,
with a brand-new installer with significant structural changes to how npm set up
your tree. This is the end of an era, in a way. `npm@4` also marks the release
when we move *both* `npm@2` and `npm@3` into maintenance: We will no longer be
updating those release branches with anything except critical bugfixes and
security patches.

While its predecessor had some pretty serious impaact, `npm@4` is expected to
have a much smaller effect on your day-to-day use of npm. Over the past year,
we've collected a handful of breaking changes that we wanted to get in which are
only breaking under a strict semver interpretation (which we follow). Some of
these are simple usability improvements, while others fix crashes and serious
issues that required a major release to include.

We hope this release sees you well, and you can look forward to an accelerated
release pace now that the CLI team is done focusing on sustaining work -- our
Windows fixing and big bugs pushes -- and we can start focusing again on
usability, features, and performance. Keep an eye out for `npm@5` in Q1 2017,
too: We're planning a major overhaul of `shrinkwrap` as well as various speed
and usability fixes for that release. It's gonna be a fun ride. I promise. 😘

#### BRIEF OVERVIEW OF **BREAKING** CHANGES

The following breaking changes are included in this release:

* `npm search` rewritten to stream results, and no longer supports sorting.
* `npm scripts` no longer prepend the path of the node executable used to run
  npm before running scripts. A `--scripts-prepend-node-path` option has been
  added to configure this behavior.
* `npat` has been removed.
* `prepublish` has been deprecated, replaced by `prepare`. A `prepublishOnly`
  script has been temporarily added, which will *only* run on `npm publish`.
* `npm outdated` exits with exit code `1` if it finds any outdated packages.
* `npm tag` has been removed after a deprecation cycle. Use `npm dist-tag`.
* Partial shrinkwraps are no longer supported. `npm-shrinkwrap.json` is
  considered a complete installation manifest except for `devDependencies`.
* npm's default git branch is no longer `master`. We'll be using `latest` from
  now on.

#### SEARCH REWRITE (**BREAKING**)

Let's face it -- `npm search` simply doesn't work anymore. Apart from the fact
that it grew slower over the years, it's reached a point where we can no longer
fit the entire registry metadata in memory, and anyone who tries to use the
command now sees a really awful memory overflow crash from node.

It's still going to be some time before the CLI, registry, and web team are able
to overhaul `npm search` altogether, but until then, we've rewritten the
previous `npm search` implementation to *stream* results on the fly, from both
the search endpoint and a local cache. In absolute terms, you won't see a
performance increase and this patch *does* come at the cost of sorting
capabilities, but what it does do is start outputting results as it finds them.
This should make the experience much better, overall, and we believe this is an
acceptable band-aid until we have that search endpoint in place.

Incidentally, if you want a really nice search experience, we recommend checking
out [npms.io](http://npms.io), which includes a handy-dandy
[`npms-cli`](https://npm.im/npms-cli) for command-line usage -- it's an npm
search site that returns high-quality results quickly and is operated by members
of the npm community.

* [`cfd43b4`](https://github.com/npm/npm/commit/cfd43b49aed36d0e8ea6c35b07ed8b303b69be61) [`2b8057b`](https://github.com/npm/npm/commit/2b8057be2e1b51e97b1f8f38d7f58edf3ce2c145)
  [#13746](https://github.com/npm/npm/pull/13746)
  Stream search process end-to-end.
  ([@zkat](https://github.com/zkat) and [@aredridel](https://github.com/aredridel))
* [`50f4ec8`](https://github.com/npm/npm/commit/50f4ec8e8ce642aa6a58cb046b2b770ccf0029db) [`70b4bc2`](https://github.com/npm/npm/commit/70b4bc22ec8e81cd33b9448f5b45afd1a50d50ba) [`8fb470f`](https://github.com/npm/npm/commit/8fb470fe755c4ad3295cb75d7b4266f8e67f8d38) [`ac3a6e0`](https://github.com/npm/npm/commit/ac3a6e0eba61fb40099b1370c74ad1598777def4) [`bad54dd`](https://github.com/npm/npm/commit/bad54dd9f1119fe900a8d065f8537c6f1968b589) [`87d504e`](https://github.com/npm/npm/commit/87d504e0a61bccf09f5e975007d018de3a1c5f50)
  [#13746](https://github.com/npm/npm/pull/13746)
  Updated search-related tests.
  ([@zkat](https://github.com/zkat))
* [`3596de8`](https://github.com/npm/npm/commit/3596de88598c69eb5bae108703c8e74ca198b20c)
  [#13746](https://github.com/npm/npm/pull/13746)
  `JSONStream@1.2.1`
  ([@zkat](https://github.com/zkat))
* [`4b09209`](https://github.com/npm/npm/commit/4b09209bb605f547243065032a8b37772669745f)
  [#13746](https://github.com/npm/npm/pull/13746)
  `mississippi@1.2.0`
  ([@zkat](https://github.com/zkat))
* [`b650b39`](https://github.com/npm/npm/commit/b650b39d42654abb9eed1c7cd463b1c595ca2ef9)
  [#13746](https://github.com/npm/npm/pull/13746)
  `sorted-union-stream@2.1.3`
  ([@zkat](https://github.com/zkat))

#### SCRIPT NODE PATH (**BREAKING**)

Thanks to some great work by [@addaleax](https://github.com/addaleax), we've
addressed a fairly tricky issue involving the node process used by `npm
scripts`.

Previously, npm would prefix the path of the node executable to the script's
`PATH`. This had the benefit of making sure that the node process would be the
same for both npm and `scripts` unless you had something like
[`node-bin`](https://npm.im/node-bin) in your `node_modules`. And it turns out
lots of people relied on this behavior being this way!

It turns out that this had some unintended consequences: it broke systems like
[`nyc`](https://npm.im/nyc), but also completely broke/defeated things like
[`rvm`](https://rvm.io/) and
[`virtualenv`](https://virtualenv.pypa.io/en/stable/) by often causing things
that relied on them to fall back to the global system versions of ruby and
python.

In the face of two perfectly valid, and used alternatives, we decided that the
second case was much more surprising for users, and that we should err on the
side of doing what those users expect. Anna put some hard work in and managed to
put together a patch that changes npm's behavior such that we no longer prepend
the node executable's path *by default*, and adds a new option,
`--scripts-prepend-node-path`, to allow users who rely on this behavior to have
it add the node path for them.

This patch also makes it so this feature is discoverable by people who might run
into the first case above, by warning if the node executable is either missing
or shadowed by another one in `PATH`. This warning can also be disabled with the
`--scripts-prepend-node-path` option as needed.

* [`3fb1eb3`](https://github.com/npm/npm/commit/3fb1eb3e00b5daf37f14e437d2818e9b65a43392) [`6a7d375`](https://github.com/npm/npm/commit/6a7d375d779ba5416fd5df154c6da673dd745d9d) [`378ae08`](https://github.com/npm/npm/commit/378ae08851882d6d2bc9b631b16b8c875d0b9704)
  [#13409](https://github.com/npm/npm/pull/13409)
  Add a `--scripts-prepend-node-path` option to configure whether npm prepends
  the current node executable's path to `PATH`.
  ([@addaleax](https://github.com/addaleax))
* [`70b352c`](https://github.com/npm/npm/commit/70b352c6db41533b9a4bfaa9d91f7a2a1178f74e)
  [#13409](https://github.com/npm/npm/pull/13409)
  Change the default behaviour of npm to never prepending the current node
  executable’s directory to `PATH` but printing a warning in the cases in which
  it previously did.
  ([@addaleax](https://github.com/addaleax))

#### REMOVE `npat` (**BREAKING**)

Let's be real here -- almost no one knows this feature ever existed, and it's a
vestigial feature of the days when the ideal for npm was to distribute full
packages that could be directly developed on, even from the registry.

It turns out the npm community decided to go a different way: primarily
publishing packages in a production-ready format, with no tests, build tools,
etc. And so, we say goodbye to `npat`.

* [`e16c14a`](https://github.com/npm/npm/commit/e16c14afb6f52cb8b7adf60b2b26427f76773f2e)
  [#14329](https://github.com/npm/npm/pull/14329)
  Remove the npat feature.
  ([@iarna](https://github.com/iarna))

#### NEW `prepare` SCRIPT. `prepublish` DEPRECATED (**BREAKING**)

If there's anything that really seemed to confuse users, it's that the
`prepublish` script ran when invoking `npm install` without any arguments.

Turns out many, many people really expected that it would only run on `npm
publish`, even if it actually did what most people expected: prepare the package
for publishing on the registry.

And so, we've added a `prepare` command that runs in the exact same cases where
`prepublish` ran, and we've begun a deprecation cycle for `prepublish` itself
**only when run by `npm install`**, which will now include a warning any time
you use it that way.

We've also added a `prepublishOnly` script which will execute **only** when `npm
publish` is invoked. Eventually, `prepublish` will stop executing on `npm
install`, and `prepublishOnly` will be removed, leaving `prepare` and
`prepublish` as two distinct lifecycles.

* [`9b4a227`](https://github.com/npm/npm/commit/9b4a2278cee0a410a107c8ea4d11614731e0a943) [`bc32078`](https://github.com/npm/npm/commit/bc32078fa798acef0e036414cb448645f135b570)
  [#14290](https://github.com/npm/npm/pull/14290)
  Add `prepare` and `prepublishOnly` lifecyle events.
  ([@othiym23](https://github.com/othiym23))
* [`52fdefd`](https://github.com/npm/npm/commit/52fdefddb48f0c39c6e8eb4c118eb306c9436117)
  [#14290](https://github.com/npm/npm/pull/14290)
  Warn when running `prepublish` on `npm pack`.
  ([@othiym23](https://github.com/othiym23))
* [`4c2a948`](https://github.com/npm/npm/commit/4c2a9481b564cae3df3f4643766db4b987018a7b) [`a55bd65`](https://github.com/npm/npm/commit/a55bd651284552b93f7d972a2e944f65c1aa6c35)
  [#14290](https://github.com/npm/npm/pull/14290)
  Added `prepublish` warnings to `npm install`.
  ([@zkat](https://github.com/zkat))
* [`c27412b`](https://github.com/npm/npm/commit/c27412bb9fc7b09f7707c7d9ad23128959ae1abc)
  [#14290](https://github.com/npm/npm/pull/14290)
  Replace `prepublish` with `prepare` in `npm help package.json` documentation.
  ([@zkat](https://github.com/zkat))

#### NO MORE PARTIAL SHRINKWRAPS (**BREAKING**)

That's right. No more partial shrinkwraps. That means that if you have an
`npm-shrinkwrap.json` in your project, npm will no longer install anything that
isn't explicitly listed there, unless it's a `devDependency`. This will open
doors to some nice optimizations and make use of `npm shrinkwrap` just generally
smoother by removing some awful corner cases. We will also skip `devDependency`
installation from `package.json` if you added `devDependencies` to your
shrinkwrap by using `npm shrinkwrap --dev`.

* [`b7dfae8`](https://github.com/npm/npm/commit/b7dfae8fd4dc0456605f7a921d20a829afd50864)
  [#14327](https://github.com/npm/npm/pull/14327)
  Use `readShrinkwrap` to read top level shrinkwrap. There's no reason for npm
  to be doing its own bespoke heirloom-grade artisanal thing here.
  ([@iarna](https://github.com/iarna))
* [`0ae1f4b`](https://github.com/npm/npm/commit/0ae1f4b9d83af2d093974beb33f26d77fcc95bb9) [`4a54997`](https://github.com/npm/npm/commit/4a549970dc818d78b6de97728af08a1edb5ae7f0) [`f22a1ae`](https://github.com/npm/npm/commit/f22a1ae54b5d47f1a056a6e70868013ebaf66b79) [`3f61189`](https://github.com/npm/npm/commit/3f61189cb3843fee9f54288fefa95ade9cace066)
  [#14327](https://github.com/npm/npm/pull/14327)
  Treat shrinkwrap as canonical. That is, don't try to fill in for partial
  shrinkwraps. Partial shrinkwraps should produce partial installs. If your
  shrinkwrap contains NO `devDependencies` then we'll still try to install them
  from your `package.json` instead of assuming you NEVER want `devDependencies`.
  ([@iarna](https://github.com/iarna))

#### `npm tag` REMOVED (**BREAKING**)

* [`94255da`](https://github.com/npm/npm/commit/94255da8ffc2d9ed6a0434001a643c1ad82fa483)
  [#14328](https://github.com/npm/npm/pull/14328)
  Remove deprecated tag command. Folks must use the `dist-tag` command from now
  on.
  ([@iarna](https://github.com/iarna))

#### NON-ZERO EXIT CODE ON OUTDATED DEPENDENCIES (**BREAKING**)

* [`40a04d8`](https://github.com/npm/npm/commit/40a04d888d10a5952d5ca4080f2f5d2339d2038a) [`e2fa18d`](https://github.com/npm/npm/commit/e2fa18d9f7904eb048db7280b40787cb2cdf87b3) [`3ee3948`](https://github.com/npm/npm/commit/3ee39488b74c7d35fbb5c14295e33b5a77578104) [`3fa25d0`](https://github.com/npm/npm/commit/3fa25d02a8ff07c42c595f84ae4821bc9ee908df)
  [#14013](https://github.com/npm/npm/pull/14013)
  Do `exit 1` if any outdated dependencies are found by `npm outdated`.
  ([@watilde](https://github.com/watilde))
* [`c81838a`](https://github.com/npm/npm/commit/c81838ae96b253f4b1ac66af619317a3a9da418e)
  [#14013](https://github.com/npm/npm/pull/14013)
  Log non-zero exit codes at `verbose` level -- this isn't something command
  line tools tend to do. It's generally the shell's job to display, if at all.
  ([@zkat](https://github.com/zkat))

#### SEND EXTRA HEADERS TO REGISTRY

For the purposes of supporting shiny new registry features, we've started
sending `Npm-Scope` and `Npm-In-CI` headers in outgoing requests.

* [`846f61c`](https://github.com/npm/npm/commit/846f61c1dd4a033f77aa736ab01c27ae6724fe1c)
  [npm/npm-registry-client#145](https://github.com/npm/npm-registry-client/pull/145)
  [npm/npm-registry-client#147](https://github.com/npm/npm-registry-client/pull/147)
  `npm-registry-client@7.3.0`:
  * Allow npm to add headers to outgoing requests.
  * Add `Npm-In-CI` header that reports whether we're running in CI.
  ([@iarna](https://github.com/iarna))
* [`6b6bb08`](https://github.com/npm/npm/commit/6b6bb08af661221224a81df8adb0b72019ca3e11)
  [#14129](https://github.com/npm/npm/pull/14129)
  Send `Npm-Scope` header along with requests to registry. `Npm-Scope` is set to
  the `@scope` of the current top level project. This will allow registries to
  implement user/scope-aware features and services.
  ([@iarna](https://github.com/iarna))
* [`506de80`](https://github.com/npm/npm/commit/506de80dc0a0576ec2aab0ed8dc3eef3c1dabc23)
  [#14129](https://github.com/npm/npm/pull/14129)
  Add test to ensure `Npm-In-CI` header is being sent when CI is set in env.
  ([@iarna](https://github.com/iarna))

#### BUGFIXES

* [`bc84012`](https://github.com/npm/npm/commit/bc84012c2c615024b08868acbd8df53a7ca8d146)
  [#14117](https://github.com/npm/npm/pull/14117)
  Fixes a bug where installing a shrinkwrapped package would fail if the
  platform failed to install an optional dependency included in the shrinkwrap.
  ([@watilde](https://github.com/watilde))
* [`a40b32d`](https://github.com/npm/npm/commit/a40b32dc7fe18f007a672219a12d6fecef800f9d)
  [#13519](https://github.com/npm/npm/pull/13519)
  If a package has malformed metadata, `node.requiredBy` is sometimes missing.
  Stop crashing when that happens.
  ([@creationix](https://github.com/creationix))

#### OTHER PATCHES

* [`643dae2`](https://github.com/npm/npm/commit/643dae2197c56f1c725ecc6539786bf82962d0fe)
  [#14244](https://github.com/npm/npm/pull/14244)
  Remove some ancient aliases that we'd rather not have around.
  ([@zkat](https://github.com/zkat))
* [`bdeac3e`](https://github.com/npm/npm/commit/bdeac3e0fb226e4777d4be5cd3c3bec8231c8044)
  [#14230](https://github.com/npm/npm/pull/14230)
  Detect unsupported Node.js versions and warn about it. Also error on really
  old versions where we know we can't work.
  ([@iarna](https://github.com/iarna))

#### DOC UPDATES

* [`9ca18ad`](https://github.com/npm/npm/commit/9ca18ada7cc1c10b2d32bbb59d5a99dd1c743109)
  [#13746](https://github.com/npm/npm/pull/13746)
  Updated docs for `npm search` options.
  ([@zkat](https://github.com/zkat))
* [`e02a47f`](https://github.com/npm/npm/commit/e02a47f9698ff082488dc2b1738afabb0912793e)
  Move the `npm@3` changelog into the archived changelogs directory.
  ([@zkat](https://github.com/zkat))
* [`c12bbf8`](https://github.com/npm/npm/commit/c12bbf8c5a5dff24a191b66ac638f552bfb76601)
  [#14290](https://github.com/npm/npm/pull/14290)
  Document prepublish-on-install deprecation.
  ([@othiym23](https://github.com/othiym23))
* [`c246a75`](https://github.com/npm/npm/commit/c246a75ac8697f4ca11d316b7e7db5f24af7972b)
  [#14129](https://github.com/npm/npm/pull/14129)
  Document headers added by npm to outgoing registry requests.
  ([@iarna](https://github.com/iarna))

#### DEPENDENCIES

* [`cb20c73`](https://github.com/npm/npm/commit/cb20c7373a32daaccba2c1ad32d0b7e1fc01a681)
  [#13953](https://github.com/npm/npm/pull/13953)
  `signal-exit@3.0.1`
  ([@benjamincoe](https://github.com/benjamincoe))
