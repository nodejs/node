### v3.10.9 (2016-10-06)

Hi everyone! This is the last of our monthly releases. We're going to give
an every-two-weeks schedule a try starting with our next release. We'll
reevaluate in a quarter, but we suspect that will be what we'll stick with.
You might be wondering _why_ we've been fiddling with the release cadence? Well,
we've been trying to tune it to to minimize the overhead for our little team.

This is ALSO the ULTIMATE release of `npm` version 3. That's right, in
just two weeks' time (October 20th for you fans of calendar time), our dear
`npm` will be hitting the big 4.0.

**DON'T PANIC**

This is gonna be a much, MUCH smaller major version than 3.x was. Maybe even
smaller than 2.x was. I can't tell you everything that'll be in there just
yet, but at the very least it's going to have what's in our
[4.x milestone](https://github.com/npm/npm/pulls?q=is%3Aopen+is%3Apr+milestone%3A4.x),
PLUS, the first steps in
[making `prepublish` work](https://github.com/npm/npm/issues/10074) the way
people expect it to.

**NOW ABOUT THIS RELEASE**

This release sees a whole slew of bug fixes. Notably a bunch of lifecycle
fixes and a really important shrinkwrap fix.

#### LIFECYCLE FIXES

* [`d388f90`](https://github.com/npm/npm/commit/d388f90732981633b3cdb4fc7fb0fababd4e64ab)
  [#13942](https://github.com/npm/npm/pull/13942)
  Fix current working directory while running shrinkwrap lifecycle scripts.
  Previously if you ran a shrinkwrap from another lifecycle script AND
  `node_modules` existed (and if you're running `npm shrinkwrap` it probably
  should) then `npm` would run the shrinkwrap lifecycle from the
  `node_modules` folder instead of the package folder.
  ([@evocateur](https://github.com/evocateur))
  ([@iarna](https://github.com/iarna))
* [`c3b6cdf`](https://github.com/npm/npm/commit/c3b6cdfedcdb4d9e7712be5245d9b274828d88d1)
  [#13964](https://github.com/npm/npm/pull/13964)
  Fix bug where the `uninstall` lifecycles weren't being run when you
  reinstalled/updated an existing module.
  ([@iarna](https://github.com/iarna))
* [`72bb89c`](https://github.com/npm/npm/commit/72bb89c1aa9811a18cbd766f3da73da76eb920c6)
  [#13344](https://github.com/npm/npm/pull/13344)
  When running lifecycles use `TMPDIR` if it's writable and fall back to the
  current working directory if not. Previously we just assumed `TMPDIR`
  wouldn't be writable (as we might have been running as `nobody` and
  `nobody` on some systems can't write to `TMPDIR`).
  ([@aaronjensen](https://github.com/aaronjensen))

#### SHRINKWRAP GIT & TAGGED DEPENDENCY FIX

* [`3b5eee0`](https://github.com/npm/npm/commit/3b5eee0d31737d1c2518ed95dcc7aaaaa93c253c)
  [#13941](https://github.com/npm/npm/pull/13941)
  Fix git and tagged dependency matching with shrinkwraps.  Previously git
  and tag (ie `foo@latest`) dependencies installed from a shrinkwrap would
  always be flagged as invalid.
  ([@iarna](https://github.com/iarna))

#### BUG FIXES

* [`bf3bd1e`](https://github.com/npm/npm/commit/bf3bd1e4347ee2c5de08d23558c4444749178c8b)
  [#14143](https://github.com/npm/npm/pull/14143)
  Fix bug in `npm version` where `npm-shrinkwrap.json` wouldn't be updated
  if you ran `npm version` from outside of your project root.
  ([@lholmquist](https://github.com/lholmquist))
* [`1089878`](https://github.com/npm/npm/commit/1089878f58977559414c8a9addfc69a9c68905b0)
  [#13613](https://github.com/npm/npm/pull/13613)
  Log 'skipping action' as 'verbose' instead of 'warn'. This removes a lot of
  clutter when there are links in your `node_modules`. The long term plan is
  to entirely blind `npm` to what's inside links, which will make this code
  go away entirely.
  ([@timoxley](https://github.com/timoxley))
* [`952f1e1`](https://github.com/npm/npm/commit/952f1e109a070ab4066179f6104ba9394300e342)
  [#13999](https://github.com/npm/npm/pull/13999)
  Fix a bug where setting `bin` to `null` in your `package.json` would result
  in `npm` crashing.
  ([@IonicaBizau](https://github.com/IonicaBizau))
* [`fcf8b11`](https://github.com/npm/npm/commit/fcf8b11fb7fcf8902f6a887c3d5f0aef2897dde0)
  [#14032](https://github.com/npm/npm/pull/14032)
  When using `npm view`, if you specified a version that didn't exist it
  would previously print `undefined` (even if you asked for JSON output). It
  now prints nothing in this situation. This brings `npm@3`'s behavior in
  line with `npm@2`.
  ([@roblg](https://github.com/roblg))
* [`93c689f`](https://github.com/npm/npm/commit/93c689ff44c6042a2dcde7fe0d74d2264237d666)
  [#14032](https://github.com/npm/npm/pull/14032)
  When using `npm view --json` with a version range that matches multiple
  versions we now return a list of all of the metadata for all of those
  versions. Previously we picked one and only returned that. This brings
  `npm@3`'s behavior in line with `npm@2`.
  ([@roblg](https://github.com/roblg))
* [`2411728`](https://github.com/npm/npm/commit/24117289e09c373b845150c45e4793d98fe7cf4b)
  [#14045](https://github.com/npm/npm/pull/14045)
  Fix a Windows-only bug in the `git` tests. The tests had rather particular
  ideas about what arguments would be passed to `git` and on Windows they
  got this wrong.
  ([@watilde](https://github.com/watilde))

#### DOCUMENTATION & MISC

* [`30772cc`](https://github.com/npm/npm/commit/30772cc5f80923bf21c003fbe53e5fed9d3a5d97)
  [#13904](https://github.com/npm/npm/pull/13904)
  Update `package.json` example to include GitHub branches.
  ([@stevokk](https://github.com/stevokk))
* [`f66876f`](https://github.com/npm/npm/commit/f66876f75c204fb78028cf2ff7979f80355bd06c)
  [#14010](https://github.com/npm/npm/pull/14010)
  Update the GitHub issue template to reflect Apple's change in name of its
  desktop operating system.
  ([@AlexChesters](https://github.com/AlexChesters))

#### DEPENDENCY UPDATES

* [`b3f9bf1`](https://github.com/npm/npm/commit/b3f9bf1ada3f93e6775f5c232350030db6635d0c)
  [#13918](https://github.com/npm/npm/issues/13918)
  `graceful-fs@4.1.9`:
  Fix the _uid must be an unsigned int_ bug that's been around forever but that
  `npm` started tickling in v3.10.8.
  ([@addaleax](https://github.com/addaleax))
  Also fixes wrapper to `fs.readdir` to actually pass through (rather than
  drop) optional arguments.
  ([@isaacs](https://github.com/isaacs))
* [`9402ead`](https://github.com/npm/npm/commit/9402ead67e3be9b431ade637fbfac86204ee96fe)
  [isaacs/node-glob#293](https://github.com/isaacs/node-glob/pull/293)
  `glob@7.1.0`:
  Add `absolute` option for `match` event.
  ([@phated](https://github.com/phated))
* [`58b83db`](https://github.com/npm/npm/commit/58b83db327dd87bf7cb5a7d503303537718f2f30)
  `asap@2.0.5`
  ([@kriskowal](https://github.com/kriskowal))
* [`5707e6e`](https://github.com/npm/npm/commit/5707e6e55b220439c3f83e77daf4c70d72eb46f0)
  `sorted-object@2.0.1`
  ([@domenic](https://github.com/domenic))
* [`9d20910`](https://github.com/npm/npm/commit/9d209107ce49a7424c50459284280cd2e6e215d1)
  `request@2.75.0`
  ([@simov](https://github.com/simov))
* [`dea4848`](https://github.com/npm/npm/commit/dea48487a9d03492edc68670d05776d32d9ee8cf)
  `path-is-inside@1.0.2`
  ([@domenic](https://github.com/domenic))
* [`b3f3db5`](https://github.com/npm/npm/commit/b3f3db52e864d607b6d9b18920e2f58acc4b1616)
  `opener@1.4.2`
  ([@dominic](https://github.com/dominic))
* [`6bb5f95`](https://github.com/npm/npm/commit/6bb5f953888bbaaeeb624d623c2a9746d1c243a0)
  `lockfile@1.0.2`
  ([@isaacs](https://github.com/isaacs))
* [`13f7c0a`](https://github.com/npm/npm/commit/13f7c0a73212284b53a2d96882fc298afbf9609c)
  `config-chain@1.1.11`
  ([@dominictarr](https://github.com/dominictarr))

### v3.10.8 (2016-09-08)

Monthly releases are so big! Just look at all this stuff!

Our quarter of monthly releases is almost over. The next one, in October, might
very well be our last one as we move to trying something different and learning
lessons from our little experiment.

You may also want to keep an eye our for `npm@4` next month, since we're
planning on finally releasing it then and including a (small) number of breaking
changes we've been meaning to do for a long time. Don't worry, though: `npm@3`
will still be around for a bit and will keep getting better and better, and is
most likely going to be the version that `node@6` uses once it goes to LTS.

As some of us have mentioned before, npm is likely to start doing more regular
semver-major bumps, while keeping those bumps significantly smaller than the
huge effort that was `npm@3` -- we're not very likely to do a world-shaking
thing like that for a while, if ever.

All that said, let's move on to the patches included in v3.10.8!

#### SHRINKWRAP LEVEL UP

The most notable part of this release is a series of commits meant to make `npm
shrinkwrap` more consistent. By itself, shrinkwrap seems like a fairly
straightforward thing to implement, but things get complicated when it starts
interacting with `devDependencies`, `optionalDependencies`, and
`bundledDependencies`. These commits address some corner cases related to these.

* [`a7eca32`](https://github.com/npm/npm/commit/a7eca3246fbbcbb05434cb6677f65d14c945d74f)
  [#10073](https://github.com/npm/npm/pull/10073)
  Record if a dependency is only used as a devDependency and exclude it from the
  shrinkwrap file.
  ([@bengl](https://github.com/bengl))
* [`1eabcd1`](https://github.com/npm/npm/commit/1eabcd16bf2590364ca20831096350073539bf3a)
  [#10073](https://github.com/npm/npm/pull/10073)
  Record if a dependency is optional to shrinkwrap.
  ([@bengl](https://github.com/bengl))
* [`03efc89`](https://github.com/npm/npm/commit/03efc89522c99ee0fa37d8f4a99bc3b44255ef98)
  [#13692](https://github.com/npm/npm/pull/13692/)
  We were doing a weird thing where we used a `package.json` field `installable`
  to check to see if we'd checked for platform compatibility, and if not did
  so. But this was the only place that was ever done so there was no reason to
  implement it in such an obfuscated manner.
  Instead it now just directly checks and then records that its done so on the
  node object with `knownInstallable`. This is useful to know because modules
  expanded via shrinkwrap don't go through this– `inflateShrinkwrap` does not
  currently have any rollback semantics and so checking this sort of thing there
  is unhelpful.
  ([@iarna](https://github.com/iarna))
* [`ff87938`](https://github.com/npm/npm/commit/ff879382fda21dac7216a5f666287b3a7e74a947)
  [#11735](https://github.com/npm/npm/issues/11735)
  Running `npm install --save-dev` will now update shrinkwrap file, but only
  if there already are devDependencies in it.
  ([@szimek](https://github.com/szimek))
* [`c00ca3a`](https://github.com/npm/npm/commit/c00ca3aef836709eeaeade91c5305bc2fbda2e8a)
  [#13394](https://github.com/npm/npm/issues/13394)
  Check installability of modules from shrinkwrap, since modules that came into
  the tree vie shrinkwrap won't already have this information recorded in
  advance.
  ([@iarna](https://github.com/iarna))

#### INSTALLER ERROR REPORTING LEVEL UP

As part of the shrinkwrap push, there were also a lot of error-reporting
improvements. Some to add more detail to error objects, others to fix bugs and
inconsistencies.

* [`2cdd713`](https://github.com/npm/npm/commit/2cdd7132abddcc7f826a355c14348ce9a5897ffe)
  Consistently set code on `ETARGET` when fetching package metadata if no
  compatible version is found.
  ([@iarna](https://github.com/iarna))
* [`cabcd17`](https://github.com/npm/npm/commit/cabcd173f2923cb5b77e7be0e42eea2339a24727)
  [#13692](https://github.com/npm/npm/pull/13692/)
  Include installer warning details at the `verbose` log level.
  ([@iarna](https://github.com/iarna))
* [`95a4044`](https://github.com/npm/npm/commit/95a4044cbae93d19d0da0f3cd04ea8fa620295d9)
  [`dbb14c2`](https://github.com/npm/npm/commit/dbb14c241d982596f1cdaee251658f5716989fd2)
  [`9994383`](https://github.com/npm/npm/commit/9994383959798f80749093301ec43a8403566bb6)
  [`7417000`](https://github.com/npm/npm/commit/74170003db0c53def9b798cb6fe3fe7fc3e06482)
  [`f45f85d`](https://github.com/npm/npm/commit/f45f85dac800372d63dfa8653afccbf5bcae7295)
  [`e79cc1b`](https://github.com/npm/npm/commit/e79cc1b11440f0d122c4744d5eff98def9553f4a)
  [`146ee39`](https://github.com/npm/npm/commit/146ee394b1f7a33cf409a30b835a85d939acb438)
  [#13692](https://github.com/npm/npm/pull/13692/)
  Improve various bits of error reporting, adding more error information and
  some related refactoring.
  ([@iarna](https://github.com/iarna))

#### MISCELLANEOUS BUGS LEVEL UP

* [`116b6c6`](https://github.com/npm/npm/commit/116b6c60a174ea0cc49e4d62717e4e26175b6534)
  [#13456](https://github.com/npm/npm/issues/13456)
  In lifecycle scripts, any `node_modules/.bin` existing in the hierarchy
  should be turned into an entry in the PATH environment variable.
  However, prior to this commit, it was splitting based on the string
  `node_modules`, rather than restricting it to only path portions like
  `/node_modules/` or `\node_modules\`. So, a path containing an entry
  like `my_node_modules` would be improperly split.
  ([@isaacs](https://github.com/isaacs))
* [`0a28dd0`](https://github.com/npm/npm/commit/0a28dd0104e5b4a8cc0cb038bd213e6a50827fe8)
  [npm/fstream-npm#23](https://github.com/npm/fstream-npm/pull/23)
  `fstream-npm@1.2.0`:
  Always ignore `*.orig` files, which are generated by git when using `git
  mergetool`, by default.
  ([@zkat](https://github.com/zkat))
* [`a3a2fb9`](https://github.com/npm/npm/commit/a3a2fb97adc87c2aa9b2b8957861b30efafc7ad0)
  [#13708](https://github.com/npm/npm/pull/13708)
  Always ignore `*.orig` files, which are generated by git when using `git
  mergetool`, by default.
  ([@boneskull](https://github.com/boneskull))

#### TOOLING LEVEL UP

* [`e1d7e6c`](https://github.com/npm/npm/commit/e1d7e6ce551cbc42026cdcadcb37ea515059c972)
  Add helper for generating test skeletons.
  ([@iarna](https://github.com/iarna))
* [`4400b35`](https://github.com/npm/npm/commit/4400b356bca9175935edad1469c608c909bc01bf)
  Fix fixture creation and cleanup in `maketest`.
  ([@iarna](https://github.com/iarna))

#### DOCUMENTATION LEVEL UP

* [`8eb9460`](https://github.com/npm/npm/commit/8eb94601fe895b97cbcf8c6134e6b371c5371a1e)
  [#13717](https://github.com/npm/npm/pull/13717)
  Document that `npm link` will link the files specified in the `bin` field of
  `package.json` to `{prefix}/bin/{name}`.
  ([@legodude17](https://github.com/legodude17))
* [`a66e5e9`](https://github.com/npm/npm/commit/a66e5e9c388878fe03fb29014c3b95d28bedd3c1)
  [#13682](https://github.com/npm/npm/pull/13682)
  Minor grammar fix in documentation for `npm scripts`.
  ([@Ajedi32](https://github.com/Ajedi32))
* [`74b8043`](https://github.com/npm/npm/commit/74b80437ffdfcf8172f6ed4f39bfb021608dd9dd)
  [#13655](https://github.com/npm/npm/pull/13655)
  Document line comment syntax for `.npmrc`.
  ([@mdjasper](https://github.com/mdjasper))
* [`b352a84`](https://github.com/npm/npm/commit/b352a84c2c7ad15e9c669af75f65cdaa964f86c0)
  [#12438](https://github.com/npm/npm/issues/12438)
  Remind folks to use `#!/usr/bin/env node` in their `bin` scripts to make files
  executable directly.
  ([@mxstbr](https://github.com/mxstbr))
* [`b82fd83`](https://github.com/npm/npm/commit/b82fd838edbfff5d2833a62f6d8ae8ea2df5a1f2)
  [#13493](https://github.com/npm/npm/pull/13493)
  Document that the user config file can itself be configured either through the
  `$NPM_CONFIG_USERCONFIG` environment variable, or `--userconfig` command line
  flag.
  ([@jasonkarns](https://github.com/jasonkarns))
* [`8a02699`](https://github.com/npm/npm/commit/8a026992a03d90e563a97c70e90926862120693b)
  [#13911](https://github.com/npm/npm/pull/13911)
  Minor documentation reword and cleanup.
  ([@othiym23](https://github.com/othiym23))

#### DEPENDENCY LEVEL UP

* [`2818fb0`](https://github.com/npm/npm/commit/2818fb0f6081d68a91f0905945ad102f26c6cf85)
  `glob@7.0.6`
  ([@isaacs](https://github.com/isaacs))
* [`d88ec81`](https://github.com/npm/npm/commit/d88ec81ad33eb2268fcd517d35346a561bc59aff)
  `graceful-fs@4.1.6`
  ([@francescoinfante](https://github.com/francescoinfante))
* [`4727f86`](https://github.com/npm/npm/commit/4727f8646daca7b3e3c1c95860e02acf583b9dae)
  `lodash.clonedeep@4.5.0`
  ([@jdalton](https://github.com/jdalton))
* [`c347678`](https://github.com/npm/npm/commit/c3476780ef4483425e4ae1d095a5884b46b8db86)
  `lodash.union@4.6.0`
  ([@jdalton](https://github.com/jdalton))
* [`530bd4d`](https://github.com/npm/npm/commit/530bd4d2ae6f704f624e4f7bf64f911f37e2b7f8)
  `lodash.uniq@4.5.0`
  ([@jdalton](https://github.com/jdalton))
* [`483d56a`](https://github.com/npm/npm/commit/483d56ae8137eca0c0f7acd5d1c88ca6d5118a6a)
  `lodash.without@4.4.0`
  ([@jdalton](https://github.com/jdalton))
* [`6c934df`](https://github.com/npm/npm/commit/6c934df6e74bacd0ed40767b319936837a43b586)
  `inherits@2.0.3`
  ([@isaacs](https://github.com/isaacs))
* [`a65ed7c`](https://github.com/npm/npm/commit/a65ed7cbd3c950383a14461a4b2c87b67ef773b9)
  `npm-registry-client@7.2.1`:
  * [npm/npm-registry-client#142](https://github.com/npm/npm-registry-client/pull/142) Fix `EventEmitter` warning spam from error handlers on socket. ([@addaleax](https://github.com/addaleax))
  * [npm/npm-registry-client#131](https://github.com/npm/npm-registry-client/pull/131) Adds support for streaming request bodies. ([@aredridel](https://github.com/aredridel))
  * Fixes [#13656](https://github.com/npm/npm/issues/13656).
  * Dependency updates.
  * Documentation improvements.
  ([@othiym23](https://github.com/othiym23))
* [`2b88d62`](https://github.com/npm/npm/commit/2b88d62e6a730716b27052c0911c094d01830a60)
  [npm/npmlog#34](https://github.com/npm/npmlog/pull/34)
  `npmlog@4.0.0`:
  Allows creating log levels that are empty strings or 0
  ([@rwaldron](https://github.com/rwaldron))
* [`242babb`](https://github.com/npm/npm/commit/242babbd02274ee2d212ae143992c20f47ef0066)
  `once@1.4.0`
  ([@zkochan](https://github.com/zkochan))
* [`6d8ba2b`](https://github.com/npm/npm/commit/6d8ba2b4918e2295211130af68ee8a67099139e0)
  `readable-stream@2.1.5`
  ([@calvinmetcalf](https://github.com/calvinmetcalf))
* [`855c099`](https://github.com/npm/npm/commit/855c099482a8d93b7f0646bd7bcf8a31f81868e0)
  `retry@0.10.0`
  ([@tim-kos](https://github.com/tim-kos))
* [`80540c5`](https://github.com/npm/npm/commit/80540c52b252615ae8a6271b3df870eabfea935e)
  `semver@5.3.0`:
  * Add `minSatisfying`
  * Add `prerelease(v)`
  ([@isaacs](https://github.com/isaacs))
* [`8aaac52`](https://github.com/npm/npm/commit/8aaac52ffae8e689fae265712913b1e2a36b1aa6)
  `which@1.2.1`
  ([@isaacs](https://github.com/isaacs))
* [`85108a2`](https://github.com/npm/npm/commit/85108a29108ab0a57997572dc14f87eb706890ba)
  `write-file-atomic@1.2.0`:
  Preserve chmod and chown from the overwritten file
  ([@iarna](https://github.com/iarna))
* [`291a377`](https://github.com/npm/npm/commit/291a377f32f5073102a8ede61a27e6a9b37154c2)
  Update npm documentation to reflect documentation for `semver@5.3.0`.
  ([@zkat](https://github.com/zkat))

### v3.10.7 (2016-08-11)

Hi all, today's our first release coming out of the new monthly release
cadence. See below for details. We're all recovered from conferences now
and raring to go! We've got some pretty keen bug fixes and a bunch of
documentation and dependency updates. It's hard to narrow it down to just a
few, but of note are scoped packages in bundled dependencies, the
`preinstall` lifecycle fix, the shrinkwrap and Git dependencies fix and the
fix to a crasher involving cycles in development dependencies.

#### NEW RELEASE CADENCE

Releasing npm has been, for the most part, a very prominent part of our
weekly process process. As part of our efforts to find the most effective
ways to allocate our team's resources, we decided last month that we would
try and slow our releases down to a monthly cadence, and see if we found
ourselves with as much extra time and attention as we expected to have.
Process experiments are useful for finding more effective ways to do our
work, and we're at least going to keep doing this for a whole quarter, and
then measure how well it worked out. It's entirely likely that we'll switch
back to a more frequent cadence, specially if we find that the value that
weekly cadence was providing the community is not worth sacrificing for a
bit of extra time. Does this affect you significantly? Let us know!

#### SCOPED PACKAGES IN BUNDLED DEPENDENCIES

Prior to this release and
[v2.15.10](https://github.com/npm/npm/releases/v2.15.10), npm had ignored
scoped modules found in `bundleDependencies`.

* [`29cf56d`](https://github.com/npm/npm/commit/29cf56dbae8e3dd16c24876f998051623842116a)
  [#8614](https://github.com/npm/npm/issues/8614)
  Include scoped packages in bundled dependencies.
  ([@forivall](https://github.com/forivall))

#### `preinstall` LIFECYCLE IN CURRENT PROJECT

* [`b7f13bc`](https://github.com/npm/npm/commit/b7f13bc80b89b025be0c53d81b90ec8f2cebfab7)
  [#13259](https://github.com/npm/npm/pull/13259)
  Run top level preinstall before installing dependencies
  ([@palmerj3](https://github.com/palmerj3))

#### BETTER SHRINKWRAP WITH GIT DEPENDENCIES

* [`0f7e319`](https://github.com/npm/npm/commit/0f7e3197bcec7a328b603efdffd3681bbc40f585)
  [#12718](https://github.com/npm/npm/issues/12718.)
  Update outdated git dependencies found in shrinkwraps. Previously, if the
  module version was the same then no update would be completed even if the
  committish had changed.
  ([@kossnocorp](https://github.com/kossnocorp))


#### CYCLES IN DEVELOPMENT DEPENDENCIES NO LONGER CRASH

* [`1691de6`](https://github.com/npm/npm/commit/1691de668d34cd92ab3de08bf3a06085388f2f07)
  [#13327](https://github.com/npm/npm/issues/13327)
  Fix bug where cycles found in development dependencies could result in
  infinite recursion that resulted in crashes.
  ([@iarna](https://github.com/iarna))

#### IMPROVE "NOT UPDATING LINKED MODULE" WARNINGS

* [`1619871`](https://github.com/npm/npm/commit/1619871ac0cc8839dc9962c78e736095976c1eb4)
  [#12893](https://github.com/npm/npm/pull/12893)
  Only warn about symlink update if version number differs
  The update-linked action outputs a warning that it needs to update the
  linked package, but can't, There is no need for the package to be updated if
  it is already at the correct version.  This change does a check before
  logging the warning.
  ([@DaveEmmerson](https://github.com/DaveEmmerson))

#### MORE BUG FIXES

* [`8f8d1b3`](https://github.com/npm/npm/commit/8f8d1b33a78c79aff9de73df362abaa7f05751d2)
  [#11398](https://github.com/npm/npm/issues/11398)
  Fix bug where `package.json` files that contained a `type` property could
  cause crashes. `type` is not a `package.json` property that npm makes use
  of and having it should be (and now is) harmless.
  ([@zkat](https://github.com/zkat))
* [`e7fa6c6`](https://github.com/npm/npm/commit/e7fa6c6a2c1de2a214479daa8c6901eebb350381)
  [#13353](https://github.com/npm/npm/issues/13353)
  Add GIT_EXEC_PATH to Git environment whitelist.
  ([@mhart](https://github.com/mhart))
* [`c23af21`](https://github.com/npm/npm/commit/c23af21d4cedd7fedcb4168672044db76ad054a8)
  [#13626](https://github.com/npm/npm/pull/13626)
  Use HTTPS issues URL in the error message for type validation errors.
  ([@watilde](https://github.com/watilde))

#### INCLUDE `npm login` IN COMMAND SUMMARY

* [`ab0c4b1`](https://github.com/npm/npm/commit/ab0c4b137b05762e75e0913038b606f087b58aa0)
  [#13581](https://github.com/npm/npm/issues/13581)
  The `login` command has long been an alias for `adduser`.
  At the same time, there is an expectation not just of that
  particular word being something to look for, but of there being
  clear symmetry with `logout`.
  So it was a bit confusing when `login` didn't show up in
  `npm help` on a technicality. This seems like an acceptable
  exception to the rule that says "no aliases in `npm help`".
  ([@zkat](https://github.com/zkat))

#### DOCUMENTATION

* [`e2d7e78`](https://github.com/npm/npm/commit/e2d7e7820a7875ed96e0382dc1e91b8df4e83746)
  [#13319](https://github.com/npm/npm/pull/13319)
  As Node.js 0.8 is no longer supported, remove mention of it from the README.
  ([@watilde](https://github.com/watilde))
* [`c565d89`](https://github.com/npm/npm/commit/c565d893a38efb6006e841450503329c9e58f100)
  [#13349](https://github.com/npm/npm/pull/13349)
  Updated the scripts documentation to explain the different between `version` and `preversion`.
  ([@christophehurpeau](https://github.com/christophehurpeau))
* [`fa8f87f`](https://github.com/npm/npm/commit/fa8f87f1ec92e543dd975156c4b184eb3e0b80cb)
  [#10167](https://github.com/npm/npm/pull/10167)
  Clarify in scope documentation that npm@2 is required for scoped packages.
  ([@danpaz](https://github.com/danpaz))

#### DEPENDENCIES

* [`124427e`](https://github.com/npm/npm/commit/124427eabbfd200aa145114e389e19692559ff1e)
  [#8614](https://github.com/npm/npm/issues/8614)
  `fstream-npm@1.1.1`:
  Fixes bug with inclusion of scoped bundled dependencies.
  ([@forivall](https://github.com/forivall))
* [`7e0cdff`](https://github.com/npm/npm/commit/7e0cdff04714709f6dc056b19422d3f937502f1c)
  [#13497](https://github.com/npm/npm/pull/13497)
  `graceful-fs@4.1.5`:
  `graceful-fs` had a [bug fix](https://github.com/isaacs/node-graceful-fs/pull/71) which
  fixes a problem ([nodejs/node#7846](https://github.com/nodejs/node/pull/7846)) exposed
  by recent changes to Node.js.
  ([@thefourtheye](https://github.com/thefourtheye))
* [`9b88cb8`](https://github.com/npm/npm/commit/9b88cb89f138443f324094685f4de073f33ecef0)
  [#9984](https://github.com/npm/npm/issues/9984)
  `request@2.74.0`:
  Update request library to at least 2.73 to fix a bug where `npm install` would crash with
  _Cannot read property 'emit' of null._

  Update `request` dependency `tough-cookie` to `2.3.0` to
  to address [https://nodesecurity.io/advisories/130](https://nodesecurity.io/advisories/130).
  Versions 0.9.7 through 2.2.2 contain a vulnerable regular expression that,
  under certain conditions involving long strings of semicolons in the
  "Set-Cookie" header, causes the event loop to block for excessive amounts of
  time.
  ([@zarenner](https://github.com/zarenner))
  ([@stash-sfdc](https://github.com/stash-sfdc))
* [`bf78ce5`](https://github.com/npm/npm/commit/bf78ce5ef5d2d6e95177193cca5362dd27bff968)
  [#13387](https://github.com/npm/npm/issues/13387)
  `minimatch@3.0.3`:
  Handle extremely long and terrible patterns more gracefully.
  There were some magic numbers that assumed that every extglob pattern starts
  and ends with a specific number of characters in the regular expression.
  Since !(||) patterns are a little bit more complicated, this led to creating
  an invalid regular expression and throwing.
  ([@isaacs](https://github.com/isaacs))
* [`803e538`](https://github.com/npm/npm/commit/803e538efaae4b56a764029742adcf6761e8398b)
  [isaacs/rimraf#111](https://github.com/isaacs/rimraf/issues/111)
  `rimraf@2.5.4`: Clarify assertions: cb is required, options are not.
  ([@isaacs](https://github.com/isaacs))
* [`a9f84ef`](https://github.com/npm/npm/commit/a9f84ef61b4c719b646bf9cda00577ef16e3a113)
  `lodash.without@4.2.0`
  ([@jdalton](https://github.com/jdalton))
* [`f59ff1c`](https://github.com/npm/npm/commit/f59ff1c2701f1bfd21bfdb97b4571823b614f694)
  `lodash.uniq@4.4.0`
  ([@jdalton](https://github.com/jdalton))
* [`8cc027e`](https://github.com/npm/npm/commit/8cc027e5e81623260a49b31fe406ce483258b203)
  `lodash.union@4.5.0`
  ([@jdalton](https://github.com/jdalton))
* [`0a6c1e4`](https://github.com/npm/npm/commit/0a6c1e4302a153fb055f495043ed33afd8324193)
  `lodash.without@4.3.0`
  ([@jdalton](https://github.com/jdalton))
* [`4ab0181`](https://github.com/npm/npm/commit/4ab0181fca2eda18888b865ef691b83d30fb0c33)
  `lodash.clonedeep@4.4.1`
  ([@jdalton](https://github.com/jdalton))

### v3.10.6 (2016-07-07)

This week we have a bunch of bug fixes for ya!  A shrinkwrap regression
introduced in 3.10.0, better lifecycle `PATH` behavior, improvements when
working with registries other than `registry.npmjs.org` and a fix for
hopefully the last _don't print a progress bar over my interactive thingy_
bug.

#### SHRINKWRAP AND DEV DEPENDENCIES

The rewrite in 3.10.0 triggered a bug where dependencies of devDependencies
would be included in your shrinkwrap even if you didn't request
devDependencies.

* [`2484529`](https://github.com/npm/npm/commit/2484529ab56a42e5d6f13c48006f39a596d9e327)
  [#13308](https://github.com/npm/npm/pull/13308)
  Fix bug where deps of devDependencies would be incorrectly included in
  shrinkwraps.
  ([@iarna](https://github.com/iarna))

#### BETTER PATH LIFECYCLE BEHAVIOR

We've been around the details on this one a few times in recent months and
hopefully this will bring is to where we want to be.

* [`81051a9`](https://github.com/npm/npm/commit/81051a90eee66a843f76eb8cccedbb1d0a5c1f47)
  [#12968](https://github.com/npm/npm/pull/12968)
  When running lifecycle scripts, only prepend directory containing the node
  binary to PATH if not already in PATH.
  ([@segrey](https://github.com/segrey))

#### BETTER INTERACTIONS WITH THIRD PARTY REGISTRIES

* [`071193c`](https://github.com/npm/npm/commit/071193c8e193767dd1656cb27556cb3751d77a3b)
  [#10869](https://github.com/npm/npm/pull/10869)
  If the registry returns a list of versions some of which are invalid, skip
  those when picking a version to install.  This can't happen with
  registry.npmjs.org as it will normalize versions published with it, but it
  can happen with other registries.
  ([@gregersrygg](https://github.com/gregersrygg))

#### ONE LAST TOO-MUCH-PROGRESS CORNER

* [`1244cc1`](https://github.com/npm/npm/commit/1244cc16dc5a0536acf26816a1deeb8e221d67eb)
  [#13305](https://github.com/npm/npm/pull/13305)
  Disable progress bar in `npm edit` and `npm config edit`.
  ([@watilde](https://github.com/watilde))

#### HTML DOCS IMPROVEMENTS

* [`58da923`](https://github.com/npm/npm/commit/58da9234ae72a5474b997f890a1155ee9785e6f1)
  [#13225](https://github.com/npm/npm/issues/13225)
  Fix HTML character set declaration in generated HTML documentation.
  ([@KenanY](https://github.com/KenanY))
* [`d1f0bf4`](https://github.com/npm/npm/commit/d1f0bf4303566f8690502034f82bbb449850958d)
  [#13250](https://github.com/npm/npm/pull/13250)
  Optimize png images using zopflipng.
  ([@PeterDaveHello](https://github.com/PeterDaveHello))

#### DEPENDENCY UPDATES (THAT MATTER)

* [`c7567e5`](https://github.com/npm/npm/commit/c7567e58618b63f97884afa104d2f560c9272dd5)
  [npm/npm-user-validate#9](https://github.com/npm/npm-user-validate/pull/9)
  `npm-user-validate@0.1.5`:
  Lower the username length limits to 214 from 576 to match `registry.npmjs.org`'s limits.
  ([@aredridel](https://github.com/aredridel))
* [`22802c9`](https://github.com/npm/npm/commit/22802c9db3cf990c905e8f61304db9b5571d7964)
  [#isaacs/rimraf](https://github.com/npm/npm/issues/isaacs/rimraf)
  `rimraf@2.5.3`:
  Fixes EPERM errors when running `lstat` on read-only directories.
  ([@isaacs](https://github.com/isaacs))
* [`ce6406f`](https://github.com/npm/npm/commit/ce6406f4b6c4dffbb5cd8a3c049f6663a5665522)
  `glob@7.0.5`:
  Forces the use of `minimatch` to 3.0.2, which improved handling of long and
  complicated patterns.
  ([@isaacs](https://github.com/isaacs))


### v3.10.5 (2016-07-05)

This is a fix to this week's testing release to correct the update of
`node-gyp` which somehow got mangled.

* [`ca97ce2`](https://github.com/npm/npm/commit/ca97ce2e8d8ba44c445b39ffa40daf397d5601b3)
  [#13256](https://github.com/npm/npm/issues/13256)
  Fresh reinstall of `node-gyp@3.4.0`.
  ([@zkat](https://github.com/zkat))

### v3.10.4 (2016-06-30)

Hey y'all! This release includes a bunch of fixes we've been working on as we
continue on our `big-bug` push. There's still [a lot of it left to
do](https://github.com/npm/npm/labels/big-bug), but once this is done, things
should just generally be more stable, installs should be more reliable and
correct, and we'll be able to move on to more future work. We'll keep doing our
best! 🙌

#### RACES AS WACKY AS [REDLINE](https://en.wikipedia.org/wiki/Redline_\(2009_film\))

Races are notoriously hard to squash, and tend to be some of the more common
recurring bugs we see on the CLI. [@julianduque](https://github.com/julianduque)
did some pretty awesome [sleuthing
work](https://github.com/npm/npm/issues/12669) to track down a cache race and
helpfully submitted a patch. There were some related races in the same area that
also got fixed at around the same time, mostly affecting Windows users.

* [`2a37c97`](https://github.com/npm/npm/commit/2a37c97121483db2b6f817fe85c2a5a77b76080e)
  [#12669](https://github.com/npm/npm/issues/12669)
  [#13023](https://github.com/npm/npm/pull/13023)
  The CLI is pretty aggressive about correcting permissions across the cache
  whenever it writes to it. This aggressiveness caused a couple of races where
  temporary cache files would get picked up by `fs.readdir`, and removed before
  `chownr` was called on them, causing `ENOENT` errors. While the solution might
  seem a bit hamfisted, it's actually perfectly safe and appropriate in this
  case to just ignore those resulting `ENOENT` errors.
  ([@julianduque](https://github.com/julianduque))
* [`ea018b9`](https://github.com/npm/npm/commit/ea018b9e3856d1798d199ae3ebce4ed07eea511b)
  [#13023](https://github.com/npm/npm/pull/13023)
  If a user were to have SUDO_UID and SUDO_GID, they'd be able to get into a
  pretty weird state. This fixes that corner case.
  ([@zkat](https://github.com/zkat))
* [`703ca3a`](https://github.com/npm/npm/commit/703ca3abbf4f1cb4dff08be32acd2142d5493482)
  [#13023](https://github.com/npm/npm/pull/13023)
  A missing `return` was causing `chownr` to be called on Windows, even though
  that's literally pointless, and causing crashes in the process, instead of
  short-circuiting. This was entirely dependent on which callback happened to be
  called first, and in some cases, the failing one would win the race. This
  should prevent this from happening in the future.
  ([@zkat](https://github.com/zkat))
* [`69267f4`](https://github.com/npm/npm/commit/69267f4fbd1467ce576f173909ced361f8fe2a9d)
  [#13023](https://github.com/npm/npm/pull/13023)
  Added tests to verify `correct-mkdir` race patch.
  ([@zkat](https://github.com/zkat))
* [`e5f50ea`](https://github.com/npm/npm/commit/e5f50ea9f84fe8cac6978d18f7efdf43834928e7)
  [#13023](https://github.com/npm/npm/pull/13023)
  Added tests to verify `addLocal` race patch.
  ([@zkat](https://github.com/zkat))

#### SHRINKWRAP IS COMPLICATED BUT IT'S BETTER NOW

[@iarna](https://github.com/iarna) did some heroic hacking to refactor a bunch
of `shrinkwrap`-related bits and fixed some resolution and pathing issues that
were biting users. The code around that stuff got more readable/maintainable in
the process, too!

* [`346bba1`](https://github.com/npm/npm/commit/346bba1e1fee9cc814b07c56f598a73be5c21686)
  [#13214](https://github.com/npm/npm/pull/13214)
  Resolve local dependencies in `npm-shrinkwrap.json` relative to the top of the
  tree.
  ([@iarna](https://github.com/iarna))
* [`4a67fdb`](https://github.com/npm/npm/commit/4a67fdbd0f160deb6644a9c4c5b587357db04d2d)
  [#13213](https://github.com/npm/npm/pull/13213)
  If you run `npm install modulename` it should, if a `npm-shrinkwrap.json` is
  present, use the version found there.  If not, it'll use the version found in
  your `package.json`, and failing *that*, use `latest`.
  This fixes a case where the first check was being bypassed because version
  resolution was being done prior to loading the shrinkwrap, and so checks to
  match the shrinkwrap version couldn't succeed.
  ([@iarna](https://github.com/iarna))
* [`afa2133`](https://github.com/npm/npm/commit/afa2133a5d8ac4f6f44cdc6083d89ad7f946f5bb)
  [#13214](https://github.com/npm/npm/pull/13214)
  Refactor shrinkwrap specifier lookup into shared function.
  ([@iarna](https://github.com/iarna))
* [`2820b56`](https://github.com/npm/npm/commit/2820b56a43e1cc1e12079a4c886f6c14fe8c4f10)
  [#13214](https://github.com/npm/npm/pull/13214)
  Refactor operations in `inflate-shrinkwrap.js` into separate functions for
  added clarity.
  ([@iarna](https://github.com/iarna))
* [`ee5bfb3`](https://github.com/npm/npm/commit/ee5bfb3e56ee7ae582bec9f741f32b224c279947)
  Fix Windows path issue in a shrinkwrap test.
  ([@zkat](https://github.com/zkat))

#### OTHER BUGFIXES

* [`a11a7b2`](https://github.com/npm/npm/commit/a11a7b2e7df9478ac9101b06eead4a74c41a648d)
  [#13212](https://github.com/npm/npm/pull/13212)
  Resolve local paths passed in through the command line relative to current
  directory, instead of relative to the `package.json`.
  ([@iarna](https://github.com/iarna))

#### DEPENDENCY UPDATES

* [`900a5b7`](https://github.com/npm/npm/commit/900a5b7f18b277786397faac05853c030263feb8)
  [#13199](https://github.com/npm/npm/pull/13199)
  [`node-gyp@3.4.0`](https://github.com/nodejs/node-gyp/blob/master/CHANGELOG.md):
  AIX, Visual Studio 2015, and logging improvements. Oh my~!
  ([@rvagg](https://github.com/rvagg))

#### DOCUMENTATION FIXES

* [`c6942a7`](https://github.com/npm/npm/commit/c6942a7d6acb2b8c73206353bbec03380a056af4)
  [#13134](https://github.com/npm/npm/pull/13134)
  Fixed a few typos in `CHANGELOG.md`.
  ([@watilde](https://github.com/watilde))
* [`e63d913`](https://github.com/npm/npm/commit/e63d913127731ece56dcd69c7c0182af21be58f8)
  [#13156](https://github.com/npm/npm/pull/13156)
  Fix old reference to `doc/install` in a source comment.
  ([@sheerun](https://github.com/sheerun))
* [`099d23c`](https://github.com/npm/npm/commit/099d23cc8f38b524dc19a25857b2ebeca13c49d6)
  [#13113](https://github.com/npm/npm/issues/13113)
  [#13189](https://github.com/npm/npm/pull/13189)
  Fixes a link to `npm-tag(3)` that was breaking to instead point to
  `npm-dist-tag(1)`, as reported by [@SimenB](https://github.com/SimenB)
  ([@macdonst](https://github.com/macdonst))

### v3.10.3 (2016-06-23)

Given that we had not one, but two updates to our RC this past week, it
should come as no surprise that this week's full release is a bit
lighter. We have some documentation patches and a couple of bug fixes via
dependency updates.

If you haven't yet checked out last week's release,
[v3.10.0](https://github.com/npm/npm/releases/tag/v3.10.0)
and the two follow up releases
[v3.10.1](https://github.com/npm/npm/releases/tag/v3.10.1)
and
[v3.10.2](https://github.com/npm/npm/releases/tag/v3.10.2),
you really should do so. They're the most important releases we've had in
quite a while, fixing a bunch of critical bugs (including an issue
impacting publishing with Node.js 6.x) and of course, bringing in the new
and improved progress bar.

#### BUM SYMLINKS BURN NO MORE

There's been a bug lurking where broken symlinks in your `node_modules`
folder could cause all manner of mischief, from crashes to empty `npm ls`
results. The intrepid [@watilde](https://github.com/watilde) tracked this
down for us.

This addresses the root cause of the outdated crasher we protected
against earlier this week in
[#13115](https://github.com/npm/npm/issues/13115).

This also fixes [#9564](https://github.com/npm/npm/issues/9564), the
problem where a bad symlink in your global modules would result in an
empty result when you ran `npm ls -g`.

This ALSO likely fixes numerous "Missing argument #1" errors. (But surely
not all of them as that's actually just a generic arity and
type-validation failure.)

* [`ca92ac4`](https://github.com/npm/npm/commit/ca92ac455b841a708dd89262ff88d503b125d717)
  [npm/read-package-tree#6](https://github.com/npm/read-package-tree/pull/6)
  `read-package-tree@5.1.5`:
  Make bad symlinks be non-fatal errors when reading the tree off disk.
  ([@watilde](https://github.com/watilde))

#### BETTER UNICODE DETECTION

* [`6c3f7f0`](https://github.com/npm/npm/commit/6c3f7f043f09fc2aa19ffd3f956787635fa6f4d0)
  `has-unicode@2.0.1`:
  Fix unicode detection on a number of Linux distributions.
  ([@Darkhogg](https://github.com/Darkhogg)) ([@gagern](https://github.com/gagern))


#### DOCUMENTATION FIXES

* [`b9243ee`](https://github.com/npm/npm/commit/b9243ee60a3d60505c2502dc8633811b42c8aaea)
  [#13127](https://github.com/npm/npm/pull/13127)
  Remove extra backtick from `npm ls` documentation.
  ([@shvaikalesh](https://github.com/shvaikalesh))
* [`e05c0c2`](https://github.com/npm/npm/commit/e05c0c243cc702f9c392c001f668a90b57eaeb0e)
  [iarna/has-unicode#3](https://github.com/iarna/has-unicode/pull/3)
  [iarna/has-unicode#4](https://github.com/iarna/has-unicode/pull/4)
  [#13084](https://github.com/npm/npm/pull/13084)
  Correct changelog entry for shrinkwrap lifecycle order.
  ([@SimenB](https://github.com/SimenB))
* [`823994f`](https://github.com/npm/npm/commit/823994f100a0e59e1dd109e312811f971968ec75)
  [#13080](https://github.com/npm/npm/pull/13080)
  Describe using `npm pack` to see a dry run of publication results in
  the `npm publish` documentation.
  ([@laughinghan](https://github.com/laughinghan))

#### DEPENDENCY UPDATES

* [`e44d2db`](https://github.com/npm/npm/commit/e44d2db1ad0d860ca08e99c81135bd399fb733b1)
  `aproba@1.0.4`: Documentation updates and minor refactoring.
  ([@iarna](https://github.com/iarna))

### v3.10.2 (2016-06-17):

This is a quick hotfix release with two small bug fixes.  First, there was
an issue where the new progress bar would overwrite interactive prompts,
that is, those found in `npm login` and `npm init`.  Second, if the
directory you were running `npm outdated` on was a bad link or otherwise had
unrecoverable errors then npm would crash instead of printing the error.

* [`fbefb86`](https://github.com/npm/npm/commit/fbefb8675b26320b295f481b4872ce99f0180807)
  [`7779e9f`](https://github.com/npm/npm/commit/7779e9fb9430f6547532c67f2471864d62bbd5bc)
  [#13105](https://github.com/npm/npm/issues/13105)
  Disable progress bar in `adduser` and `init`.
* [`6a33b2c`](https://github.com/npm/npm/commit/6a33b2c13f637a41e25cd0339925bc430b50358a)
  [#13115](https://github.com/npm/npm/issues/13115)
  Ensure that errors reading the package tree for `outdated` does not result
  in crashes.
  ([@iarna](https://github.com/iarna))

### v3.10.1 (2016-06-17):

There are two very important bug fixes and one long-awaited (and significant!)
deprecation in this hotfix release. [Hold on.](http://butt.holdings/)

#### *WHOA*

When Node.js 6.0.0 was released, the CLI team noticed an alarming upsurge in
bugs related to important files (like `README.md`) not being included in
published packages. The new bugs looked much like
[#5082](https://github.com/npm/npm/issues/5082), which had been around in one
form or another since April, 2014. #5082 used to be a very rare (and obnoxious)
bug that the CLI team hadn't had much luck reproducing, and we'd basically
marked it down as a race condition that arose on machines using slow and / or
rotating-media-based hard drives.

Under 6.0.0, the behavior was reliable enough to be nearly deterministic, and
made it very difficult for publishers using `.npmignore` files in combination
with `"files"` stanzas in `package.json` to get their packages onto the
registry without one or more files missing from the packed tarball. The entire
saga is contained within [the issue](https://github.com/npm/npm/issues/5082),
but the summary is that an improvement to the performance of
[`fs.realpath()`](https://nodejs.org/api/fs.html#fs_fs_realpath_path_options_callback)
made it much more likely that the packing code would lose the race.

Fixing this has proven to be very difficult, in part because the code used by
npm to produce package tarballs is more complicated than, strictly speaking, it
needs to be. [**@evanlucas**](https://github.com/evanlucas) contributed [a
patch](https://github.com/npm/fstream/pull/50) that passed the tests in a
[special test suite](https://github.com/othiym23/eliminate-5082) that I
([**@othiym23**](https://github.com/othiym23)) created (with help from
[**@addaleax**](https://github.com/addaleax)), but only _after_ we'd released
the fixed version of that package did we learn that it actually made the
problem _worse_ in other situations in npm proper. Eventually,
[**@rvagg**](https://github.com/rvagg) put together a more durable fix that
appears to completely address the errant behavior under Node.js 6.0.0. That's
the patch included in this release. Everybody should chip in for redback
insurance for Rod and his family; he's done the community a huge favor.

Does this mean the long (2+ year) saga of #5082 is now over? At this point, I'm
going to quote from my latest summary on the issue:

> The CLI team (mostly me, with input from the rest of the team) has decided that
> the overall complexity of the interaction between `fstream`, `fstream-ignore`,
> `fstream-npm`, and `node-tar` has grown more convoluted than the team is
> comfortable (maybe even capable of) supporting.
>
> - While I believe that @rvagg's (very targeted) fix addresses _this_ issue, I
>   would be shocked if there aren't other race conditions in npm's packing
>   logic. I've already identified a couple other places in the code that are
>   most likely race conditions, even if they're harder to trigger than the
>   current one.
> - The way that dependency bundling is integrated leads to a situation in
>   which a bunch of logic is duplicated between `fstream-npm` and
>   `lib/utils/tar.js` in npm itself, and the way `fstream`'s extension
>   mechanism works makes this difficult to clean up. This caused a nasty
>   regression ([#13088](https://github.com/npm/fstream/pull/50), see below) as
>   of ~`npm@3.8.7` where the dependencies of `bundledDependencies` were no
>   longer being included in the built package tarballs.
> - The interaction between `.npmignore`, `.gitignore`, and `files` is hopelessly
>   complicated, scattered in many places throughout the code. We've been
>   discussing [making the ignores and includes logic clearer and more
>   predictable](https://github.com/npm/npm/wiki/Files-and-Ignores), and the
>   current code fights our efforts to clean that up.
>
> So, our intention is still to replace `fstream`, `fstream-ignore`, and
> `fstream-npm` with something much simpler and purpose-built. There's no real
> reason to have a stream abstraction here when a simple recursive-descent
> filesystem visitor and a synchronous function that can answer whether a given
> path should be included in the packed tarball would do the job adequately.
>
> What's not yet clear is whether we'll need to replace `node-tar` in the
> process. `node-tar` is a very robust implementation of tar (it handles, like,
> everything), and it also includes some very important tweaks to prevent several
> classes of security exploits involving maliciously crafted packages. However,
> its packing API involves passing in an `fstream` instance, so we'd either need
> to produce something that follows enough of `fstream`'s contract for `node-tar`
> to keep working, or swap `node-tar` out for something like `tar-stream` (and
> then ensuring that our use of `tar-stream` is secure, which could involve
> security patches for either npm or `tar-stream`).

The testing and review of `fstream@1.0.10` that the team has done leads us to
believe that this bug is fixed, but I'm feeling more than a little paranoid
about fstream now, so it's important that people keep a close eye on their
publishes for a while and let us know immediately if they notice any
irregularities.

* [`8802f6c`](https://github.com/npm/npm/commit/8802f6c152ea35cb9e5269c077c3a2f9df411afc)
  [#5082](https://github.com/npm/npm/issues/5082) `fstream@1.0.10`: Ensure that
  entries are collected after a paused stream resumes.
  ([@rvagg](https://github.com/rvagg))
* [`c189723`](https://github.com/npm/npm/commit/c189723110497a17dac3b0596f2916deeed93ee7)
  [#5082](https://github.com/npm/npm/issues/5082) Remove the warning introduced
  in `npm@3.10.0`, because it should no longer be necessary.
  ([@othiym23](https://github.com/othiym23))

#### *ERK*

Because the interaction between `fstream`, `fstream-ignore`, `fsream-npm`, and
`node-tar` is so complex, it's proven difficult to add support for npm features
like `bundledDependencies` without duplicating some logic within npm's code
base. While [fixing a completely unrelated
bug](https://github.com/npm/npm/issues/9642), we "cleaned up" some of this
seemingly duplicated code, and in the process removed the code that ensured
that the dependencies of `bundledDependencies` are themselves bundled. We've
brought that code back into the code base (without reopening #9642), and added
a test to ensure that this regression can't recur.

* [`1b6ceca`](https://github.com/npm/npm/commit/1b6ceca32fc81ca7cc7ac2eb7d11f687e6f87f26)
  [#13088](https://github.com/npm/npm/issues/13088) Partially restore npm's own
  version of the `fstream-npm` function `applyIgnores` to ensure that the
  dependencies of `bundledDependencies` are included in published packages.
  ([@iarna](https://github.com/iarna))

#### GOODBYE, FAITHFUL FRIEND

At NodeConf Adventure 2016 (RIP in peace, Mikeal Rogers's NodeConf!), the CLI
team had an opportunity to talk to representatives from some of the larger
companies that we knew were still using Node.js 0.8 in production. After asking
them whether they were still using 0.8, we got back blank stares and questions
like, "0.8? You mean, from four years ago?" After establishing that being able
to run npm in their legacy environments was no longer necessary, the CLI team
made the decision to drop support for 0.8. (Faithful observers of our [team
meetings](https://github.com/npm/npm/issues?utf8=%E2%9C%93&q=is%3Aissue+npm+cli+team+meeting+)
will have known this was the plan for NodeConf since the beginning of 2016.)

In practice, this means only what's in the commit below: we've removed 0.8 from
our continuous integration test matrix below, and will no longer be habitually
testing changes under Node 0.8.  We may also give ourselves permission to use
`setImmediate()` in test code. However, since the project still supports
Node.js 0.10 and 0.12, it's unlikely that patches that rely on ES 2015
functionality will land anytime soon.

Looking forward, the team's current plan is to drop support for Node.js 0.10
when its LTS maintenance window expires in October, 2016, and 0.12 when its
maintenance / LTS window ends at the end of 2016. We will also drop support for
Node.js 5.x when Node.js 6 becomes LTS and Node.js 7 is released, also in the
October-December 2016 timeframe.

(Confused about Node.js's LTS policy? [Don't
be!](https://github.com/nodejs/LTS) If you look at [this
diagram](https://github.com/nodejs/LTS/blob/ce364a94b0e0619eba570cd57be396573e1ef889/schedule.png),
it should make all of the preceding clear.)

If, in practice, this doesn't work with distribution packagers or other
community stakeholders responsible for packaging and distributing Node.js and
npm, please reach out to us. Aligning the npm CLI's LTS policy with Node's
helps everybody minimize the amount of work they need to do, and since all of
our teams are small and very busy, this is somewhere between a necessity and
non-negotiable.

* [`d6afd5f`](https://github.com/npm/npm/commit/d6afd5ffb1b19e5d94aeee666afcb8adaced58db)
  Remove 0.8 from the Node.js testing matrix, and reorder to match real-world
  priority, with comments. ([@othiym23](https://github.com/othiym23))

### v3.10.0 (2016-06-16):

Do we have a release for you!  We have our first new lifecycle since
`version`, a new progress bar and a bunch of bug fixes.
[I'm](https://github.com/iarna) really excited about this release, let me
tell you!!

#### DANGER: PUBLISHING ON NODE 6.0.0

Publishing and packing are buggy under Node versions greater than 6.0.0.
Please use Node.js LTS (4.4.x) to publish packages.  See
[#5082](https://github.com/npm/npm/issues/5082) for details and current
status.

* [`4e52cef`](https://github.com/npm/npm/commit/4e52cef3d4170c8abab98149666ec599f8363233)
  [#13077](https://github.com/npm/npm/pull/13077)
  Warn when using Node 6+.
  ([@othiym23](https://github.com/othiym23))

#### NEW LIFECYCLE SCRIPT: `shrinkwrap`

* [`e8c80f2`](https://github.com/npm/npm/commit/e8c80f20bfd5d1618e85dbab41660d6f3e5ce405)
  [#10744](https://github.com/npm/npm/issues/10744)
  You can now add `preshrinkwrap`, `shrinkwrap` and `postshrinkwrap` to your `package.json`
  scripts section. They are run when you run `npm shrinkwrap` or `npm install --save` with
  an `npm-shrinkwrap.json` present in your module directory.

  `preshrinkwrap` and `shrinkwrap` is run prior to generating the new `npm-shrinkwrap.json`
  and `postshrinkwrap` is run after.
  ([@SimenB](https://github.com/SimenB))

#### NEW PROGRESS BAR

![Install with new progress bar](http://shared.by.re-becca.org/misc-images/new-gauge-color.gif)

We have a new progress bar and a bunch of related improvements!

##### BLOCKING BLOCKING

**!!WARNING!!** As a part of this change we now explicitly set
`process.stdout` and `process.stderr` to be _blocking_ if they are ttys,
using [set-blocking](https://www.npmjs.com/package/set-blocking). This is
necessary to ensure that we can fully erase the progress bar before we start
writing other things out to the console.

Prior to Node.js 6.0.0, they were already blocking on Windows, and MacOS.
Meanwhile, on Linux they were always non-blocking but had large (64kb)
buffers, which largely made this a non-issue there.  Starting with Node.js
6.0.0 they became non-blocking on MacOS and that caused some unexpected
issues (see [nodejs/node#6456](https://github.com/nodejs/node/issues/6456)).

If you are a Linux user, it's plausible that this might have a performance
impact if your terminal can't keep up with output rate.  If you experience
this, we want to know! Please [file an
issue](https://github.com/npm/npm/issues/new) at our issue tracker.

##### BETTER LAYOUT

Let's start by talking about what goes into the new progress bar:

```
⸨░░░░░░░░░░⠂⠂⠂⠂⠂⠂⠂⠂⸩ ⠹ loadExtraneous: verb afterAdd /Users/rebecca/.npm/null/0.0.0/package/package.json written
 ↑‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾  ↑ ‾‾‾‾‾‾‾‾‾↑‾‾‾‾   ‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾↑‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾
 percent complete     spinner    current thing we're doing     most recent log line
```

The _spinner_ is intended as an activity indicator–it moves whenever
npm sends something to its logs.  It also spins at a constant speed while
waiting on the network.

The _current thing we're doing_ relates to how we track how much work has
been done.  It's the name of the unit of work we most recently started or
completed some of.  Sometimes these names are more obvious than others and
that's something we'll look at improving over time.

And finally, the _most recent log line_ is exactly that, it's the most
recent line that you would have seen if you were running with
`--loglevel=silly` or were watching the `npm-debug.log`.  These are written
to be useful to the npm developers above all else, so they may sometimes be
a little cryptic.

* [`6789978`](https://github.com/npm/npm/commit/6789978ab0713f67928177a9109fed43953ccbda)
  [#13075](https://github.com/npm/npm/pull/13075)
  `npmlog@3.1.2`: Update to the latest npmlog, which includes the new and
  improved progress bar layout.
  ([@iarna](https://github.com/iarna))

##### MORE PERFORMANT

The underlying code for the progress bar was rewritten, in part with
performance in mind.  Previously whenever you updated the progress bar it
would check an internal variable for how long it had been since the last
update and if it had been long enough, it would print out what you gave it.
With the new progress bar we do updates at a fixed interval (with
`setInterval`) and "updating" the progress bar just updates some variables
that will be used when the next tick of the progress bar happens. Currently
progress bar updates happen every 50ms, although that's open to tuning.

##### WIDE(R) COMPATIBILITY

I spent a lot of time working our Unicode support.  There were a few issues
that plagued us:

Previously one of the characters we used was _ambiguous width_ which means
that it was possible to configure your terminal to display it as _full
width_.  If you did this, the output would be broken because we assumed it
was a _half width_ character. We no longer use any of these characters.

Previously, we defaulted to using Unicode on Windows.  This isn't a safe
assumption, however, as folks in non-US locales often use other code pages
for their terminals.  Windows doesn't provide* any facility available to
Node.js for determining the current code page, so we no longer try to use
Unicode on Windows.

_\* The facilities it does provide are a command line tool and a windows
system call.  The former isn't satisfactory for speed reasons and the latter
can't be accessed from a JS-only Node.js program._

##### FOR THE FUTURE: THEMES

The new version of the progress bar library supports plugable themes. Adding
support to npm shouldn't be too difficult. The built in themes are:

* `ASCII` – The fallback theme which is always available.
* `colorASCII` – Inverts the color of the completed portion of the progress
  bar.  The default on Windows and usually on Linux.  (Color support is
  determined by looking at the `TERM` environment variable.)
* `brailleSpinner` – A braille based spinner and other unicode enhancements. MacOS only.
* `colorBrailleSpinner` – The default on MacOS, a combination of the above two.

##### LESS GARBLED OUTPUT

As a part of landing this I've also taken the opportunity to more
systematically disable the progress bar prior to printing to `stdout` or
running external commands (in particular: git).  This should ensure that the
progress bar doesn't get left on screen after something else prints
something.  We also are now much more zealous about erasing the progress bar
on exit, so if you `Ctrl-C` out of an install we'll still cleanup the
progress bar.

* [`63f153c`](https://github.com/npm/npm/commit/63f153c743f9354376bfb9dad42bd028a320fd1f)
  [#13075](https://github.com/npm/npm/pull/13075)
  Consistently make sure that the progress bar is hidden before we try to
  write to stdout.
  ([@iarna](https://github.com/iarna))
* [`8da79fa`](https://github.com/npm/npm/commit/8da79fa60de4972dca406887623d4e430d1609a1)
  [#13075](https://github.com/npm/npm/pull/13075)
  Be more methodical about disabling progress bars before running external
  commands.
  ([@iarna](https://github.com/iarna))

#### REPLACE `process.nextTick` WITH `asap` ASAP

* [`5873b56`](https://github.com/npm/npm/commit/5873b56cb315437dfe97e747811c0b9c297bfd38)
  [`254ad7e`](https://github.com/npm/npm/commit/254ad7e38f978b81046d242297fe8b122bfb5852)
  [#12754](https://github.com/npm/npm/issues/12754)
  Use `asap` in preference over `process.nextTick` to avoid recursion warnings.
  Under the hood `asap` uses `setImmediate` when available and falls back to
  `process.nextTick` when it's not.  Versions of node that don't support
  `setImmediate` have a version of `process.nextTick` that actually behaves
  like the current `setImmediate`.
  ([@lxe](https://github.com/lxe))

#### FIXES AND REFACTORING

Sometimes the installer would get it into its head that it could move or
remove things that it really shouldn't have.  While the reproducers for this were
often a bit complicated (the core reproducer involved five symlinks(!)), it turns
out this is an easy scenario to end up in if your project has a bunch of small
modules and you're linking them while developing them.

Fixing this ended up involving doing an important and overdue rewrite of how
the installer keeps track of (and interrogates) the relationships between
modules.  This likely fixes other related bugs, and in the coming weeks
we'll verify and close them as we find them.  There are a whole slew of
commits related to this rewrite, and if you'd like to learn more check
out the PR where I describe what I did in detail: [#12775](https://github.com/npm/npm/pull/12775)

* [`8f3e111`](https://github.com/npm/npm/commit/8f3e111fdd2ce7824864f77b04e5206bdaf961a1)
  [`c0b0ed1`](https://github.com/npm/npm/commit/c0b0ed1e9945c01b2e68bf22af3fe4005aa4bcd4)
  [#10800](https://github.com/npm/npm/issues/10800)
  Remove install pruning stage–this was obsoleted by making the installer keep
  itself up to date as it goes along. This is NOT related to `npm prune`.
  ([@iarna](https://github.com/iarna))

#### MAKE OUTDATED MORE WIDELY LEGIBLE

* [`21c60e9`](https://github.com/npm/npm/commit/21c60e9bb56d47da17b79681f2142b3dcf4c804b)
  [#12843](https://github.com/npm/npm/pull/12843)
  In `npm outdated, stop coloring the _Location_ and _Package Type_ columns.
  Previously they were colored dark gray, which was hard to read for some
  users.
  ([@tribou](https://github.com/tribou))

#### DOCUMENTATION UPDATE

* [`eb0a72e`](https://github.com/npm/npm/commit/eb0a72eb95862c1d0d41a259d138ab601d538793)
  [#12983](https://github.com/npm/npm/pull/12983)
  Describe how to run the lifecycle scripts of dependencies. How you do
  this changed with `npm` v2.
  ([@Tapppi](https://github.com/Tapppi))

### DEPENDENCY UPDATES

* [`da743dc`](https://github.com/npm/npm/commit/da743dc2153fed8baca3dada611b188f53ab5931)
  `which@1.2.10`:
  Fix bug where unnecessary special case path handling for Windows could
  produce unexpected results on Unix systems.
  ([@isaacs](https://github.com/isaacs))
* [`4533bd5`](https://github.com/npm/npm/commit/4533bd501d54aeedfec3884f4fd54e8c2edd6020)
  `npm-user-validate@0.1.4`:
  Validate the length of usernames.
  ([@aredridel](https://github.com/aredridel))
* [`4a18922`](https://github.com/npm/npm/commit/4a18922e56f9dc902fbb4daa8f5fafa4a1b89376)
  `glob@7.0.4`:
  Fixes issues with Node 6 and "long or excessively symlink-looping paths".
  ([@isaacs](https://github.com/isaacs))
* [`257fe11`](https://github.com/npm/npm/commit/257fe11052987e5cfec2abdf52392dd95a6c6ef3)
  `npm-package-arg@4.2.0`:
  Add `escapedName` to the result.  It is suitable for passing through to a
  registry without further processing.
  ([@nexdrew](https://github.com/nexdrew))
* [`dda3ca7`](https://github.com/npm/npm/commit/dda3ca70f74879106589ef29e167c8b91ef5aa4c)
  `wrappy@1.0.2`
  ([@zkat](https://github.com/zkat))
* [`25f1db5`](https://github.com/npm/npm/commit/25f1db504d0fd8c97211835f0027027fe95e0ef3)
  `readable-stream@2.1.4`
  ([@calvinmetcalf](https://github.com/calvinmetcalf))
* [`9d64fe6`](https://github.com/npm/npm/commit/9d64fe676ebc6949c687ffb85bd93eca3137fc0d)
  `abbrev@1.0.9`
  ([@isaacs](https://github.com/isaacs))

### v3.9.6 (2016-06-02):

#### SMALL OUTPUT TWEAK

* [`0bdc9d1`](https://github.com/npm/npm/commit/0bdc9d13b73df07e63a58470ea001fda490e5869)
  [#12879](https://github.com/npm/npm/pull/12879)
  The usage output for npm commands was somehow under the impression that
  the singular form of `aliases` is `aliase`. This has been corrected to show
  `alias` instead.
  ([@intelliot](https://github.com/intelliot))

#### DOC UPDATES

* [`f771b49`](https://github.com/npm/npm/commit/f771b49f5d65bbef540c231fbfcca71cacdce4db)
  [#12933](https://github.com/npm/npm/pull/12933)
  Add `config.gypi` to list of files that are always ignored in the
  `package.json` manpage.
  ([@Jokero](https://github.com/Jokero))

#### DEPENDENCY UPDATES

* [`61c1d9c`](https://github.com/npm/npm/commit/61c1d9cd4b2296bd41d55a5c58e35ca5f028b9bc)
  [#12926](https://github.com/npm/npm/pull/12926)
  Removed unused dependency `lodash.isarray`.
  ([@mmalecki](https://github.com/mmalecki))
* [`168ed28`](https://github.com/npm/npm/commit/168ed2834b2c6db8bb39f81baadc0bf275807328)
  [#12926](https://github.com/npm/npm/pull/12926)
  Removed unused dependency `lodash.keys`.
  ([@mmalecki](https://github.com/mmalecki))

### v3.9.5 (2016-05-27):

Just a quick point release. We had an issue where I (Kat) included the
`.nyc_output/` directory in `npm@3.9.3` and `npm@3.9.4`. The issue got reported
right after that second release
([`#12873`](https://github.com/npm/npm/issues/12873)), and now there's this
small point release that's there to fix the issue sooner.

* [`f96aea0`](https://github.com/npm/npm/commit/f96aea085be981cdb59bd09f16da40717426f981)
  [#12878](https://github.com/npm/npm/pull/12878)
  Ignore `.nyc_output` to avoid an accidental publish or commit filled with
  code coverage data.
  ([@TheAlphaNerd](https://github.com/TheAlphaNerd))

### v3.9.4 (2016-05-26):

Hey all! It's that time again!

This week continues our current `big-bug` squashing push, although there's none
that are ready to release quite yet -- we're working on it!

It's also worth noting that we're entering the main part of conference season,
so you can probably expect a bit of a dev slowdown as a lot of us wombats attend
or speak at the various conferences. Remember [npm.camp](npm.camp) is happening
in 2 months and the lineup is looking pretty great! Tickets are still on sale.
Come hang out with us! WOO FUN! 🎉😸

#### BUGFIX

* [`cac0038`](https://github.com/npm/npm/commit/cac0038868b18295f9f299e762e20034f32a3e11)
  [#12845](https://github.com/npm/npm/pull/12845)
  Progress bar during tarball packing now prints `pack:packagename` instead of
  `pack:[object Object]`.
  ([@iarna](https://github.com/iarna))

#### DOC UPDATES

* [`0b81622`](https://github.com/npm/npm/commit/0b816225c743c9203db5d92fb4dd3a9293833298)
  [#12840](https://github.com/npm/npm/pull/12840)
  Remove sexualized language from comment in code.
  ([@geek](https://github.com/geek))
* [`d6dff24`](https://github.com/npm/npm/commit/d6dff2481cb587c392f22afb893ac3136371a64c)
  [#12802](https://github.com/npm/npm/pull/12802)
  Small grammar fix in `cli/npm.md`.
  ([@andresilveira](https://github.com/andresilveira))
* [`cb38e0f`](https://github.com/npm/npm/commit/cb38e0fff82a6c1c110026b95b07a8c32e27ec01)
  [#12782](https://github.com/npm/npm/pull/12782)
  Documents that `NOTICE` files started getting included after
  [npm/fstream-npm#17](https://github.com/npm/fstream-npm/pull/17).
  ([@SimenB](https://github.com/SimenB))
* [`70a3ae4`](https://github.com/npm/npm/commit/70a3ae4d4ec76b3ec51f00bf5261f1147829f9fe)
  [#12776](https://github.com/npm/npm/pull/12776)
  `npm run-script` used to have a `<pkg>` argument that allowed you to target
  specific packages' scripts. This was removed as one of the breaking changes
  for `npm@2`.
  This patch removes a mention of that argument, which really doesn't exist
  anymore.
  ([@fibo](https://github.com/fibo))

#### DEP UPDATES

* [`4a4470d`](https://github.com/npm/npm/commit/4a4470ddd1d9b0b62cb94f3bff5ab6b8e6db527a)
  `aproba@1.0.3`
  ([@iarna](https://github.com/iarna))

#### TEST IMPROVEMENTS

So it turns out, `t.comment` in `tap` is actually pretty nice!
There's also a couple other test improvements by Rebecca landing here.

* [`9fd04dd`](https://github.com/npm/npm/commit/9fd04dd6be493465d7ac5f14dd9328e66069c1bf)
  [#12851](https://github.com/npm/npm/pull/12851)
  Rewrite `shrinkwrap-prod-dependency-also` test to use `common.npm`
  ([@iarna](https://github.com/iarna))
* [`3bc4a8e`](https://github.com/npm/npm/commit/3bc4a8ee58cb0e0adc84b4f135330f2b1e20d992)
  [#12851](https://github.com/npm/npm/pull/12851)
  Clean up `rm-linked` test.
  ([@iarna](https://github.com/iarna))
* [`bf7f7f2`](https://github.com/npm/npm/commit/bf7f7f273a794f7573bbbc84b1c216fdcd9e0ef9)
  [#12851](https://github.com/npm/npm/pull/12851)
  Clean up `outdated-symlink` test.
  ([@iarna](https://github.com/iarna))
* [`ca0baa4`](https://github.com/npm/npm/commit/ca0baa4dac85b1df4e26ef0c73d39314ca6858ca)
  [#12851](https://github.com/npm/npm/pull/12851)
  Improve diagnostics for `shrinkwrap-scoped-auth` test.
  ([@iarna](https://github.com/iarna))
* [`fbec9fd`](https://github.com/npm/npm/commit/fbec9fd5bb0abce589120d14c1f2b03b58cecce1)
  [#12851](https://github.com/npm/npm/pull/12851)
  Rewrite `shrinkwrap-dev-dependency` test to use `common.npm`.
  ([@iarna](https://github.com/iarna))

### v3.9.3 (2016-05-19):

This week continues our `big-bug` squashing adventure! Things are churning along
nicely, and we've gotten a lot of fantastic contributions from the community.
Please keep it up!

A quick note on last week's release: We had a small `npm shrinkwrap`-related
crasher in `npm@3.9.1`, so once this release goes out, `v3.9.2` is going to be
`npm@latest`. Please update if you ended up in with that previous version!

Remember we have a weekly team meeting, and you can [suggest agenda items in the
GitHub issue](https://github.com/npm/npm/issues/12761). Keep an eye out for the
`#npmweekly` tag on Twitter, too, and join the conversation! We'll do our best
to address questions y'all send us. ✌

#### FIXES

* [`42d71be`](https://github.com/npm/npm/commit/42d71be2cec674dd9e860ad414f53184f667620d)
  [#12685](https://github.com/npm/npm/pull/12685)
  When using `npm ls <pkg>` without a semver specifier, `npm ls` would skip
  any packages in your tree that matched by name, but had a prerelease version
  in their `package.json`. This patch fixes it so `npm ls` does a simple name
  match unless you use the `npm ls <pkg>@<version>` format.
  ([@zkat](https://github.com/zkat))
* [`c698ae6`](https://github.com/npm/npm/commit/c698ae666afc92fbc0fcba3c082cfa9b34a4420d)
  [#12685](https://github.com/npm/npm/pull/12685)
  Added some tests for more basic `npm ls` functionality.
  ([@zkat](https://github.com/zkat))

### NOTABLE DEPENDENCY UPDATES

* [`3a6fe23`](https://github.com/npm/npm/commit/3a6fe2373c45e80a1f28aaf176d552f6f97cf131)
  [npm/fstream-npm#17](https://github.com/npm/fstream-npm/pull/17)
  `fstream-npm@1.1.0`:
  `fstream-npm` always includes NOTICE files now.
  ([@kemitchell](https://github.com/kemitchell))
* [`df04e05`](https://github.com/npm/npm/commit/df04e05af1f257a1903372e1baf334c0969fbdbd)
  [#10013](https://github.com/npm/npm/issues/10013)
  `read-package-tree@5.1.4`:
  Fixes an issue where `npm install` would fail if your `node_modules` was
  symlinked.
  ([@iarna](https://github.com/iarna))
* [`584676f`](https://github.com/npm/npm/commit/584676f85eaebcb9d6c4d70d2ad320be8a8d6a74)
  [npm/init-package-json#62](https://github.com/npm/init-package-json/pull/62)
  `init-package-json@1.9.4`:
  Stop using `package` for a variable, which defeats some bundlers and linters.
  ([@adius](https://github.com/adius))
* [`935a7e3`](https://github.com/npm/npm/commit/935a7e359535e13924934811b77924cbad82619a)
  `readable-stream@2.1.3`:
  Node 6 build and buffer-related updates.
  ([@calvinmetcalf](https://github.com/calvinmetcalf))

#### OTHER DEPENDENCY UPDATES

* [`4c4609e`](https://github.com/npm/npm/commit/4c4609ea49e77303f9d72af6757620e6b3a9a6a9)
  `inflight@1.0.5`
  ([@zkat](https://github.com/zkat))
* [`7a3030d`](https://github.com/npm/npm/commit/7a3030d3d44ea2136425f72950ba22e6efd441d9)
  `hosted-git-info@2.1.5`
  ([@zkat](https://github.com/zkat))
* [`5ed4b58`](https://github.com/npm/npm/commit/5ed4b58409eeb134bca1c96252682fd7600d9906)
  `which@1.2.9`
  ([@isaacs](https://github.com/isaacs))

### v3.9.2 (2016-05-17)

This is a quick patch release.  The previous release, 3.9.1, introduced a
bug where npm would crash given a combination of specific package tree on
disk and a shrinkwrap.

* [`cde367f`](https://github.com/npm/npm/commit/cde367fbb6eebc5db68a44b12a5c7bea158d70db)
  [#12724](https://github.com/npm/npm/issues/12724)
  Fix crasher when inflating shrinkwraps with packages on disk that were
  installed by older npm versions.
  ([@iarna](https://github.com/iarna))

### v3.9.1 (2016-05-12)

HI all!  We have bug fixes to a couple of the hairy corners of `npm`, in the
form of shrinkwraps and bundled dependencies. Plus some documentation improvements
and our lodash deps bot a bump.

This is our first week really focused on getting the
[big bugs](https://github.com/npm/npm/issues?q=is%3Aopen+is%3Aissue+label:big-bug)
list down.  Our work from this week will be landing next week, and I can't
wait to tell you about that! (It's about symlinks!)

#### SHRINKWRAP FIX

* [`b894413`](https://github.com/npm/npm/commit/b8944139a935680c4a267468bb2d3c3082b5609f)
  [#12372](https://github.com/npm/npm/issues/12372)
  Changing a nested dependency in an `npm-shrinkwrap.json` and then running `npm install`
  would not get up the updated package. This corrects that.
  ([@misterbyrne](https://github.com/misterbyrne))

#### BUNDLED DEPENDENCIES FIX

* [`d0c6d19`](https://github.com/npm/npm/commit/d0c6d194471be8ce3e7b41b744b24f63dd1a3f6f)
  [#12476](https://github.com/npm/npm/pull/12476)
  Protects against a crasher when a bundled dep is missing a package.json.
  ([@dflupu](https://github.com/dflupu))

#### DOCS IMPROVEMENTS

* [`6699aa5`](https://github.com/npm/npm/commit/6699aa53c0a729cfc921ac1d8107c320e5a5ac95)
  [#12585](https://github.com/npm/npm/pull/12585)
  Document that engineStrict is quite gone. Not "deprecated" so much as "extirpated".
  ([@othiym23](https://github.com/othiym23))
* [`7a41a84`](https://github.com/npm/npm/commit/7a41a84b655be3204d2e80848278a510e42c80e7)
  [#12636](https://github.com/npm/npm/pull/12636)
  Improve `npm-scripts` documentation regarding when `node-gyp` is used.
  ([@reconbot](https://github.com/reconbot))
* [`4c4b4ba`](https://github.com/npm/npm/commit/4c4b4badf09b9b50cdca85314429a0111bb35cb1)
  [#12586](https://github.com/npm/npm/pull/12586)
  Correct `package.json` documentation as to when `node-gyp rebuild` called.
  This now matches https://docs.npmjs.com/misc/scripts#default-values
  ([@reconbot](https://github.com/reconbot))

#### DEPENDENCY UPDATES

* [`cfa797f`](https://github.com/npm/npm/commit/cfa797fedd34696d45b61e3ae0398407afece880)
  `lodash._baseuniq@4.6.0`
  ([@jdalton](https://github.com/jdalton))
* [`ab6f180`](https://github.com/npm/npm/commit/ab6f1801971b513f9294b4b8902034ab402af02d)
  `lodash.keys@4.0.7`
  ([@jdalton](https://github.com/jdalton))
* [`4b8d8b6`](https://github.com/npm/npm/commit/4b8d8b63e760a8aa03e8bffa974495dfafbfcb06)
  `lodash.union@4.4.0`
  ([@jdalton](https://github.com/jdalton))
* [`46099d3`](https://github.com/npm/npm/commit/46099d34542760098e5d13c7468a405a724ca407)
  `lodash.uniq@4.3.0`
  ([@jdalton](https://github.com/jdalton))
* [`fff89c6`](https://github.com/npm/npm/commit/fff89c6826c86e9e789adcc9c398385539306042)
  `lodash.without@4.2.0`
  ([@jdalton](https://github.com/jdalton))

### v3.9.0 (2016-05-05)

Wow!  This is a big release week!  We've completed the fixes that let the
test suite pass on Windows, plus more general bug fixes we found while
fixing things on Windows.  Plus a warning to help folks work around a common
footgun.  PLUS an improvement to how npm works with long cache timeouts.

#### INFINITE CACHE A LITTLE BETTER

* [`111ae3e`](https://github.com/npm/npm/commit/111ae3ec366ece7ebcf5988f5bc2a7cd70737dfe)
  [#8581](https://github.com/npm/npm/issues/8581)
  When a package is fetched from the cache which cannot satisfy the version
  requirements, an attempt to fetch it from the network is made.  This is
  helpful for folks using high values for `--cache-min` who are willing to
  accept possibly not-the-most-recent modules in return for less network
  traffic.
  ([@Zirak](https://github.com/Zirak))

#### WARNING: FOOTGUN

* [`60b9a05`](https://github.com/npm/npm/commit/60b9a051aa46b8892fe63b3681839a6fd6642bfd)
  [#12475](https://github.com/npm/npm/pull/12475)
  Options can only start with ASCII dashes.  Ordinarily this isn't a problem
  but many web documentation tools "helpfully" convert `--` into an emdash
  (–), or `-` into an endash (–).  If you copy and paste from this documentation
  your commands won't work the way you expect. This adds a warning that tries
  to be a little more descriptive about why your command is failing.
  ([@iarna](https://github.com/iarna))

#### WINDOWS CI

We have [Windows CI](https://ci.appveyor.com/project/npm/npm) setup now! We still have to
tweak it a little bit around paths to the git binaries, but it's otherwise ready!

* [`bb5d6cb`](https://github.com/npm/npm/commit/bb5d6cbf46b2609243d3b384caadd196e665a797)
  [#11444](https://github.com/npm/npm/pull/11444)
  Add AppVeyor to CI matrix.
  ([@othiym23](https://github.com/othiym23))

#### COVERAGE DATA

Not only do our tests produce coverage reports after they run now, we also
automatically [update Coveralls](https://coveralls.io/github/npm/npm) with
results from [Travis CI](travis-ci.org/npm/npm) runs.

* [`044cbab`](https://github.com/npm/npm/commit/044cbab0d49adeeb0d9310c64fee6c9759cc7428)
  [#11444](https://github.com/npm/npm/pull/11444)
  Enable coverage reporting for every test run.
  ([@othiym23](https://github.com/othiym23))

#### EVERYONE BUGS

* [`37c6a51`](https://github.com/npm/npm/commit/37c6a51c71b0feec8f639b3199a8a9172e58deec)
  [#12150](https://github.com/npm/npm/pull/12150)
  Ensure that 'npm cache ls' outputs real filenames.  Previously it would
  sometimes double up the package name in the path it printed.
  ([@isaacs](https://github.com/isaacs))
* [`d3ce0b2`](https://github.com/npm/npm/commit/d3ce0b253eb519375071aee29db4ee129dbcdf5c)
  [#11444](https://github.com/npm/npm/pull/11444)
  Fix unbuilding bins for scoped modules.
  ([@iarna](https://github.com/iarna))
* [`e928a30`](https://github.com/npm/npm/commit/e928a30947477a09245f54e9381f46b97bee32d5)
  [#11444](https://github.com/npm/npm/pull/11444)
  Make handling of local modules (eg `npm install /path/to/my/module`) more
  consistent when saved to a `package.json`. There were bugs previously where
  it wouldn't consistently resolve relative paths in the same way.
  ([@iarna](https://github.com/iarna))
* [`b820ed4`](https://github.com/npm/npm/commit/b820ed4fc04e21577fa66f7c9482b5ab002e7985)
  [#11444](https://github.com/npm/npm/pull/11444)
  Under certain circumstances the paths produced for linking, either
  relative or absolute, would end up basing off the wrong virtual cwd.
  This resulted in failures for `npm link` in this situations.
  ([@iarna](https://github.com/iarna))

#### WINDOWS BUGS

* [`7380425`](https://github.com/npm/npm/commit/7380425d810fb8bfc69405a9cbbdec19978a7bee)
  [#11444](https://github.com/npm/npm/pull/11444)
  Scoped module names were not being correctly inferred from the path on Windows.
  ([@zkat](https://github.com/zkat))
* [`91fc24f`](https://github.com/npm/npm/commit/91fc24f2763c2e0591093099ffc866c735f27fde)
  [#11444](https://github.com/npm/npm/pull/11444)
  Explore with a command to run didn't work properly in Windows– it would pop open a new
  cmd window and leave it there.
  ([@iarna](https://github.com/iarna))

#### WINDOWS REFACTORING

* [`f07e643`](https://github.com/npm/npm/commit/f07e6430d4ca02f811138f6140a8bad927607a1f)
  [#11444](https://github.com/npm/npm/pull/11444)
  Move exec path escaping out to its own function.  This turns out to be
  tricky to get right because how you escape commands to run on Windows via
  cmd is different then how you escape them at other times.  Specifically,
  you HAVE to quote each directory segment that has a quote in it, that is:
  `C:\"Program Files"\MyApp\MyApp.exe` By contrast, if that were an argument
  to a command being run, you CAN'T DO quote it that way, instead you have
  to wrap the entire path in quotes, like so: `"C:\Program
  Files\MyApp\MyApp.exe"`.
  ([@iarna](https://github.com/iarna))
* [`2e01d29`](https://github.com/npm/npm/commit/2e01d299f8244134b1aa040cab1b59c72c9df4da)
  [#11444](https://github.com/npm/npm/pull/11444)
  Create a single function for detecting if we're running on Windows (and
  using a Windows shell like cmd) and use this instead of doing it one-off
  all over the place.
  ([@iarna](https://github.com/iarna))

#### FIX WINDOWS TESTS

As I said before, our tests are passing on Windows! 🎉

* [`ef0dd74`](https://github.com/npm/npm/commit/ef0dd74583be25c72343ed07d1127e4d0cc02df9)
  [#11444](https://github.com/npm/npm/pull/11444)
  The fruits of many weeks of labor, fix our tests to pass on Windows.
  ([@zkat](https://github.com/zkat))
  ([@iarna](https://github.com/iarna))

#### DEPENDENCY UPDATES

* [`8fccda8`](https://github.com/npm/npm/commit/8fccda8587209659c469ab55c608b0e2d7533530)
  [#11444](https://github.com/npm/npm/pull/11444)
  `normalize-git-url@3.0.2`:
  Fix file URLs on Windows.
  ([@zkat](https://github.com/zkat))
* [`f53a154`](https://github.com/npm/npm/commit/f53a154df8e0696623e6a71f33e0a7c11a7555aa)
  `readable-stream@2.1.2`:
  When readable-stream is disabled, reuse result of `require('stream')`
  instead of calling it every time.
  ([@calvinmetcalf](https://github.com/calvinmetcalf))
* [`02841cf`](https://github.com/npm/npm/commit/02841cfb81d6ba86f691ab43d9bbdac29aec27e7)
  [#11444](https://github.com/npm/npm/pull/11444)
  `realize-package-specifier@3.0.2`:
  Resolve local package paths relative to package root, not cwd.
  ([@zkat](https://github.com/zkat))
  ([@iarna](https://github.com/iarna))
* [`247c1c5`](https://github.com/npm/npm/commit/247c1c5ae08c882c9232ca605731039168bae6ed)
  [#11444](https://github.com/npm/npm/pull/11444)
  `npm-package-arg@4.1.1`:
  Fix Windows file URIs with leading slashes.
  ([@zkat](https://github.com/zkat))
* [`365c72b`](https://github.com/npm/npm/commit/365c72bc3ecd9e45f9649725dd635d5625219d8c)
  `which@1.2.8`
  ([@isaacs](https://github.com/isaacs))
* [`e568caa`](https://github.com/npm/npm/commit/e568caabb8390a924ce1cfa51fc914ee6c1637a2)
  `graceful-fs@4.1.4`
  ([@isaacs](https://github.com/isaacs))
* [`304b974`](https://github.com/npm/npm/commit/304b97434959a58f84383bcccc0357c51a4eb39a)
  [#11444](https://github.com/npm/npm/pull/11444)
  `standard@6.0.8`
  ([@feross](https://github.com/feross))

### v3.8.9 (2016-04-28)

Our biggest news this week is that we got the
[Windows test suite passing](https://github.com/npm/npm/pull/11444)!
It'll take a little longer to get it passing in our
[Windows CI](https://ci.appveyor.com/project/npm/npm/) but that's coming
soon too.

That means we'll be shifting gears away from tests to fixing
[Big Bugs™](https://github.com/npm/npm/issues?q=is%3Aopen+is%3Aissue+label%3Abig-bug) again.
Join us at our [team meeting](https://github.com/npm/npm/issues/12517) next
Tuesday to learn more about that.

#### BUG FIXES AND REFACTORING

* [`60da618`](https://github.com/npm/npm/commit/60da61862885fa904afba7d121860b4282a5b0df)
  [#12347](https://github.com/npm/npm/issues/12347)
  Fix a bug that could result in shrinkwraps missing the `resolved` field, which is
  necessary in producing a fully reproducible build.
  ([@sminnee](https://github.com/sminnee))
* [`8597ba4`](https://github.com/npm/npm/commit/8597ba432e91245a1000953b612eb01308178bad)
  [#12009](https://github.com/npm/npm/issues/12009)
  Fix a bug in `npm view <packagename> versions` that resulted in bad output if you
  didn't also pass in `--json`.
  ([@watilde](https://github.com/watilde))
* [`20125f1`](https://github.com/npm/npm/commit/20125f19b96fd05af63f8c0bd243ffb25780279a)
  [`a53feac`](https://github.com/npm/npm/commit/a53feac2647f7dc4245f1700dfbdd1aba8745672)
  [`6cfbae4`](https://github.com/npm/npm/commit/6cfbae403abc3cf690565b09569f71cdd41a8372)
  [#12485](https://github.com/npm/npm/pull/12485)
  Refactor how the help summaries for commands are produced, such that we only have
  one list of command aliases.
  ([@watilde](https://github.com/watilde))
* [`2ae210c`](https://github.com/npm/npm/commit/2ae210c76ab6fd15fcf15dc1808b01ca0b94fc9e)
  `read-package-json@2.0.4`:
  Fix a crash we discovered while fixing up the Windows test suite where if
  you had a file in your `node_modules` it would cause a crash on Windows
  (but not MacOS/Linux).

  This makes the error code you get on Windows match that from MacOS/Linux
  if you try to read a `package.json` from a path that includes a file, not
  a folder.
  ([@zkat](https://github.com/zkat))

### v3.8.8 (2016-04-21)

Hi all! Long time no see! We've been heads-down working through getting
[our test suite passing on Windows](https://github.com/npm/npm/pull/11444).
Did you know that we have
[Windows CI](https://ci.appveyor.com/project/npm/npm) now running over at
Appveyor?  In the meantime, we've got a bunch of dependency updates, some
nice documentation improvements and error messages when your `package.json`
contains invalid JSON. (Yeah, I thought we did that last one before too!)

#### BAD JSON IS BAD

* [`769e620`](https://github.com/npm/npm/commit/769e6200722d8060b6769e47354032c51cfa85a1)
  [#12406](https://github.com/npm/npm/pull/12406)
  Failing to parse the top level `package.json` should be an error.
  ([@watilde](https://github.com/watilde))

#### DOCUMENTATION

* [`7d64301`](https://github.com/npm/npm/commit/7d643018af5051c920cc73f17bfe32b7ff86e108)
  [#12415](https://github.com/npm/npm/pull/12415)
  Clarify that when configuring client-side certificates for authenticating
  to non-npm registries that `cert` and `key` are not filesystem paths and should
  actually include the certificate and key data.
  ([@rvedotrc](https://github.com/rvedotrc))
* [`f8539b8`](https://github.com/npm/npm/commit/f8539b8c986e81771ccc8ced7e716718423d3187)
  [#12324](https://github.com/npm/npm/pull/12324)
  Describe how `npm run` sets `NODE` and `PATH` in more detail.
  Note that `npm run` changes `PATH` to include the current node
  interpreter’s directory.
  ([@addaleax](https://github.com/addaleax))
* [`2b57606`](https://github.com/npm/npm/commit/2b57606852a2c2a03e4c4b7dcda85b807619c2cf)
  [#11461](https://github.com/npm/npm/pull/11461)
  Clarify the documentation for the package.json homepage field.
  ([@stevemao](https://github.com/stevemao))

#### TESTS

* [`b5a0fbb`](https://github.com/npm/npm/commit/b5a0fbb9e1a2c4fb003dd748264571aa6e3c9e70)
  [#12329](https://github.com/npm/npm/pull/12329)
  Fix progress config testing to ignore local user configs.
  Previously, _any_ local setting would cause the tests to fail as
  they were trying to test what the default values for the progress
  bar would be in different environments and any explicit setting
  overrides those defaults.
  ([@iarna](https://github.com/iarna))
* [`3d195bc`](https://github.com/npm/npm/commit/3d195bc0a72b40df02a5c56e4f3be44152e8222b)
  The lifecycle-signal test could crash on v0.8 due to its use of `Number.parseInt`, which
  isn't available in that version of node.  Fortunately `global.parseInt` _is_, so
  we just use that instead.
  ([@iarna](https://github.com/iarna))

#### DEPENDENCY UPDATES

* [`05a28e3`](https://github.com/npm/npm/commit/05a28e38586082ac4bbf26ee6f863cc8d07054d6)
  `npm-package-arg@4.1.1`:
  Under some circumstances `file://` URLs on Windows were not handled correctly.

  Also, stop converting local module/tarballs into full paths in this
  module.  We do already do that in `realize-package-specifier`, which is
  more appropriate as it knows what package we're installing relative to.
  ([@zkat](https://github.com/zkat))
* [`ada2e93`](https://github.com/npm/npm/commit/ada2e93e8b276000150a9aa93fff69ec366e03d6)
  `realize-package-specifier@3.0.3`:
  Require the new `npm-package-arg`, plus fix a case where specifiers that were
  maybe a tag, maybe a local filename were resolved differently than those that were
  definitely a local filename.
  ([@zkat](https://github.com/zkat)) ([@iarna](https://github.com/iarna))
* [`adc515b`](https://github.com/npm/npm/commit/adc515b22775871386cd62390079fb4bf8e1714a)
  `fs-vacuum@1.2.9`:
  A fix for AIX where a non-empty directory can cause `fs.rmDir` to fail with `EEXIST` instead of `ENOTEMPTY`
  and three new tests
  ([@richardlau](https://github.com/richardlau))

  Code cleanup, CI & dependency updates.
  ([@othiym23](https://github.com/othiym23))
* [`ef53a46`](https://github.com/npm/npm/commit/ef53a46906ce872a4541b605dd42a563cc26e614)
  `tap@5.7.1`
  ([@isaacs](https://github.com/isaacs))
* [`df1f2e4`](https://github.com/npm/npm/commit/df1f2e4838b4d7ea2ea2321a95ae868c0ec0a520)
  `request@2.72.0`:
  Fix crashes when response headers indicate gzipped content but the body is
  empty.
  Add support for the deflate content encoding.
  ([@simov](https://github.com/simov))
* [`776c599`](https://github.com/npm/npm/commit/776c599b204632aca9d29fd92ea5c4f099fdea9f)
  `readable-stream@2.1.0`:
  Adds READABLE_STREAM env var that, if set to `disable`, will make
  `readable-stream` use the local native node streams instead.
  ([@calvinmetcalf](https://github.com/calvinmetcalf))
* [`10d6d55`](https://github.com/npm/npm/commit/10d6d5547354fcf50e930c7932ba4d63c0b6009c)
  `normalize-git-url@3.0.2`:
  Add support `git+file://` type URLs.
  ([@zkat](https://github.com/zkat))
* [`75017ae`](https://github.com/npm/npm/commit/75017aeecec69a1efd546df908aa5befc4467f36)
  `lodash.union@4.3.0`
  ([@jdalton](https://github.com/jdalton))

### v3.8.7 (2016-04-07)

#### IMPROVED DIAGNOSTICS

* [`38cf79f`](https://github.com/npm/npm/commit/38cf79ffa564ef5cb6677b476e06d0e45351592a)
  [#12083](https://github.com/npm/npm/pull/12083)
  If you `ignore-scripts` to disable lifecycles, this makes npm report when it skips running
  a script.
  ([@bfred-it](https://github.com/bfred-it))

#### IMPROVE AUTO-INCLUDES

* [`c615182`](https://github.com/npm/npm/commit/c615182c8b47e418338eb1317b99bb66987cda54)
  [#11995](https://github.com/npm/npm/pull/11995)
  There were bugs where modules whose names matched the special files that npm always
  includes would be included, for example, the `history` package was always included.

  With `npm@3` such extraneously bundled modules would not be ordinarily
  used, as things in `node_modules` in packages are ignored entirely if the
  package isn't marked as bundling modules.

  Because of this `npm@3` behavior, the `files-and-ignores` test failed to catch this as
  it was testing _install output_ not what got packed. That has also been fixed.
  ([@glenjamin](https://github.com/glenjamin))

#### DOCUMENTATION UPDATES

* [`823d9df`](https://github.com/npm/npm/commit/823d9dfa91d7086a26620f007aee4e3cd77b6153)
  [#12107](https://github.com/npm/npm/pull/12107)
  In the command summary for `adduser` mention that `login` is an alias.
  ([@gnerkus](https://github.com/gnerkus))
* [`7aaf47e`](https://github.com/npm/npm/commit/7aaf47e124c45dde72c961638b770ee535fb2776)
  [#12244](https://github.com/npm/npm/pull/12244)
  Update the README to suggest npm@3 for Windows users. Also add a reference to
  [Microsoft's npm upgrade tool](https://github.com/felixrieseberg/npm-windows-upgrade).
  ([@felixrieseberg](https://github.com/felixrieseberg))

#### DEPENDENCY UPDATES

* [`486bbc0`](https://github.com/npm/npm/commit/486bbc0e1b101f847e890e6f1925dc8cb253cf3e)
  `request@2.70.0`
  ([@simov](https://github.com/simov))
* [`b1aff34`](https://github.com/npm/npm/commit/b1aff346fc41f13e3306b437e1831942aacf2f54)
  `lodash.keys@4.0.6`
  ([@jdalton](https://github.com/jdalton))

### v3.8.6 (2016-03-31)

Heeeeeey y'all.

Kat here! Rebecca's been schmoozing with folks at [Microsoft
Build](https://build.microsoft.com/), so I'm doing the `npm@3` release this
week.

Speaking of Build, it looks like Microsoft is doing some bash thing. This might
be really good news for our Windows users once it rolls around. We're keeping an
eye out and feeling hopeful. 🙆

As far as the release goes: We're really happy to be getting more and more
community contributions! Keep it up! We really appreciate folks trying to help
us, and we'll do our best to help point you in the right direction. Even things
like documentation are a huge help. And remember -- you get socks for it, too!

#### FIXES

* [`f8fb4d8`](https://github.com/npm/npm/commit/f8fb4d83923810eb78d075bd200a9376c64c3e3a)
  [#12079](https://github.com/npm/npm/pull/12079)
  Back in `npm@3.2.2` we included [a patch that made it so `npm install pkg` was
  basically `npm install pkg@latest` instead of
  `pkg@*`](https://github.com/npm/npm/pull/9170)
  This is probably what most users expected, but it also ended up [breaking `npm
  deprecate`](https://github.com/npm/npm/pull/9170) when no version was provided
  for a package. In that case, we were using `*` to mean "deprecate all
  versions" and relying on the `pkg` -> `pkg@*` conversion.
  This patch fixes `npm deprecate pkg` to work as it used to by special casing
  that particular command's behavior.
  ([@polm](https://github.com/polm))
* [`458f773`](https://github.com/npm/npm/commit/458f7734f3376aba0b6ff16d34a25892f7717e40)
  [#12146](https://github.com/npm/npm/pull/12146)
  Adds `make doc-clean` to `prepublish` script, to clear out previously built
  docs before publishing a new npm version
  ([@watilde](https://github.com/watilde))
* [`f0d1521`](https://github.com/npm/npm/commit/f0d1521038e956b2197673f36c464684293ce99d)
  [#12146](https://github.com/npm/npm/pull/12146)
  Adds `doc-clean` phony target to `make publish`.
  ([@watilde](https://github.com/watilde))

#### DOC UPDATES

* [`ea92ffc`](https://github.com/npm/npm/commit/ea92ffc9dd2a063896353fc52c104e85ec061360)
  [#12147](https://github.com/npm/npm/pull/12147)
  Document that the current behavior of `engines` is just to warn if the node
  platform is incompatible.
  ([@reconbot](https://github.com/reconbot))
* [`cd1ba44`](https://github.com/npm/npm/commit/cd1ba4423b3ca889c741141b95b0d9472b9f71ea)
  [#12143](https://github.com/npm/npm/pull/12143)
  Remove `npm faq` command, since the [FAQ was
  removed](https://github.com/npm/npm/pull/10547).
  ([@watilde](https://github.com/watilde))
* [`50a12cb`](https://github.com/npm/npm/commit/50a12cb1f5f158af78d6962ad20ff0a98bc18f18)
  [#12143](https://github.com/npm/npm/pull/12143)
  Remove references to the FAQ from the docs, since [it was
  removed](https://github.com/npm/npm/pull/10547).
  ([@watilde](https://github.com/watilde))
* [`60051c2`](https://github.com/npm/npm/commit/60051c25e2ab80c667137dfcd04b242eea25980e)
  [#12093](https://github.com/npm/npm/pull/12093)
  Update `bugs` url in `package.json` to use the `https` URL for Github.
  ([@watilde](https://github.com/watilde))
* [`af30c37`](https://github.com/npm/npm/commit/af30c374ef22ed1a1c71b14fced7c4b8350e4e82)
  [#12075](https://github.com/npm/npm/pull/12075)
  Add the `--ignore-scripts` flag to the `npm install` docs.
  ([@paulirish](https://github.com/paulirish))
* [`632b214`](https://github.com/npm/npm/commit/632b214b2f2450e844410792e5947e46844612ff)
  [#12063](https://github.com/npm/npm/pull/12063)
  Various minor fixes to the html docs homepage.
  ([@watilde](https://github.com/watilde))

#### DEP BUMPS

* [`3da0171`](https://github.com/npm/npm/commit/3da01716a0e41d6b5adee2b4fc70fcaf08c0eb24)
  `lodash.without@4.1.2`
  ([@jdalton](https://github.com/jdalton))
* [`69ccf6d`](https://github.com/npm/npm/commit/69ccf6dd4caf95cd0628054307487cae1885acd0)
  `lodash.uniq@4.2.1`
  ([@jdalton](https://github.com/jdalton))
* [`b50c41a`](https://github.com/npm/npm/commit/b50c41a9930dc5353a23c5ae2ff87bb99e11d482)
  `lodash.union@4.2.1`
  ([@jdalton](https://github.com/jdalton))
* [`59c1ad7`](https://github.com/npm/npm/commit/59c1ad7b6f243d07618ed5703bd11d787732fc57)
  `lodash.clonedeep@4.3.2`
  ([@jdalton](https://github.com/jdalton))
* [`2b4f797`](https://github.com/npm/npm/commit/2b4f797dba8e7a1376c8335b7223e82d02cd8243)
  `lodash._baseuniq@4.5.1`
  ([@jdalton](https://github.com/jdalton))

### v3.8.5 (2016-03-24)

Like my esteemed colleague [@zkat](https://github.com/zkat) said in this
week's [LTS release notes](https://github.com/npm/npm/releases/tag/v2.15.2),
this week is another small release but we are continuing to work on our
[Windows efforts](https://github.com/npm/npm/pull/11444).

You may also be interested in reading the [LTS process and
policy](https://github.com/npm/npm/wiki/LTS) that
[@othiym23](https://github.com/othiym23) put together recently. If you have any
feedback, we would love to hear.

#### DOCTOR IT HURTS WHEN LINK TO MY LINK

Well then, don't do that.

* [`0d4a0b1`](https://github.com/npm/npm/commit/0d4a0b1)
  [#11442](https://github.com/npm/npm/pull/11442)
  Fail if the user asks us to make a link from a module back on to itself.
  ([@antialias](https://github.com/antialias))

#### ERR MODULE LIST TOO LONG

* [`b271ed2`](https://github.com/npm/npm/commit/b271ed2)
  [#11983](https://github.com/npm/npm/issues/11983)
  Exit early if no arguments were provided to search instead of trying to display all the modules,
  running out of memory, and then crashing.
  ([@SimenB](https://github.com/SimenB))

#### ELIMINATE UNUSED MODULE

* [`b8c7cd7`](https://github.com/npm/npm/commit/b8c7cd7)
  [#12000](https://github.com/npm/npm/pull/12000)
  Stop depending on [`async-some`](https://npmjs.com/package/async-some) as it's no
  longer used in npm.
  ([@watilde](https://github.com/watilde))

#### DOCUMENTATION IMPROVEMENTS

* [`fdd6b28`](https://github.com/npm/npm/commit/fdd6b28)
  [#11884](https://github.com/npm/npm/pull/11884)
  Include `node_modules` in the list of files and directories that npm won't
  include in packages ordinarily. (Modules listed in `bundledDependencies` and things
  that those modules rely on, ARE included of course.)
  ([@Jameskmonger](https://github.com/Jameskmonger))
* [`aac15eb`](https://github.com/npm/npm/commit/aac15eb)
  [#12006](https://github.com/npm/npm/pull/12006)
  Fix typo in npm-orgs documentation, where teams docs went to access docs and vice versa.
  ([@yaelz](https://github.com/yaelz))

#### FEWER NETWORK TESTS

* [`3e41360`](https://github.com/npm/npm/commit/3e41360)
  [#11987](https://github.com/npm/npm/pull/11987)
  Fix test that was inappropriately hitting the network
  ([@yodeyer](https://github.com/yodeyer))

### v3.8.4 (2016-03-24)

Was erroneously released with just a changelog typo correction and was
otherwise the same as 3.8.3.

### v3.8.3 (2016-03-17):

#### SECURITY ADVISORY: BEARER TOKEN DISCLOSURE

This release includes [the fix for a
vulnerability](https://github.com/npm/npm/commit/f67ecad59e99a03e5aad8e93cd1a086ae087cb29)
that could cause the unintentional leakage of bearer tokens.

Here are details on this vulnerability and how it affects you.

##### DETAILS

Since 2014, npm’s registry has used HTTP bearer tokens to authenticate requests
from the npm’s command-line interface. A design flaw meant that the CLI was
sending these bearer tokens with _every_ request made by logged-in users,
regardless of the destination of their request. (The bearers only should have
been included for requests made against a registry or registries used for the
current install.)

An attacker could exploit this flaw by setting up an HTTP server that could
collect authentication information, then use this authentication information to
impersonate the users whose tokens they collected. This impersonation would
allow them to do anything the compromised users could do, including publishing
new versions of packages.

With the fixes we’ve released, the CLI will only send bearer tokens with
requests made against a registry.

##### THINK YOU'RE AT RISK? REGENERATE YOUR TOKENS

If you believe that your bearer token may have been leaked, [invalidate your
current npm bearer tokens](https://www.npmjs.com/settings/tokens) and rerun
`npm login` to generate new tokens. Keep in mind that this may cause continuous
integration builds in services like Travis to break, in which case you’ll need
to update the tokens in your CI server’s configuration.

##### WILL THIS BREAK MY CURRENT SETUP?

Maybe.

npm’s CLI team believes that the fix won’t break any existing registry setups.
Due to the large number of registry software suites out in the wild, though,
it’s possible our change will be breaking in some cases.

If so, please [file an issue](https://github.com/npm/npm/issues/new) describing
the software you’re using and how it broke. Our team will work with you to
mitigate the breakage.

##### CREDIT & THANKS

Thanks to Mitar, Will White & the team at Mapbox, Max Motovilov, and James
Taylor for reporting this vulnerability to npm.

#### PERFORMANCE IMPROVEMENTS

The updated [`are-we-there-yet`](https://npmjs.com/package/are-we-there-yet)
changes how it tracks how complete things are to be much more efficient.
The summary is that `are-we-there-yet` was refactored to remove an expensive
tree walk.

The result for you should be faster installs when working with very large trees.

Previously `are-we-there-yet` computed this when you asked by passing the request down
its tree of progress indicators, totaling up the results. In doing so, it had to walk the
entire tree of progress indicators.

By contrast, `are-we-there-yet` now updates a running total when a change
is made, bubbling that up the tree from whatever branch made progress.  This
bubbling was already going on so there was nearly no cost associated with taking advantage of it.

* [`32f2bd0`](https://github.com/npm/npm/commit/32f2bd0e26116db253e619d67c4feae1de3ad2c2)
  `npmlog@2.0.3`:
  Bring in substantial performance improvements from `are-we-there-yet`.
  ([@iarna](https://github.com/iarna))

#### DUCT TAPE FOR BUGS

* [`473d324`](https://github.com/npm/npm/commit/473d3244a8ddfd6b260d0aa0d395b119d595bf97)
  [#11947](https://github.com/npm/npm/pull/11947)
  Guard against bugs that could cause the installer to crash with errors like:

  ```
  TypeError: Cannot read property 'target' of null
  ```

  This doesn't fix the bugs, but it does at least make the installer less
  likely to explode.
  ([@thefourtheye](https://github.com/thefourtheye))

#### DOC FIXES

* [`ffa428a`](https://github.com/npm/npm/commit/ffa428a4eee482aa620819bc8df994a76fad7b0c)
  [#11880](https://github.com/npm/npm/pull/11880)
  Fix typo in `npm install` documentation.
  ([@watilde](https://github.com/watilde))

#### DEPENDENCY UPDATES

* [`7537fe1`](https://github.com/npm/npm/commit/7537fe1748c27e6f1144b279b256cd3376d5c41c)
  `sorted-object@2.0.0`:
  Create objects with `{}` instead of `Object.create(null)` to make the results
  strictly equal to what, say, parsed JSON would provide.
  ([@domenic](https://github.com/domenic))
* [`8defb0f`](https://github.com/npm/npm/commit/8defb0f7b3ebdbe15c9ef5036052c10eda7e3161)
  `readable-stream@2.0.6`:
  Fix sync write issue on 0.10.
  ([@calvinmetcalf](https://github.com/calvinmetcalf))

#### TEST FIXES FOR THE SELF TESTS

* [`c3edeab`](https://github.com/npm/npm/commit/c3edeabece4400308264e7cf4bc4448bd2729f55)
  [#11912](https://github.com/npm/npm/pull/11912)
  Change the self installation test to do its work in `/tmp`.
  Previously this was installing into a temp subdir in `test/tap`, which
  wouldn't catch the case where a module was installed in the local
  `node_modules` folder but not in dependencies, as node would look up
  the tree and use the copy from the version of npm being tested.
  ([@iarna](https://github.com/iarna))

### v3.8.2 (2016-03-10):

#### HAVING TROUBLE INSTALLING C MODULES ON ANDROID?

This release includes an updated `node-gyp` with fixes for Android.

* [`634ecba`](https://github.com/npm/npm/commit/634ecba320fb5a3287e8b7debfd8b931827b9e19)
  `node-gyp@3.3.1`:
  Fix bug in builds for Android.
  ([@bnoordhuis](https://github.com/bnoordhuis))

#### NPM LOGOUT CLEANS UP BETTER

* [`460ed21`](https://github.com/npm/npm/commit/460ed217876ac78d21477c288f1c06563fb770b4)
  [#10529](https://github.com/npm/npm/issues/10529)
  If you ran `npm logout` with a scope, while we did invalidate your auth
  token, we weren't removing the auth token from your config file. This patch causes
  the auth token to be removed.
  ([@wyze](https://github.com/wyze))

#### HELP MORE HELPFUL

* [`d1d0233`](https://github.com/npm/npm/commit/d1d02335d297da2734b538de44d8967bdcd354cf)
  [#11003](https://github.com/npm/npm/issues/11003)
  Update help to only show command names and their shortcuts. Previously
  some typo corrections were shown, along with various alternate
  spellings.
  ([@watilde](https://github.com/watilde))
* [`47928cd`](https://github.com/npm/npm/commit/47928cd6264e1d6d0ef67435b71c66d01bea664a)
  [#11003](https://github.com/npm/npm/issues/11003)
  Remove "verison" typo from the help listing.
  ([@doug-wade](https://github.com/doug-wade))

#### MORE COMPLETE CONFIG LISTINGS

* [`cf5fd40`](https://github.com/npm/npm/commit/cf5fd401494d96325d74a8bb8c326aa0045a714c)
  [#11472](https://github.com/npm/npm/issues/11472)
  Make `npm config list` include the per-project `.npmrc` in the output.
  ([@mjomble](https://github.com/mjomble))

#### DEPTH LIMITED PARSEABLE DEP LISTINGS

* [`611070f`](https://github.com/npm/npm/commit/611070f0f7a1e185c75cadae46179194084b398f)
  [#11495](https://github.com/npm/npm/issues/11495)
  Made `npm ls --parseable` honor the `--depth=#` option.
  ([@zacdoe](https://github.com/zacdoe))

#### PROGRESS FOR THE (NON) UNICODE REVOLUTION

* [`ff90382`](https://github.com/npm/npm/commit/ff9038227a1976b5e936442716d9877f43c6c9b4)
  [#11781](https://github.com/npm/npm/issues/11781)
  Make the progress bars honor the unicode option.
  ([@watilde](https://github.com/watilde))

#### `npm view --json`, NOW ACTUALLY JSON

* [`24ab70a`](https://github.com/npm/npm/commit/24ab70a4ccfeaa005b80252da313bb589510668e)
  [#11808](https://github.com/npm/npm/issues/11808)
  Make `npm view` produce valid JSON when requested with `--json`.
  Previously `npm view` produced some sort of weird hybrid output, with multiple
  JSON docs.
  ([@doug-wade](https://github.com/doug-wade))

#### DOCUMENTATION CHANGES

* [`6fb0499`](https://github.com/npm/npm/commit/6fb0499bea868fdc637656d210c94f051481ecd4)
  [#11726](https://github.com/npm/npm/issues/11726)
  Previously we patched the `npm update` docs to suggest using `--depth
  Infinity` instead of `--depth 9999`, but that was a mistake. We forgot
  that `npm outdated` (on which `npm update` is built) has a special
  case where it treats `Infinity` as `0`. This reverts that patch.
  ([@GriffinSchneider](https://github.com/GriffinSchneider))
* [`f0bf684`](https://github.com/npm/npm/commit/f0bf684a87ea5eea03432a17f38678fed4960d43)
  [#11748](https://github.com/npm/npm/pull/11748)
  Document all of the various aliases for commands in the documentation
  for those commands.
  ([@watilde](https://github.com/watilde))
* [`fe04443`](https://github.com/npm/npm/commit/fe04443d8988e2e41bd4047078e06a26d05d380d)
  [#10968](https://github.com/npm/npm/issues/10968)
  The `npm-scope` document notes that scopes have been available on the
  public registry for a while. This adds that you'll need `npm@2` or later
  to use them.
  ([@doug-wade](https://github.com/doug-wade))
* [`3db37a5`](https://github.com/npm/npm/commit/3db37a52b2b2e3193ef250ad2cf96dfd2def2777)
  [#11820](https://github.com/npm/npm/pull/11820)
  The command `npm link` should be linking package from local folder to
  global, and `npm link package-name` should be from global to local. The
  description in the documentation was reversed and this fixes that.
  ([@rhgb](https://github.com/rhgb))

#### GLOB FOR THE GLOB THRONE

* [`be55882`](https://github.com/npm/npm/commit/be55882dc4ee5ce0777b4badc9141dab5bf5be4d)
  `glob@7.0.3`:
  Fix a race condition and some windows edge cases.
  ([@isaacs](https://github.com/isaacs))

### v3.8.1 (2016-03-03):

This week the install summary got better, killing your npm process now
also kills the scripts it was running and a rarely used search flag got
documented.

Our improvements on the test suite on Windows are beginning to pick up
steam, you can follow along by
[watching the PR](https://github.com/npm/npm/pull/11444).

#### BETTER INSTALL SUMMARIES

* [`e40d457`](https://github.com/npm/npm/commit/e40d4572cc98db06757df5b8bb6b7dbd0546d3d7)
  [#11699](https://github.com/npm/npm/issues/11699)
  Ensure that flags like `--production` passed to install don't result in
  the summary at the end being incorrectly filtered. That summary is
  produced by the same code as `npm ls` and therefore responds to flags
  the same way it does. This is undesirable when it's an install summary,
  however, as we don't want it to filter anything.

  This fixes an issue where `npm install --production <module>` would
  result in npm exiting with an error code. The `--production` flag would
  make `npm ls` filter out `<module>` as it wasn't saved to the
  `package.json` and thus wasn't a production dependency. The install
  report is limited to show just the modules installed, so with that
  filtered out nothing is available. With nothing available `npm ls`
  would set `npm` to exit with an error code.
  ([@ixalon](https://github.com/ixalon))
* [`99337b4`](https://github.com/npm/npm/commit/99337b469163a4b211b9c6ff1aa9712ae0d601d2)
  [#11600](https://github.com/npm/npm/pull/11600)
  Make the report of installed modules really only show those modules
  that were installed. Previously it selected which modules from your
  tree to display based on `name@version` which worked great when your
  tree was deduped but would list things it hadn't touched when there
  were duplicates.
  ([@iarna](https://github.com/iarna))

#### SCRIPTS BETTER FOLLOW THE LEADER

* [`5454347`](https://github.com/npm/npm/commit/545434766eb3681d3f40b745f9f3187ed63f310a)
  [#10868](https://github.com/npm/npm/pull/10868)
  When running a lifecycle script, say through `npm start`, killing npm
  wouldn't forward that on to the children. It does now.
  ([@daniel-pedersen](https://github.com/daniel-pedersen))

#### SEARCHING SPECIFIC REGISTRIES

* [`6020447`](https://github.com/npm/npm/commit/60204479f76458a9864aa530cda2b3333f95c2b0)
  [#11490](https://github.com/npm/npm/pull/11490)
  Add docs for using the `--registry` flag with search.
  ([@plumlee](https://github.com/plumlee))

#### LODASH UPDATES

* [`bb14204`](https://github.com/npm/npm/commit/bb14204183dad620a6650452a26cdc64111f8136)
  `lodash.without@4.1.1`
  ([@jdalton](https://github.com/jdalton))
* [`0089059`](https://github.com/npm/npm/commit/0089059c562aee9ad0398e55d2c12c68a6150e79)
  `lodash.keys@4.0.5`
  ([@jdalton](https://github.com/jdalton))
* [`6ee1de4`](https://github.com/npm/npm/commit/6ee1de4474d9683a1f7023067d440780eeb10311)
  `lodash.clonedeep@4.3.1`
  ([@jdalton](https://github.com/jdalton))

### v3.8.0 (2016-02-25):

This week brings a quality of life improvement for some Windows users, and
an important knob to be tuned for folks experiencing network problems.

#### LIMIT CONCURRENT REQUESTS

We've long known that `npm`'s tendency to try to request all your
dependencies simultaneously upset some network hardware (particular,
consumer grade routers & proxies of all sorts). One of the reasons that we're
planning to write our own npm specific version of `request` is to be able to
more easily control this sort of thing.

But fortunately, you don't have to wait for that.
[@misterbyrne](https://github.com/misterbyrne) took a look at our existing
code and realized it could be added painlessly TODAY.  The new default
maximum is `50`, instead of `Infinity`.  If you're having network issues you
can try setting that value down to something lower (if you do, please let us
know...  the default is subject to tuning).

* [`910f9ac`](https://github.com/npm/npm/commit/910f9accf398466b8497952bee9f566ab50ade8c)
  [`f7be667`](https://github.com/npm/npm/commit/f7be667548a132ec190ac9d60a31885a7b4fe2b3)
  Add a new config option, `maxsockets` and `npm-registry-client@7.1.0` to
  take advantage of it.
  ([@misterbyrne](https://github.com/misterbyrne))

#### WINDOWS GIT BASH

We think it's pretty keen too, we were making it really hard to actually
upgrade if you were using it. NO MORE!

* [`d60351c`](https://github.com/npm/npm/commit/d60351ccae87d71a5f5eac73e3085c6290b52a69)
  [#11524](https://github.com/npm/npm/issues/11524)
  Prefer locally installed npm in Git Bash -- previous behavior was to use
  the global one.  This was done previously for other shells, but not for Git
  Bash.
  ([@destroyerofbuilds](https://github.com/destroyerofbuilds))

#### DOCUMENTATION IMPROVEMENTS

* [`b63de3c`](https://github.com/npm/npm/commit/b63de3c97c4c27078944249a4d5bbe1c502c23bc)
  [#11636](https://github.com/npm/npm/issues/11636)
  Document `--save-bundle` option in main install page.
  ([@datyayu](https://github.com/datyayu))
* [`3d26453`](https://github.com/npm/npm/commit/3d264532d6d9df60420e985334aebb53c668d32b)
  [#11644](https://github.com/npm/npm/pull/11644)
  Add `directories.test` to the `package.json` documentation.
  ([@lewiscowper](https://github.com/lewiscowper))
* [`b64d124`](https://github.com/npm/npm/commit/b64d12432fdad344199b678d700306340d3607eb)
  [#11441](https://github.com/npm/npm/pull/11441)
  Add a link in documentation to the contribution guidelines.
  ([@watilde](https://github.com/watilde))
* [`82fc548`](https://github.com/npm/npm/commit/82fc548b0e2abbdc4f7968c20b118c30cca79a24)
  [#11441](https://github.com/npm/npm/pull/11441/commits)
  Remove mentions of the long defunct Google group.
  ([@watilde](https://github.com/watilde))
* [`c6ad091`](https://github.com/npm/npm/commit/c6ad09131af2e2766d6034257a8fcaa294184121)
  [#11474](https://github.com/npm/npm/pull/11474)
  Correct invalid JSON in npm-update docs.
  ([@robludwig](https://github.com/robludwig))
* [`4906c90`](https://github.com/npm/npm/commit/4906c90ed2668adf59ebee759c7ebb811aa46e57)
  Expand on the documentation for `bundlededDependencies`, explaining what they are
  and when you might want to use them.
  ([@gnerkus](https://github.com/gnerkus))

#### DEPENDENCY UPDATES

* [`93cdc25`](https://github.com/npm/npm/commit/93cdc25432b71cbc9c25c54ae316770e18f4b01e)
  `strip-ansi@3.0.1`:
  Non-user visible tests & maintainer doc updates.
  ([@jbnicolai](https://github.com/jbnicolai))
* [`3b2ccef`](https://github.com/npm/npm/commit/3b2ccef30dc2038b99ba93cd1404a1d01dac8790)
  `lodash.keys@4.0.4`
  ([@jdalton](https://github.com/jdalton))
* [`30e9eb9`](https://github.com/npm/npm/commit/30e9eb97397a8f85081d328ea9aa54c2a7852613)
  `lodash._baseuniq@4.5.0`
  ([@jdalton](https://github.com/jdalton))


### v3.7.5 (2016-02-22):

A quick fixup release because when I updated glob, I missed the subdep copies of itself
that it installed deeper in the tree. =/

This only effected people trying to update to `3.7.4` from `npm@2` or `npm@1`. Updates from
`npm@3` worked fine (as it fixes up the missing subdeps during installation).

#### OH MY GLOB

* [`63fa704`](https://github.com/npm/npm/commit/63fa7044569127e6e29510dc499a865189806076)
  [#11633](https://github.com/npm/npm/issues/11633)
  When updating the top level `npm` to `glob@7`, the subdeps that
  still depended on `glob@6` got new versions installed but they
  weren't added to the commit. This adds them back in.
  ([@iarna](https://github.com/iarna))

### v3.7.4 (2016-02-18):

I'm ([@iarna](https://github.com/iarna)) back from vacation in the frozen
wastes of Maine!  This release sees a couple of bug fixes, some
documentation updates, a bunch of dependency updates and improvements to our
test suite.

#### FIXES FOR `update`, FIXES FOR `ls`

* [`53cdb96`](https://github.com/npm/npm/commit/53cdb96634fc329378b4ea4e767ba9987986a76e)
  [#11362](https://github.com/npm/npm/issues/11362)
  Make `npm update` stop trying to update linked packages.
  ([@rhendric](https://github.com/rhendric))
* [`8d90d25`](https://github.com/npm/npm/commit/8d90d25b3da086843ce43911329c9572bd109078)
  [#11559](https://github.com/npm/npm/issues/11559)
  Only list runtime dependencies when doing `npm ls --production`.
  ([@yibn2008](https://github.com/yibn2008))

#### @wyze, DOCUMENTATION HERO OF THE PEOPLE, GETS THEIR OWN HEADER

* [`b78b301`](https://github.com/npm/npm/commit/b78b30171038ab737eff0b070281277e35af25b4)
  [#11416](https://github.com/npm/npm/pull/11416)
  Logout docs were using a section copy-pasted from the adduser docs.
  ([@wyze](https://github.com/wyze))
* [`649e28f`](https://github.com/npm/npm/commit/649e28f50aa323e75202eeedb824434535a0a4a0)
  [#11414](https://github.com/npm/npm/pull/11414)
  Add colon for consistency.
  ([@wyze](https://github.com/wyze))

#### WHITTLING AWAY AT PATH LENGTHS

So for all of you who don't know -- Node.js does, in fact, support long Windows
paths. Unfortunately, depending on the tool and the Windows version, a lot of
external tooling does not. This means, for example, that some (all?) versions of
Windows Explorer *can literally never delete npm from their system entirely
because of deeply-nested npm dependencies*. Which is pretty gnarly.

Incidentally, if you run into that in particularly, you can use
[rimraf](npm.im/rimraf) to remove such files 💁.

The latest victim of this issue was the Node.js CI setup for testing on Windows,
which uses some tooling or another that croaks on the usual path length limit
for that OS: 255 characters.

This isn't ordinarily an issue with `npm@3` as it produces mostly flat
trees, but you may be surprised to learn that `npm`'s own distribution isn't
flat, due to needing to be compatible with `npm@1.2`, which ships with
`node@0.8`!

We've taken another baby step towards alleviating this in this release by
updating a couple of dependencies that were preventing `npmlog` from deduping,
and then doing a dedupe on that and `gauge`. Hopefully it helps.

* [`f3c32bc`](https://github.com/npm/npm/commit/f3c32bc3127301741d2fa3a26be6f5f127a35908)
  [#11528](https://github.com/npm/npm/pull/11528)
  `node-gyp@3.3.0`:
  Update to a more recent version that uses a version of npmlog compatible
  with npm itself.  Also adds: AIX support, new `gyp`, `--cafile` command
  line option, and allows configuration of Node.js and io.js mirrors.
  ([@rvagg](https://github.com/rvagg))

#### INTERNAL TEST IMPROVEMENTS

The `npm` core team's time recently has been sunk into `npm`'s many years of
tech debt. Specifically, we've been working on improving the test suite.
This isn't user visible, but in future should mean a more stable, easier to
contribute to `npm`. Ordinarily we don't report these kinds of changes in
the change log, but I thought I might share this week as this chunk is
bigger than usual.

* [`07f020a`](https://github.com/npm/npm/commit/07f020a09e94ae393c67526985444e128ef6f83c)
  [#11292](https://github.com/npm/npm/pull/11292)
  `tacks@1.0.9`:
  Add a package that provides a tool to generate fixtures from folders and, relatedly,
  a module that an create and tear down filesystem fixtures easily.
  ([@iarna](https://github.com/iarna))
* [`0837346`](https://github.com/npm/npm/commit/083734631f9b11b17c08bca8ba8cb736a7b1e3fb)
  [#11292](https://github.com/npm/npm/pull/11292)
  Remove all the relatively cryptic legacy tests and creates new tap tests
  that check the same functionality.  The *legacy* tests were tests that
  were originally a shell script that was ported to javascript early in
  `npm`'s history.
  ([@iarna](https://github.com/iarna))
  ([@zkat](https://github.com/zkat))
* [`5a701e7`](https://github.com/npm/npm/commit/5a701e71a0130787fb98450f9de92117b4ef88e1)
  [#11292](https://github.com/npm/npm/pull/11292)
  Test that we don't leak auth info into the environment.
  ([@zkat](https://github.com/zkat))
* [`502d7d0`](https://github.com/npm/npm/commit/502d7d0628f08b09d8d13538ebccc63de8b3edf5)
  [#11292](https://github.com/npm/npm/pull/11292)
  Test that env vars properly passed into scripts.
  ([@zkat](https://github.com/zkat))
* [`420f267`](https://github.com/npm/npm/commit/420f2672ee8c909f18bee10b1fc7d4ad91cf328b)
  [#11292](https://github.com/npm/npm/pull/11292)
  Test that npm's distribution binary is complete and can be installed and used.
  ([@iarna](https://github.com/iarna))
* [`b7e99be`](https://github.com/npm/npm/commit/b7e99be1b1086f2d6098c653c1e20791269c9177)
  [#11292](https://github.com/npm/npm/pull/11292)
  Test that the `package.json` `files` section and `.npmignore` do what
  they're supposed to.
  ([@zkat](https://github.com/zkat))

#### DEPENDENCY UPDATES

* [`4611098`](https://github.com/npm/npm/commit/4611098fd8c65d61a0645deb05bf38c81300ffca)
  `rimraf@2.5.2`:
  Use `glob@7.0.0`.
  ([@isaacs](https://github.com/isaacs))
* [`41b2772`](https://github.com/npm/npm/commit/41b2772cb83627f3b5b926cf81e150e7148cb124)
  `glob@7.0.0`:
  Raise error if `options.cwd` is specified, and not a directory.
  ([@isaacs](https://github.com/isaacs))
* [`c14e74a`](https://github.com/npm/npm/commit/c14e74ab5d17c764f3aa37123a9632fa965f8760)
  `gauge@1.2.7`: Update to newer lodash versions, for a smaller tree.
  ([@iarna](https://github.com/iarna))
* [`d629363`](https://github.com/npm/npm/commit/d6293630ddc25bfa26d19b6be4fd2685976d7358)
  `lodash.without@4.1.0`
  ([@jdalton](https://github.com/jdalton))
* [`3ea4c80`](https://github.com/npm/npm/commit/3ea4c8049ca8df9f64426b1db8a29b9579950134)
  `lodash.uniq@4.2.0`
  ([@jdalton](https://github.com/jdalton))
* [`8ddcc8d`](https://github.com/npm/npm/commit/8ddcc8deb554660a3f7f474fae9758c967d94552)
  `lodash.union@4.2.0`
  ([@jdalton](https://github.com/jdalton))
* [`2b656a6`](https://github.com/npm/npm/commit/2b656a672d351f32ee2af24dcee528356dcd64f4)
  `lodash.keys@4.0.3`
  ([@jdalton](https://github.com/jdalton))
* [`ac171f8`](https://github.com/npm/npm/commit/ac171f8f0318a7dd3c515f3b83502dfa9e87adb8)
  `lodash.isarguments@3.0.7`
  ([@jdalton](https://github.com/jdalton))
* [`bcccd90`](https://github.com/npm/npm/commit/bcccd9057b75d800c799ab15f00924f700415d3e)
  `lodash.clonedeep@4.3.0`
  ([@jdalton](https://github.com/jdalton))
* [`8165bca`](https://github.com/npm/npm/commit/8165bca537d86305a3d08f080f86223a26615aa8)
  `lodash._baseuniq@4.4.0`
  ([@jdalton](https://github.com/jdalton))

### v3.7.3 (2016-02-11):

Hey all! We've got a pretty small release this week -- just documentation
updates and a couple of dependencies. This release also includes a particular
dependency upgrade that makes it so we're exclusively using the latest version
of `graceful-fs`, which'll make it so things keep working with future Node.js
releases.

A certain internal Node.js API was deprecated and slated for future removal from
Node Core. This API was critical for versions of `graceful-fs@<4`, before a
different approach was used to achieve similar ends. By upgrading this library,
and making sure all our dependencies are also updated, we've ensured npm will
continue to work once the API is finally removed. Older versions of npm, on the
other hand, will simply not work on future versions of Node.js.

#### DEPENDENCY UPGRADES

* [`29536f4`](https://github.com/npm/npm/commit/29536f42da6c06091c9acbc8952f72daa8a9412c)
  `cmd-shim@2.0.2`:
  Final straggler using `graceful-fs@<4`.
  ([@ForbesLindesay](https://github.com/ForbesLindesay))
* [`5f59e74`](https://github.com/npm/npm/commit/5f59e748ef4c066756bb204a452cecd0543c7a2f)
  `lodash.uniq@4.1.0`
  ([@jdalton](https://github.com/jdalton))
* [`987cabe`](https://github.com/npm/npm/commit/987cabe8a18abcb5a685685958bf74c7258a979c)
  `lodash.union@4.1.0`
  ([@jdalton](https://github.com/jdalton))
* [`5c641f0`](https://github.com/npm/npm/commit/5c641f05fdc153c6bb06a89c46fe2a345ce413db)
  `lodash.clonedeep@4.1.0`
  ([@jdalton](https://github.com/jdalton))

#### EVERYONE GETTING SOCKS LIKE IT'S OPRAH'S SHOW

* [`9ea5658`](https://github.com/npm/npm/commit/9ea56582ca4d0991dbed44f992c88f08a643cb4b)
  [#11410](https://github.com/npm/npm/pull/11410)
  Fixed a small spelling error in `npm-config.md`.
  ([@pra85](https://github.com/pra85))
* [`2a11e56`](https://github.com/npm/npm/commit/2a11e562a14bce18b6ddca6c20d17f97b6a8ec2f)
  [#11403](https://github.com/npm/npm/pull/11403)
  Removes `--depth Infinity` warning from documentation -- this operation should
  actually be totally safe as of `npm@3`. (The warning remains for `npm@2`.)
  ([@Aourin](https://github.com/Aourin))
* [`42a4727`](https://github.com/npm/npm/commit/42a4727bfb1e21c890b8e2babda55e06ac2bda29)
  [#11391](https://github.com/npm/npm/pull/11391)
  Fixed versions of `shrinkwrap.json` in examples in documentation for `npm
  shrinkwrap`, which did not quite match up.
  ([@xcatliu](https://github.com/xcatliu))

### v3.7.2 (2016-02-04):

This week, the CLI team has been busy working on rewriting tests to support
getting coverage reports going and running all of our tests on Windows.
Meanwhile, we've got a bunch of dependency updates and one or two other
things.

#### TESTS WENT INTO HIDING

Last week we took a patch from [@substack](https://github.com/substack) to
stop the installer from reordering arrays in an installed module's
`package.json`...  but somehow I dropped the test when I was rebasing.

* [`21b9271`](https://github.com/npm/npm/commit/21b927182514a0ff6d9f34480bfc39f72e3e9f8c)
  [#10063](https://github.com/npm/npm/issues/10063)
  Restore test that verifies that we don't re-order arrays in a module's
  `package.json` on install.
  ([@substack](https://github.com/substack))

#### DOCUMENTATION FIXES

* [`c67521d`](https://github.com/npm/npm/commit/c67521dc6c1e41d39d02c74105e41442851d23bb)
  [#11348](https://github.com/npm/npm/pull/11348)
  Improve the documentation around which files are ALWAYS included in published packages
  and which are ALWAYS excluded.
  ([@jscissr](https://github.com/jscissr))
* [`7ef6793`](https://github.com/npm/npm/commit/7ef6793cd191cc8d88340f7e1ce9c9e3d6f0b2f4)
  [#11348](https://github.com/npm/npm/pull/11348)
  The release date on the 3.7.0 changelog entry was wrong.  I honestly don't
  know how I keep doing this. =D
  ([@rafek](https://github.com/rafek))

#### DEPENDENCY UPDATES

* [`8a3c80c`](https://github.com/npm/npm/commit/8a3c80c4fd3d82fe937f30bc7cbd3dee51a8a893)
  `graceful-fs@4.1.3`:
  Fix a bug where close wasn't getting made graceful.
  ([@isaacs](https://github.com/isaacs))

`lodash` saw updates across most of its modules this week with browser
campatibility fixes that don't really impact us.

* [`2df342b`](https://github.com/npm/npm/commit/2df342bf30efa99b98016acc8a5dc03e00b58b9c)
  `lodash.without@4.0.2`
  ([@jdalton](https://github.com/jdalton))
* [`86aa91d`](https://github.com/npm/npm/commit/86aa91dce60f6b6a92bb3ba2bf6e6be1f6afc750)
  `lodash.uniq@4.0.2`
  ([@jdalton](https://github.com/jdalton))
* [`0a94bf6`](https://github.com/npm/npm/commit/0a94bf6af0ebd38d080f92257e0cd9bae40b31ff)
  `lodash.union@4.0.2`
  ([@jdalton](https://github.com/jdalton))
* [`b4c9582`](https://github.com/npm/npm/commit/b4c9582b4ef5991f3d155e0c6142ed1c631860af)
  `lodash.isarguments@3.0.6`
  ([@jdalton](https://github.com/jdalton))
* [`efe766c`](https://github.com/npm/npm/commit/efe766c63c0948a4ae4c0d12f2b834629ab86e92)
  `lodash.keys@4.0.2`: Minor code cleanup and the above.
  ([@jdalton](https://github.com/jdalton))
* [`36abb24`](https://github.com/npm/npm/commit/36abb24ef31017adbf325e7f833d5d4b0f03f5d4)
  `lodash.clonedeep@4.0.4`:
  Add support for cloning prototype objects and the above.
  ([@jdalton](https://github.com/jdalton))

### v3.7.1 (2016-02-01):

Super quick Monday patch on last week's release.

If you ever wondered why we release things to the `npm@next` tag for a week
before promoting them to `npm@latest`, this is it!

#### RELEASE TRAIN VINDICATED (again)

* [`adcaf04`](adcaf047811dcc475ab1984fc93fe34540fc03d7)
  [#11349](https://github.com/npm/npm/issues/11349)
  Revert last weeks change to use JSON clone instead of `lodash.cloneDeep`.
  ([@iarna](https://github.com/iarna))

### v3.7.0 (2016-01-29):

Hi all! This week brings us some important performance improvements,
support for git submodules(!) and a bunch of bug fixes.

#### PERFORMANCE

`gauge`, the module responsible for drawing `npm`'s progress bars, had an
embarrassing bug in its debounce implementation that resulted in it, on many
systems, actually being _slower_ than if it hadn't been debouncing. This was
due to it destroying and then creating a timer object any time it got an
update while waiting on its minimum update period to elapse. This only was
a measurable slowdown when sending thousands of updates a second, but
unfortunately parts of `npm`'s logging do exactly that. This has been patched
to eliminate that churn, and our testing shows the progress bar as being
eliminated as a source of slow down.

Meanwhile, `are-we-there-yet` is the module that tracks just how complete
our big asynchronous install process is. [@STRML](https://github.com/STRML)
spent some time auditing its source and made a few smaller performance
improvements to it. Most impactful was eliminating a bizarre bit of code
that was both binding to AND closing over the current object. I don't have
any explanation for how that crept in. =D

* [`c680fa9`](https://github.com/npm/npm/commit/c680fa9f8135759eb5512f4b86e47fa265733f79)
  `npmlog@2.0.2`: New `are-we-there-yet` with performance patches from
  [@STRML](https://github.com/STRML). New `gauge` with timer churn
  performance patch.
  ([@iarna](https://github.com/iarna))

We were also using `lodash`'s `cloneDeep` on `package.json` data which is
definitely overkill, seeing as `package.json` data has all the restrictions
of being `json`. The fix for this is just swapping that out for something
that does a pair of `JSON.stringify`/`JSON.parse`, which is distinctly more
speedy.

* [`1d1ea7e`](https://github.com/npm/npm/commit/1d1ea7eeb958034878eb6573149aeecc686888d3)
  [#11306](https://github.com/npm/npm/pull/11306)
  Use JSON clone instead of `lodash.cloneDeep`.
  ([@STRML](https://github.com/STRML))

#### NEW FEATURE: GIT SUBMODULE SUPPORT

Long, long requested– the referenced issue is from 2011– we're finally
getting rudimentary git submodule support.

* [`39dea9c`](https://github.com/npm/npm/commit/39dea9ca4216c6ea628f5ca47d2b34a4b251a1ed)
  [#1876](https://github.com/npm/npm/issues/1876)
  Add support for git submodules in git remotes. This is a fairly simple
  approach, which does not leverage the git caching mechanism to cache
  submodules. It also doesn't provide a means to disable automatic
  initialization, e.g. via a setting in the `.gitmodules` file.
  ([@gagern](https://github.com/gagern))

#### ROBUSTNESS

* [`5dec02a`](https://github.com/npm/npm/commit/5dec02a3d0e82202c021e27aff9d006283fdc25a)
  [#10347](https://github.com/npm/npm/issues/10347)
  There is an obscure feature that lets you monkey-patch npm when it starts
  up. If the module being required with this feature failed, it would
  previously just make `npm` error out– this reduces that to a warning.
  ([@evanlucas](https://github.com/evanlucas))

#### BUG FIXES

* [`9ab8b8d`](https://github.com/npm/npm/commit/9ab8b8d047792612ae7f9a6079745d51d5283a53)
  [#10820](https://github.com/npm/npm/issues/10820)
  Fix a bug with `npm ls` where if you asked for ONLY production dependencies in output
  it would exclude dependencies that were BOTH production AND development dependencies.
  ([@davidvgalbraith](https://github.com/davidvgalbraith))
* [`6803fed`](https://github.com/npm/npm/commit/6803fedadb8f9b36cd85f7338ecf75d1d183c833)
  [#8982](https://github.com/npm/npm/issues/8982)
  Fix a bug where, under some circumstances, if you had a path that
  contained the name of a package being installed somewhere in it, `npm`
  would incorrectly refuse to run lifecycle scripts.
  ([@elvanja](https://github.com/elvanja))
* [`3eae40b`](https://github.com/npm/npm/commit/3eae40b7a681aa067dfe4fea8c9a76da5b508b48)
  [#9253](https://github.com/npm/npm/issues/9253)
  Fix a bug where, when running lifecycle scripts, if the Node.js binary you ran
  `npm` with wasn't in your `PATH`, `npm` wouldn't use it to run your scripts.
  ([@segrey](https://github.com/segrey))
* [`61daa6a`](https://github.com/npm/npm/commit/61daa6ae8cbc041d3a0d8a6f8f268b47dd8176eb)
  [#11014](https://github.com/npm/npm/issues/11014)
  Fix a bug where running `rimraf node_modules/<package>` followed by `npm
  rm --save <package>` would fail. `npm` now correctly removes the module
  from your `package.json` even though it doesn't exist on disk.
  ([@davidvgalbraith](https://github.com/davidvgalbraith))
* [`a605586`](https://github.com/npm/npm/commit/a605586df134ee97c95f89c4b4bd6bc73f7aa439)
  [#9679](https://github.com/npm/npm/issues/9679)
  Fix a bug where `npm install --save git+https://…` would save a `https://`
  url to your `package.json` which was a problem because `npm` wouldn't then
  know that it was a git repo.
  ([@gagern](https://github.com/gagern))
* [`bbdc700`](https://github.com/npm/npm/commit/bbdc70024467c365cc4e06b8410947c04b6f145b)
  [#10063](https://github.com/npm/npm/issues/10063)
  Fix a bug where `npm` would change the order of array properties in the
  `package.json` files of dependencies.  `npm` adds a bunch of stuff to
  `package.json` files in your `node_modules` folder for debugging and
  bookkeeping purposes.  As a part of this process it sorts the object to
  reduce file churn when it does updates.  This fixes a bug where the arrays
  in the object were also getting sorted.  This wasn't a problem for
  properties that `npm` itself maintains, but _is_ a problem for properties
  used by other packages.
  ([@substack](https://github.com/substack))

#### DOCS IMPROVEMENTS

* [`2609a29`](https://github.com/npm/npm/commit/2609a2950704f577ac888668e81ba514568fab44)
  [#11273](https://github.com/npm/npm/pull/11273)
  Include an example of viewing package version history in the `npm view` documentation.
  ([@vedatmahir](https://github.com/vedatmahir))
* [`719ea9c`](https://github.com/npm/npm/commit/719ea9c45a5c3233f3afde043b89824aad2df0a7)
  [#11272](https://github.com/npm/npm/pull/11272)
  Fix typographical issue in `npm update` documentation.
  ([@jonathanp](https://github.com/jonathanp))
* [`cb9df5a`](https://github.com/npm/npm/commit/cb9df5a37091e06071d8704b629e7ebaa41c37fe)
  [#11215](https://github.com/npm/npm/pull/11215)
  Do not call `SEE LICENSE IN <filename>` an _SPDX expression_, as it's not.
  ([@kemitchell](https://github.com/kemitchell))
* [`f427934`](https://github.com/npm/npm/commit/f4279346c368da4bca09385f773e8eed1d389e5e)
  [#11196](https://github.com/npm/npm/pull/11196)
  Correct the `package.json` examples in the `npm update` documentation to actually be
  valid JSON and not just JavaScript object literals.
  ([@s100](https://github.com/s100))

#### DEPENDENCY UPDATES

* [`a7b2407`](https://github.com/npm/npm/commit/a7b24074cb59a1ab17c0d8eff1498047e6a123e5)
  `retry@0.9.0`: New features and interface agnostic refactoring.
  ([@tim-kos](https://github.com/tim-kos))
* [`220fc77`](https://github.com/npm/npm/commit/220fc7702ae3e5d601dfefd3e95c14e9b32327de)
  `request@2.69.0`:
  A bunch of small bug fixes and module updates.
  ([@simov](https://github.com/simov))
* [`9e5c84f`](https://github.com/npm/npm/commit/9e5c84f1903748897e54f8ff099729ff744eab0f)
  `which@1.2.4`:
  Update `isexe` and fix bug in `pathExt`, in which files without extensions
  would sometimes be preferred to files with extensions on Windows, even though
  those without extensions aren't executable.
  `pathExt` is a list of extensions that are considered executable (exe, cmd,
  bat, com on Windows).
  ([@isaacs](https://github.com/isaacs))
* [`375b9c4`](https://github.com/npm/npm/commit/375b9c42fe0c6de47ac2f92527354b2ea79b7968)
  `rimraf@2.5.1`: Minor doc formatting fixes.
  ([@isaacs](https://github.com/isaacs))
* [`ef1971e`](https://github.com/npm/npm/commit/ef1971e6270c2bc72e6392b51a8b84f52708f7e7)
  `lodash.clonedeep@4.0.2`:
  Misc minor code cleanup. No functional changes.
  ([@jdalton](https://github.com/jdalton))

### v3.6.0 (2016-01-20):

Hi all!  This is a bigger release, in part 'cause we didn't have one last
week. The most important thing you need to know is that when `npm@3.6.0` replaces
`npm@3.5.4` as `next`, `npm@3.5.4` WILL NOT be moved on to `latest`. This is due to
a packaging error that tickles bugs in some earlier releases and makes upgrades to it
from those versions break the install.

#### NEW FEATURES‼

* [`ff504d4`](https://github.com/npm/npm/commit/ff504d449ea1fa996cbb02c8078964643c51e5f6)
  [#8752](https://github.com/npm/npm/issues/8752)
  In `npm outdated`, report symlinked packages as having a wanted & latest
  version of `linked`.
  ([@halhenke](https://github.com/halhenke))
* [`f44d8c9`](https://github.com/npm/npm/commit/f44d8c9a3940f7041f8136f8754a54b13f1f9d60)
  [#10775](https://github.com/npm/npm/issues/10775)
  Add a success message to `adduser` / `login`.
  ([@ekmartin](https://github.com/ekmartin))
* [`3109303`](https://github.com/npm/npm/commit/310930395c9bf1577cf085b9742210bfc71bb019)
  [#10043](https://github.com/npm/npm/pull/10043)
  Warn if you try to use `npm run x` if you don't have a `node_modules` folder, since
  whatever you're trying to do _probably_ won't work.
  ([@timkrins](https://github.com/timkrins))

* [`9ed2849`](https://github.com/npm/npm/commit/9ed2849cd7e8cc97111dca42a940905284afe55d)
  [`e9f1ad8`](https://github.com/npm/npm/commit/e9f1ad88ce58ecd111811e11afa52ac19fc8696e)
  [`f10d300`](https://github.com/npm/npm/commit/f10d300e5effa7a5756c8d461eef284c283a41d1)
  [`8b593d8`](https://github.com/npm/npm/commit/8b593d8d187d6ac85d2a59cbe647afb5516c1b94)
  [#10717](https://github.com/npm/npm/pull/10717)
  `npm version` can now take a `from-git` argument, which instructs `npm` to read the
  version from git and update your `package.json` to what it finds. This is in contrast
  to its normal use where `npm` _tells_ git about your new version.
  ([@ekmartin](https://github.com/ekmartin))

#### 3.5.4 WAS NOT SO GREAT

The `npm@3.5.4` package was missing some dependencies.  Specifically, `glob`
and `has-unicode` had major release updates which meant that subdeps that
relied on older major versions couldn't use the npm supplied versions any
more, and so they needed their own copies.

This went undetected because the actions necessary to run the tests (which
check for this sort of thing) resolved the missing modules.

Further, it didn't have symptoms when upgrading from _most_ versions of npm.
Unfortunately, some versions had bugs that were tickled by this and resulted
in broken upgrades, most notably, `npm@3.3.12`, the version that's been in
Node.js 5.

* [`1d3325c`](https://github.com/npm/npm/commit/1d3325c040621a4792db80fb232f4994b9d5c5f2)
  [`02611c6`](https://github.com/npm/npm/commit/02611c673a4d2bbe8fcef8d48407768da31c90d2)
  [`39d5fea`](https://github.com/npm/npm/commit/39d5feadefdde38d75a18f23343bc6ec37153638)
  [`7d0e830`](https://github.com/npm/npm/commit/7d0e830f26c73b9d9277b29949227ba9cca27fd9)
  [#11129](https://github.com/npm/npm/pull/11129)
  Update the underlying dependencies to allow use for the new versions of
  `glob` and `has-unicode`.
  ([@iarna](https://github.com/iarna))

#### WHEN MISSING PATHS ARE OK

* [`bb638fa`](https://github.com/npm/npm/commit/bb638fa4f48d24d2c9935861d5d751c5621eea49)
  [#11212](https://github.com/npm/npm/pull/11212)
  When trying to determine if a file was controlled by npm before going to
  remove it, we check to see if it is inside any of a list of paths that npm
  considers to be under its control.  Not all of those paths always exist
  (and that's ok!) Previously we were calling it a failure to match if ANY
  of them didn't exist.  We now only do so if NONE of them exist.  If some
  do, then we do our usual checks on them.

  This showed up as an error where you would see something like:
  ```
  npm warn gentlyRm not removing /path/to/thing as it wasn't installed by /path/to/other/thing
  ```
  But it totally was installed by it.
  ([@iarna](https://github.com/iarna))

#### BETTER NODE PRE-RELEASE SUPPORT

Historically, if you used a pre-release version of Node.js, you would get
dozens and dozens of warnings when EVERY engine check failed across all of
your modules, because `>= 0.10.0` doesn't match prereleases.

You might find this stream of redundent warnings undesirable. I do.

We've moved this into a SINGLE warning you'll get about using a pre-release
version of Node.js and now suppress those other warnings.

* [`6952f79`](https://github.com/npm/npm/commit/6952f7981e451a2d599a4f513573af208bdfe103)
  [#11212](https://github.com/npm/npm/pull/11212)
  Engine check warnings are now issued along with any other warnings about
  your tree, instead of emitting in the middle of your install (and then
  disappearing behind the giant tree of stuff installed).
  ([@iarna](https://github.com/iarna))
* [`ee2ebe9`](https://github.com/npm/npm/commit/ee2ebe96fb3d105787835b72085bbd2eee66a629)
  [#11212](https://github.com/npm/npm/pull/11212)
  Suppress engine verification warnings about pre-release versions of Node.js.
  ([@iarna](https://github.com/iarna))
* [`135b7e0`](https://github.com/npm/npm/commit/135b7e078311e8b4e2c8e2b662eed9ba6c2e2537)
  [#11212](https://github.com/npm/npm/pull/11212)
  Explicitly warn, in only one place, if you are using a pre-release version
  of Node.js.
  ([@iarna](https://github.com/iarna))

#### BUG FIXES

* [`ea331c8`](https://github.com/npm/npm/commit/ea331c82157c65f7643cd4b49fd24031c84bf601)
  [#10938](https://github.com/npm/npm/issues/10938)
  When removing a package, sometimes the `node_modules/.bin` wouldn't be
  cleaned up entirely.  This would result in package folders that contained
  only a `node_modules/.bin` directory.  In turn, this would result in `npm
  ls` and other tools complaining about these broken directories.
  To fix this, the `unbuild` step now explicitly deletes the
  `node_modules/.bin` folder as its final step.
  ([@chrisirhc](https://github.com/chrisirhc))
* [`00720db`](https://github.com/npm/npm/commit/00720db2c326cf8f968c662444a4575ae8c3020a)
  [#11158](https://github.com/npm/npm/pull/11158)
  On Windows, the `node-gyp` wrapper would fail if your path to `node-gyp`
  contained spaces. This fixes that problem by quoting use of that path.
  ([@orangemocha](https://github.com/orangemocha))
* [`69ac933`](https://github.com/npm/npm/commit/69ac9333506752bf2e5af70b3b3e03c6181de3e7)
  [#11142](https://github.com/npm/npm/pull/11142)
  Fix a race condition when making directories in the cache, which could
  lead to `ENOENT` failures.
  ([@Jimbly](https://github.com/Jimbly))
* [`e982858`](https://github.com/npm/npm/commit/e982858d9bed65cede9cbb12df9216a4bb9e6fc9)
  [#9696](https://github.com/npm/npm/issues/9696)
  When replacing the `package.json` in the cache you sometimes see `EPERM` errors on
  Windows that you wouldn't on Unix-like operating systems. This ignores those errors
  and allows Windows to continue. Longer term, we'll be adding something to retry
  these errors, but ultimately fail if there really is an ongoing permissions issue.
  ([@orangemocha](https://github.com/orangemocha))

#### DOC CHANGES

* [`3666081`](https://github.com/npm/npm/commit/3666081abd02184ba97a7cdb6ae238085d640b4b)
  [#11188](https://github.com/npm/npm/pull/11188)
  Add brief description to publish documentation of what's included in
  published tarballs.
  ([@beaugunderson](https://github.com/beaugunderson))
* [`b463e34`](https://github.com/npm/npm/commit/b463e3424b296cfc4bd384fc8bfe0e2329649164)
  [#11150](https://github.com/npm/npm/pull/11150)
  In npm update docs, advise use of `--depth Infinity` instead of `--depth
  9999`.
  ([@halhenke](https://github.com/halhenke))
* [`382e71a`](https://github.com/npm/npm/commit/382e71a7ee5d1ca3dba55c1e753d529eb8ae6895)
  [#11128](https://github.com/npm/npm/pull/11128)
  In the `package.json` docs, make the reference to the "Local Paths" section
  a link to it as well.
  ([@orangejulius](https://github.com/orangejulius))
* [`5277e7f`](https://github.com/npm/npm/commit/5277e7f236e8cb40d7f4a1054506f2d3d159716e)
  [#11090](https://github.com/npm/npm/pull/11090)
  Fix the 3.5.4 release date in CHANGELOG.md.
  ([@ashleygwilliams](https://github.com/ashleygwilliams))
* [`e6d238a`](https://github.com/npm/npm/commit/e6d238a3d90beeb0af23fa75a9b5e50671d6e4c5)
  [#11130](https://github.com/npm/npm/pull/11130)
  Eliminate the "using npm programmatically" section from the README. The
  documentation for this was removed a while ago and is unsupported.
  ([@ljharb](https://github.com/ljharb))

#### DEPENDENCY UPDATES

* [`b0dde5c`](https://github.com/npm/npm/commit/b0dde5c3407b58d78969d3da01af2629fcba1c73)
  `config-chain@1.1.10`: Update tests for most recent version of `ini`.
  ([@dominictarr](https://github.com/dominictarr))
* [`c62f414`](https://github.com/npm/npm/commit/c62f414534971761a48ce3cbc3e25214fb09e494)
  `glob@6.0.4`: Eliminated use of `util._extend`.
  ([@isaacs](https://github.com/isaacs))
* [`98a6779`](https://github.com/npm/npm/commit/98a67797978ed7ce534e16b705d3a2a9ca0e6cc1)
  `lodash.clonedeep@4.0.1`: Bug fixes, including the non-linear performance
  that was biting npm a while back.
  ([@jdalton](https://github.com/jdalton))
* [`0e8c4ce`](https://github.com/npm/npm/commit/0e8c4cebddaefbf5eca0abaad512db266c6722c9)
  `lodash.without@4.0.1`
  ([@jdalton](https://github.com/jdalton))
* [`1fd19f5`](https://github.com/npm/npm/commit/1fd19f57a3551d7d30a6b8a9ce967ef50e0ff0ba)
  `lodash.uniq@4.0.1`
  ([@jdalton](https://github.com/jdalton))
* [`b7486c5`](https://github.com/npm/npm/commit/b7486c550f3391f733d1e1907652be95fddf4368)
  `lodash.union@4.0.1`
  ([@jdalton](https://github.com/jdalton))
* [`54bb591`](https://github.com/npm/npm/commit/54bb5911e18f8fb86eb94159f34b13f0c0aa2e30)
  `lodash.keys@4.0.0`
  ([@jdalton](https://github.com/jdalton))
* [`26f7a7a`](https://github.com/npm/npm/commit/26f7a7aaae0575a85deba2241ee69b433dd1ba98)
  `lodash.isarray@4.0.0`
  ([@jdalton](https://github.com/jdalton))
* [`ed38bd3`](https://github.com/npm/npm/commit/ed38bd3baf544dfc0630fd321d279f137700bd4d)
  `lodash.isarguments@3.0.5`
  ([@jdalton](https://github.com/jdalton))

### v3.5.4 (2016-01-07):

I hope you all had fantastic winter holidays, if it's winter where you are
and if there are holidays‼ We went a few weeks without releases because
staff was taking time away from work here and there.  A new year has come
and we're back now, and refreshed and ready to dig in!

This week brings us a bunch of documentation improvements and some module
updates.  The core team's focus continues to be on improving tests,
particularly with Windows, so there's not too much to call out here.

#### DOCUMENTATION IMPROVEMENTS

* [`6b0031e`](https://github.com/npm/npm/commit/6b0031e28c0b10fb2622fdadde41f5cd294348e8)
  [#11044](https://github.com/npm/npm/pull/11044)
  Correct documentation regarding the defaults for the `color` config option.
  ([@scottaddie](https://github.com/scottaddie))
* [`c6ce69e`](https://github.com/npm/npm/commit/c6ce69eaed7f17b5f1876ac13ecfae3d14a72f24)
  [#10990](https://github.com/npm/npm/pull/10990)
  Drop mentions in documentation of `process.installPrefix`, as it hasn't
  been a thing since Node.js 0.6 and we don't support that.
  ([@jeffmcmahan](https://github.com/jeffmcmahan))
* [`dee92d1`](https://github.com/npm/npm/commit/dee92d1f78608a10becf57aae86d5d495f2272bd)
  [#11037](https://github.com/npm/npm/pull/11037)
  Clarify the documentation on the max length of the `name` property in
  `package.json` files.
  ([@scottaddie](https://github.com/scottaddie))
* [`4b9d7bb`](https://github.com/npm/npm/commit/4b9d7bb1a4fc3f1edcf563379abfd2273af10881)
  [#10787](https://github.com/npm/npm/pull/10787)
  Make the formatting in the documentation for `npm dist-tag` more
  consistent with other docs.
  ([@cvrebert](https://github.com/cvrebert))
* [`7f77a80`](https://github.com/npm/npm/commit/7f77a80d561ee4b2b8c0aba1226fe89dfe339bcd)
  [#10787](https://github.com/npm/npm/pull/10787)
  Add documentation to the `npm dist-tag` docs that explains in greater
  detail how `latest` is different than other tags.  Further, improve the
  documentation with better examples.  Add a discussion of common practice
  for using dist tags to manage alpha's and beta's.
  ([@cvrebert](https://github.com/cvrebert))
* [`6db58dd`](https://github.com/npm/npm/commit/6db58dd0d7719c4675a239d43164edc066842b14)
  [`2ee6371`](https://github.com/npm/npm/commit/2ee6371911bd3a4d566c5d7bc8734facc60cb27c)
  [#10788](https://github.com/npm/npm/pull/10788)
  [#10789](https://github.com/npm/npm/pull/10789)
  Improve documentation cross referencing.
  ([@cvrebert](https://github.com/cvrebert))
* [`7ba629a`](https://github.com/npm/npm/commit/7ba629a2ad3eaf736529e053b533cabe3a0d7123)
  [#10790](https://github.com/npm/npm/pull/10790)
  Document more clearly that `npm install foo` means `npm install
  foo@latest`.
  ([@cvrebert](https://github.com/cvrebert))

#### A FEW MODULE UPDATES

* [`fc2e8d5`](https://github.com/npm/npm/commit/fc2e8d58a91728cb06936eea686efaa4fdec3f06)
  `glob@6.0.3`: Remove deprecated features and fix a bunch of bugs.
  ([@isaacs](https://github.com/isaacs))
* [`5b820c4`](https://github.com/npm/npm/commit/5b820c4e17c907fa8c23771c0cd8e74dd5fdaa51)
  `has-unicode@2.0.0`: Change the default on Windows to be false, as
  international Windows installs often install to non-unicode codepages and
  there's no way to detect this short of a system call or a call to a
  command line program.
  ([@iarna](https://github.com/iarna))
* [`238fe84`](https://github.com/npm/npm/commit/238fe84ac61297f1d71701d80368afaa40463305)
  `which@1.2.1`: Fixed bugs with uid/gid checks and with quoted Windows PATH
  parts.
  ([@isaacs](https://github.com/isaacs))
* [`5e510e1`](https://github.com/npm/npm/commit/5e510e13d022a22d58742b126482d3b38a14cc83)
  `rimraf@2.5.0`: Add ability to disable glob support / pass in options.
  ([@isaacs](https://github.com/isaacs))
* [`7558215`](https://github.com/npm/npm/commit/755821569466b7be0883f4b0573eeb83c24109eb)
  `readable-stream@2.0.5`: Minor performance improvements.
  ([@calvinmetcalf](https://github.com/calvinmetcalf))
* [`64e8499`](https://github.com/npm/npm/commit/64e84992c812a73d590be443c09a6977d0ae9040)
  `fs-write-stream-atomic@1.0.8`: Rewrite to use modern streams even on 0.8
  plus a bunch of tests.
  ([@iarna](https://github.com/iarna))
* [`74d92a0`](https://github.com/npm/npm/commit/74d92a08d72ce3603244de4bb3e3706d2b928cef)
  `columnify@1.5.4`: Some bug fixes around large inputs.
  ([@timoxley](https://github.com/timoxley))

#### FIX NPM'S TESTS ON 0.8

This doesn't impact you as a user of npm, and ordinarily that means we
wouldn't call it out here, but if you've ever wanted to contribute, having
that green travis badge makes it a lot easier to do so with confidence!

* [`b14cdbb`](https://github.com/npm/npm/commit/b14cdbb6002b04bfbefaff70cc45810c20d5a366)
  [#10872](https://github.com/npm/npm/pull/10872)
  Rewrite tests using nock to use other alternatives.
  ([@zkat](https://github.com/zkat))
* [`59ed01a`](https://github.com/npm/npm/commit/59ed01a8ea7960b1467aed52164fc36a03c77770)
  [#10872](https://github.com/npm/npm/pull/10872)
  Work around Node.js 0.8 http back-pressure bug.

  0.8 http streams have a bug, where if they're paused with data in their
  buffers when the socket closes, they call `end` before emptying those
  buffers, which results in the entire pipeline ending and thus the point
  that applied backpressure never being able to trigger a `resume`.

  We work around this by piping into a pass through stream that has
  unlimited buffering.  The pass through stream is from readable-stream and
  is thus a current streams3 implementation that is free of these bugs even
  on 0.8.
  ([@iarna](https://github.com/iarna))

### v3.5.3 (2015-12-10):

Did you know that Bob Ross reached the rank of master sergeant in the US Air
Force before becoming perhaps the most soothing painter of all time?

#### TWO HAPPY LITTLE BUG FIXES

* [`71c9590`](https://github.com/npm/npm/commit/71c9590be61b6a7b7fa8b6dc19baa588cda26a27)
  [#10505](https://github.com/npm/npm/issues/10505) `npm ls --json --depth=0`
  now respects the depth parameter, when it is zero and when it is not zero.
  ([@MarkReeder](https://github.com/MarkReeder))
* [`954fa67`](https://github.com/npm/npm/commit/954fa67f1ca3739992abd244e217a0aaf8465660)
  [#9099](https://github.com/npm/npm/issues/9099) I had always thought you
  could run `npm version` from subdirectories in your project, which is great,
  because now you can. I guess I was just ahead of my time.
  ([@ekmartin](https://github.com/ekmartin))

#### NOW PAINT IN SOME NICE DOCS CHANGES

* [`b88c37c`](https://github.com/npm/npm/commit/b88c37c1cced40e9e41402cc54a5efc3c33cd13e)
  [#10546](https://github.com/npm/npm/issues/10546) Goodbye, FAQ! You were
  cheeky and fun until you weren't! Don't worry: npm still loves everyone,
  especially you! ([@ashleygwilliams](https://github.com/ashleygwilliams))
* [`2d3afe9`](https://github.com/npm/npm/commit/2d3afe9644ba69681a36721e79c45d27def71939)
  [#10570](https://github.com/npm/npm/issues/10570) Update documentation URLs
  to be HTTPS everywhere sensible. No HTTP shall be spared!
  ([@rsp](https://github.com/rsp))
* [`6abd0e0`](https://github.com/npm/npm/commit/6abd0e0626d0f642ce0dae0e128ced80433f15a1)
  [#10650](https://github.com/npm/npm/issues/10650) Correctly note that there
  are two lifecycle scripts run by an install phase in an example, instead of
  three. ([@eymengunay](https://github.com/eymengunay))
* [`a5e8df5`](https://github.com/npm/npm/commit/a5e8df53b8d6d75398cb6a55a44dcf374b0f1661)
  [#10687](https://github.com/npm/npm/issues/10687) `npm outdated`'s output can
  be a little puzzling sometimes. I've attempted to make it clearer, with some
  examples, of what's going on with "wanted" and "latest" in more cases.
  ([@othiym23](https://github.com/othiym23))
* [`8f52833`](https://github.com/npm/npm/commit/8f52833f5d15c4f94467234607d40e75198af1aa)
  [#10700](https://github.com/npm/npm/issues/10700) Hey, do you remember when
  `search.npmjs.org` was a thing? I think I do? The last time I used it was in
  like 2012, and it's gone now, so remove it from the docs.
  ([@gagern](https://github.com/gagern))
* [`b6a53b8`](https://github.com/npm/npm/commit/b6a53b889c948053dcbf6d7aab9ad1cd4226dc32)
  [npm/docs#477](https://github.com/npm/docs/issues/477) Continue to airbrush
  the CLI API docs out of history. ([@verpixelt](https://github.com/verpixelt))
* [`b835b72`](https://github.com/npm/npm/commit/b835b72d1dd23b0a17321a85d8d395322d18005d)
  `semver@5.1.0`: Include BNF for SemVer expression grammar (which is also now
  included in `npm help semver`). ([@isaacs](https://github.com/isaacs))

#### LAND YOUR DEPENDENCY UPGRADES IN PAIRS SO EVERYONE HAS A FRIEND

* [`95e99fa`](https://github.com/npm/npm/commit/95e99faadcdc85a16210dd79c0e7d83add1b9f3e)
  `request@2.67.0` ([@simov](https://github.com/simov))
* [`b49199a`](https://github.com/npm/npm/commit/b49199ac96dfb1afe5719286621a318576dd69ae)
  [isaacs/rimraf#89](https://github.com/isaacs/rimraf/pull/89) `rimraf@2.4.4`
  ([@zerok](https://github.com/zerok))
* [`6632418`](https://github.com/npm/npm/commit/66324189a734a1665e1b78a06ba44089d9c3a11c)
  [npm/nopt#51](https://github.com/npm/nopt/pull/51) `nopt@3.0.6`
  ([@wbecker](https://github.com/wbecker))
* [`f0a3b3e`](https://github.com/npm/npm/commit/f0a3b3e0dbbdaf11ec55dccd59cc21bfa05f9240)
  [isaacs/once#7](https://github.com/isaacs/once/pull/7) `once@1.3.3`
  ([@floatdrop](https://github.com/floatdrop))

### v3.5.2 (2015-12-03):

Weeeelcome to another npm release! The short version is that we fixed
some `ENOENT` and some modules that resulted in modules going missing. We
also eliminated the use of MD5 in our code base to help folks using
Node.js in FIPS mode. And we fixed a bad URL in our license file.

#### FIX URL IN LICENSE

The license incorrectly identified the registry URL as
`registry.npmjs.com` and this has been corrected to `registry.npmjs.org`.

* [`cb6d81b`](https://github.com/npm/npm/commit/cb6d81bd611f68c6126a90127a9dfe5604d46c8c)
  [#10685](https://github.com/npm/npm/pull/10685)
  Fix npm public registry URL in notices.
  ([@kemitchell](https://github.com/kemitchell))

#### ENOENT? MORE LIKE ENOMOREBUGS

The headliner this week was uncovered by the fixes to bundled dependency
handling over the past few releases. What had been a frustratingly
intermittent and hard to reproduce bug became something that happened
every time in Travis. This fixes another whole bunch of errors where you
would, while running an install have it crash with an `ENOENT` on
`rename`, or the install would finish but some modules would be
mysteriously missing and you'd have to install a second time.

What's going on was a bit involved, so bear with me:

`npm@3` generates a list of actions to take against the tree on disk.
With the exception of lifecycle scripts, it expects these all to be able
to act independently without interfering with each other.

This means, for instance, that one should be able to upgrade `b` in
`a→b→c` without having npm reinstall `c`.

That works fine by the way.

But it also means that the move action should be able to move `b` in
`a→b→c@1.0.1` to `a→d→b→c@1.0.2` without moving or removing `c@1.0.1` and
while leaving `c@1.0.2` in place if it was already installed.

That is, the `move` action moves an individual node, replacing itself
with an empty spot if it had children. This is not, as it might first
appear, something where you move an entire branch to another location on
the tree.

When moving `b` we already took care to leave `c@1.0.1` in place so that
other moves (or removes) could handle it, but we were stomping on the
destination and so `c@1.0.2` was being removed.

* [`f4385d8`](https://github.com/npm/npm/commit/f4385d8e7678349e75c80fae8a1f8f366f197937)
  [#10655](https://github.com/npm/npm/pull/10655)
  Preserve destination `node_modules` when moving.
  ([@iarna](https://github.com/iarna))

There was also a bug with `remove` where it was pruning the entire tree
at the remove point, prior to running moves and adds.

This was fine most of the time, but if we were moving one of the deps out
from inside it, kaboom.

* [`19c626d`](https://github.com/npm/npm/commit/19c626d69888f0cdc6e960254b3fdf523ec4b52c)
  [#10655](https://github.com/npm/npm/pull/10655)
  Get rid of the remove commit phase– we could have it prune _just_ the
  module being removed, but that isn't gaining us anything.
  ([@iarna](https://github.com/iarna))

After all that, we shouldn't be upgrading the `add` of a bundled package
to a `move`. Moves save us from having to extract the package, but with a
bundled dependency it's included in another package already so that
doesn't gain us anything.

* [`641a93b`](https://github.com/npm/npm/commit/641a93bd66a6aa4edf2d6167344b50d1a2afb593)
  [#10655](https://github.com/npm/npm/pull/10655)
  Don't convert adds to moves with bundled deps.
  ([@iarna](https://github.com/iarna))

While I was in there, I also took some time to improve diagnostics to
make this sort of thing easier to track down in the future:

* [`a04ec04`](https://github.com/npm/npm/commit/a04ec04804e562b511cd31afe89c8ba94aa37ff2)
  [#10655](https://github.com/npm/ npm/pull/10655)
  Wrap rename so errors have stack traces.
  ([@iarna](https://github.com/iarna))
* [`8ea142f`](https://github.com/npm/npm/commit/8ea142f896a2764290ca5472442b27b047ab7a1a)
  [#10655](https://github.com/npm/npm/pull/10655)
  Add silly logging so function is debuggable
  ([@iarna](https://github.com/iarna))

#### NO MORE MD5

We updated modules that had been using MD5 for non-security purposes.
While this is perfectly safe, if you compile Node in FIPS-compliance mode
it will explode if you try to use MD5. We've replaced MD5 with Murmur,
which conveys our intent better and is faster to boot.

* [`f068b26`](https://github.com/npm/npm/commit/f068b2661a8d0269c184867e003cd08cb6c56cf2)
  [#10629](https://github.com/npm/npm/issues/10629)
  `unique-filename@1.1.0`
  ([@iarna](https://github.com/iarna))
* [`dba1b24`](https://github.com/npm/npm/commit/dba1b2402aaa2beceec798d3bd22d00650e01069)
  [#10629](https://github.com/npm/npm/issues/10629)
  `write-file-atomic@1.1.4`
  ([@othiym23](https://github.com/othiym23))
* [`8347a30`](https://github.com/npm/npm/commit/8347a308ef0d2cf0f58f96bba3635af642ec611f)
  [#10629](https://github.com/npm/npm/issues/10629)
  `fs-write-stream-atomic@1.0.5`
  ([@othiym23](https://github.com/othiym23))

#### DEPENDENCY UPDATES

* [`9e2a2bb`](https://github.com/npm/npm/commit/9e2a2bb5bc71a0ab3b3637e8eec212aa22d5c99f)
  [nodejs/node-gyp#831](https://github.com/nodejs/node-gyp/pull/831)
  `node-gyp@3.2.1`:
  Improved \*BSD support.
  ([@bnoordhuis](https://github.com/bnoordhuis))

### v3.5.1 (2015-11-25):

#### THE npm CLI !== THE npm REGISTRY !== npm, INC.

npm-the-CLI is licensed under the terms of the [Artistic License
2.0](https://github.com/npm/npm/blob/8d79c1a39dae908f27eaa37ff6b23515d505ef29/LICENSE),
which is a liberal open-source license that allows you to take this code and do
pretty much whatever you like with it (that is, of course, not legal language,
and if you're doing anything with npm that leaves you in doubt about your legal
rights, please seek the review of qualified counsel, which is to say, not
members of the CLI team, none of whom have passed the bar, to my knowledge). At
the same time the primary registry the CLI uses when looking up and downloading
packages is a commercial service run by npm, Inc., and it has its own [Terms of
Use](https://www.npmjs.com/policies/terms).

Aside from clarifying the terms of use (and trying to make sure they're more
widely known), the only recent changes to npm's licenses have been making the
split between the CLI and registry clearer. You are still free to do whatever
you like with the CLI's source, and you are free to view, download, and publish
packages to and from `registry.npmjs.org`, but now the existing terms under
which you can do so are more clearly documented. Aside from the two commits
below, see also [the release notes for
`npm@3.4.1`](https://github.com/npm/npm/releases/tag/v3.4.1), which is where
the split between the CLI's code and the terms of use for the registry was
first made more clear.

* [`35a5dd5`](https://github.com/npm/npm/commit/35a5dd5abbfeec4f98a2b4534ec4ef5d16760581)
  [#10532](https://github.com/npm/npm/issues/10532) Clarify that
  `registry.npmjs.org` is the default, but that you're free to use the npm CLI
  with whatever registry you wish. ([@kemitchell](https://github.com/kemitchell))
* [`fa6b013`](https://github.com/npm/npm/commit/fa6b0136a0e4a19d8979b2013622e5ff3f0446f8)
  [#10532](https://github.com/npm/npm/issues/10532) Having semi-duplicate
  release information in `README.md` was confusing and potentially inaccurate,
  so remove it. ([@kemitchell](https://github.com/kemitchell))

#### EASE UP ON WINDOWS BASH USERS

It turns out that a fair number of us use bash on Windows (through MINGW or
bundled with Git, plz – Cygwin is still a bridge too far, for both npm and
Node.js). [@jakub-g](https://github.com/jakub-g) did us all a favor and relaxed
the check for npm completion to support MINGW bash. Thanks, Jakub!

* [`09498e4`](https://github.com/npm/npm/commit/09498e45c5c9e683f092ab1372670f81db4762b6)
  [#10156](https://github.com/npm/npm/issues/10156) completion: enable on
  Windows in git bash ([@jakub-g](https://github.com/jakub-g))

#### THE ONGOING SAGA OF BUNDLED DEPENDENCIES

`npm@3.5.0` fixed up a serious issue with how `npm@3.4.1` (and potentially
`npm@3.4.0` and `npm@3.3.12`) handled the case in which dependencies bundled
into a package tarball are handled improperly when one or more of their own
dependencies are older than what's latest on the registry. Unfortunately, in
fixing that (quite severe) regression (see [`npm@3.5.0`'s release notes' for
details](https://github.com/npm/npm/releases/tag/v3.5.0)), we introduced a new
(small, and fortunately cosmetic) issue where npm superfluously warns you about
bundled dependencies being stale. We have now fixed that, and hope that we
haven't introduced any _other_ regressions in the process. :D

* [`20824a7`](https://github.com/npm/npm/commit/20824a75bf7639fb0951a588e3c017a370ae6ec2)
  [#10501](https://github.com/npm/npm/issues/10501) Only warn about replacing
  bundled dependencies when actually doing so. ([@iarna](https://github.com/iarna))

#### MAKE NODE-GYP A LITTLE BLUER

* [`1d14d88`](https://github.com/npm/npm/commit/1d14d882c3b5af0a7fee46e8e0e343d07e4c38cb)
  `node-gyp@3.2.0`: Support AIX, use `which` to find Python, updated to a newer
  version of `gyp`, and more! ([@bnoordhuis](https://github.com/bnoordhuis))

#### A BOUNTEOUS THANKSGIVING CORNUCOPIA OF DOC TWEAKS

These are great! Keep them coming! Sorry for letting them pile up so deep,
everybody. Also, a belated Thanksgiving to our Canadian friends, and a happy
Thanksgiving to all our friends in the USA.

* [`4659f1c`](https://github.com/npm/npm/commit/4659f1c5ad617c46a5e89b48abf0b1c4e6f04307)
  [#10244](https://github.com/npm/npm/issues/10244) In `npm@3`, `npm dedupe`
  doesn't take any arguments, so update documentation to reflect that.
  ([@bengotow](https://github.com/bengotow))
* [`625a7ee`](https://github.com/npm/npm/commit/625a7ee6b4391e90cb28a95f20a73fd794e1eebe)
  [#10250](https://github.com/npm/npm/issues/10250) Correct order of `org:team`
  in `npm team` documentation. ([@louislarry](https://github.com/louislarry))
* [`bea7f87`](https://github.com/npm/npm/commit/bea7f87399d784e3a6d3393afcca658a61a40d77)
  [#10371](https://github.com/npm/npm/issues/10371) Remove broken / duplicate
  link to tag. ([@WickyNilliams](https://github.com/WickyNilliams))
* [`0a25e29`](https://github.com/npm/npm/commit/0a25e2956e9ddd4065d6bd929559321afc512fde)
  [#10419](https://github.com/npm/npm/issues/10419) Remove references to
  nonexistent `npm-rm(1)` documentation. ([@KenanY](https://github.com/KenanY))
* [`19b94e1`](https://github.com/npm/npm/commit/19b94e1e6781fe2f98ada0a3f49a1bda25e3e32d)
  [#10474](https://github.com/npm/npm/issues/10474) Clarify that install finds
  dependencies in `package.json`. ([@sleekweasel](https://github.com/sleekweasel))
* [`b25efc8`](https://github.com/npm/npm/commit/b25efc88067c843ffdda86ea0f50f95d136a638e)
  [#9948](https://github.com/npm/npm/issues/9948) Encourage users to file an
  issue, rather than emailing authors. ([@trodrigues](https://github.com/trodrigues))
* [`24f4ced`](https://github.com/npm/npm/commit/24f4cedc83b10061f26362bf2f005ab935e0cbfe)
  [#10497](https://github.com/npm/npm/issues/10497) Clarify what a package is
  slightly. ([@aredridel](https://github.com/aredridel))
* [`e8168d4`](https://github.com/npm/npm/commit/e8168d40caae00b2914ea09dbe4bd1b09ba3dcd5)
  [#10539](https://github.com/npm/npm/issues/10539) Remove an extra, spuriously
  capitalized letter. ([@alexlukin-softgrad](https://github.com/alexlukin-softgrad))

### v3.5.0 (2015-11-19):

#### TEEN ORCS AT THE GATES

This week heralds the general release of the primary npm registry's [new
support for private packages for
organizations](http://blog.npmjs.org/post/133542170540/private-packages-for-organizations).
For many potential users, it's the missing piece needed to make it easy for you
to move your organization's private work onto npm. And now it's here! The
functionality to support it has been in place in the CLI for a while now,
thanks to [@zkat](https://github.com/zkat)'s hard work.

During our final testing before the release, our ace support team member
[@snopeks](https://github.com/snopeks) noticed that there had been some drift
between the CLI team's implementation and what npm was actually preparing to
ship. In the interests of everyone having a smooth experience with this
_extremely useful_ new feature, we quickly made a few changes to square up the
CLI and the web site experiences.

* [`d7fb92d`](https://github.com/npm/npm/commit/d7fb92d1c53ba5196ad6dd2101a06792a4c0412b)
  [#9327](https://github.com/npm/npm/issues/9327) `npm access` no longer has
  problems when run in a directory that doesn't contain a `package.json`.
  ([@othiym23](https://github.com/othiym23))
* [`17df3b5`](https://github.com/npm/npm/commit/17df3b5d5dffb2e9c223b9cfa2d5fd78c39492a4)
  [npm/npm-registry-client#126](https://github.com/npm/npm-registry-client/issues/126)
  `npm-registry-client@7.0.8`: Allow the CLI to grant, revoke, and list
  permissions on unscoped (public) packages on the primary registry.
  ([@othiym23](https://github.com/othiym23))

#### NON-OPTIONAL INSTALLS, DEFINITELY NON-OPTIONAL

* [`180263b`](https://github.com/npm/npm/commit/180263b)
  [#10465](https://github.com/npm/npm/pull/10465)
  When a non-optional dep fails, we check to see if it's only required by
  ONLY optional dependencies.  If it is, we make it fail all the deps in
  that chain (and roll them back).  If it isn't then we give an error.

  We do this by walking up through all of our ancestors until we either hit an
  optional dependency or the top of the tree. If we hit the top, we know to
  give the error.

  If you installed a module by hand but didn't `--save` it, your module
  won't have the top of the tree as an anscestor and so this code was
  failing to abort the install with an error

  This updates the logic so that hitting the top OR a module that was
  requested by the user will trigger the error message.
  ([@iarna](https://github.com/iarna))

* [`b726a0e`](https://github.com/npm/npm/commit/b726a0e)
  [#9204](https://github.com/npm/npm/issues/9204)
  Ideally we would like warnings about your install to come AFTER the
  output from your compile steps or the giant tree of installed modules.

  To that end, we've moved warnings about failed optional deps to the show
  after your install completes.
  ([@iarna](https://github.com/iarna))

#### OVERRIDING BUNDLING

* [`aed71fb`](https://github.com/npm/npm/commit/aed71fb)
  [#10482](https://github.com/npm/npm/issues/10482)
  We've been in our bundled modules code a lot lately, and our last go at
  this introduced a new bug, where if you had a module `a` that bundled
  a module `b`, which in turn required `c`, and the version of `c` that
  got bundled wasn't compatible with `b`'s `package.json`, we would then
  install a compatible version of `c`, but also erase `b` at the same time.

  This fixes that. It also reworks our bundled module support to be much
  closer to being in line with how we handle non-bundled modules and we're
  hopeful this will reduce any future errors around them. The new structure
  is hopefully much easier to reason about anyway.
  ([@iarna](https://github.com/iarna))

#### A BRIEF NOTE ON NPM'S BACKWARDS COMPATIBILITY

We don't often have much to say about the changes we make to our internal
testing and tooling, but I'm going to take this opportunity to reiterate that
npm tries hard to maintain compatibility with a wide variety of Node versions.
As this change shows, we want to ensure that npm works the same across:

* Node.js 0.8
* Node.js 0.10
* Node.js 0.12
* the latest io.js release
* Node.js 4 LTS
* Node.js 5

Contributors who send us pull requests often notice that it's very rare that
our tests pass across all of those versions (ironically, almost entirely due to
the packages we use for testing instead of any issues within npm itself). We're
currently beginning an effort, lasting the rest of 2015, to clean up our test
suite, and not only get it passing on all of the above versions of Node.js, but
working solidly on Windows as well. This is a compounding form of technical
debt that we're finally paying down, and our hope is that cleaning up the tests
will produce a more robust CLI that's a lot easier to write patches for.

* [`791ec6b`](https://github.com/npm/npm/commit/791ec6b1bac0d1df59f5ebb4ccd16a29a5dc73f0)
  [#10233](https://github.com/npm/npm/issues/10233) Update Node.js versions
  that Travis uses to test npm. ([@iarna](https://github.com/iarna))

#### 0.8 + npm <1.4 COMPATIBLE? SURE WHY NOT

Hey, you found the feature we added!

* [`231c58a`](https://github.com/npm/npm/commit/231c58a)
  [#10337](https://github.com/npm/npm/pull/10337)
  Add two new flags, first `--legacy-bundling` which installs your
  dependencies such that if you bundle those dependencies, npm versions
  prior to `1.4` can still install them. This eliminates all automatic
  deduping.

  Second, `--global-style` which will install modules in your `node_modules`
  folder with the same layout as global modules.  Only your direct
  dependencies will show in `node_modules` and everything they depend on
  will be flattened in their `node_modules` folders.  This obviously will
  elminate some deduping.
  ([@iarna](https://github.com/iarna))

#### TYPOS IN THE LICENSE, OH MY

* [`8d79c1a`](https://github.com/npm/npm/commit/8d79c1a39dae908f27eaa37ff6b23515d505ef29)
  [#10478](https://github.com/npm/npm/issues/10478) Correct two typos in npm's
  LICENSE. ([@jorrit](https://github.com/jorrit))

### v3.4.1 (2015-11-12):

#### ASK FOR NOTHING, GET LATEST

When you run `npm install foo`, you probably expect that you'll get the
`latest` version of `foo`, whatever that is. And good news! That's what
this change makes it do.

We _think_ this is what everyone wants, but if this causes problems for
you, we want to know! If it proves problematic for people we will consider
reverting it (preferrably before this becomes `npm@latest`).

Previously, when you ran `npm install foo` we would act as if you typed
`npm install foo@*`. Now, like any range-type specifier, in addition to
matching the range, it would also have to be `<=` the value of the
`latest` dist-tag. Further, it would exclude prerelease versions from the
list of versions considered for a match.

This worked as expected most of the time, unless your `latest` was a
prerelease version, in which case that version wouldn't be used, to
everyone's surprise. Worse, if all your versions were prerelease versions
it would just refuse to install anything. (We fixed that in
[`npm@3.2.2`](https://github.com/npm/npm/releases/tag/v3.2.2) with
[`e4a38080`](https://github.com/npm/npm/commit/e4a38080).)

* [`1e834c2`](https://github.com/npm/npm/commit/1e834c2)
  [#10189](https://github.com/npm/npm/issues/10189)
  `npm-package-arg@4.1.0` Change the default version from `*` to `latest`.
  ([@zkat](https://github.com/zkat))

#### BUGS

* [`bec4a84`](https://github.com/npm/npm/commit/bec4a84)
  [#10338](https://github.com/npm/npm/pull/10338)
  Failed installs could result in more rollback (removal of just installed
  packages) than we intended. This bug was first introduced by
  [`83975520`](https://github.com/npm/npm/commit/83975520).
  ([@iarna](https://github.com/iarna))
* [`06c732f`](https://github.com/npm/npm/commit/06c732f)
  [#10338](https://github.com/npm/npm/pull/10338)
  Updating a module could result in the module stealing some of its
  dependencies from the top level, potentially breaking other modules or
  resulting in many redundent installations. This bug was first introduced
  by [`971fd47a`](https://github.com/npm/npm/commit/971fd47a).
  ([@iarna](https://github.com/iarna))
* [`5653366`](https://github.com/npm/npm/commit/5653366)
  [#9980](https://github.com/npm/npm/issues/9980)
  npm, when removing a module, would refuse to remove the symlinked
  binaries if the module itself was symlinked as well. npm goes to some
  effort to ensure that it doesn't remove things that aren't is, and this
  code was being too conservative. This code has been rewritten to be
  easier to follow and to be unit-testable.
  ([@iarna](https://github.com/iarna))

#### LICENSE CLARIFICATION

* [`80acf20`](https://github.com/npm/npm/commit/80acf20)
  [#10326](https://github.com/npm/npm/pull/10326)
  Update npm's licensing to more completely cover all of the various
  things that are npm.
  ([@kemitchell](https://github.com/kemitchell))

#### CLOSER TO GREEN TRAVIS

* [`fc12da9`](https://github.com/npm/npm/commit/fc12da9)
  [#10232](https://github.com/npm/npm/pull/10232)
  `nock@1.9.0`
  Downgrade nock to a version that doesn't depend on streams2 in core so
  that more of our tests can pass in 0.8.
  ([@iarna](https://github.com/iarna))

### v3.4.0 (2015-11-05):

#### A NEW FEATURE

This was a group effort, with [@isaacs](https://github.com/isaacs)
dropping the implementation in back in August. Then, a few days ago,
[@ashleygwilliams](https://github.com/ashleygwilliams) wrote up docs and
just today [@othiym23](https://github.com/othiym23) wrote a test.

It's a handy shortcut to update a dependency and then make sure tests
still pass.

This new command:

```
npm install-test x
```

is the equivalent of running:

```
npm install x && npm test
```

* [`1ac3e08`](https://github.com/npm/npm/commit/1ac3e08)
  [`bcb04f6`](https://github.com/npm/npm/commit/bcb04f6)
  [`b6c17dd`](https://github.com/npm/npm/commit/b6c17dd)
  [#9443](https://github.com/npm/npm/pull/9443)
  Add `npm install-test` command, alias `npm it`.
  ([@isaacs](https://github.com/isaacs),
  [@ashleygwilliams](https://github.com/ashleygwilliams),
  [@othiym23](https://github.com/othiym23))

#### BUG FIXES VIA DEPENDENCY UPDATES

* [`31c0080`](https://github.com/npm/npm/commit/31c0080)
  [#8640](https://github.com/npm/npm/issues/8640)
  [npm/normalize-package-data#69](https://github.com/npm/normalize-package-data/pull/69)
  `normalize-package-data@2.3.5`:
  Fix a bug where if you didn't specify the name of a scoped module's
  binary, it would install it such that it was impossible to call it.
  ([@iarna](https://github.com/iarna))
* [`02b37bc`](https://github.com/npm/npm/commit/02b37bc)
  [npm/fstream-npm#14](https://github.com/npm/fstream-npm/pull/14)
  `fstream-npm@1.0.7`:
  Only filter `config.gypi` when it's in the build directory.
  ([@mscdex](https://github.com/mscdex))
* [`accb9d2`](https://github.com/npm/npm/commit/accb9d2)
  [npm/fstream-npm#15](https://github.com/npm/fstream-npm/pull/15)
  `fstream-npm@1.0.6`:
  Stop including directories that happened to have names matching whitelisted
  npm files in npm module tarballs. The most common cause was that if you had
  a README directory then everything in it would be included if wanted it
  or not.
  ([@taion](https://github.com/taion))

#### DOCUMENTATION FIXES

* [`7cf6366`](https://github.com/npm/npm/commit/7cf6366)
  [#10036](https://github.com/npm/npm/pull/10036)
  Fix typo / over-abbreviation.
  ([@ifdattic](https://github.com/ifdattic))
* [`d0ad8f4`](https://github.com/npm/npm/commit/d0ad8f4)
  [#10176](https://github.com/npm/npm/pull/10176)
  Fix broken link, scopes => scope.
  ([@ashleygwilliams](https://github.com/ashleygwilliams))
* [`d623783`](https://github.com/npm/npm/commit/d623783)
  [#9460](https://github.com/npm/npm/issue/9460)
  Specifying the default command run by "npm start" and the
  fact that you can pass it arguments.
  ([@JuanCaicedo](https://github.com/JuanCaicedo))

#### DEPENDENCY UPDATES FOR THEIR OWN SAKE

* [`0a4c29e`](https://github.com/npm/npm/commit/0a4c29e)
  [npm/npmlog#19](https://github.com/npm/npmlog/pull/19)
  `npmlog@2.0.0`: Make it possible to emit log messages with `error` as the
  prefix.
  ([@bengl](https://github.com/bengl))
* [`9463ce9`](https://github.com/npm/npm/commit/9463ce9)
  `read-package-json@2.0.2`:
  Minor cleanups.
  ([@KenanY](https://github.com/KenanY))

### v3.3.12 (2015-11-02):

Hi, a little hot-fix release for a bug introduced in 3.3.11.  The ENOENT fix
last week ([`f0e2088`](https://github.com/npm/npm/commit/f0e2088)) broke
upgrades of modules that have bundled dependencies (like `npm`, augh!)

* [`aedf7cf`](https://github.com/npm/npm/commit/aedf7cf)
  [#10192](//github.com/npm/npm/pull/10192)
  If a bundled module is going to be replacing a module that's currently on
  disk (for instance, when you upgrade a module that includes bundled
  dependencies) we want to select the version from the bundle in preference
  over the one that was there previously.
  ([@iarna](https://github.com/iarna))

### v3.3.11 (2015-10-29):

This is a dependency update week, so that means no PRs from our lovely
users. Look for those next week.  As it happens, the dependencies updated
were just devdeps, so nothing for you all to worry about.

But the bug fixes, oh geez, I tracked down some really long standing stuff
this week!!  The headliner is those intermittent `ENOENT` errors that no one
could reproduce consistently?  I think they're nailed! But also pretty
important, the bug where `hapi` would install w/ a dep missing? Squashed!

#### EEEEEEENOENT

* [`f0e2088`](https://github.com/npm/npm/commit/f0e2088)
  [#10026](https://github.com/npm/npm/issues/10026)
  Eliminate some, if not many, of the `ENOENT` errors `npm@3` has seen over
  the past few months.  This was happening when npm would, in its own mind,
  correct a bundled dependency, due to a `package.json` specifying an
  incompatible version.  Then, when npm extracted the bundled version, what
  was on disk didn't match its mind and… well, when it tried to act on what
  was in its mind, we got an `ENOENT` because it didn't actually exist on
  disk.
  ([@iarna](https://github.com/iarna))

#### PARTIAL SHRINKWRAPS, NO LONGER A BAD DAY

* [`712fd9c`](https://github.com/npm/npm/commit/712fd9c)
  [#10153](https://github.com/npm/npm/pull/10153)
  Imagine that you have a module, let's call it `fun-time`, and it depends
  on two dependencies, `need-fun@1` and `need-time`.  Further, `need-time`
  requires `need-fun@2`.  So after install the logical tree will look like
  this:

  ```
  fun-time
  ├── need-fun@1
  └── need-time
      └── need-fun@2
  ```

  Now, the `fun-time` author also distributes a shrinkwrap, but it only includes
  the `need-fun@1` in it.

  Resolving dependencies would look something like this:

  1. Require `need-fun@1`: Use version from shrinkwrap (ignoring version)
  2. Require `need-time`: User version in package.json
    1. Require `need-fun@2`: Use version from shrinkwrap, which oh hey, is
       already installed at the top level, so no further action is needed.

  Which results in this tree:

  ```
  fun-time
  ├── need-fun@1
  └── need-time
  ```

  We're ignoring the version check on things specified in the shrinkwrap
  so that you can override the version that will be installed. This is
  because you may want to  use a different version than is specified
  by your dependencies' dependencies' `package.json` files.

  To fix this, we now only allow overrides of a dependency version when
  that dependency is a child (in the tree) of the thing that requires it.
  This means that when we're looking for `need-fun@2` we'll see `need-fun@1`
  and reject it because, although it's from a shrinkwrap, it's parent is
  `fun-time` and the package doing the requiring is `need-time`.

  ([@iarna](https://github.com/iarna))

#### STRING `package.bin` AND NON-NPMJS REGISTRIES

* [`3de1463`](https://github.com/npm/npm/commit/3de1463)
  [#9187](https://github.com/npm/npm/issues/9187)
  If you were using a module with the `bin` field in your `package.json` set
  to a string on a non-npmjs registry then npm would crash, due to the our
  expectation that the `bin` field would be an object.  We now pass all
  `package.json` data through a routine that normalizes the format,
  including the `bin` field.  (This is the same routine that your
  `package.json` is passed through when read off of disk or sent to the
  registry for publication.) Doing this also ensures that older modules on
  npm's own registry will be treated exactly the same as new ones.  (In the
  past we weren't always super careful about scrubbing `package.json` data
  on publish.  And even when we were, those rules have subtly changed over
  time.)
  ([@iarna](https://github.com/iarna))

### v3.3.10 (2015-10-22):

Hey you all!  Welcome to a busy bug fix and PR week.  We've got changes
to how `npm install` replaces dependencies during updates, improvements
to shrinkwrap behavior, and all sorts of doc updates.

In other news, `npm@3` landed in node master in preparation for `node@5`
with [`41923c0`](https://github.com/nodejs/node/commit/41923c0).

#### UPDATED DEPS NOW MAKE MORE SENSE

* [`971fd47`](https://github.com/npm/npm/commit/971fd47)
  [#9929](https://github.com/npm/npm/pull/9929)
  Make the tree more consistent by doing updates in place. This means
  that trees after a dependency version update will more often look
  the same as after a fresh install.
  ([@iarna](https://github.com/iarna))

#### SHRINKWRAP + DEV DEPS NOW RESPECTED

* [`eb28a8c`](https://github.com/npm/npm/commit/eb28a8c)
  [#9647](https://github.com/npm/npm/issues/9647)
  If a shrinkwrap already has dev deps, don't throw them away when
  someone later runs `npm install --save`.
  ([@iarna](https://github.com/iarna))

#### FANTASTIC DOCUMENTATION UPDATES

* [`291162c`](https://github.com/npm/npm/commit/291162c)
  [#10021](https://github.com/npm/npm/pull/10021)
  Improve wording in the FAQ to be more empathetic and less jokey.
  ([@TaMe3971](https://github.com/TaMe3971))
* [`9a28c54`](https://github.com/npm/npm/commit/9a28c54)
  [#10020](https://github.com/npm/npm/pull/10020)
  Document the command to see the list of config defaults in the section
  on config defaults.
  ([@lady3bean](https://github.com/lady3bean))
* [`8770b0a`](https://github.com/npm/npm/commit/8770b0a)
  [#7600](https://github.com/npm/npm/issues/7600)
  Add shortcuts to all command documentation.
  ([@RichardLitt](https://github.com/RichardLitt))
* [`e9b7d0d`](https://github.com/npm/npm/commit/e9b7d0d)
  [#9950](https://github.com/npm/npm/pull/9950)
  On errors that can be caused by outdated node & npm, suggest updating
  as a part of the error message.
  ([@ForbesLindesay](https://github.com/ForbesLindesay))

#### NEW STANDARD HAS ALWAYS BEEN STANDARD

* [`40c1b0f`](https://github.com/npm/npm/commit/40c1b0f)
  [#9954](https://github.com/npm/npm/pull/9954)
  Update to `standard@5` and reformat the source to work with it.
  ([@cbas](https://github.com/cbas))

### v3.3.9 (2015-10-15):

This week sees a few small changes ready to land:

#### TRAVIS NODE 0.8 BUILDS REJOICE

* [`25a234b`](https://github.com/npm/npm/commit/25a234b)
  [#9668](https://github.com/npm/npm/issues/9668)
  Install `npm@3`'s bundled dependencies with `npm@2`, so that the ancient npm
  that ships with node 0.8 can install `npm@3` directly.
  ([@othiym23](https://github.com/othiym23))

#### SMALL ERROR MESSAGE IMPROVEMENT

* [`a332f61`](https://github.com/npm/npm/commit/a332f61)
  [#9927](https://github.com/npm/npm/pull/9927)
  Update error messages where we report a list of versions that you could
  have installed to show this as a comma separated list instead of as JSON.
  ([@iarna](https://github.com/iarna))

#### DEPENDENCY UPDATES

* [`4cd74b0`](https://github.com/npm/npm/commit/4cd74b0)
  `nock@2.15.0`
  ([@pgte](https://github.com/pgte))
* [`9360976`](https://github.com/npm/npm/commit/9360976)
  `tap@2.1.1`
  ([@isaacs](https://github.com/isaacs))
* [`1ead0a4`](https://github.com/npm/npm/commit/1ead0a4)
  `which@1.2.0`
  ([@isaacs](https://github.com/isaacs))
* [`759f88a`](https://github.com/npm/npm/commit/759f88a)
  `has-unicode@1.0.1`
  ([@iarna](https://github.com/iarna))

### v3.3.8 (2015-10-12):

This is a small update release, we're reverting
[`22a3af0`](https://github.com/npm/npm/commit/22a3af0) from last week's
release, as it is resulting in crashes.  We'll revisit this PR during this
week.

* [`ddde1d5`](https://github.com/npm/npm/commit/ddde1d5)
  Revert "lifecycle: Swap out custom logic with add-to-path module"
  ([@iarna](https://github.com/iarna))

### v3.3.7 (2015-10-08):

So, as Kat mentioned in last week's 2.x release, we're now swapping weeks
between accepting PRs and doing dependency updates, in an effort to keep
release management work from taking over our lives.  This week is a PR week,
so we've got a bunch of goodies for you.

Relatedly, this week means 3.3.6 is now `latest` and it is WAY faster than
previous 3.x releases. Give it or this a look!

#### OPTIONAL DEPS, MORE OPTIONAL

* [`2289234`](https://github.com/npm/npm/commit/2289234)
  [#9643](https://github.com/npm/npm/issues/9643)
  [#9664](https://github.com/npm/npm/issues/9664)
  `npm@3` was triggering `npm@2`'s build mechanics when it was linking bin files
  into the tree.  This was originally intended to trigger rebuilds of
  bundled modules, but `npm@3`'s flat module structure confused it.  This
  caused two seemingly unrelated issues.  First, failing optional
  dependencies could under some circumstances (if they were built during
  this phase) trigger a full build failure.  And second, rebuilds were being
  triggered of already installed modules, again, in some circumstances.
  Both of these are fixed by disabling the `npm@2` mechanics and adding a
  special rebuild phase for the initial installation of bundled modules.
  ([@iarna](https://github.com/iarna))

#### BAD NAME, NO CRASH

* [`b78fec9`](https://github.com/npm/npm/commit/b78fec9)
  [#9766](https://github.com/npm/npm/issues/9766)
  Refactor all attempts to read the module name or package name to go via a
  single function, with appropriate guards unusual circumstances where they
  aren't where we expect them.  This ultimately will ensure we don't see any
  more recurrences of the `localeCompare` error and related crashers.
  ([@iarna](https://github.com/iarna))

#### MISCELLANEOUS BUG FIXES

* [`22a3af0`](https://github.com/npm/npm/commit/22a3af0)
  [#9553](https://github.com/npm/npm/pull/9553)
  Factor the lifecycle code to manage paths out into its own module and use that.
  ([@kentcdodds](https://github.com/kentcdodds))
* [`6a29fe3`](https://github.com/npm/npm/commit/6a29fe3)
  [#9677](https://github.com/npm/npm/pull/9677)
  Start testing our stuff in node 4 on travis
  ([@fscherwi](https://github.com/fscherwi))
* [`508c6a4`](https://github.com/npm/npm/commit/508c6a4)
  [#9669](https://github.com/npm/npm/issues/9669)
  Make `recalculateMetadata` more resilient to unexpectedly bogus dependency specifiers.
  ([@tmct](https://github.com/tmct))
* [`3c44763`](https://github.com/npm/npm/commit/3c44763)
  [#9643](https://github.com/npm/npm/issues/9463)
  Update `install --only` to ignore the `NODE_ENV` var and _just_ use the only
  value, if specified.
  ([@watilde](https://github.com/watilde))
* [`87336c3`](https://github.com/npm/npm/commit/87336c3)
  [#9879](https://github.com/npm/npm/pull/9879)
  `npm@3`'s shrinkwrap was refusing to shrinkwrap if an optional dependency
  was missing– patch it to allow this.
  ([@mantoni](https://github.com/mantoni))

#### DOCUMENTATION UPDATES

* [`82659fd`](https://github.com/npm/npm/commit/82659fd)
  [#9208](https://github.com/npm/npm/issues/9208)
  Correct the npm style guide around quote usage
  ([@aaroncrows](https://github.com/aaroncrows))
* [`a69c83a`](https://github.com/npm/npm/commit/a69c83a)
  [#9645](https://github.com/npm/npm/pull/9645)
  Fix spelling error in README
  ([@dkoleary88](https://github.com/dkoleary88))
* [`f2cf054`](https://github.com/npm/npm/commit/f2cf054)
  [#9714](https://github.com/npm/npm/pull/9714)
  Fix typos in our documentation
  ([@reggi](https://github.com/reggi))
* [`7224bef`](https://github.com/npm/npm/commit/7224bef)
  [#9759](https://github.com/npm/npm/pull/9759)
  Fix typo in npm-team docs
  ([@zkat](https://github.com/zkat))
* [`7e6e007`](https://github.com/npm/npm/commit/7e6e007)
  [#9820](https://github.com/npm/npm/pull/9820)
  Correct documentation as to `binding.gyp`
  ([@KenanY](https://github.com/KenanY))

### v3.3.6 (2015-09-30):

I have the most exciting news for you this week.  YOU HAVE NO IDEA.  Well,
ok, maybe you do if you follow my twitter.

Performance just got 5 bazillion times better (under some circumstances,
ymmv, etc).  So– my test scenario is our very own website.  In `npm@2`, on my
macbook running `npm ls` takes about 5 seconds. Personally it's more than
I'd like, but it's entire workable. In `npm@3` it has been taking _50_ seconds,
which is appalling. But after doing some work on Monday isolating the performance
issues I've been able to reduce `npm@3`'s run time back down to 5 seconds.

Other scenarios were even worse, there was one that until now in `npm@3` that
took almost 6 minutes, and has been reduced to 14 seconds.

* [`7bc0d4c`](https://github.com/npm/npm/commit/7bc0d4c)
  [`cf42217`](https://github.com/npm/npm/commit/cf42217)
  [#8826](https://github.com/npm/npm/issues/8826)
  Stop using deepclone on super big datastructures. Avoid cloning
  all-together even when that means mutating things, when possible.
  Otherwise use a custom written tree-copying function that understands
  the underlying datastructure well enough to only copy what we absolutely
  need to.
  ([@iarna](https://github.com/iarna))

In other news, look for us this Friday and Saturday at the amazing
[Open Source and Feelings](https://osfeels.com) conference, where something like a
third of the company will be attending.

#### And finally a dependency update

* [`a6a4437`](https://github.com/npm/npm/commit/a6a4437)
  `glob@5.0.15`
  ([@isaacs](https://github.com/isaacs))

#### And some subdep updates

* [`cc5e6a0`](https://github.com/npm/npm/commit/cc5e6a0)
  `hoek@2.16.3`
  ([@nlf](https://github.com/nlf))
* [`912a516`](https://github.com/npm/npm/commit/912a516)
  `boom@2.9.0`
  ([@arb](https://github.com/arb))
* [`63944e9`](https://github.com/npm/npm/commit/63944e9)
  `bluebird@2.10.1`
  ([@petkaantonov](https://github.com/petkaantonov))
* [`ef16003`](https://github.com/npm/npm/commit/ef16003)
  `mime-types@2.1.7` & `mime-db@1.19.0`
  ([@dougwilson](https://github.com/dougwilson))
* [`2b8c0dd`](https://github.com/npm/npm/commit/2b8c0dd)
  `request@2.64.0`
  ([@simov](https://github.com/simov))
* [`8139124`](https://github.com/npm/npm/commit/8139124)
  `brace-expansion@1.1.1`
  ([@juliangruber](https://github.com/juliangruber))

### v3.3.5 (2015-09-24):

Some of you all may not be aware, but npm is ALSO a company. I tell you this
'cause npm-the-company had an all-staff get together this week, flying in
our remote folks from around the world. That was great, but it also
basically eliminated normal work on Monday and Tuesday.

Still, we've got a couple of really important bug fixes this week.  Plus a
lil bit from the [now LTS 2.x branch](https://github.com/npm/npm/releases/tag/v2.14.6).

#### ATTENTION WINDOWS USERS

If you previously updated to npm 3 and you try to update again, you may get
an error messaging telling you that npm won't install npm into itself. Until you
are at 3.3.5 or greater, you can get around this with `npm install -f -g npm`.

* [`bef06f5`](https://github.com/npm/npm/commit/bef06f5)
  [#9741](https://github.com/npm/npm/pull/9741) Uh...  so...  er...  it
  seems that since `npm@3.2.0` on Windows with a default configuration, it's
  been impossible to update npm.  Well, that's not actually true, there's a
  work around (see above), but it shouldn't be complaining in the first
  place.
  ([@iarna](https://github.com/iarna))

#### STACK OVERFLOWS ON PUBLISH

* [`330b496`](https://github.com/npm/npm/commit/330b496)
  [#9667](https://github.com/npm/npm/pull/9667)
  We were keeping track of metadata about your project while packing the
  tree in a way that resulted in this data being written to packed tar files
  headers. When this metadata included cycles, it resulted in the the tar
  file entering an infinite recursive loop and eventually crashing with a
  stack overflow.

  I've patched this by keeping track of your metadata by closing over the
  variables in question instead, and I've further restricted gathering and
  tracking the metadata to times when it's actually needed. (Which is only
  if you need bundled modules.)
  ([@iarna](https://github.com/iarna))

#### LESS CRASHY ERROR MESSAGES ON BAD PACKAGES

* [`829921f`](https://github.com/npm/npm/commit/829921f)
  [#9741](https://github.com/npm/npm/pull/9741)
  Packages with invalid names or versions were crashing the installer. These
  are now captured and warned as was originally intended.
  ([@iarna](https://github.com/iarna))

#### ONE DEPENDENCY UPDATE

* [`963295c`](https://github.com/npm/npm/commit/963295c)
  `npm-install-checks@2.0.1`
  ([@iarna](https://github.com/iarna))

#### AND ONE SUBDEPENDENCY

* [`448737d`](https://github.com/npm/npm/commit/448737d)
  `request@2.63.0`
  ([@simov](https://github.com/simov))

### v3.3.4 (2015-09-17):

This is a relatively quiet release, bringing a few bug fixes and
some module updates, plus via the
[2.14.5 release](https://github.com/npm/npm/releases/tag/v2.14.5)
some forward compatibility fixes with versions of Node that
aren't yet released.

#### NO BETA NOTICE THIS TIME!!

But, EXCITING NEWS FRIENDS, this week marks the exit of `npm@3`
from beta. This means that the week of this release,
[v3.3.3](https://github.com/npm/npm/releases/tag/v3.3.3) will
become `latest` and this version (v3.3.4) will become `next`!!

#### CRUFT FOR THE CRUFT GODS

What I call "cruft", by which I mean, files sitting around in
your `node_modules` folder, will no longer produce warnings in
`npm ls` nor during `npm install`. This brings `npm@3`'s behavior
in line with `npm@2`.

* [`a127801`](https://github.com/npm/npm/commit/a127801)
  [#9285](https://github.com/npm/npm/pull/9586)
  Stop warning about cruft in module directories.
  ([@iarna](https://github.com/iarna))

#### BETTER ERROR MESSAGE

* [`95ee92c`](https://github.com/npm/npm/commit/95ee92c)
  [#9433](https://github.com/npm/npm/issues/9433)
  Give better error messages for invalid URLs in the dependecy
  list.
  ([@jamietre](https://github.com/jamietre))

#### MODULE UPDATES

* [`ebb92ca`](https://github.com/npm/npm/commit/ebb92ca)
  `retry@0.8.0` ([@tim-kos](https://github.com/tim-kos))
* [`55f1285`](https://github.com/npm/npm/commit/55f1285)
  `normalize-package-data@2.3.4` ([@zkat](https://github.com/zkat))
* [`6d4ebff`](https://github.com/npm/npm/commit/6d4ebff)
  `sha@2.0.1` ([@ForbesLindesay](https://github.com/ForbesLindesay))
* [`09a9c7a`](https://github.com/npm/npm/commit/09a9c7a)
  `semver@5.0.3` ([@isaacs](https://github.com/isaacs))
* [`745000f`](https://github.com/npm/npm/commit/745000f)
  `node-gyp@3.0.3` ([@rvagg](https://github.com/rvagg))

#### SUB DEP MODULE UPDATES

* [`578ca25`](https://github.com/npm/npm/commit/578ca25)
  `request@2.62.0` ([@simov](https://github.com/simov))
* [`1d8996e`](https://github.com/npm/npm/commit/1d8996e)
  `jju@1.2.1` ([@rlidwka](https://github.com/rlidwka))
* [`6da1ba4`](https://github.com/npm/npm/commit/6da1ba4)
  `hoek@2.16.2` ([@nlf](https://github.com/nlf))

### v3.3.3 (2015-09-10):

This short week brought us brings us a few small bug fixes, a
doc change and a whole lotta dependency updates.

Plus, as usual, this includes a forward port of everything in
[`npm@2.14.4`](https://github.com/npm/npm/releases/tag/v2.14.4).

#### BETA BUT NOT FOREVER

**_THIS IS BETA SOFTWARE_**. `npm@3` will remain in beta until
we're confident that it's stable and have assessed the effect of
the breaking changes on the community. During that time we will
still be doing `npm@2` releases, with `npm@2` tagged as `latest`
and `next`. We'll _also_ be publishing new releases of `npm@3`
as `npm@v3.x-next` and `npm@v3.x-latest` alongside those
versions until we're ready to switch everyone over to `npm@3`.
We need your help to find and fix its remaining bugs. It's a
significant rewrite, so we are _sure_ there still significant
bugs remaining. So do us a solid and deploy it in non-critical
CI environments and for day-to-day use, but maybe don't use it
for production maintenance or frontline continuous deployment
just yet.

#### REMOVE INSTALLED BINARIES ON WINDOWS

So waaaay back at the start of August, I fixed a bug with
[#9198](https://github.com/npm/npm/pull/9198). That fix made it
so that if you had two modules installed that both installed the
same binary (eg `gulp` & `gulp-cli`), that removing one wouldn't
remove the binary if it was owned by the other.

It did this by doing some hocus-pocus that, turns out, was
Unix-specific, so on Windows it just threw up its hands and
stopped removing installed binaries at all. Not great.

So today we're fixing that– it let us maintain the same safety
that we added in #9198, but ALSO works with Windows.

* [`25fbaed`](https://github.com/npm/npm/commit/25fbaed)
  [#9394](https://github.com/npm/npm/issues/9394)
  Treat cmd-shims the same way we treat symlinks
  ([@iarna](https://github.com/iarna))

#### API DOCUMENTATION HAS BEEN SACRIFICED THE API GOD

The documentation of the internal APIs of npm is going away,
because it would lead people into thinking they should integrate
with npm by using it. Please don't do that! In the future, we'd
like to give you a suite of stand alone modules that provide
better, more stand alone APIs for your applications to build on.
But for now, call the npm binary with `process.exec` or
`process.spawn` instead.

* [`2fb60bf`](https://github.com/npm/npm/commit/2fb60bf)
  Remove misleading API documentation
  ([@othiym23](https://github.com/othiym23))

#### ALLOW `npm link` ON WINDOWS W/ PRERELEASE VERSIONS OF NODE

We never meant to have this be a restriction in the first place
and it was only just discovered with the recent node 4.0.0
release candidate.

* [`6665e54`](https://github.com/npm/npm/commit/6665e54)
  [#9505](https://github.com/npm/npm/pull/9505)
  Allow npm link to run on Windows with prerelease versions of
  node
  ([@jon-hall](https://github.com/jon-hall))

#### graceful-fs update

We're updating all of npm's deps to use the most recent
`graceful-fs`. This turns out to be important for future not yet
released versions of node, because older versions monkey-patch
`fs` in ways that will break in the future. Plus it ALSO makes
use of `process.binding` which is an internal API that npm
definitely shouldn't have been using. We're not done yet, but
this is the bulk of them.

* [`e7bc98e`](https://github.com/npm/npm/commit/e7bc98e)
  `write-file-atomic@1.1.3`
  ([@iarna](https://github.com/iarna))
* [`7417600`](https://github.com/npm/npm/commit/7417600)
  `tar@2.2.1`
  ([@zkat](https://github.com/zkat))
* [`e4e9d40`](https://github.com/npm/npm/commit/e4e9d40)
  `read-package-json@2.0.1`
  ([@zkat](https://github.com/zkat))
* [`481611d`](https://github.com/npm/npm/commit/481611d)
  `read-installed@4.0.3`
  ([@zkat](https://github.com/zkat))
* [`0dabbda`](https://github.com/npm/npm/commit/0dabbda)
  `npm-registry-client@7.0.4`
  ([@zkat](https://github.com/zkat))
* [`c075a91`](https://github.com/npm/npm/commit/c075a91)
  `fstream@1.0.8`
  ([@zkat](https://github.com/zkat))
* [`2e4341a`](https://github.com/npm/npm/commit/2e4341a)
  `fs-write-stream-atomic@1.0.4`
  ([@zkat](https://github.com/zkat))
* [`18ad16e`](https://github.com/npm/npm/commit/18ad16e)
  `fs-vacuum@1.2.7`
  ([@zkat](https://github.com/zkat))

#### DEPENDENCY UPDATES

* [`9d6666b`](https://github.com/npm/npm/commit/9d6666b)
  `node-gyp@3.0.1`
  ([@rvagg](https://github.com/rvagg))
* [`349c4df`](https://github.com/npm/npm/commit/349c4df)
  `retry@0.7.0`
  ([@tim-kos](https://github.com/tim-kos))
* [`f507551`](https://github.com/npm/npm/commit/f507551)
  `which@1.1.2`
  ([@isaacs](https://github.com/isaacs))
* [`e5b6743`](https://github.com/npm/npm/commit/e5b6743)
  `nopt@3.0.4`
  ([@zkat](https://github.com/zkat))

#### THE DEPENDENCIES OF OUR DEPENDENCIES ARE OUR DEPENDENCIES UPDATES

* [`316382d`](https://github.com/npm/npm/commit/316382d)
  `mime-types@2.1.6` & `mime-db@1.18.0`
* [`64b741e`](https://github.com/npm/npm/commit/64b741e)
  `spdx-correct@1.0.1`
* [`fff62ac`](https://github.com/npm/npm/commit/fff62ac)
  `process-nextick-args@1.0.3`
* [`9d6488c`](https://github.com/npm/npm/commit/9d6488c)
  `cryptiles@2.0.5`
* [`1912012`](https://github.com/npm/npm/commit/1912012)
  `bluebird@2.10.0`
* [`4d09402`](https://github.com/npm/npm/commit/4d09402)
  `readdir-scoped-modules@1.0.2`

### v3.3.2 (2015-09-04):

#### PLEASE HOLD FOR THE NEXT AVAILABLE MAINTAINER

This is a tiny little maintenance release, both to update dependencies and to
keep `npm@3` up to date with changes made to `npm@2`.
[@othiym23](https://github.com/othiym23) is putting out this release (again) as
his esteemed colleague [@iarna](https://github.com/iarna) finishes relocating
herself, her family, and her sizable anime collection all the way across North
America. It contains [all the goodies in
`npm@2.14.3`](https://github.com/npm/npm/releases/tag/v2.14.3) and one other
dependency update.

#### BETA WARNINGS FOR FUN AND PROFIT

**_THIS IS BETA SOFTWARE_**. `npm@3` will remain in beta until we're
confident that it's stable and have assessed the effect of the breaking
changes on the community.  During that time we will still be doing `npm@2`
releases, with `npm@2` tagged as `latest` and `next`.  We'll _also_ be
publishing new releases of `npm@3` as `npm@v3.x-next` and `npm@v3.x-latest`
alongside those versions until we're ready to switch everyone over to
`npm@3`.  We need your help to find and fix its remaining bugs.  It's a
significant rewrite, so we are _sure_ there still significant bugs
remaining.  So do us a solid and deploy it in non-critical CI environments
and for day-to-day use, but maybe don't use it for production maintenance or
frontline continuous deployment just yet.

That said, it's getting there! It will be leaving beta very soon!

#### ONE OTHER DEPENDENCY UPDATE

* [`bb5de34`](https://github.com/npm/npm/commit/bb5de3493531228df0bd3f0742d5493c826be6dd)
  `is-my-json-valid@2.12.2`: Upgrade to a new, modernized version of
  `json-pointer`. ([@mafintosh](https://github.com/mafintosh))

### v3.3.1 (2015-08-27):

Hi all, this `npm@3` update brings you another round of bug fixes.  The
headliner here is that `npm update` works again.  We're running down the
clock on blocker 3.x issues!  Shortly after that hits zero we'll be
promoting 3.x to latest!!

And of course, we have changes that were brought forward from 2.x. Check out
the release notes for
[2.14.1](https://github.com/npm/npm/releases/tag/v2.14.1) and
[2.14.2](https://github.com/npm/npm/releases/tag/v2.14.2).

#### BETA WARNINGS FOR FUN AND PROFIT

**_THIS IS BETA SOFTWARE_**. `npm@3` will remain in beta until we're
confident that it's stable and have assessed the effect of the breaking
changes on the community.  During that time we will still be doing `npm@2`
releases, with `npm@2` tagged as `latest` and `next`.  We'll _also_ be
publishing new releases of `npm@3` as `npm@v3.x-next` and `npm@v3.x-latest`
alongside those versions until we're ready to switch everyone over to
`npm@3`.  We need your help to find and fix its remaining bugs.  It's a
significant rewrite, so we are _sure_ there still significant bugs
remaining.  So do us a solid and deploy it in non-critical CI environments
and for day-to-day use, but maybe don't use it for production maintenance or
frontline continuous deployment just yet.

#### NPM UPDATE, NOW AGAIN YOUR FRIEND

* [`f130a00`](https://github.com/npm/npm/commit/f130a00)
  [#9095](https://github.com/npm/npm/issues/9095)
  `npm update` once again works! Previously, after selecting packages
  to update, it would then pick the wrong location to run the install
  from. ([@iarna](https://github.com/iarna))

#### MORE VERBOSING FOR YOUR VERBOSE LIFECYCLES

* [`d088b7d`](https://github.com/npm/npm/commit/d088b7d)
  [#9227](https://github.com/npm/npm/pull/9227)
  Add some additional logging at the verbose and silly levels
  when running lifecycle scripts. Hopefully this will make
  debugging issues with them a bit easier!
  ([@saper](https://github.com/saper))

#### AND SOME OTHER BUG FIXES…

* [`f4a5784`](https://github.com/npm/npm/commit/f4a5784)
  [#9308](https://github.com/npm/npm/issues/9308)
  Make fetching metadata for local modules faster! This ALSO means
  that doing things like running `npm repo` won't build your
  module and maybe run `prepublish`.
  ([@iarna](https://github.com/iarna))

* [`4468c92`](https://github.com/npm/npm/commit/4468c92)
  [#9205](https://github.com/npm/npm/issues/9205)
  Fix a bug where local modules would sometimes not resolve relative
  links using the correct base path.
  ([@iarna](https://github.com/iarna))

* [`d395a6b`](https://github.com/npm/npm/commit/d395a6b)
  [#8995](https://github.com/npm/npm/issues/8995)
  Certain combinations of packages could result in different install orders for their
  initial installation than for reinstalls run on the same folder.
  ([@iarna](https://github.com/iarna))

* [`d119ea6`](https://github.com/npm/npm/commit/d119ea6)
  [#9113](https://github.com/npm/npm/issues/9113)
  Make extraneous packages _always_ up in `npm ls`. Previously, if an
  extraneous package had a dependency that depended back on the original
  package this would result in the package not showing up in `ls`.
  ([@iarna](https://github.com/iarna))

* [`02420dc`](https://github.com/npm/npm/commit/02420dc)
  [#9113](https://github.com/npm/npm/issues/9113)
  Stop warning about missing top level package.json files. Errors in said
  files will still be reported.
  ([@iarna](https://github.com/iarna))

#### SOME DEP UPDATES

* [`1ed1364`](https://github.com/npm/npm/commit/1ed1364) `rimraf@2.4.3`
  ([@isaacs](https://github.com/isaacs)) Added EPERM to delay/retry loop
* [`e7b8315`](https://github.com/npm/npm/commit/e7b8315) `read@1.0.7`
  Smaller distribution package, better metadata
  ([@isaacs](https://github.com/isaacs))

#### SOME DEPS OF DEPS UPDATES

* [`b273bcc`](https://github.com/npm/npm/commit/b273bcc) `mime-types@2.1.5`
* [`df6e225`](https://github.com/npm/npm/commit/df6e225) `mime-db@1.17.0`
* [`785f2ad`](https://github.com/npm/npm/commit/785f2ad) `is-my-json-valid@2.12.1`
* [`88170dd`](https://github.com/npm/npm/commit/88170dd) `form-data@1.0.0-rc3`
* [`af5357b`](https://github.com/npm/npm/commit/af5357b) `request@2.61.0`
* [`337f96a`](https://github.com/npm/npm/commit/337f96a) `chalk@1.1.1`
* [`3dfd74d`](https://github.com/npm/npm/commit/3dfd74d) `async@1.4.2`

### v3.3.0 (2015-08-13):

This is a pretty EXCITING week.  But I may be a little excitable– or
possibly sleep deprived, it's sometimes hard to tell them apart. =D So
[Kat](https://github.com/zkat) really went the extra mile this week and got
the client side support for teams and orgs out in this week's 2.x release.
You can't use that just yet, 'cause we have to turn on some server side
stuff too, but this way it'll be there for you all the moment we do!  Check
out the details over in the [2.14.0 release
notes](https://github.com/npm/npm/releases/tag/v2.14.0)!

But we over here in 3.x ALSO got a new feature this week, check out the new
`--only` and `--also` flags for better control over when dev and production
dependencies are used by various npm commands.

That, and some important bug fixes round out this week. Enjoy everyone!

#### NEVER SHALL NOT BETA THE BETA

**_THIS IS BETA SOFTWARE_**.  EXCITING NEW BETA WARNING!!!  Ok, I fibbed,
EXACTLY THE SAME BETA WARNINGS: `npm@3` will remain in beta until we're
confident that it's stable and have assessed the effect of the breaking
changes on the community.  During that time we will still be doing `npm@2`
releases, with `npm@2` tagged as `latest` and `next`.  We'll _also_ be
publishing new releases of `npm@3` as `npm@v3.x-next` and `npm@v3.x-latest`
alongside those versions until we're ready to switch everyone over to
`npm@3`.  We need your help to find and fix its remaining bugs.  It's a
significant rewrite, so we are _sure_ there still significant bugs
remaining.  So do us a solid and deploy it in non-critical CI environments
and for day-to-day use, but maybe don't use it for production maintenance or
frontline continuous deployment just yet.

#### ONLY ALSO DEV

Hey we've got a SUPER cool new feature for you all, thanks to the fantastic
work of [@davglass](https://github.com/davglass) and
[@bengl](https://github.com/bengl) we have `--only=prod`,
`--only=dev`, `--also=prod` and `--also=dev` options. These apply in
various ways to: `npm install`, `npm ls`, `npm outdated` and `npm update`.

So for instance:

```
npm install --only=dev
```

Only installs dev dependencies. By contrast:

```
npm install --only=prod
```

Will only install prod dependencies and is very similar to `--production`
but differs in that it doesn't set the environment variables that
`--production` does.

The related new flag, `--also` is most useful with things like:

```
npm shrinkwrap --also=dev
```

As shrinkwraps don't include dev deps by default.  This replaces passing in
`--dev` in that scenario.

And that leads into the fact that this deprecates `--dev` as its semantics
across commands were inconsistent and confusing.

* [`3ab1eea`](https://github.com/npm/npm/commit/3ab1eea)
  [#9024](https://github.com/npm/npm/pull/9024)
  Add support for `--only`, `--also` and deprecate `--dev`
  ([@bengl](https://github.com/bengl))

#### DON'T TOUCH! THAT'S NOT YOUR BIN

* [`b31812e`](https://github.com/npm/npm/commit/b31812e)
  [#8996](https://github.com/npm/npm/pull/8996)
  When removing a module that has bin files, if one that we're going to
  remove is a symlink to a DIFFERENT module, leave it alone. This only happens
  when you have two modules that try to provide the same bin.
  ([@iarna](https://github.com/iarna))

#### THERE'S AN END IN SIGHT

* [`d2178a9`](https://github.com/npm/npm/commit/d2178a9)
  [#9223](https://github.com/npm/npm/pull/9223)
  Close a bunch of infinite loops that could show up with symlink cycles in your dependencies.
  ([@iarna](https://github.com/iarna))

#### OOPS DIDN'T MEAN TO FIX THAT

Well, not _just_ yet.  This was scheduled for next week, but it snuck into
2.x this week.

* [`139dd92`](https://github.com/npm/npm/commit/139dd92)
  [#8716](https://github.com/npm/npm/pull/8716)
  `npm init` will now only pick up the modules you install, not everything
  else that got flattened with them.
  ([@iarna](https://github.com/iarna))

### v3.2.2 (2015-08-08):

Lot's of lovely bug fixes for `npm@3`.  I'm also suuuuper excited that I
think we have a handle on stack explosions that effect a small portion of
our users.  We also have some tantalizing clues as to where some low hanging
fruit may be for performance issues.

And of course, in addition to the `npm@3` specific bug fixes, there are some
great one's coming in from `npm@2`!  [@othiym23](https://github.com/othiym23)
put together that release this week– check out its
[release notes](https://github.com/npm/npm/releases/tag/v2.13.4) for the deets.

#### AS ALWAYS STILL BETA

**_THIS IS BETA SOFTWARE_**.  Just like the airline safety announcements,
we're not taking this plane off till we finish telling you: `npm@3` will
remain in beta until we're confident that it's stable and have assessed the
effect of the breaking changes on the community.  During that time we will
still be doing `npm@2` releases, with `npm@2` tagged as `latest` and `next`.
We'll _also_ be publishing new releases of `npm@3` as `npm@v3.x-next` and
`npm@v3.x-latest` alongside those versions until we're ready to switch
everyone over to `npm@3`.  We need your help to find and fix its remaining
bugs.  It's a significant rewrite, so we are _sure_ there still significant
bugs remaining.  So do us a solid and deploy it in non-critical CI
environments and for day-to-day use, but maybe don't use it for production
maintenance or frontline continuous deployment just yet.

#### BUG FIXES

* [`a8c8a13`](https://github.com/npm/npm/commit/a8c8a13)
  [#9050](https://github.com/npm/npm/issues/9050)
  Resolve peer deps relative to the parent of the requirer
  ([@iarna](http://github.com/iarna))
* [`05f0226`](https://github.com/npm/npm/commit/05f0226)
  [#9077](https://github.com/npm/npm/issues/9077)
  Fix crash when saving `git+ssh` urls
  ([@iarna](http://github.com/iarna))
* [`e4a3808`](https://github.com/npm/npm/commit/e4a3808)
  [#8951](https://github.com/npm/npm/issues/8951)
  Extend our patch to allow `*` to match something when a package only has
  prerelease versions to everything and not just the cache.
  ([@iarna](http://github.com/iarna))
* [`d135abf`](https://github.com/npm/npm/commit/d135abf)
  [#8871](https://github.com/npm/npm/issues/8871)
  Don't warn about a missing `package.json` or missing fields in the global
  install directory.
  ([@iarna](http://github.com/iarna))

#### DEP VERSION BUMPS

* [`990ee4f`](https://github.com/npm/npm/commit/990ee4f)
  `path-is-inside@1.0.1` ([@domenic](https://github.com/domenic))
* [`1f71ec0`](https://github.com/npm/npm/commit/1f71ec0)
  `lodash.clonedeep@3.0.2` ([@jdalton](https://github.com/jdalton))
* [`a091354`](https://github.com/npm/npm/commit/a091354)
  `marked@0.3.5` ([@chjj](https://github.com/chjj))
* [`fc51f28`](https://github.com/npm/npm/commit/fc51f28)
  `tap@1.3.2` ([@isaacs](https://github.com/isaacs))
* [`3569ec0`](https://github.com/npm/npm/commit/3569ec0)
  `nock@2.10.0` ([@pgte](https://github.com/pgte))
* [`ad5f6fd`](https://github.com/npm/npm/commit/ad5f6fd)
  `npm-registry-mock@1.0.1` ([@isaacs](https://github.com/isaacs))

### v3.2.1 (2015-07-31):

#### AN EXTRA QUIET RELEASE

A bunch of stuff got deferred for various reasons, which just means more
branches to land next week!

Don't forget to check out [Kat's 2.x release](https://github.com/npm/npm/releases/tag/v2.13.4) for other quiet goodies.

#### AS ALWAYS STILL BETA

**_THIS IS BETA SOFTWARE_**.  Yes, we're still reminding you of this.  No,
you can't be excused.  `npm@3` will remain in beta until we're confident
that it's stable and have assessed the effect of the breaking changes on the
community.  During that time we will still be doing `npm@2` releases, with
`npm@2` tagged as `latest` and `next`.  We'll _also_ be publishing new
releases of `npm@3` as `npm@v3.x-next` and `npm@v3.x-latest` alongside those
versions until we're ready to switch everyone over to `npm@3`.  We need your
help to find and fix its remaining bugs.  It's a significant rewrite, so we
are _sure_ there still significant bugs remaining.  So do us a solid and
deploy it in non-critical CI environments and for day-to-day use, but maybe
don't use it for production maintenance or frontline continuous deployment
just yet.


#### MAKING OUR TESTS TEST THE THING THEY TEST

* [`6e53c3d`](https://github.com/npm/npm/commit/6e53c3d)
  [#8985](https://github.com/npm/npm/pull/8985)
  Many thanks to @bengl for noticing that one of our tests wasn't testing
  what it claimed it was testing! ([@bengl](https://github.com/bengl))

#### MY PACKAGE.JSON WAS ALREADY IN THE RIGHT ORDER

* [`eb2c7aa`](https://github.com/npm/npm/commit/d00d0f)
  [#9068](https://github.com/npm/npm/pull/9079)
  Stop sorting keys in the `package.json` that we haven't edited.  Many
  thanks to [@Qix-](https://github.com/Qix-) for bringing this up and
  providing a first pass at a patch for this.
  ([@iarna](https://github.com/iarna))

#### DEV DEP UPDATE

* [`555f60c`](https://github.com/npm/npm/commit/555f60c) `marked@0.3.4`

### v3.2.0 (2015-07-24):

#### MORE CONFIG, BETTER WINDOWS AND A BUG FIX

This is a smallish release with a new config option and some bug fixes.  And
lots of module updates.

#### BETA BETAS ON

**_THIS IS BETA SOFTWARE_**.  Yes, we're still reminding you of this.  No,
you can't be excused.  `npm@3` will remain in beta until we're confident
that it's stable and have assessed the effect of the breaking changes on the
community.  During that time we will still be doing `npm@2` releases, with
`npm@2` tagged as `latest` and `next`.  We'll _also_ be publishing new
releases of `npm@3` as `npm@v3.x-next` and `npm@v3.x-latest` alongside those
versions until we're ready to switch everyone over to `npm@3`.  We need your
help to find and fix its remaining bugs.  It's a significant rewrite, so we
are _sure_ there still significant bugs remaining.  So do us a solid and
deploy it in non-critical CI environments and for day-to-day use, but maybe
don't use it for production maintenance or frontline continuous deployment
just yet.


#### NEW CONFIGS, LESS PROGRESS

* [`423d8f7`](https://github.com/npm/npm/commit/423d8f7)
  [#8704](https://github.com/npm/npm/issues/8704)
  Add the ability to disable the new progress bar with `--no-progress`
  ([@iarna](https://github.com/iarna))

#### AND BUG FIXES

* [`b3ee452`](https://github.com/npm/npm/commit/b3ee452)
  [#9038](https://github.com/npm/npm/pull/9038)
  We previously disabled the use of the new `fs.access` API on Windows, but
  the bug we were seeing is fixed in `io.js@1.5.0` so we now use `fs.access`
  if you're using that version or greater.
  ([@iarna](https://github.com/iarna))

* [`b181fa3`](https://github.com/npm/npm/commit/b181fa3)
  [#8921](https://github.com/npm/npm/issues/8921)
  [#8637](https://github.com/npm/npm/issues/8637)
  Rejigger how we validate modules for install. This allow is to fix
  a problem where arch/os checking wasn't being done at all.
  It also made it easy to add back in a check that declines to
  install a module in itself unless you force it.
  ([@iarna](https://github.com/iarna))

#### AND A WHOLE BUNCH OF SUBDEP VERSIONS

These are all development dependencies and semver-compatible subdep
upgrades, so they should not have visible impact on users.

* [`6b3f6d9`](https://github.com/npm/npm/commit/6b3f6d9) `standard@4.3.3`
* [`f4e22e5`](https://github.com/npm/npm/commit/f4e22e5) `readable-stream@2.0.2` (inside concat-stream)
* [`f130bfc`](https://github.com/npm/npm/commit/f130bfc) `minimatch@2.0.10` (inside node-gyp's copy of glob)
* [`36c6a0d`](https://github.com/npm/npm/commit/36c6a0d) `caseless@0.11.0`
* [`80df59c`](https://github.com/npm/npm/commit/80df59c) `chalk@1.1.0`
* [`ea935d9`](https://github.com/npm/npm/commit/ea935d9) `bluebird@2.9.34`
* [`3588a0c`](https://github.com/npm/npm/commit/3588a0c) `extend@3.0.0`
* [`c6a8450`](https://github.com/npm/npm/commit/c6a8450) `form-data@1.0.0-rc2`
* [`a04925b`](https://github.com/npm/npm/commit/a04925b) `har-validator@1.8.0`
* [`ee7c095`](https://github.com/npm/npm/commit/ee7c095) `has-ansi@2.0.0`
* [`944fc34`](https://github.com/npm/npm/commit/944fc34) `hawk@3.1.0`
* [`783dc7b`](https://github.com/npm/npm/commit/783dc7b) `lodash._basecallback@3.3.1`
* [`acef0fe`](https://github.com/npm/npm/commit/acef0fe) `lodash._baseclone@3.3.0`
* [`dfe959a`](https://github.com/npm/npm/commit/dfe959a) `lodash._basedifference@3.0.3`
* [`a03bc76`](https://github.com/npm/npm/commit/a03bc76) `lodash._baseflatten@3.1.4`
* [`8a07d50`](https://github.com/npm/npm/commit/8a07d50) `lodash._basetostring@3.0.1`
* [`7785e3f`](https://github.com/npm/npm/commit/7785e3f) `lodash._baseuniq@3.0.3`
* [`826fb35`](https://github.com/npm/npm/commit/826fb35) `lodash._createcache@3.1.2`
* [`76030b3`](https://github.com/npm/npm/commit/76030b3) `lodash._createpadding@3.6.1`
* [`1a49ec6`](https://github.com/npm/npm/commit/1a49ec6) `lodash._getnative@3.9.1`
* [`eebe47f`](https://github.com/npm/npm/commit/eebe47f) `lodash.isarguments@3.0.4`
* [`09994d4`](https://github.com/npm/npm/commit/09994d4) `lodash.isarray@3.0.4`
* [`b6f8dbf`](https://github.com/npm/npm/commit/b6f8dbf) `lodash.keys@3.1.2`
* [`c67dd6b`](https://github.com/npm/npm/commit/c67dd6b) `lodash.pad@3.1.1`
* [`4add042`](https://github.com/npm/npm/commit/4add042) `lodash.repeat@3.0.1`
* [`e04993c`](https://github.com/npm/npm/commit/e04993c) `lru-cache@2.6.5`
* [`2ed7da4`](https://github.com/npm/npm/commit/2ed7da4) `mime-db@1.15.0`
* [`ae08244`](https://github.com/npm/npm/commit/ae08244) `mime-types@2.1.3`
* [`e71410e`](https://github.com/npm/npm/commit/e71410e) `os-homedir@1.0.1`
* [`67c13e0`](https://github.com/npm/npm/commit/67c13e0) `process-nextick-args@1.0.2`
* [`12ee041`](https://github.com/npm/npm/commit/12ee041) `qs@4.0.0`
* [`15564a6`](https://github.com/npm/npm/commit/15564a6) `spdx-license-ids@1.0.2`
* [`8733bff`](https://github.com/npm/npm/commit/8733bff) `supports-color@2.0.0`
* [`230943c`](https://github.com/npm/npm/commit/230943c) `tunnel-agent@0.4.1`
* [`26a4653`](https://github.com/npm/npm/commit/26a4653) `ansi-styles@2.1.0`
* [`3d27081`](https://github.com/npm/npm/commit/3d27081) `bl@1.0.0`
* [`9efa110`](https://github.com/npm/npm/commit/9efa110) `async@1.4.0`

#### MERGED FORWARD

* As usual, we've ported all the `npm@2` goodies in this week's
  [v2.13.3](https://github.com/npm/npm/releases/tag/v2.13.3)
  release.

### v3.1.3 (2015-07-17):

Rebecca: So Kat, I hear this week's other release uses a dialog between us to
explain what changed?

Kat: Well, you could say that…

Rebecca: I would! This week I fixed more `npm@3` bugs!

Kat: That sounds familiar.

Rebecca: Eheheheh, well, before we look at those, a word from our sponsor…

#### BETA IS AS BETA DOES

**_THIS IS BETA SOFTWARE_**.  Yes, we're still reminding you of this.  No,
you can't be excused.  `npm@3` will remain in beta until we're confident
that it's stable and have assessed the effect of the breaking changes on the
community.  During that time we will still be doing `npm@2` releases, with
`npm@2` tagged as `latest` and `next`.  We'll _also_ be publishing new
releases of `npm@3` as `npm@v3.x-next` and `npm@v3.x-latest` alongside those
versions until we're ready to switch everyone over to `npm@3`.  We need your
help to find and fix its remaining bugs.  It's a significant rewrite, so we
are _sure_ there still significant bugs remaining.  So do us a solid and
deploy it in non-critical CI environments and for day-to-day use, but maybe
don't use it for production maintenance or frontline continuous deployment
just yet.

Rebecca: Ok, enough of the dialoguing, that's Kat's schtick.  But do remember
kids, betas hide in dark hallways waiting to break your stuff, stuff like…

#### SO MANY LINKS YOU COULD MAKE A CHAIN

* [`6d69ec9`](https://github.com/npm/npm/6d69ec9)
  [#8967](https://github.com/npm/npm/issues/8967)
  Removing a module linked into your globals would result in having
  all of its subdeps removed. Since the npm release process does
  exactly this, it burned me -every- -single- -week-. =D
  While we're here, we also removed extraneous warns that used to
  spill out when you'd remove a symlink.
  ([@iarna](https://github.com/iarna))

* [`fdb360f`](https://github.com/npm/npm/fdb360f)
  [#8874](https://github.com/npm/npm/issues/8874)
  Linking scoped modules was failing outright, but this fixes that
  and updates our tests so we don't do it again.
  ([@iarna](https://github.com/iarna))

#### WE'LL TRY NOT TO CRACK YOUR WINDOWS

* [`9fafb18`](https://github.com/npm/npm/9fafb18)
  [#8701](https://github.com/npm/npm/issues/8701)
  `npm@3` introduced permissions checks that run before it actually tries to
  do something. This saves you from having an install fail half way
  through. We did this using the shiny new `fs.access` function available
  in `node 0.12` and `io.js`, with fallback options for older nodes. Unfortunately
  the way we implemented the fallback caused racey problems for Windows systems.
  This fixes that by ensuring we only ever run any one check on a directory once.
  BUT it turns out there are bugs in `fs.access` on Windows. So this ALSO just disables
  the use of `fs.access` on Windows entirely until that settles out.
  ([@iarna](https://github.com/iarna))

#### ZOOM ZOOM, DEP UPDATES

* [`5656baa`](https://github.com/npm/npm/5656baa)
  `gauge@1.2.2`: Better handle terminal resizes while printing the progress bar
  ([@iarna](https://github.com/iarna))

#### MERGED FORWARD

* Check out Kat's [super-fresh release notes for v2.13.2](https://github.com/npm/npm/releases/tag/v2.13.2)
  and see all the changes we ported from `npm@2`.

### v3.1.2

#### SO VERY BETA RELEASE

So, `v3.1.1` managed to actually break installing local modules.  And then
immediately after I drove to an island for the weekend. 😁  So let's get
this fixed outside the usual release train!

Fortunately it didn't break installing _global_ modules and so you could
swap it out for another version at least.

#### DISCLAIMER MEANS WHAT IT SAYS

**_THIS IS BETA SOFTWARE_**.  Yes, we're still reminding you of this.  No,
you can't be excused.  `npm@3` will remain in beta until we're confident
that it's stable and have assessed the effect of the breaking changes on the
community.  During that time we will still be doing `npm@2` releases, with
`npm@2` tagged as `latest` and `next`.  We'll _also_ be publishing new
releases of `npm@3` as `npm@v3.x-next` and `npm@v3.x-latest` alongside those
versions until we're ready to switch everyone over to `npm@3`.  We need your
help to find and fix its remaining bugs.  It's a significant rewrite, so we
are _sure_ there still significant bugs remaining.  So do us a solid and
deploy it in non-critical CI environments and for day-to-day use, but maybe
don't use it for production maintenance or frontline continuous deployment
just yet.

#### THIS IS IT, THE REASON

* [`f5e19df`](https://github.com/npm/npm/commit/f5e19df)
  [#8893](https://github.com/npm/npm/issues/8893)
  Fix crash when installing local modules introduced by the fix for
  [#8608](https://github.com/npm/npm/issues/8608)
  ([@iarna](https://github.com/iarna)

### v3.1.1

#### RED EYE RELEASE

Rebecca's up too late writing tests, so you can have `npm@3` bug fixes!  Lots
of great new issues from you all! ❤️️  Keep it up!

#### YUP STILL BETA, PLEASE PAY ATTENTION

**_THIS IS BETA SOFTWARE_**.  Yes, we're still reminding you of this.  No,
you can't be excused.  `npm@3` will remain in beta until we're confident
that it's stable and have assessed the effect of the breaking changes on the
community.  During that time we will still be doing `npm@2` releases, with
`npm@2` tagged as `latest` and `next`.  We'll _also_ be publishing new
releases of `npm@3` as `npm@v3.x-next` and `npm@v3.x-latest` alongside those
versions until we're ready to switch everyone over to `npm@3`.  We need your
help to find and fix its remaining bugs.  It's a significant rewrite, so we
are _sure_ there still significant bugs remaining.  So do us a solid and
deploy it in non-critical CI environments and for day-to-day use, but maybe
don't use it for production maintenance or frontline continuous deployment
just yet.

#### BOOGS

* [`9badfd6`](https://github.com/npm/npm/commit/9babfd63f19f2d80b2d2624e0963b0bdb0d76ef4)
  [#8608](https://github.com/npm/npm/issues/8608)
  Make global installs and uninstalls MUCH faster by only reading the directories of
  modules referred to by arguments.
  ([@iarna](https://github.com/iarna)
* [`075a5f0`](https://github.com/npm/npm/commit/075a5f046ab6837f489b08d44cb601e9fdb369b7)
  [#8660](https://github.com/npm/npm/issues/8660)
  Failed optional deps would still result in the optional deps own
  dependencies being installed. We now find them and fail them out of the
  tree.
  ([@iarna](https://github.com/iarna)
* [`c9fbbb5`](https://github.com/npm/npm/commit/c9fbbb540083396ea58fd179d81131d959d8e049)
  [#8863](https://github.com/npm/npm/issues/8863)
  The "no compatible version found" error message was including only the
  version requested, not the name of the package we wanted. Ooops!
  ([@iarna](https://github.com/iarna)
* [`32e6bbd`](https://github.com/npm/npm/commit/32e6bbd21744dcbe8c0720ab53f60caa7f2a0588)
  [#8806](https://github.com/npm/npm/issues/8806)
  The "uninstall" lifecycle was being run after all of a module's dependencies has been
  removed. This reverses that order-- this means "uninstall" lifecycles can make use
  of the package's dependencies.
  ([@iarna](https://github.com/iarna)

#### MERGED FORWARD

* Check out the [v2.13.1 release notes](https://github.com/npm/npm/releases/tag/v2.13.1)
  and see all the changes we ported from `npm@2`.

### v3.1.0 (2015-07-02):

This has been a brief week of bug fixes, plus some fun stuff merged forward
from this weeks 2.x release. See the
[2.13.0 release notes](https://github.com/npm/npm/releases/tag/v2.13.0)
for details on that.

You all have been AWESOME with
[all](https://github.com/npm/npm/milestones/3.x)
[the](https://github.com/npm/npm/milestones/3.2.0)
`npm@3` bug reports! Thank you and keep up the great work!

#### NEW PLACE, SAME CODE

Remember how last week we said `npm@3` would go to `3.0-next` and latest
tags? Yeaaah, no, please use `npm@v3.x-next` and `npm@v3.x-latest` going forward.

I dunno why we said "suuure, we'll never do a feature release till we're out
of beta" when we're still forward porting `npm@2.x` features. `¯\_(ツ)_/¯`

If you do accidentally use the old tag names, I'll be maintaining them
for a few releases, but they won't be around forever.

#### YUP STILL BETA, PLEASE PAY ATTENTION

**_THIS IS BETA SOFTWARE_**. `npm@3` will remain in beta until we're
confident that it's stable and have assessed the effect of the breaking
changes on the community. During that time we will still be doing `npm@2`
releases, with `npm@2` tagged as `latest` and `next`. We'll _also_ be
publishing new releases of `npm@3` as `npm@v3.x-next` and `npm@v3.x-latest`
alongside those versions until we're ready to switch everyone over to
`npm@3`. We need your help to find and fix its remaining bugs. It's a
significant rewrite, so we are _sure_ there still significant bugs
remaining. So do us a solid and deploy it in non-critical CI environments
and for day-to-day use, but maybe don't use it for production maintenance
or frontline continuous deployment just yet.

#### BUGS ON THE WINDOWS

  * [`0030ade`](https://github.com/npm/npm/commit/0030ade)
    [#8685](https://github.com/npm/npm/issues/8685)
    Windows would hang when trying to clone git repos
    ([@euprogramador](https://github.com/npm/npm/pull/8777))
  * [`b259bcc`](https://github.com/npm/npm/commit/b259bcc)
    [#8786](https://github.com/npm/npm/pull/8786)
    Windows permissions checks would cause installations to fail under some
    circumstances. We're disabling the checks entirely for this release.
    I'm hoping to check back with this next week to get a Windows friendly
    fix in.
    ([@iarna](https://github.com/iarna))

#### SO MANY BUGS SQUASHED, JUST CALL US RAID

  * [`0848698`](https://github.com/npm/npm/commit/0848698)
    [#8686](https://github.com/npm/npm/pull/8686)
    Stop leaving progress bar cruft on the screen during publication
    ([@ajcrites](https://github.com/ajcrites))
  * [`57c3cea`](https://github.com/npm/npm/commit/57c3cea)
    [#8695](https://github.com/npm/npm/pull/8695)
    Remote packages with shrinkwraps made npm cause node + iojs to explode
    and catch fire. NO MORE.
    ([@iarna](https://github.com/iarna))
  * [`2875ba3`](https://github.com/npm/npm/commit/2875ba3)
    [#8723](https://github.com/npm/npm/pull/8723)
    I uh, told you that engineStrict checking had gone away last week.
    TURNS OUT I LIED. So this is making that actually be true.
    ([@iarna](https://github.com/iarna))
  * [`28064e5`](https://github.com/npm/npm/commit/28064e5)
    [#3358](https://github.com/npm/npm/issues/3358)
    Consistently allow Unicode BOMs at the start of package.json files.
    Previously this was allowed some of time, like when you were installing
    modules, but not others, like running npm version or installing w/
    `--save`.
    ([@iarna](https://github.com/iarna))
  * [`3cb6ad2`](https://github.com/npm/npm/commit/3cb6ad2)
    [#8736](https://github.com/npm/npm/issues/8766)
    `npm@3` wasn't running the "install" lifecycle in your current (toplevel)
    module. This broke modules that relied on C compilation. BOO.
    ([@iarna](https://github.com/iarna))
  * [`68da583`](https://github.com/npm/npm/commit/68da583)
    [#8766](https://github.com/npm/npm/issues/8766)
    To my great shame, `npm link package` wasn't working AT ALL if you
    didn't have `package` already installed.
    ([@iarna](https://github.com/iarna))
  * [`edd7448`](https://github.com/npm/npm/commit/edd7448)
    `read-package-tree@5.0.0`: This update makes read-package-tree not explode
    when there's bad data in your node_modules folder. `npm@2` silently
    ignores this sort of thing.
    ([@iarna](https://github.com/iarna))
  * [`0bb08c8`](https://github.com/npm/npm/commit/0bb08c8)
    [#8778](https://github.com/npm/npm/pull/8778)
    RELATEDLY, we now show any errors from your node_modules folder after
    your installation completes as warnings. We're also reporting these in
    `npm ls` now.
    ([@iarna](https://github.com/iarna))
  * [`6c248ff`](https://github.com/npm/npm/commit/6c248ff)
    [#8779](https://github.com/npm/npm/pull/8779)
    Hey, you know how we used to complain if your `package.json` was
    missing stuff? Well guess what, we are again. I know, I know, you can
    thank me later.
    ([@iarna](https://github.com/iarna))
  * [`d6f7c98`](https://github.com/npm/npm/commit/d6f7c98)
    So, when we were rolling back after errors we had untested code that
    tried to undo moves. Being untested it turns out it was very broken.
    I've removed it until we have time to do this right.
    ([@iarna](https://github.com/iarna))

#### NEW VERSION

Just the one. Others came in via the 2.x release. Do check out its
changelog, immediately following this message.

  * [`4e602c5`](https://github.com/npm/npm/commit/4e602c5) `lodash@3.2.2`

### v3.0.0 (2015-06-25):

Wow, it's finally here! This has been a long time coming. We are all
delighted and proud to be getting this out into the world, and are looking
forward to working with the npm user community to get it production-ready
as quickly as possible.

`npm@3` constitutes a nearly complete rewrite of npm's installer to be
easier to maintain, and to bring a bunch of valuable new features and
design improvements to you all.

[@othiym23](https://github.com/othiym23) and
[@isaacs](https://github.com/isaacs) have been
[talking about the changes](http://blog.npmjs.org/post/91303926460/npm-cli-roadmap-a-periodic-update)
in this release for well over a year, and it's been the primary focus of
[@iarna](https://github.com/iarna) since she joined the team.

Given that this is a near-total rewrite, all changes listed here are
[@iarna](https://github.com/iarna)'s work unless otherwise specified.

#### NO, REALLY, READ THIS PARAGRAPH. IT'S THE IMPORTANT ONE.

**_THIS IS BETA SOFTWARE_**. `npm@3` will remain in beta until we're
confident that it's stable and have assessed the effect of the breaking
changes on the community. During that time we will still be doing `npm@2`
releases, with `npm@2` tagged as `latest` and `next`. We'll _also_ be
publishing new releases of `npm@3` as `npm@3.0-next` and `npm@3.0-latest`
alongside those versions until we're ready to switch everyone over to
`npm@3`. We need your help to find and fix its remaining bugs. It's a
significant rewrite, so we are _sure_ there still significant bugs
remaining. So do us a solid and deploy it in non-critical CI environments
and for day-to-day use, but maybe don't use it for production maintenance
or frontline continuous deployment just yet.

#### BREAKING CHANGES

##### `peerDependencies`

`grunt`, `gulp`, and `broccoli` plugin maintainers take note! You will be
affected by this change!

* [#6930](https://github.com/npm/npm/issues/6930)
  ([#6565](https://github.com/npm/npm/issues/6565))
  `peerDependencies` no longer cause _anything_ to be implicitly installed.
  Instead, npm will now warn if a packages `peerDependencies` are missing,
  but it's up to the consumer of the module (i.e. you) to ensure the peers
  get installed / are included in `package.json` as direct `dependencies`
  or `devDependencies` of your package.
* [#3803](https://github.com/npm/npm/issues/3803)
  npm also no longer checks `peerDependencies` until after it has fully
  resolved the tree.

This shifts the responsibility for fulfilling peer dependencies from library
/ framework / plugin maintainers to application authors, and is intended to
get users out of the dependency hell caused by conflicting `peerDependency`
constraints. npm's job is to keep you _out_ of dependency hell, not put you
in it.

##### `engineStrict`

* [#6931](https://github.com/npm/npm/issues/6931) The rarely-used
  `package.json` option `engineStrict` has been deprecated for several
  months, producing warnings when it was used. Starting with `npm@3`, the
  value of the field is ignored, and engine violations will only produce
  warnings. If you, as a user, want strict `engines` field enforcement,
  just run `npm config set engine-strict true`.

As with the peer dependencies change, this is about shifting control from
module authors to application authors. It turns out `engineStrict` was very
difficult to understand even harder to use correctly, and more often than
not just made modules using it difficult to deploy.

##### `npm view`

* [`77f1aec`](https://github.com/npm/npm/commit/77f1aec) With `npm view` (aka
  `npm info`), always return arrays for versions, maintainers, etc. Previously
  npm would return a plain value if there was only one, and multiple values if
  there were more. ([@KenanY](https://github.com/KenanY))

#### KNOWN BUGS

Again, this is a _**BETA RELEASE**_, so not everything is working just yet.
Here are the issues that we already know about. If you run into something
that isn't on this list,
[let us know](https://github.com/npm/npm/issues/new)!

* [#8575](https://github.com/npm/npm/issues/8575)
  Circular deps will never be removed by the prune-on-uninstall code.
* [#8588](https://github.com/npm/npm/issues/8588)
  Local deps where the dep name and the name in the package.json differ
  don't result in an error.
* [#8637](https://github.com/npm/npm/issues/8637)
  Modules can install themselves as direct dependencies. `npm@2` declined to
  do this.
* [#8660](https://github.com/npm/npm/issues/8660)
  Dependencies of failed optional dependencies aren't rolled back when the
  optional dependency is, and then are reported as extraneous thereafter.

#### NEW FEATURES

##### The multi-stage installer!

* [#5919](https://github.com/npm/npm/issues/5919)
  Previously the installer had a set of steps it executed for each package
  and it would immediately start executing them as soon as it decided to
  act on a package.

  But now it executes each of those steps at the same time for all
  packages, waiting for all of one stage to complete before moving on. This
  eliminates many race conditions and makes the code easier to reason
  about.

This fixes, for instance:

* [#6926](https://github.com/npm/npm/issues/6926)
  ([#5001](https://github.com/npm/npm/issues/5001),
  [#6170](https://github.com/npm/npm/issues/6170))
  `install` and `postinstall` lifecycle scripts now only execute `after`
  all the module with the script's dependencies are installed.

##### Install: it looks different!

You'll now get a tree much like the one produced by `npm ls` that
highlights in orange the packages that were installed. Similarly, any
removed packages will have their names prefixed by a `-`.

Also, `npm outdated` used to include the name of the module in the
`Location` field:

```
Package                Current  Wanted  Latest  Location
deep-equal             MISSING   1.0.0   1.0.0  deep-equal
glob                     4.5.3   4.5.3  5.0.10  rimraf > glob
```

Now it shows the module that required it as the final point in the
`Location` field:

```
Package                Current  Wanted  Latest  Location
deep-equal             MISSING   1.0.0   1.0.0  npm
glob                     4.5.3   4.5.3  5.0.10  npm > rimraf
```

Previously the `Location` field was telling you where the module was on
disk. Now it tells you what requires the module. When more than one thing
requires the module you'll see it listed once for each thing requiring it.

##### Install: it works different!

* [#6928](https://github.com/npm/npm/issues/6928)
  ([#2931](https://github.com/npm/npm/issues/2931)
  [#2950](https://github.com/npm/npm/issues/2950))
  `npm install` when you have an `npm-shrinkwrap.json` will ensure you have
  the modules specified in it are installed in exactly the shape specified
  no matter what you had when you started.
* [#6913](https://github.com/npm/npm/issues/6913)
  ([#1341](https://github.com/npm/npm/issues/1341)
  [#3124](https://github.com/npm/npm/issues/3124)
  [#4956](https://github.com/npm/npm/issues/4956)
  [#6349](https://github.com/npm/npm/issues/6349)
  [#5465](https://github.com/npm/npm/issues/5465))
  `npm install` when some of your dependencies are missing sub-dependencies
  will result in those sub-dependencies being installed. That is, `npm
  install` now knows how to fix broken installs, most of the time.
* [#5465](https://github.com/npm/npm/issues/5465)
  If you directly `npm install` a module that's already a subdep of
  something else and your new version is incompatible, it will now install
  the previous version nested in the things that need it.
* [`a2b50cf`](https://github.com/npm/npm/commit/a2b50cf)
  [#5693](https://github.com/npm/npm/issues/5693)
  When installing a new module, if it's mentioned in your
  `npm-shrinkwrap.json` or your `package.json` use the version specifier
  from there if you didn't specify one yourself.

##### Flat, flat, flat!

Your dependencies will now be installed *maximally flat*.  Insofar as is
possible, all of your dependencies, and their dependencies, and THEIR
dependencies will be installed in your project's `node_modules` folder with no
nesting.  You'll only see modules nested underneath one another when two (or
more) modules have conflicting dependencies.

* [#3697](https://github.com/npm/npm/issues/3697)
  This will hopefully eliminate most cases where Windows users ended up
  with paths that were too long for Explorer and other standard tools to
  deal with.
* [#6912](https://github.com/npm/npm/issues/6912)
  ([#4761](https://github.com/npm/npm/issues/4761)
  [#4037](https://github.com/npm/npm/issues/4037))
  This also means that your installs will be deduped from the start.
* [#5827](https://github.com/npm/npm/issues/5827)
  This deduping even extends to git deps.
* [#6936](https://github.com/npm/npm/issues/6936)
  ([#5698](https://github.com/npm/npm/issues/5698))
  Various commands are dedupe aware now.

This has some implications for the behavior of other commands:

* `npm uninstall` removes any dependencies of the module that you specified
  that aren't required by any other module. Previously, it would only
  remove those that happened to be installed under it, resulting in left
  over cruft if you'd ever deduped.
* `npm ls` now shows you your dependency tree organized around what
  requires what, rather than where those modules are on disk.
* [#6937](https://github.com/npm/npm/issues/6937)
  `npm dedupe` now flattens the tree in addition to deduping.

And bundling of dependencies when packing or publishing changes too:

* [#2442](https://github.com/npm/npm/issues/2442)
  bundledDependencies no longer requires that you specify deduped sub deps.
  npm can now see that a dependency is required by something bundled and
  automatically include it. To put that another way, bundledDependencies
  should ONLY include things that you included in dependencies,
  optionalDependencies or devDependencies.
* [#5437](https://github.com/npm/npm/issues/5437)
  When bundling a dependency that's both a `devDependency` and the child of
  a regular `dependency`, npm bundles the child dependency.

As a demonstration of our confidence in our own work, npm's own
dependencies are now flattened, deduped, and bundled in the `npm@3` style.
This means that `npm@3` can't be packed or published by `npm@2`, which is
something to be aware of if you're hacking on npm.

##### Shrinkwraps: they are a-changin'!

First of all, they should be idempotent now
([#5779](https://github.com/npm/npm/issues/5779)). No more differences
because the first time you install (without `npm-shrinkwrap.json`) and the
second time (with `npm-shrinkwrap.json`).

* [#6781](https://github.com/npm/npm/issues/6781)
  Second, if you save your changes to `package.json` and you have
  `npm-shrinkwrap.json`, then it will be updated as well. This applies to
  all of the commands that update your tree:
  * `npm install --save`
  * `npm update --save`
  * `npm dedupe --save` ([#6410](https://github.com/npm/npm/issues/6410))
  * `npm uninstall --save`
* [#4944](https://github.com/npm/npm/issues/4944)
  ([#5161](https://github.com/npm/npm/issues/5161)
  [#5448](https://github.com/npm/npm/issues/5448))
  Third, because `node_modules` folders are now deduped and flat,
  shrinkwrap has to also be smart enough to handle this.

And finally, enjoy this shrinkwrap bug fix:

* [#3675](https://github.com/npm/npm/issues/3675)
  When shrinkwrapping a dependency that's both a `devDependency` and the
  child of a regular `dependency`, npm now correctly includes the child.

##### The Age of Progress (Bars)!

* [#6911](https://github.com/npm/npm/issues/6911)
  ([#1257](https://github.com/npm/npm/issues/1257)
  [#5340](https://github.com/npm/npm/issues/5340)
  [#6420](https://github.com/npm/npm/issues/6420))
  The spinner is gone (yay? boo? will you miss it?), and in its place npm
  has _progress bars_, so you actually have some sense of how long installs
  will take. It's provided in Unicode and non-Unicode variants, and Unicode
  support is automatically detected from your environment.

#### TINY JEWELS

The bottom is where we usually hide the less interesting bits of each
release, but each of these are small but incredibly useful bits of this
release, and very much worth checking out:

* [`9ebe312`](https://github.com/npm/npm/commit/9ebe312)
  Build system maintainers, rejoice: npm does a better job of cleaning up
  after itself in your temporary folder.
* [#6942](https://github.com/npm/npm/issues/6942)
  Check for permissions issues prior to actually trying to install
  anything.
* Emit warnings at the end of the installation when possible, so that
  they'll be on your screen when npm stops.
* [#3505](https://github.com/npm/npm/issues/3505)
  `npm --dry-run`: You can now ask that npm only report what it _would have
  done_ with the new `--dry-run` flag. This can be passed to any of the
  commands that change your `node_modules` folder: `install`, `uninstall`,
  `update` and `dedupe`.
* [`81b46fb`](https://github.com/npm/npm/commit/81b46fb)
  npm now knows the correct URLs for `npm bugs` and `npm repo` for
  repositories hosted on Bitbucket and GitLab, just like it does for GitHub
  (and GitHub support now extends to projects hosted as gists as well as
  traditional repositories).
* [`5be4008a`](https://github.com/npm/npm/commit/5be4008a09730cfa3891d9f145e4ec7f2accd144)
  npm has been cleaned up to pass the [`standard`](http://npm.im/standard)
  style checker. Forrest and Rebecca both feel this makes it easier to read
  and understand the code, and should also make it easier for new
  contributors to put merge-ready patches.
  ([@othiym23](https://github.com/othiym23))

#### ZARRO BOOGS

* [`6401643`](https://github.com/npm/npm/commit/6401643)
  Make sure the global install directory exists before installing to it.
  ([@thefourtheye](https://github.com/thefourtheye))
* [#6158](https://github.com/npm/npm/issues/6158)
  When we remove modules we do so inside-out running unbuild for each one.
* [`960a765`](https://github.com/npm/npm/commit/960a765)
  The short usage information for each subcommand has been brought in sync
  with the documentation. ([@smikes](https://github.com/smikes))
