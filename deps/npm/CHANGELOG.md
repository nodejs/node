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
  On windows, the `node-gyp` wrapper would fail if your path to `node-gyp`
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
  `has-unicode@2.0.0`: Change the default on windows to be false, as
  international windows installs often install to non-unicode codepages and
  there's no way to detect this short of a system call or a call to a
  command line program.
  ([@iarna](https://github.com/iarna))
* [`238fe84`](https://github.com/npm/npm/commit/238fe84ac61297f1d71701d80368afaa40463305)
  `which@1.2.1`: Fixed bugs with uid/gid checks and with quoted windows PATH
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
  Give better error messages for invalid urls in the dependecy
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
that we added in #9198, but ALSO works with windows.

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
  Allow npm link to run on windows with prerelease versions of
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
  This will hopefully eliminate most cases where windows users ended up
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
