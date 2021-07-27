### v2.15.12 (2017-03-24):

This version brings the latest `node-gyp` to a soon to be released Node.js
4.x.  The `node-gyp` update is particularly important to Windows folks due to
its addition of Visual Studio 2017 support.

* [`cdd60e733`](https://github.com/npm/npm/commit/cdd60e733905a9994e1d6d832996bfdd12abeaee)
  `node-gyp@3.6.0`:
  Improvements to how Python is located. New `--devdir` flag.
  Support for VS2017.
  Chakracore support on ARM.
  Remove path-array dependency, reducing size significantly.
  ([@bnoordhuis](https://github.com/bnoordhuis))
  ([@mhart](https://github.com/mhart))
  ([@refack](https://github.com/refack))
  ([@kunalspathak](https://github.com/kunalspathak))

### v2.15.11 (2016-09-08):

On we go with our monthly release cadence! This week is pretty much all
dependency updates and some documentation changes, as can be expected by now.

Note that `npm@4` will almost certainly be released next month! It's not final
what we'll end up doing as far as LTS support goes, but the current thinking is
that, considering how small and resource-constrained our team is, support for
`npm@2` will be reduced to essentially maintenance, so we can better focus on
`npm@3` as the new LTS version (which will go into `node@6`), and `npm@4` as our
next main development version.

#### DOCUMENTATION UPDATES

* [`8f71038`](https://github.com/npm/npm/commit/8f71038310501ad5bc7445b2fa2ff0eaa377919a)
  [#13892](https://github.com/npm/npm/pull/13892)
  Update `LICENSE` file to match license on `master`.
  ([@rvagg](https://github.com/rvagg))
* [`e81b4f1`](https://github.com/npm/npm/commit/e81b4f1d18a4d79b7af8342747f2ed7dc3e84f0a)
  [#12438](https://github.com/npm/npm/issues/12438)
  Remind folks to use `#!/usr/bin/env node` in their `bin` scripts to make files
  executable directly.
  ([@mxstbr](https://github.com/mxstbr))
* [`f89789f`](https://github.com/npm/npm/commit/f89789f43d65bfc74f64f15a99356841377e1af3)
  [#13655](https://github.com/npm/npm/pull/13655)
  Document line comment syntax for `.npmrc`.
  ([@mdjasper](https://github.com/mdjasper))
* [`5cd3abc`](https://github.com/npm/npm/commit/5cd3abc3511515e09b4a1b781c0520e84c267c5b)
  [#13493](https://github.com/npm/npm/pull/13493)
  Document that the user config file can itself be configured either through the
  `$NPM_CONFIG_USERCONFIG` environment variable, or `--userconfig` command line
  flag.
  ([@jasonkarns](https://github.com/jasonkarns))
* [`dd71ca0`](https://github.com/npm/npm/commit/dd71ca0efc2094b824ccc9e23af0fc915499f2e6)
  [#13911](https://github.com/npm/npm/pull/13911)
  Minor documentation reword and cleanup.
  ([@othiym23](https://github.com/othiym23))
* [`f7a320c`](https://github.com/npm/npm/commit/f7a320c816947d578a050c97e0fb9878954be0e8)
  [#13682](https://github.com/npm/npm/pull/13682)
  Minor grammar fix in documentation for `npm scripts`.
  ([@Ajedi32](https://github.com/Ajedi32))
* [`e5cb5e8`](https://github.com/npm/npm/commit/e5cb5e8fcf4642836fedf3f3421c994a8e27e19b)
  [#13717](https://github.com/npm/npm/pull/13717)
  Document that `npm link` will link the files specified in the `bin` field of
  `package.json` to `{prefix}/bin/{name}`.
  ([@legodude17](https://github.com/legodude17))

#### DEPENDENCY UPDATES
* [`8bef026`](https://github.com/npm/npm/commit/8bef026603b6da888edf0d41308d9e532abfcd54)
  `graceful-fs@4.1.6`
  ([@francescoinfante](https://github.com/francescoinfante))
* [`9f73f4a`](https://github.com/npm/npm/commit/9f73f4aab5f56b256c5cf9e461e81abfa2844945)
  `glob@7.0.6`
  ([@isaacs](https://github.com/isaacs))
* [`5391b7e`](https://github.com/npm/npm/commit/5391b7e8cd4401fbadbf54e810fdc965a3662a21)
  `which@1.2.1`
  ([@isaacs](https://github.com/isaacs))
* [`43bfec8`](https://github.com/npm/npm/commit/43bfec8376dd8ded7d56a8dabd6139919544760e)
  `retry@0.10.0`
  ([@tim-kos](https://github.com/tim-kos))
* [`39305f1`](https://github.com/npm/npm/commit/39305f1c76f74bf9789c769ef72a94ea9a81d119)
  `readable-stream@2.1.5`
  ([@calvinmetcalf](https://github.com/calvinmetcalf))
* [`a5512fa`](https://github.com/npm/npm/commit/a5512fafd72e23755e77e28f1122b008bc12a733)
  `once@1.4.0`
  ([@zkochan](https://github.com/zkochan))
* [`06a208b`](https://github.com/npm/npm/commit/06a208b178c1de3d0da58bc35a854d200fea8ef0)
  `npm-registry-client@7.2.1`:
  * [npm/npm-registry-client#142](https://github.com/npm/npm-registry-client/pull/142) Fix `EventEmitter` warning spam from error handlers on socket. ([@addaleax](https://github.com/addaleax))
  * [npm/npm-registry-client#131](https://github.com/npm/npm-registry-client/pull/131) Adds support for streaming request bodies. ([@aredridel](https://github.com/aredridel))
  * Fixes [#13656](https://github.com/npm/npm/issues/13656).
  * Dependency updates.
  * Documentation improvements.
  ([@othiym23](https://github.com/othiym23))
* [`4f759be`](https://github.com/npm/npm/commit/4f759be1fb5e23180b970350e58f40a513daa680)
  `inherits@2.0.3`
  ([@isaacs](https://github.com/isaacs))
* [`4258b76`](https://github.com/npm/npm/commit/4258b764e2565f6294ae1e34a5653895290b62e3)
  `tap@7.1.1`
  ([@isaacs](https://github.com/isaacs))

### v2.15.10 (2016-08-11):

Hi all, today's our first release coming out of the new monthly release
cadence. See below for details. We're all recovered from conferences now and
raring to go! For LTS we see some bug fixes, documentation improvements and
a host of dependency updates.

The most dramatic bug fix is probably the inclusion of scoped modules in
bundled dependencies. Prior to this release and
[v3.10.7](https://github.com/npm/npm/releases/v3.10.7), npm had ignored
scoped modules found in `bundleDependencies` entirely.

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

#### WINDOWS CORNER CASES

* [`405c404`](https://github.com/npm/npm/commit/405c4048c69c14d66e6179aba0c8a35e504e8041)
  [#13023](https://github.com/npm/npm/pull/13023)
  Fixed a Windows issue with the cache where callbacks could be called more than once.
  ([@zkat](https://github.com/zkat))

* [`bf348dc`](https://github.com/npm/npm/commit/bf348dcfb944dc4b9f71b779bf172f86a2e1f474)
  [#13023](https://github.com/npm/npm/pull/13023)
  Fixed a Windows corner case with correct-mkdir where if SUDO_UID or
  SUDO_GID were set then we would try to chown things even though that can't
  work on Windows.
  ([@zkat](https://github.com/zkat))

#### RACES IN THE CACHE

* [`68f29f1`](https://github.com/npm/npm/commit/68f29f18f65c7a7e1c58eb6933af41d786971379)
  [#12669](https://github.com/npm/npm/issues/12669)
  Ignore ENOENT errors on chownr while adding packages to cache. This change
  works around problems with race conditions and local packages.
  ([@julianduque](https://github.com/julianduque))

#### BETTER GIT ENVIRONMENT WHITELISTING

* [`5e96566`](https://github.com/npm/npm/commit/5e96566088f0d88c1ed10c5a9cbb7c0cd4aa2aee)
  [#13358](https://github.com/npm/npm/pull/13358)
  Add GIT_EXEC_PATH to Git environment whitelist.
  ([@mhart](https://github.com/mhart))

#### DOCUMENTATION

* [`363e381`](https://github.com/npm/npm/commit/363e381a4076ead89707a00cc4a447b1d59df3bc)
  [#13319](https://github.com/npm/npm/pull/13319)
  As Node.js 0.8 is no longer supported, remove mention of it from the README.
  ([@watilde](https://github.com/watilde))
* [`e8fafa8`](https://github.com/npm/npm/commit/e8fafa887c60eb8842c76c4b3dffe85eb49fa434)
  [#10167](https://github.com/npm/npm/pull/10167)
  Clarify in scope documentation that npm@2 is required for scoped packages.
  ([@danpaz](https://github.com/danpaz))

#### DEPENDENCIES

* [`66ef279`](https://github.com/npm/npm/commit/66ef279b7c3b3e4f9454474dddd057cc1f21873b)
  [npm/fstream-npm#22](https://github.com/npm/fstream-npm/pull/22)
  `fstream@1.1.1`:
  Always include NOTICE files now. Fix inclusion of scoped modules as bundled dependencies.
  ([@kemitchell](https://github.com/kemitchell))
  ([@forivall](https://github.com/forivall))
* [`fe8385b`](https://github.com/npm/npm/commit/fe8385bd655502feb175eed175a6a06cafb2247a)
  `glob@7.0.5`:
  Update minimatch dep for security fix. See the minimatch update below for details.
  ([@isaacs](https://github.com/isaacs))
* [`51d49d2`](https://github.com/npm/npm/commit/51d49d2f79b4c69264de73a492ed54f87188d554)
  [isaacs/node-graceful-fs#71](https://github.com/isaacs/node-graceful-fs/pull/71)
  `graceful-fs@4.1.5`:
  `graceful-fs` had a [bug fix](https://github.com/isaacs/node-graceful-fs/pull/71) which
  fixes a problem ([nodejs/node#7846](https://github.com/nodejs/node/pull/7846)) exposed
  by recent changes to Node.js.
  ([@thefourtheye](https://github.com/thefourtheye))
* [`5c8f39d`](https://github.com/npm/npm/commit/5c8f39d152c43e96b9006ffe865646a36a433a8a)
  `minimatch@3.0.3`:
  Handle extremely long and terrible patterns more gracefully.
  There were some magic numbers that assumed that every extglob pattern starts
  and ends with a specific number of characters in the regular expression.
  Since !(||) patterns are a little bit more complicated, this led to creating
  an invalid regular expression and throwing.
  ([@isaacs](https://github.com/isaacs))
* [`d681e16`](https://github.com/npm/npm/commit/d681e16a475a49d6196af9a5cedaaf88712f3a9f)
  [npm/npm-user-validate#9](https://github.com/npm/npm-user-validate/pull/9)
  `npm-user-validate@0.1.5`:
  Use correct, lower username length limit.
  ([@aredridel](https://github.com/aredridel))
* [`f918994`](https://github.com/npm/npm/commit/f918994bd05ca965766cd573606ac35fb3032d6e)
  `request@2.74.0`:
  Update `request` dependency `tough-cookie` to `2.3.0` to
  to address [https://nodesecurity.io/advisories/130](https://nodesecurity.io/advisories/130).
  Versions 0.9.7 through 2.2.2 contain a vulnerable regular expression that,
  under certain conditions involving long strings of semicolons in the
  "Set-Cookie" header, causes the event loop to block for excessive amounts of
  time.
  ([@stash-sfdc](https://github.com/stash-sfdc))
* [`5540cc4`](https://github.com/npm/npm/commit/5540cc4d6bde65071fb6fc2cb074e8598bd1276f)
  [isaacs/rimraf#111](https://github.com/isaacs/rimraf/issues/111)
  `rimraf@2.5.4`: Clarify assertions: cb is required, options are not.
  ([@isaacs](https://github.com/isaacs))
* [`6357928`](https://github.com/npm/npm/commit/6357928673be85f520dae2104fea58c35742bd65)
  `spdx-license-ids@1.2.2`:
  New licenses synced from spdx.org.
  ([@shinnn](https://github.com/shinnn))

### v2.15.9 (2016-06-30):

What's this? An LTS release? Yes, that is indeed so. Small, as usual, and as
LTSs should be, really, but a release nonetheless!

The star of the show is an updated `node-gyp` with some goodies. The rest is
just docs and some CI stuff.

Happy hacking!

#### DEPENDENCY UPDATE!

* [`f9a07cc`](https://github.com/npm/npm/commit/f9a07cc873f1915827d8df97d0c43204d1eb128c)
  [#13200](https://github.com/npm/npm/pull/13200)
  [`node-gyp@3.4.0`](https://github.com/nodejs/node-gyp/blob/master/CHANGELOG.md):
  AIX, Visual Studio 2015, and logging improvements. Oh my~!
  ([@rvagg](https://github.com/rvagg))

#### CI TWEAKS

* [`bee83b8`](https://github.com/npm/npm/commit/bee83b8500c31aba65451dfcb082f9b5d1d5ce34)
  Globally install `rimraf` on CI to make the LTS self-install work better.
  ([@othiym23](https://github.com/othiym23))
* [`6b8c0ab`](https://github.com/npm/npm/commit/6b8c0ab6fcbf8a37e8693acb8bbac22293b10893)
  This new Travis configuration only runs coverage checks against Node.js LTS,
  which speeds up all the other test runs. By, like, a lot. Also, the entire
  file has been extensively commented, so the next time we need to mess with it,
  we'll be able to better remember why all the weird bits are there.
  ([@othiym23](https://github.com/othiym23))

#### DOCUMENTATION FIXES

* [`2c7a5be`](https://github.com/npm/npm/commit/2c7a5be080276e3fdca3375ab0f8f5edffff753e)
  [#13156](https://github.com/npm/npm/pull/13156)
  Fix old reference to `doc/install` in a source comment.
  ([@sheerun](https://github.com/sheerun))
* [`e1cf78c`](https://github.com/npm/npm/commit/e1cf78c5b77f95383bd4a7fc6eeb8adbbe68e12e)
  [#13189](https://github.com/npm/npm/pull/13189)
  [#13113](https://github.com/npm/npm/issues/13113)
  [#13189](https://github.com/npm/npm/pull/13189)
  Fixes a link to `npm-tag(3)` that was breaking to instead point to
  `npm-dist-tag(1)`, as reported by [@SimenB](https://github.com/SimenB)
  ([@macdonst](https://github.com/macdonst))

### v2.15.8 (2016-06-17):

There's a very important bug fix and a long-awaited (and significant!)
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

* [`2c49265`](https://github.com/npm/npm/commit/2c49265c6746d29ae0cd5f3532d28c5950f9847e)
  [#5082](https://github.com/npm/npm/issues/5082) `fstream@1.0.10`: Ensure that
  entries are collected after a paused stream resumes.
  ([@rvagg](https://github.com/rvagg))
* [`92e4344`](https://github.com/npm/npm/commit/92e43444d9204f749f83512aeab5d5e0a2d085a7)
  [#5082](https://github.com/npm/npm/issues/5082) Remove the warning introduced
  in `npm@3.10.0`, because it should no longer be necessary.
  ([@othiym23](https://github.com/othiym23))

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

* [`4a1ecc0`](https://github.com/npm/npm/commit/4a1ecc068fb2660bd9bc3e2e2372aa0176d2193b)
  Remove 0.8 from the Node.js testing matrix, and reorder to match real-world
  priority, with comments. ([@othiym23](https://github.com/othiym23))

### v2.15.7 (2016-06-16):

It pains me greatly that we haven't been able to fix
[#5082](https://github.com/npm/npm/issues/5082) yet, but warning you away from
potentially publishing incomplete packages takes priority over feeling cheesy
about landing a warning to help keep y'all out of trouble, so here you go
(_please read this next bit_ (_please clap_)):

#### DANGER: PUBLISHING ON NODE 6.0.0

Publishing and packing are buggy under Node versions greater than 6.0.0.
Please use Node.js LTS (4.4.x) to publish packages.  See
[#5082](https://github.com/npm/npm/issues/5082) for details and current
status.

* [`dff00ce`](https://github.com/npm/npm/commit/dff00cedd56b9c04370f840299a7e657a7a835c6)
  [#13077](https://github.com/npm/npm/pull/13077)
  Warn when using Node 6+.
  ([@othiym23](https://github.com/othiym23))

#### PACKAGING CHANGES

* [`1877171`](https://github.com/npm/npm/commit/1877171648e20595a82de34073b643f7e01a339f)
  [#12873](https://github.com/npm/npm/issues/12873)
  Ignore `.nyc_output`. This will help avoid an accidental publish or commit filled with
  code coverage data.
  ([@TheAlphaNerd](https://github.com/TheAlphaNerd))

#### DOCUMENTATION CHANGES

* [`470ae86`](https://github.com/npm/npm/commit/470ae86e052ae2f29ebec15b7547230b6240042e)
  [#12983](https://github.com/npm/npm/pull/12983)
  Describe how to run the lifecycle scripts of dependencies. How you do
  this changed with `npm` v2.
  ([@Tapppi](https://github.com/Tapppi))
* [`9cedf37`](https://github.com/npm/npm/commit/9cedf37e5a3e26d0ffd6351af8cac974e3e011c2)
  [#12776](https://github.com/npm/npm/pull/12776)
  Remove mention of `<pkg>` arg for `run-script`.
  ([@fibo](https://github.com/fibo))
* [`55b8424`](https://github.com/npm/npm/commit/55b8424d7229f2021cac55f0b03de72403e7c0ff)
  [#12840](https://github.com/npm/npm/pull/12840)
  Remove sexualized language from comment.
  ([@geek](https://github.com/geek))
* [`d6bf0c3`](https://github.com/npm/npm/commit/d6bf0c393788a6398bf80b41c57956f2dbcf3b39)
  [#12802](https://github.com/npm/npm/pull/12802)
  Small grammar fix in `doc/cli/npm.md`.
  ([@andresilveira](https://github.com/andresilveira))

#### DEPENDENCY UPDATES

* [`2c2c568`](https://github.com/npm/npm/commit/2c2c56857ff801d5fe1b6d3157870cd16e65891b)
  `readable-stream@2.1.4`: Brought up to date with Node 6.1.0's streams implementation.
  ([@calvinmetcalf](https://github.com/calvinmetcalf))
* [`d682e64`](https://github.com/npm/npm/commit/d682e6445845b0a2584935d5e2942409c43f6916)
  [npm/npm-user-validate#8](https://github.com/npm/npm-user-validate/pull/8)
  `npm-user-validate@0.1.4`: Add a maximum length limit for usernames based on
  the (arbitrary) limit imposed by the primary npm registry.
  ([@aredridel](https://github.com/aredridel))
* [`448b65b`](https://github.com/npm/npm/commit/448b65b48cda3b782b714057fb4b8311cc1fa36a)
  `which@1.2.10`: Remove unused dependency `is-absolute`, bug fixes.
  ([@isaacs](https://github.com/isaacs))
* [`7d15434`](https://github.com/npm/npm/commit/7d15434f0b0af8e70b119835b21968217224664f)
  `require-inject@1.4.0`: Add `requireInject.withEmptyCache` and
  `requireInject.installGlobally.andClearCache` to support loading modules to be
  injected with an empty cache.
  ([@iarna](https://github.com/iarna))
* [`31845c0`](https://github.com/npm/npm/commit/31845c081bc6f3f8a2f3d83a3c792dccffbaa2a8)
  `init-package-json@1.9.4`:
  Replace use of reserved identifier `package` in, uh, the package.
  ([@adius](https://github.com/adius))
* [`d73ef3e`](https://github.com/npm/npm/commit/d73ef3e6b18d4905de668c5115bc6042905a02d9)
  `glob@7.0.4`: Use userland `fs.realpath` implementation to get glob working under Node 6.
  ([@isaacs](https://github.com/isaacs))
* [`b47da85`](https://github.com/npm/npm/commit/b47da85cf83b946f2c8d29ab612c92028f31f6b0)
  `inflight@1.0.5`: Correct link to package repository, add `"files"` stanza.
  ([@iarna](https://github.com/iarna), [@jamestalmage](https://github.com/jamestalmage))
* [`04815e4`](https://github.com/npm/npm/commit/04815e436035de785279fd000cdbc821cc1f3447)
  [npm/npmlog#32](https://github.com/npm/npmlog/pull/32)
  `npmlog@2.0.4`: Add `"files"` stanza to `package.json`.
  ([@jamestalmage](https://github.com/jamestalmage))
* [`9e29ad2`](https://github.com/npm/npm/commit/9e29ad227300bb970e7bcd21029944d4733e40db)
  `wrappy@1.0.2`: Add `"files"` stanza to `package.json`.
  ([@jamestalmage](https://github.com/jamestalmage))
* [`44af4d4`](https://github.com/npm/npm/commit/44af4d475ac65bdce6d088173273ce4a4f74a49e)
  `abbrev@1.0.9` ([@jorrit](https://github.com/jorrit))
* [`6c977c0`](https://github.com/npm/npm/commit/6c977c0031d074479a26c7bec6ec83fd6c6526b2)
  `npm-registry-client@7.1.2`: Add support for newer versions of `npmlog`.
  ([@iarna](https://github.com/iarna))

### v2.15.6 (2016-05-12):

I have a couple of doc fixes and a shrinkwrap fix for you all this week.

#### PEER DEPENDENCIES AND SHRINKWRAPS

* [`55c998a`](https://github.com/npm/npm/commit/55c998a098a306b90a84beef163a8890f9a616b1)
  [#5135](https://github.com/npm/npm/issues/5135)
  Fix a bug where peerDependencies & shrinkwraps didn't play nice together. (Where
  the peerDependency resolver would end up installing its dep when it wasn't needed.)
  ([@majgis](https://github.com/majgis))

#### NPM AND `node-gyp` DOCS IMPROVEMENTS

* [`1826908`](https://github.com/npm/npm/commit/1826908b991510d8fbc71a0d0f2c01ff24fd83c2)
  [#12636](https://github.com/npm/npm/pull/12636)
  Improve `npm-scripts` documentation regarding when `node-gyp` is used.
  ([@reconbot](https://github.com/reconbot))
* [`f9ff7f3`](https://github.com/npm/npm/commit/f9ff7f36cc2c2c3fbb4f6eef91491b589d049d5f)
  [#12586](https://github.com/npm/npm/pull/12586)
  Correct `package.json` documentation as to when `node-gyp rebuild` called.
  This now matches https://docs.npmjs.com/misc/scripts#default-values
  ([@reconbot](https://github.com/reconbot))

### v2.15.5 (2016-05-05):

This is a minor LTS release, bringing dependencies up to date and updating
our CI matrix to match what we support.

Some of the dependency updates come out of our getting the development
branch's tests passing on Windows and so bring in fixes for a few Windows
related corner cases.

#### CI UPDATES

* [`bb6f0e5`](https://github.com/npm/npm/commit/bb6f0e5c95d4ad186768b1c962dd4c399f90ddb1)
  [#12487](https://github.com/npm/npm/pull/12487)
  Remove iojs from CI, add Node.js 6, prioritize 4 over 5.
  ([@othiym23](https://github.com/othiym23))

#### DEPENDENCY UPDATES

* [`f2f8753`](https://github.com/npm/npm/commit/f2f8753c4aef2a604a4bdca2677711c940234b8f)
  `which@1.2.8`:
  Properly handle relative path executables.
  ([@isaacs](https://github.com/isaacs))
* [`e287ca9`](https://github.com/npm/npm/commit/e287ca99c37680d8e4cfacf4cfebe2da98884865)
  `read-package-json@2.0.4`:
  Fix Windows issue with ENOTDIR detection.
  ([@zkat](https://github.com/zkat))
* [`1a0ce6c`](https://github.com/npm/npm/commit/1a0ce6cff4c347bad035dc89bba2ceed9dacbf73)
  `realize-package-specifier@3.0.3`:
  Use npa with windows fix.
  Fix relative path resolution when the local file might also be a tag.
  ([@zkat](https://github.com/zkat))
  ([@iarna](https://github.com/iarna))
* [`a475c9a`](https://github.com/npm/npm/commit/a475c9a4e4b36d00080b11f379657ce68185adc6)
  `lru-cache@4.0.1`:
  Use Symbol if available.
  ([@isaacs](https://github.com/isaacs))
* [`7141e08`](https://github.com/npm/npm/commit/7141e08816c620b1889d7537c30dc5b254de4d1f)
  `sorted-object@2.0.0`
  ([@iamstarkov](https://github.com/iamstarkov))
* [`27c6190`](https://github.com/npm/npm/commit/27c6190216cc8a5a280f0efbabb3444581968d40)
  `request@2.72.0`
  ([@simov](https://github.com/simov))
* [`ab90daf`](https://github.com/npm/npm/commit/ab90daf70ba51b51f722fb4cd74ac5267621c4b4)
  `readable-stream@2.1.2`
  ([@calvinmetcalf](https://github.com/calvinmetcalf))
* [`b1715f8`](https://github.com/npm/npm/commit/b1715f805426403273225bcfa91d1a52d7b56eb8)
  `graceful-fs@4.1.4`
  ([@isaacs](https://github.com/isaacs))
* [`ca97de6`](https://github.com/npm/npm/commit/ca97de6c18059ef420235f4706898ad8758904e6)
  `block-stream@0.0.9`
  ([@isaacs](https://github.com/isaacs))

### v2.15.4 (2016-04-21):

Gosh, it's been a peaceful couple of weeks!

Overall, the CLI team has been focused on the project to [get the test suite
passing on Windows](https://github.com/npm/npm/pull/11444). Our efforts should
be paying off soon -- there's only a couple of tests left!

It's very unlikely those particular changes will make their way into our current
`npm@2` LTS release, I think, but it will help `npm@3` a lot, as well as
whatever version makes it into [`node@6`, which will eventually be the next
Node.js LTS](https://github.com/nodejs/node/pull/6155).

As far as this week goes, we've got a couple of dep updates and doc fixes.
Always happy to see community contributions flying in. 💚

#### DEP UPDATE MAGIC

* [`b178c4a`](https://github.com/npm/npm/commit/b178c4ac9ce91c0a0794526a38b553c759132d18)
  `spdx-license-ids@1.2.1`:
  Minor project-related tweaks -- no license changes.
  ([@shinnn](https://github.com/shinnn))
* [`1adf179`](https://github.com/npm/npm/commit/1adf179948ab8cb97dfb2f46a61e9f37d944c42a)
  `normalize-git-url@3.0.2`:
  Fixes `file://` URLs on Windows. Turns out stuff like `file://C:\hello` is
  actually fairly weird for a URL (it's not actually a valid URL, but we're just
  gonna pretend.😉)
  ([@zkat](https://github.com/zkat))
* [`9cfd56c`](https://github.com/npm/npm/commit/9cfd56cdadc040c0b2fa7654cdb5e7d22dbef7cb)
  `fs-vacuum@1.2.9`:
  This one goes out to our fans at Big Blue: There was an AIX-specific issue
  where `fs.rmDir` was failing with `EEXIST` instead of `ENOTEMPTY` with
  non-empty directories.
  ([@richardlau](https://github.com/richardlau))

#### HOORAY DOC CONTRIBUTIONS

No seriously, we love these. Keep 'em comin'!

* [`2afe8bf`](https://github.com/npm/npm/commit/2afe8bf415a159baa181a8102f72c96e1d189bc9)
  [#12415](https://github.com/npm/npm/pull/12415)
  Clarify that the `--cert` and `--key` options are actual certs and keys, not
  paths to files containing them.
  ([@rvedotrc](https://github.com/rvedotrc))
* [`3522560`](https://github.com/npm/npm/commit/3522560b0a4bb6c9717a34f9728f156fd9760cad)
  [#12107](https://github.com/npm/npm/pull/12107)
  Document `npm login` as an alias to `npm adduser`. People are still surprised
  by this so often.
  ([@gnerkus](https://github.com/gnerkus))

### v2.15.3 (2016-03-31):

Hiiiiiii!~👋

We're really happy to be getting more and more community contributions! Keep it
up! We really appreciate folks trying to help us, and we'll do our best to help
point you in the right direction. Even things like documentation are a huge
help. And remember -- you get socks for it, too!🎁

This week is as quiet as usual, aside from fixing a regression to `npm
deprecate` you might want to pay attention to! Other than that, just docs and
deps, as any good LTS release train should be. 🙆

#### FIXME

* [`6e0b66e`](https://github.com/npm/npm/commit/6e0b66e282aa27d1b5371e2babaa859924121730)
  [#11884](https://github.com/npm/npm/pull/11884)
  Include `node_modules` in the list of files and directories that npm won't
  include in packages ordinarily. (Modules listed in `bundledDependencies` and
  things that those modules rely on, ARE included of course.)
  ([@Jameskmonger](https://github.com/Jameskmonger))
* [`9896290`](https://github.com/npm/npm/commit/98962909b160364030705575202ad133971033c1)
  [#12079](https://github.com/npm/npm/pull/12079)
  Back in `npm@2.13.1` we included [a patch that made it so `npm install pkg`
  was basically `npm install pkg@latest` instead of
  `pkg@*`](https://github.com/npm/npm/pull/9170) This is probably what most
  users expected, but it also ended up [breaking `npm
  deprecate`](https://github.com/npm/npm/pull/9170) when no version was provided
  for a package. In that case, we were using `*` to mean "deprecate all
  versions" and relying on the `pkg` -> `pkg@*` conversion. This patch fixes
  `npm deprecate pkg` to work as it used to by special casing that particular
  command's behavior.
  ([@polm](https://github.com/polm))
* [`6c1628f`](https://github.com/npm/npm/commit/6c1628f62b657db6c116be13849d00933a3388cd)
  [#12146](https://github.com/npm/npm/pull/12146)
  Adds `make doc-clean` to `prepublish` script, to clear out previously built
  docs before publishing a new npm version.
  ([@watilde](https://github.com/watilde))
* [`6d3017e`](https://github.com/npm/npm/commit/6d3017e6eed8a771b395d10130ac1f498e2d3211)
  [#12146](https://github.com/npm/npm/pull/12146)
  Adds `doc-clean` phony target to `make publish`.
  ([@watilde](https://github.com/watilde))

#### DOCS

* [`d43921c`](https://github.com/npm/npm/commit/d43921c546617cdb94bbee444d7d67ef55f38dc5)
  [#12147](https://github.com/npm/npm/pull/12147)
  Document that the current behavior of `engines` is just to warn if the node
  platform is incompatible.
  ([@reconbot](https://github.com/reconbot))
* [`3cfe99e`](https://github.com/npm/npm/commit/3cfe99e3a757c5d8cbb1c2789410e9802563abac)
  [#12093](https://github.com/npm/npm/pull/12093)
  Update `bugs` url in `package.json` to use the `https` URL for Github.
  ([@watilde](https://github.com/watilde))
* [`ecf865f`](https://github.com/npm/npm/commit/ecf865f4eed1419c75442e0d52bc34ba1647de15)
  [#12075](https://github.com/npm/npm/pull/12075)
  Add the `--ignore-scripts` flag to the `npm install` docs.
  ([@paulirish](https://github.com/paulirish))
* [`f0e6db3`](https://github.com/npm/npm/commit/f0e6db32827d88680ef2320e60c0863754a4fbc5)
  [#12063](https://github.com/npm/npm/pull/12063)
  Various minor fixes to the html docs homepage.
  ([@watilde](https://github.com/watilde))

#### DEPS

* [`e2660de`](https://github.com/npm/npm/commit/e2660de1c08ed68a1c6fc4ee75d10376595979be)
  `npmlog@2.0.3`
  ([@iarna](https://github.com/iarna))

### v2.15.2 (2016-03-24):

It's always nice to see new contributors. 💚

This week sees another small release, but we're still chugging along on our
[Windows efforts](https://github.com/npm/npm/pull/11444).

There's also some small process changes to our LTS process relatively recently
that you might wanna know about! 💁

For one, the `2.x` branch was removed in favor of just `lts`. If you're making
PRs exclusively against npm's LTS, please use that name from now on. `2.x` was
deleted.

Also, [@othiym23](https://github.com/othiym23) put some time into [writing down
our LTS process and policy](https://github.com/npm/npm/wiki/LTS). Check it out
and ping us if you have questions or comments about it!

In general, we're trying to make sure all our policy and such for our
contributors is written down, and we hope it makes it easier in general for
y'all. Forrest is also working on a shiny new Contributor's Guide right now, but
we'll link to that in the (near?) future, when it's ready to roll out.

#### TESTS

* [`1d0e468`](https://github.com/npm/npm/commit/1d0e468c06c7b8e2b95b7fe874a3399a16d9db74)
  [#11931](https://github.com/npm/npm/pull/11931)
  Removes a bunch of old, disabled tests that have just been sitting around,
  doing nothing.
  ([@othiym23](https://github.com/othiym23))
* [`7ae8aa1`](https://github.com/npm/npm/commit/7ae8aa1d9dc47761024f6756114205db3fb2c80b)
  [#11987](https://github.com/npm/npm/pull/11987)
  There was a failure in the `outdated-symlink` test caused by using the default
  registry instead of the mock registry tests.
  ([@yodeyer](https://github.com/yodeyer))

#### DOCS

* [`b2649fb`](https://github.com/npm/npm/commit/b2649fb360f239aadef1ab51a580cbf4fdf29722)
  [#12006](https://github.com/npm/npm/pull/12006)
  Access was Team and Team was Access, but someone from the community rolled
  around and corrected it for us. Thanks a bunch!
  ([@yaelz](https://github.com/yaelz))

### v2.15.1 (2016-03-17):

#### SECURITY ADVISORY: BEARER TOKEN DISCLOSURE

This release includes [the fix for a
vulnerability](https://github.com/npm/npm/commit/fea8cc92cee02c720b58f95f14d315507ccad401)
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

### BACK TO YOUR REGULARLY SCHEDULED PROGRAMMING

Aside from that, it's another one of those releases again! Docs and tests, it
turns out, have a pretty easy time getting into LTS releases, and boring is
exactly how LTS should be. 💁

#### DOCS

* [`981c89c`](https://github.com/npm/npm/commit/981c89c8e398ca22ab6bf466123b25728ef6f543)
  [#11820](https://github.com/npm/npm/pull/11820)
  The basic explanation for how `npm link` works was a bit confusing, and
  somewhat incorrect. It should be clearer now.
  ([@rhgb](https://github.com/rhgb))
* [`35b2b45`](https://github.com/npm/npm/commit/35b2b45f181dcbfb297f53b577dc1f26efcf3aba)
  [#11787](https://github.com/npm/npm/pull/11787)
  The `verison` alias for `npm version` no longer shows up in the command list
  when you do `npm -h`.
  ([@doug-wade](https://github.com/doug-wade))
* [`1c9d00f`](https://github.com/npm/npm/commit/1c9d00f788298a81a8a7293d7dcf430f01bdd7fd)
  [#11786](https://github.com/npm/npm/pull/11786)
  Add a comment to the `npm-scope.md` docs about `npm@>=2` being required in
  order to use scoped packaged.
  ([@doug-wade](https://github.com/doug-wade))
* [`7d64fb1`](https://github.com/npm/npm/commit/7d64fb1452d360aa736f31c85d6776ce570b2365)
  [#11762](https://github.com/npm/npm/pull/11762)
  Roll back patch that previously advised people to use `--depth Infinity`
  instead of `--depth 9999`. Just keep using `--depth 9999`.
  ([@GriffinSchneider](https://github.com/GriffinSchneider))

#### TESTS

* [`98a9ee4`](https://github.com/npm/npm/commit/98a9ee4773f83994b8eb63c0ff75a9283408ba1a)
  [#11912](https://github.com/npm/npm/pull/11912)
  Did you know npm can install itself? `npm install -g npm` is the way to
  upgrade! Turns out that one of the tests that verified this functionality got
  rewritten as part of our recent push for better tests, and in the process
  omitted a detail about *how* the test ran. We're testing that corner case
  again, now, by moving the install folder to `/tmp`, where the original legacy
  test ran.
  ([@iarna](https://github.com/iarna))

### v2.15.0 (2016-03-10):

#### WHY IS THIS SEMVER-MINOR I THOUGHT THIS WAS LTS

A brief note about LTS this week!

npm, as you may know if you're using this `2.x` branch, has an LTS process for
releases. We also try and play nice with [Node.js' own LTS release
process](https://github.com/nodejs/LTS#lts-plan). That means we generally try to
avoid things like minor version bumps on our `2.x` branch (which is also tagged
`lts` in the `dist-tag`s).

That said, we had a minor-bump update recently for `npm@3.8.0` which added a
`maxsockets` option to allow users to configure the number of concurrent sockets
that npm would keep open at a time -- a setting that has the potential to help a
bunch for people with fussy routers or internet connections that aren't very
happy with Node.js applications' usual concurrency storm. This change was done
to `npm-registry-client`, which we don't have a parallel LTS-tracking branch
for.

After talking it over, we ended up deciding that this was a reasonable enough
addition to LTS, even though it's *technically* a `semver-minor` bump, taking
into account both its potential for bugfixing (specially on `2.x`!) and the
general hassle it would be to maintain another branch for `npm-registry-client`.


* [`6dd61e7`](https://github.com/npm/npm/commit/6dd61e781c145480dc255a3e6a748729868443fd)
  Expose `maxsockets` config setting from new `npm-registry-client`.
  ([@misterbyrne](https://github.com/misterbyrne))
* [`8a021c3`](https://github.com/npm/npm/commit/8a021c35184e665bd1f3f70ae2f478af812ab614)
  `npm-registry-client@7.1.0`:
  Adds support for configuring the max number of concurrent sockets, defaulting
  to `50`.
  ([@iarna](https://github.com/iarna))

#### DOC PATCH IS HERE TOO

* [`0ae9f74`](https://github.com/npm/npm/commit/0ae9f740001a1bdf5920bc464cf9e284d5d139f0)
  [#11748](https://github.com/npm/npm/pull/11748)
  Add command aliases as a separate section in documentation for npm
  subcommands.
  ([@watilde](https://github.com/watilde))

#### DEP UPDATES

* [`bfc3888`](https://github.com/npm/npm/commit/bfc38887f832f701c16b7ee410c4e0220a90399f)
  `strip-ansi@3.0.1`
  ([@jbnicolai](https://github.com/jbnicolai))
* [`d5f4d51`](https://github.com/npm/npm/commit/d5f4d51a1b7ea78d7431c7ed4fed30200b2622f8)
  `node-gyp@3.3.1`: Fixes Android generator
  ([@bnoordhuis](https://github.com/bnoordhuis))
* [`4119df8`](https://github.com/npm/npm/commit/4119df8aecd2ae57b0492ad8c9a480d900833008)
  `glob@7.0.3`: Some path-related fixes for Windows.
  ([@isaacs](https://github.com/isaacs))

### v2.14.22 (2016-03-03):

This week is all documentation improvements. In case you hadn't noticed, we
*love* doc patches. We love them so much, we give socks away if you submit
documentation PRs!

These folks are all getting socks if they ask for them. The socks are
super-sweet. Do you have yours yet? 👣

* [`3f3c7d0`](https://github.com/npm/npm/commit/3f3c7d080f052a5db91ff6091f8b1b13f26b53d6)
  [#11441](https://github.com/npm/npm/pull/11441)
  Add a link to the [Contribution
  Guidelines](https://github.com/npm/npm/wiki/Contributing-Guidelines) to the
  main npm docs.
  ([@watilde](https://github.com/watilde))
* [`9f87bb1`](https://github.com/npm/npm/commit/9f87bb1934acb33b678c17b7827165b17c071a82)
  [#11441](https://github.com/npm/npm/pull/11441)
  Remove Google Group email from npm docs about contributing.
  ([@watilde](https://github.com/watilde))
* [`93eaab3`](https://github.com/npm/npm/commit/93eaab3ee5ad16c7d90d1a4b38a95403fcf3f0f6)
  [#11474](https://github.com/npm/npm/pull/11474)
  Fix an invalid JSON error overlooked in
  [#11196](https://github.com/npm/npm/pull/11196).
  ([@robludwig](https://github.com/robludwig))
* [`a407ca2`](https://github.com/npm/npm/commit/a407ca2bcf6a05117e55cf2ab69376e09094995e)
  [#11483](https://github.com/npm/npm/pull/11483)
  Add more details and an example to the documentation for bundledDependencies.
  ([@gnerkus](https://github.com/gnerkus))
* [`2c851a2`](https://github.com/npm/npm/commit/2c851a231afd874baa77c42ea5ba539c454ac79c)
  [#11490](https://github.com/npm/npm/pull/11490)
  Document the `--registry` flag for `npm search`.
  ([@plumlee](https://github.com/plumlee))

### v2.14.21 (2016-02-25):

Good news, everyone! There's a new LTS release with a few shinies here and there!

#### USE THIS ONE INSTEAD

We had some cases where the versions of npm and node used in some scripting situations were different than the ideal, or what folks actually expected. These should be particularly helpful to our Windows friends! <3

* [`02813c5`](https://github.com/npm/npm/commit/02813c55782a9def23f7f1e614edc38c6c88aed3) [#9253](https://github.com/npm/npm/issues/9253) Fix a bug where, when running lifecycle scripts, if the Node.js binary you ran `npm` with wasn't in your `PATH`, `npm` wouldn't use it to run your scripts. ([@segrey](https://github.com/segrey) and [@narqo](https://github.com/narqo))
* [`a985dd5`](https://github.com/npm/npm/commit/a985dd50e06ee51ba5544577f977c7440c227ba2) [#11526](https://github.com/npm/npm/pull/11526) Prefer locally installed npm in Git Bash -- previous behavior was to use the global one. This was done previously for other shells, but not for Git Bash. ([@destroyerofbuilds](https://github.com/destroyerofbuilds))

#### SOCKS FOR THE SOCK GOD

* [`f961092`](https://github.com/npm/npm/commit/f9610920079d8b88ae464b30007a92c594bd85a8)
  [#11636.](https://github.com/npm/npm/issues/11636.)
  Document the `--save-bundle` option for `npm install`.
  ([@datyayu](https://github.com/datyayu))
* [`7c908b6`](https://github.com/npm/npm/commit/7c908b618f7123f0a3b860c71eb779e33df35964)
  [#11644](https://github.com/npm/npm/pull/11644)
  Add documentation for the `test` directory for packages.
  ([@lewiscowper](https://github.com/lewiscowper))

#### INTERNAL TEST IMPROVEMENTS

The npm CLI team's time recently has been sunk into npm's many years of tech debt. Specifically, we've been working on improving the test suite. This isn't user visible, but in future should mean a more stable, easier to contribute to npm. Ordinarily we don't report these kinds of changes in the change log, but I thought I might share this week as this chunk is bigger than usual.

These patches were previously released for `npm@3`, and then ported back to `npm@2` LTS.

* [`437c537`](https://github.com/npm/npm/commit/437c537e2be5923c6d2c2753154564ba13db8fd9) [#11613](https://github.com/npm/npm/pull/11613) Fix up one of the tests after rebasing the legacy test rewrite to `npm@2`. ([@zkat](https://github.com/zkat))
* [`55abd0c`](https://github.com/npm/npm/commit/55abd0cc20e87a144d33ce2d459f65e7506da576) [#11613](https://github.com/npm/npm/pull/11613) Test that the `package.json` `files` section and `.npmignore` do what they're supposed to. ([@zkat](https://github.com/zkat))
* [`a2b99b6`](https://github.com/npm/npm/commit/a2b99b6273ada14b2121ebc0acb7933e630edd9d) [#11613](https://github.com/npm/npm/pull/11613) Test that npm's distribution binary is complete and can be installed and used. ([@iarna](https://github.com/iarna))
* [`8a8c36c`](https://github.com/npm/npm/commit/8a8c36ce51166006022e5c5d4f8655bbc458d651) [#11613](https://github.com/npm/npm/pull/11613) Test that environment variables are properly passed into scripts.
  ([@iarna](https://github.com/zkat))
* [`a95b550`](https://github.com/npm/npm/commit/a95b5507616bd51e83d7eab5f2337b1aff6480b1) [#11613](https://github.com/npm/npm/pull/11613) Test that we don't leak auth info into the environment. ([@iarna](https://github.com/iarna))
* [`a1c1c52`](https://github.com/npm/npm/commit/a1c1c52efeab24f6dba154d054f85d9efc833486) [#11613](https://github.com/npm/npm/pull/11613) Remove all the relatively cryptic legacy tests and creates new tap tests that check the same functionality. The *legacy* tests were tests that were originally a shell script that was ported to javascript early in `npm`'s history. ([@iarna](https:\\github.com/iarna) and [@zkat](https://github.com/zkat))
* [`9d89581`](https://github.com/npm/npm/commit/9d895811d3ee70c2e672f3d8fa06574495b5b488) [#11613](https://github.com/npm/npm/pull/11613) `tacks@1.0.9`: Add a package that provides a tool to generate fixtures from folders and, relatedly, a module that an create and tear down filesystem fixtures easily. ([@iarna](https://github.com/iarna))

### v2.14.20 (2016-02-18):

Hope y'all are having a nice week! As usual, it's a fairly limited release. The
most notable thing is some dependency updates that might help the Node.js CI
setup for Windows run a little better, even if we have some work to do on that
path length things, still.

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

This issue, of course, is largely not a problem as of `npm@3`, with its flat
trees, but it still occasionally and viciously bites LTS.

We've taken another baby step towards alleviating this in this release by
updating a couple of dependencies that were preventing `npmlog` from deduping,
and then doing a dedupe on that and `gauge`. Hopefully it helps.

* [`4199551`](https://github.com/npm/npm/commit/41995517e617674710748ab6d262670c96124393)
  [#11528](https://github.com/npm/npm/pull/11528)
  `npm-install-checks@1.0.7`: Just updates the version of npmlog so we can
  dedupe it better.
  ([@zkat](https://github.com/zkat))
* [`14d72c7`](https://github.com/npm/npm/commit/14d72c756b89e2d167eb52c1849263dbddcb9f35)
  [#11552](https://github.com/npm/npm/pull/11552)
  [#11528](https://github.com/npm/npm/pull/11528)
  `node-gyp@3.3.0`: AIX support, new `gyp`, update `npmlog` (for the dedupe),
  adds `--cafile` command line option, and allows configuration of Node.js and
  io.js mirrors.
  ([@rvagg](https://github.com/rvagg))
* [`0453cb9`](https://github.com/npm/npm/commit/0453cb94b33520eb723b7072cd2654b1d0142533)
  [#11528](https://github.com/npm/npm/pull/11528)
  Do a `dedupe` on `gauge` to flatten our dependencies a bit more.
  ([@zkat](https://github.com/zkat))

#### OTHER DEP STUFF

* [`686c0b3`](https://github.com/npm/npm/commit/686c0b37ec3a7b65f9b3849e1099805e5221c408)
  `rimraf@2.5.2`: Just updates to glob@7.
  ([@isaacs](https://github.com/isaacs))

#### @wyze, DOCUMENTATION HERO OF THE PEOPLE, GETS THEIR OWN HEADER

* [`7232948`](https://github.com/npm/npm/commit/72329484c775376cb40d5b348f453eaaf2f0b821)
  [#11416](https://github.com/npm/npm/pull/11416)
  Logout docs were using a section copy-pasted from the adduser docs.
  ([@wyze](https://github.com/wyze))
* [`922b33a`](https://github.com/npm/npm/commit/922b33aba4362e1e90f42e9348f061a1cc73eafb)
  [#11414](https://github.com/npm/npm/pull/11414)
  Add colon for consistency.
  ([@wyze](https://github.com/wyze))

### v2.14.19 (2016-02-11):

Really tiny micro-release this week! The main thing to note is a dependency
update that means we no longer have `graceful-fs@3` in our dependency tree. This
has some implications for being able to run on future Node.js releases, so
better to get this out the door. 😁

#### DEPS

* [`a556e0f`](https://github.com/npm/npm/commit/a556e0f9dcb5d7b44224ba9c16c9d0dc6c8d2532)
  `cmd-shim@2.0.2`: Final straggler using `graceful-fs@<4`.
  ([@ForbesLindesay](https://github.com/ForbesLindesay))

#### DOCS

* [`69a2d59`](https://github.com/npm/npm/commit/69a2d599bf0cba674ee268483e9bd5c14333b89f)
  [#11391](https://github.com/npm/npm/pull/11391)
  Fixed versions of `shrinkwrap.json` in examples in documentation for `npm
  shrinkwrap`, which did not quite match up.
  ([@xcatliu](https://github.com/xcatliu))

### v2.14.18 (2016-02-04):

Clearly our docs are perfect after all those wonderful PRs, 'cause this week's
gonna be all about dependency updates. Note: There is a small security-related
fix included here!

#### SECURITY-RELATED DEPENDENCY UPDATE

* [`5c095ef`](https://github.com/npm/npm/commit/5c095eff8dc006980d4d083f2007e4dacff23be3)
  [#11341](https://github.com/npm/npm/pull/11341)
  `request@2.69.0`: Includes security-related dependency updates involving
  `hawk` and `is-my-json-valid`
  ([@remy](https://github.com/remy) and [@simov](https://github.com/simov))

#### OTHER DEPENDENCY UPDATES

* [`f9c2668`](https://github.com/npm/npm/commit/f9c2668ca3e6e2602d91250ce61280e5e12d0a00)
  `which@1.2.4`
  ([@isaacs](https://github.com/isaacs))
* [`2907c43`](https://github.com/npm/npm/commit/2907c43ad4ef87e5f730c2576f680d6837fcbad0)
  `spdx-license-ids@1.2.0`
  ([@shinnn](https://github.com/shinnn))
* [`7734069`](https://github.com/npm/npm/commit/773406960bf7f4a87b2ecb6ebf593c62d0e9f95d)
  `rimraf@2.5.1`
  ([@isaacs](https://github.com/isaacs))
* [`f4b39a7`](https://github.com/npm/npm/commit/f4b39a7dd5e1335d92aa22c46d99abb33f271b8b)
  `retry@0.9.0`
  ([@tim-kos](https://github.com/tim-kos))
* [`ded1e7a`](https://github.com/npm/npm/commit/ded1e7a1c9c7bec29bb7c30a8f85546670e75b56)
  Nest `retry@0.8.0` inside `npm-registry-client` to prevent invalid
  dependency issue until the latter gets a dependency update.
  ([@zkat](https://github.com/zkat))
* [`ab9f867`](https://github.com/npm/npm/commit/ab9f8679f9687f91ad03adaab6211a897aeebbae)
  `read-package-json@2.0.3`
  ([@iarna](https://github.com/iarna))
* [`b638c41`](https://github.com/npm/npm/commit/b638c41607bb936b9eaaceba2aeeda1d34e3a9b2)
  `npmlog@2.0.2`
  ([@iarna](https://github.com/iarna))
* [`49f34af`](https://github.com/npm/npm/commit/49f34af463a674359269025d8438feb6a7c69960)
  `init-package-json@1.9.3`
  ([@iarna](https://github.com/iarna))
* [`2305dab`](https://github.com/npm/npm/commit/2305dab4e7bff09bb7686cec653cf1e663dbf15d)
  `graceful-fs@4.1.3`: Fixed `.close()` not being patched.
  ([@isaacs](https://github.com/isaacs))
* [`18496d9`](https://github.com/npm/npm/commit/18496d9a0fff94e3652655998e8333056aa52b15)
  `fs-write-stream-atomic@1.0.8`
  ([@iarna](https://github.com/iarna))
* [`6637bc7`](https://github.com/npm/npm/commit/6637bc7a0e194d82554cd7c91e1794018fef5943)
  `config-chain@1.1.10`
  ([@dominictarr](https://github.com/dominictarr))
* [`4222bad`](https://github.com/npm/npm/commit/4222badffed9e9edacea6a8a96a99a164d376158)
  `columnify@1.5.4`
  ([@timoxley](https://github.com/timoxley))
* [`df9016f`](https://github.com/npm/npm/commit/df9016f327a2a9ce492ebc75b882b03069438e13)
  `ansi@0.3.1`: Added a license file.
  ([@TooTallNate](https://github.com/TooTallNate))

### v2.14.17 (2016-01-28):

Another week, another small LTS release!

#### BETTER ERROR REPORTING YAY

So as it turns out, when stuff goes wrong, it's actually nice to give people a
better clue rather than just say "oh well 😏".

* [`5b8ccb9`](https://github.com/npm/npm/commit/5b8ccb91cf11b4edb463609cd4ed1dee84ed4db0)
  [#11289](https://github.com/npm/npm/pull/11289)
  There is an obscure feature that lets you monkey-patch npm when it starts up.
  If the module being required with this feature failed, it would previous just
  make npm error out– this reduces that to a warning.
  ([@evanlucas](https://github.com/evanlucas))
* [`556e42a`](https://github.com/npm/npm/commit/556e42ac6bab078722ddc1dc6cce4428d001133b)
  [#11300](https://github.com/npm/npm/pull/11300)
  Report symlinked packages as 'linked' in the output for `npm outdated`.
  ([@halhenke](https://github.com/halhenke))
* [`3842317`](https://github.com/npm/npm/commit/3842317583e0ea2eca78e39aa03f5bc06ba21de7)
  [#11290](https://github.com/npm/npm/pull/11290)
  Suppress warnings about pre-release node versions. This should get node's CI
  passing on non-Windows platforms without needing to modify the node version to
  get rid of the pre-release suffix.
  ([@iarna](https://github.com/iarna))

#### EVERYONE WANTS THOSE NPM SOCKS, GEEZE

Did you know that you can get npm socks for contributing to our docs? I bet
these people do, and now so do you!

* [`dcde451`](https://github.com/npm/npm/commit/dcde451cb85a6ca08acc6ef45782c652f1d8fc89)
  [#11232](https://github.com/npm/npm/pull/11232)
  Update automatically included/excluded packages in `package.json`.
  ([@jscissr](https://github.com/jscissr))
* [`e3f8d5b`](https://github.com/npm/npm/commit/e3f8d5be5ac5ec1d72db42f7abf50cc4a8c5935c)
  [#11273](https://github.com/npm/npm/pull/11273)
  Add an example for `npm view <pkg> versions`.
  ([@vedatmahir](https://github.com/vedatmahir))
* [`6a06ef2`](https://github.com/npm/npm/commit/6a06ef2252748089f0013de951f2d06160b90306)
  [#11272](https://github.com/npm/npm/pull/11272)
  Fix a typo in `npm-update.md`.
  ([@jonathanp](https://github.com/jonathanp))
* [`2515ff1`](https://github.com/npm/npm/commit/2515ff1de28f0b261fb25c79a66bd762a65961c4)
  [#11215](https://github.com/npm/npm/pull/11215)
  Correct small thinko in docs for SPDX expressions.
  ([@kemitchell](https://github.com/kemitchell))
* [`70f897b`](https://github.com/npm/npm/commit/70f897b03da9a5d5d4fd34614e9ee40e6f9e9653)
  [#11196](https://github.com/npm/npm/pull/11196)
  Make JSON snippets valid JSON in `npm update` docs.
  ([@s100](https://github.com/s100))

### v2.14.16 (2016-01-21):

Good to see you all again! It's been a while since we had an LTS release, and
the team continues to work hard to both get the issue tracker under control, and
get our test suite to be awesome and reliable.

This is also the first LTS release of this year.

We're gonna have an interesting time -- most of our focus this year will be
around stability and maintainability of the CLI, so you might actually end up
seeing a number of updates even over here, just for the sake of making sure
we're stable, that bugs get fixed, and tests have proper coverage.

What better way to start this effort, then, than getting Travis tests green, fix
a few things here and there, and tweak a bunch of documentation? 😁

#### FIX ALL THE BUGS AND TWEAK ALL THE THINGS

* [`24b13fb`](https://github.com/npm/npm/commit/24b13fbc57d34db1d5b0a37bcca122c00deba978)
  [#11158](https://github.com/npm/npm/pull/11158)
  Fix custom node-gyp env var quoting on Windows.
  ([@orangemocha](https://github.com/orangemocha))
* [`e2503f2`](https://github.com/npm/npm/commit/e2503f2be40157b05a9c500ec3b5d16090ffee50)
  [#11142](https://github.com/npm/npm/pull/11142)
  Fix race condition with `correctMkdir` in the cache directory.
  ([@Jimbly](https://github.com/Jimbly))

* [`5c0e4c4`](https://github.com/npm/npm/commit/5c0e4c45a29d774ab729e86044377d4e5e424252)
  [#10940](https://github.com/npm/npm/pull/10940)
  Ignore failures replacing `package.json`. writeFileAtomic is not atomic in
  Windows, it fails if the file is being accessed concurrently.
  ([@orangemocha](https://github.com/orangemocha))
* [`2c44d8d`](https://github.com/npm/npm/commit/2c44d8dc8c267d5e054d0175ce2f4750f0986463)
  [#10903](https://github.com/npm/npm/pull/10903)
  Add tests for `npm adduser --scope`.
  ([@ekmartin](https://github.com/ekmartin))
* [`4cb25d0`](https://github.com/npm/npm/commit/4cb25d0fed5c7792dfd1aec891380ecc1f8a5761)
  [#10903](https://github.com/npm/npm/pull/10903)
  Add a message informing users when they have been successfully logged in.
  ([@ekmartin](https://github.com/ekmartin))
* [`fe3ec6d`](https://github.com/npm/npm/commit/fe3ec6d6658262054c0c19c55373c21e84ab9f17)
  [#10628](https://github.com/npm/npm/pull/10628)
  Tell users how to open an issue with a package that has errored.
  ([@trodrigues](https://github.com/trodrigues))

#### DOCS DOCS DOCS

We got a TON of lovely documentation patches, too! Thanks all for submitting!

* [`22482a1`](https://github.com/npm/npm/commit/22482a1f22079d72c3f8ca55c2f0c153bdd024c0)
  [#11188](https://github.com/npm/npm/pull/11188)
  Briefly explain what's included when you publish.
  ([@beaugunderson](https://github.com/beaugunderson))
* [`fa47724`](https://github.com/npm/npm/commit/fa4772438df0c66a19309dd1c1a3ce43cbee5461)
  [#11150](https://github.com/npm/npm/pull/11150)
  Advise use of `--depth Infinity` instead of `--depth 9999` in `npm update`.
  ([@halhenke](https://github.com/halhenke))
* [`248ddfe`](https://github.com/npm/npm/commit/248ddfe8f7ddd3318e14bf61de41cab4a638c8a3)
  [#11130](https://github.com/npm/npm/pull/11130)
  Nuke "using npm programmatically" section from README. The programmatic npm
  API is unsupported, and is not guaranteed not to break in non-major versions.
  Removing this section so newcomers aren't encouraged to discover or use it.
  ([@ljharb](https://github.com/ljharb))
* [`ae9c452`](https://github.com/npm/npm/commit/ae9c4521222d60ab4a69c19fee5e361c62f41fae)
  [#11128](https://github.com/npm/npm/pull/11128)
  Add link to local paths section indocs for `package.json`.
  ([@orangejulius](https://github.com/orangejulius))
* [`663a8c6`](https://github.com/npm/npm/commit/663a8c6b4b1647f9b86c15ef32e30023edc8c060)
  [#11044](https://github.com/npm/npm/pull/11044)
  Update default value documentation for the color option in npm's config.
  ([@scottaddie](https://github.com/scottaddie))
* [`5c1dda0`](https://github.com/npm/npm/commit/5c1dda0d3a18b2954872dba33fbc696ff0700ffe)
  [#11037](https://github.com/npm/npm/pull/11037)
  Correct the name property max length constraint verbiage.
  ([@scottaddie](https://github.com/scottaddie))
* [`8288365`](https://github.com/npm/npm/commit/8288365d08e97fa3a5b0d31703c015a8be49e07f)
  [#10990](https://github.com/npm/npm/pull/10990)
  Update folder docs to reflect that process.installPrefix was removed as of
  0.8.x.
  ([@jeffmcmahan](https://github.com/jeffmcmahan))
* [`61d63fa`](https://github.com/npm/npm/commit/61d63fa22c4f09742180c2de460a4ffb6c32738e)
  [#10790](https://github.com/npm/npm/pull/10790)
  Clarify that `npm install foo` is the same as `npm install foo@latest` now.
  ([@cvrebert](https://github.com/cvrebert))
* [`442c920`](https://github.com/npm/npm/commit/442c9207f375354c91d36df8711ba2d33e1c97f3)
  [#10789](https://github.com/npm/npm/pull/10789)
  Link over to `npm-dist-tag(1)` in `npm install` docs when they talk about the
  `pkg@<tag>` syntax.
  ([@cvrebert](https://github.com/cvrebert))
* [`dca7a5e`](https://github.com/npm/npm/commit/dca7a5e2be3bfa306a870a123707d35c732406c0)
  [#10788](https://github.com/npm/npm/pull/10788)
  Link to tag docs in docs for `npm publish --tag`.
  ([@cvrebert](https://github.com/cvrebert))
* [`a72904e`](https://github.com/npm/npm/commit/a72904e8d4ab1d43ae8150fbe3f6468b0cbb1efd)
  [#10787](https://github.com/npm/npm/pull/10787)
  Explain why the `latest` tag matters.
  ([@cvrebert](https://github.com/cvrebert))
* [`9d0697a`](https://github.com/npm/npm/commit/9d0697a534046df7efda32170014041bbc1f4e7d)
  [#10785](https://github.com/npm/npm/pull/10785)
  Replace some quite marks in `npm dist-tag` docs for the sake of consistency.
  ([@cvrebert](https://github.com/cvrebert))

#### I REALLY LIKE GREEN. CAN YOU TELL?

So Travis is all green now on `npm@2`, thanks to the removal of nock and a few
other test suite tweaks. This is a fantastic step towards making sure we can all
have confidence in our test suite! 🎉

* [`64995be`](https://github.com/npm/npm/commit/64995be6d874356b15c136f9867302d805dfe1e9) [`75ab216`](https://github.com/npm/npm/commit/75ab2164cf79e28ac7f7ebe714f3c5aee99c6626) [`a9f6fe9`](https://github.com/npm/npm/commit/a9f6fe9dc558f17c4a7b9eb83329ac080f7df4b7) [`649c193`](https://github.com/npm/npm/commit/649c193adadf714c2819837f9372a29d724a5ec0) [`94cb05e`](https://github.com/npm/npm/commit/94cb05eaa9e5ad6675cf15c4ac0a44fbdde05900) [`6541690`](https://github.com/npm/npm/commit/65416907008061ac5a5f66b1630a57776803b526) [`255be6f`](https://github.com/npm/npm/commit/255be6f5bca9e3d216f3a5cbdf6714c6c9fcf132) [`9e84fa4`](https://github.com/npm/npm/commit/9e84fa43c49d04cf86ca1678e2a61412f5559cb9) [`8a587b0`](https://github.com/npm/npm/commit/8a587b0c1696ae7302891fa6355fc3e8670e00d3) [`bf812a5`](https://github.com/npm/npm/commit/bf812a54e497a573493346399798aa0b9373ac24)
  [#10903](https://github.com/npm/npm/pull/10903)
  Get rid of nock from tests, and get Travis green.
  ([@zkat](https://github.com/zkat) and [@iarna](https://github.com/iarna))
* [`70a5310`](https://github.com/npm/npm/commit/70a5310712c6666e753ca8f3bfff4a780ec6292d)
  `npm-registry-couchapp@2.6.12`:
  Better 0.8 compatibility, and ability to run in travis docker stuff. This
  means the test suite should run a lot faster, too!
  ([@iarna](https://github.com/iarna))
* [`28fae39`](https://github.com/npm/npm/commit/28fae399212eda5554e6c0ffd8c9591144ab7b9d)
  Get rid of sudo, for Travis!
  ([@zkat](https://github.com/zkat))

### v2.14.15 (2015-12-10):

Did you know that Bob Ross reached the rank of master sergeant in the US Air
Force before becoming perhaps the most soothing painter of all time?

#### TWO HAPPY LITTLE BUG FIXES

* [`f482664`](https://github.com/npm/npm/commit/f4826645dc6b5c0f05c5f9187efb28c1a293554f)
  [#10505](https://github.com/npm/npm/issues/10505) `npm ls --json --depth=0`
  now respects the depth parameter, when it is zero and when it is not zero.
  ([@MarkReeder](https://github.com/MarkReeder))
* [`529fa1f`](https://github.com/npm/npm/commit/529fa1ff2c6432a773af99a1c5209c0865f7a19c)
  [#9099](https://github.com/npm/npm/issues/9099) I had always thought you
  could run `npm version` from subdirectories in your project, which is great,
  because now you can. I guess I was just ahead of my time.
  ([@ekmartin](https://github.com/ekmartin))

#### NOW PAINT IN SOME NICE DOCS CHANGES

* [`1fc7f2b`](https://github.com/npm/npm/commit/1fc7f2b523ea760e08adb9b861b28e3ba450e565)
  [#10546](https://github.com/npm/npm/issues/10546) Goodbye, FAQ! You were
  cheeky and fun until you weren't! Don't worry: npm still loves everyone,
  especially you! ([@ashleygwilliams](https://github.com/ashleygwilliams))
* [`7fe6950`](https://github.com/npm/npm/commit/7fe6950b44d241bb4d90857a44d89d750af1e2b3)
  [#10570](https://github.com/npm/npm/issues/10570) Update documentation URLs
  to be HTTPS everywhere sensible. No HTTP shall be spared!
  ([@rsp](https://github.com/rsp))
* [`96ebb90`](https://github.com/npm/npm/commit/96ebb902439e4f6f37f8beffb589769146fecf24)
  [#10650](https://github.com/npm/npm/issues/10650) Correctly note that there
  are two lifecycle scripts run by an install phase in an example, instead of
  three. ([@eymengunay](https://github.com/eymengunay))
* [`5196893`](https://github.com/npm/npm/commit/5196893a7496f68a514b83641ff6b72f14d664dd)
  [#10687](https://github.com/npm/npm/issues/10687) `npm outdated`'s output can
  be a little puzzling sometimes. I've attempted to make it clearer, with some
  examples, of what's going on with "wanted" and "latest" in more cases.
  ([@othiym23](https://github.com/othiym23))
* [`8e6712d`](https://github.com/npm/npm/commit/8e6712d4ee128858cab36c77723e35bddbb977ba)
  [#10700](https://github.com/npm/npm/issues/10700) Hey, do you remember when
  `search.npmjs.org` was a thing? I think I do? The last time I used it was in
  like 2012, and it's gone now, so remove it from the docs.
  ([@gagern](https://github.com/gagern))
* [`27d2612`](https://github.com/npm/npm/commit/27d2612b3f5aa88b12c943d04e162ce4c3a350ae)
  `semver@5.1.0`: Include BNF for SemVer expression grammar (which is also now
  included in `npm help semver`). ([@isaacs](https://github.com/isaacs))

#### LAND YOUR DEPENDENCY UPGRADES IN PAIRS SO EVERYONE HAS A FRIEND

* [`fc6c3c5`](https://github.com/npm/npm/commit/fc6c3c53a31e9e11c2616fcd378202e5b80bf286)
  `request@2.67.0` ([@simov](https://github.com/simov))
* [`07013fd`](https://github.com/npm/npm/commit/07013fd0fd55a2eb31fb9334631ee5d0dd5c41bb)
  [isaacs/rimraf#89](https://github.com/isaacs/rimraf/pull/89) `rimraf@2.4.4`
  ([@zerok](https://github.com/zerok))
* [`bc149be`](https://github.com/npm/npm/commit/bc149bef871f0f00639509898cece531af3aa8b3)
  [isaacs/once#7](https://github.com/isaacs/once/pull/7) `once@1.3.3`
  ([@floatdrop](https://github.com/floatdrop))
* [`ac598d3`](https://github.com/npm/npm/commit/ac598d36e1ad207bc0d8a7eadfd84b26146aec1f)
  `lru-cache@3.2.0` ([@isaacs](https://github.com/isaacs))
* [`1b915ce`](https://github.com/npm/npm/commit/1b915ce1e0787ccb6d8aa235d002d66565f2175d)
  `npm-registry-client@7.0.9` ([@othiym23](https://github.com/othiym23))
* [`df7dd78`](https://github.com/npm/npm/commit/df7dd78b8fe3cc58202996fa6c994fc55419bfa5)
  `tap@2.3.1` ([@isaacs](https://github.com/isaacs))

### v2.14.14 (2015-12-03):

#### FIX URL IN LICENSE

The license incorrectly identified the registry URL as `registry.npmjs.com` and
this has been corrected to `registry.npmjs.org`.

* [`6051a69`](https://github.com/npm/npm/commit/6051a69b1adc80f5f200077067e831643f655bd4)
  [#10685](https://github.com/npm/npm/pull/10685)
  Fix npm public registry URL in notices.
  ([@kemitchell](https://github.com/kemitchell))

#### NO MORE MD5

We updated modules that had been using MD5 for non-security purposes.  While
this is perfectly safe, if you compile Node in FIPS-compliance mode it will
explode if you try to use MD5. We've replaced MD5 with Murmur, which conveys
our intent better and is faster to boot.

* [`30b5994`](https://github.com/npm/npm/commit/30b599496a9762482e1cef945a378e3a534fd366)
  [#10629](https://github.com/npm/npm/issues/10629)
  `write-file-atomic@1.1.4`
  ([@othiym23](https://github.com/othiym23))
* [`68c63ff`](https://github.com/npm/npm/commit/68c63ff1279d3d5ea7b2c970ab5562a8e0536f27)
  [#10629](https://github.com/npm/npm/issues/10629)
  `fs-write-stream-atomic@1.0.5`
  ([@othiym23](https://github.com/othiym23))

#### DEPENDENCY UPDATES

* [`e48e5a9`](https://github.com/npm/npm/commit/e48e5a90b4dcf76124b7e9ea3b295c1383e7f0c8)
  [nodejs/node-gyp#831](https://github.com/nodejs/node-gyp/pull/831)
  `node-gyp@3.2.1`: Improved \*BSD support.
  ([@bnoordhuis](https://github.com/bnoordhuis))

### v2.14.13 (2015-11-25):

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
`npm@2.14.11`](https://github.com/npm/npm/releases/tag/v2.14.11), which is where
the split between the CLI's code and the terms of use for the registry was
first made more clear.

* [`1f3e936`](https://github.com/npm/npm/commit/1f3e936aab6840667948ef281e0c3621df365131)
  [#10532](https://github.com/npm/npm/issues/10532) Clarify that
  `registry.npmjs.org` is the default, but that you're free to use the npm CLI
  with whatever registry you wish. ([@kemitchell](https://github.com/kemitchell))
* [`6733539`](https://github.com/npm/npm/commit/6733539eeb9b32a5f2d1a6aa797987e2252fa760)
  [#10532](https://github.com/npm/npm/issues/10532) Having semi-duplicate
  release information in `README.md` was confusing and potentially inaccurate,
  so remove it. ([@kemitchell](https://github.com/kemitchell))

#### EASE UP ON WINDOWS BASH USERS

It turns out that a fair number of us use bash on Windows (through MINGW or
bundled with Git, plz – Cygwin is still a bridge too far, for both npm and
Node.js). [@jakub-g](https://github.com/jakub-g) did us all a favor and relaxed
the check for npm completion to support MINGW bash. Thanks, Jakub!

* [`460cc09`](https://github.com/npm/npm/commit/460cc0950fd6a005c4e5c4f85af807814209b2bb)
  [#10156](https://github.com/npm/npm/issues/10156) completion: enable on
  Windows in git bash ([@jakub-g](https://github.com/jakub-g))

#### MAKE NODE-GYP A LITTLE BLUER

* [`333e118`](https://github.com/npm/npm/commit/333e1181082842c21edc62f0ce515928424dff1f)
  `node-gyp@3.2.0`: Support AIX, use `which` to find Python, updated to a newer
  version of `gyp`, and more! ([@bnoordhuis](https://github.com/bnoordhuis))

#### WE LIKE SPDX AND ALL BUT IT'S NOT ACTUALLY A DIRECT DEP, SORRY

* [`1f4b4bb`](https://github.com/npm/npm/commit/1f4b4bbdf8758281beecb7eaf75d05a6c4a77c15)
  Removed `spdx` as a direct npm dependency, since we don't actually need it at
  that level, and updated subdeps for `validate-npm-package-license`
  ([@othiym23](https://github.com/othiym23))

#### A BOUNTEOUS THANKSGIVING CORNUCOPIA OF DOC TWEAKS

These are great! Keep them coming! Sorry for letting them pile up so deep,
everybody. Also, a belated Thanksgiving to our Canadian friends, and a happy
Thanksgiving to all our friends in the USA.

* [`6101f44`](https://github.com/npm/npm/commit/6101f44737645d9379c3396fae81bbc4d94e1f7e)
  [#10250](https://github.com/npm/npm/issues/10250) Correct order of `org:team`
  in `npm team` documentation. ([@louislarry](https://github.com/louislarry))
* [`e8769f9`](https://github.com/npm/npm/commit/e8769f9807b91582c15ef130733e2e72b6c7bda4)
  [#10371](https://github.com/npm/npm/issues/10371) Remove broken / duplicate
  link to tag. ([@WickyNilliams](https://github.com/WickyNilliams))
* [`1ae2dbe`](https://github.com/npm/npm/commit/1ae2dbe759feb80d8634569221ec6ee2c6d1d1ff)
  [#10419](https://github.com/npm/npm/issues/10419) Remove references to
  nonexistent `npm-rm(1)` documentation. ([@KenanY](https://github.com/KenanY))
* [`777a271`](https://github.com/npm/npm/commit/777a271830a42d4ee62540a89f764a6e7d62de19)
  [#10474](https://github.com/npm/npm/issues/10474) Clarify that install finds
  dependencies in `package.json`. ([@sleekweasel](https://github.com/sleekweasel))
* [`dcf4b5c`](https://github.com/npm/npm/commit/dcf4b5cbece1b0ef55ab7665d9acacc0b6b7cd6e)
  [#10497](https://github.com/npm/npm/issues/10497) Clarify what a package is
  slightly. ([@aredridel](https://github.com/aredridel))
* [`447b3d6`](https://github.com/npm/npm/commit/447b3d669b2b6c483b8203754ac0a002c67bf015)
  [#10539](https://github.com/npm/npm/issues/10539) Remove an extra, spuriously
  capitalized letter. ([@alexlukin-softgrad](https://github.com/alexlukin-softgrad))

### v2.14.12 (2015-11-19):

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

* [`0e8b15e`](https://github.com/npm/npm/commit/0e8b15e9fbc89e31bd00e573b648846beddfb835)
  [#9327](https://github.com/npm/npm/issues/9327) `npm access` no longer has
  problems when run in a directory that doesn't contain a `package.json`.
  ([@othiym23](https://github.com/othiym23))
* [`c4e939c`](https://github.com/npm/npm/commit/c4e939c1d493601d25dcb88e6ffcca73076fd3fd)
  [npm/npm-registry-client#126](https://github.com/npm/npm-registry-client/issues/126)
  `npm-registry-client@7.0.8`: Allow the CLI to grant, revoke, and list
  permissions on unscoped (public) packages on the primary registry.
  ([@othiym23](https://github.com/othiym23))

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

* [`d743620`](https://github.com/npm/npm/commit/d743620a0005213a65d25de771661b4d48a09717)
  [#10233](https://github.com/npm/npm/issues/10233) Update Node.js versions
  that Travis uses to test npm. ([@iarna](https://github.com/iarna))

#### TYPOS IN THE LICENSE, OH MY

* [`58ac241`](https://github.com/npm/npm/commit/58ac241f556b2c202a8ee33321965e2540361ca7)
  [#10478](https://github.com/npm/npm/issues/10478) Correct two typos in npm's
  LICENSE. ([@jorrit](https://github.com/jorrit))

### v2.14.11 (2015-11-12):

#### ASK FOR NOTHING, GET LATEST

When you run `npm install foo`, you probably expect that you'll get the
`latest` version of `foo`, whatever that is. And good news! That's what this
change makes it do.

We _think_ this is what everyone wants, but if this causes problems for you, we
want to know! If it proves problematic for people we will consider reverting it
(preferably before this becomes `npm@latest`).

Previously, when you ran `npm install foo` we would act as if you typed `npm
install foo@*`. Now, like any range-type specifier, in addition to matching the
range, it would also have to be `<=` the value of the `latest` dist-tag.
Further, it would exclude prerelease versions from the list of versions
considered for a match.

This worked as expected most of the time, unless your `latest` was a prerelease
version, in which case that version wouldn't be used, to everyone's surprise.

* [`6f0a646`](https://github.com/npm/npm/commit/6f0a646cd865b24fe3ff25365bf5421780e63e01)
  [#10189](https://github.com/npm/npm/issues/10189) `npm-package-arg@4.1.0`:
  Change the default version from `*` to `latest`.
  ([@zkat](https://github.com/zkat))

#### LICENSE CLARIFICATION

* [`54a9046`](https://github.com/npm/npm/commit/54a90461f068ea89baa5d70248cdf1581897936d)
  [#10326](https://github.com/npm/npm/issues/10326) Clarify what-all is covered
  by npm's license and point to the registry's terms of use.
  ([@kemitchell](https://github.com/kemitchell))

#### CLOSER TO GREEN TRAVIS

* [`28efd3d`](https://github.com/npm/npm/commit/28efd3d7dfb2fa3755076ae706ea4d38c6ee6900)
  [#10232](https://github.com/npm/npm/issues/10232) `nock@1.9.0`: Downgrade
  nock to a version that doesn't depend on streams2 in core so that more of our
  tests can pass in 0.8. ([@iarna](https://github.com/iarna))

#### A BUG FIX

* [`eacac8f`](https://github.com/npm/npm/commit/eacac8f05014d15217c3d8264d0b00a72eafe2d2)
  [#9965](https://github.com/npm/npm/issues/9965) Fix a corrupt `package.json`
  file introduced by a merge conflict in
  [`022691a`](https://github.com/npm/npm/commit/022691a).
  ([@waynebloss](https://github.com/waynebloss))

#### A DEPENDENCY UPGRADE

* [`ea7d8e0`](https://github.com/npm/npm/commit/ea7d8e00a67a3d5877ed72c9728909c848468a9b)
  [npm/nopt#51](https://github.com/npm/nopt/pull/51) `nopt@3.0.6`: Allow
  types checked to be validated by passed-in name in addition to the JS name of
  the type / class. ([@wbecker](https://github.com/wbecker))

### v2.14.10 (2015-11-05):

There's nothing in here that that isn't in the `npm@3.4.0` release notes, but
all of the commit shasums have been adjusted to be correct. Enjoy!

#### BUG FIXES VIA DEPENDENCY UPDATES

* [`204c558`](https://github.com/npm/npm/commit/204c558c06637a753c0b41d0cf19f564a1ac3715)
  [#8640](https://github.com/npm/npm/issues/8640)
  [npm/normalize-package-data#69](https://github.com/npm/normalize-package-data/pull/69)
  `normalize-package-data@2.3.5`: Fix a bug where if you didn't specify the
  name of a scoped module's binary, it would install it such that it was
  impossible to call it.  ([@iarna](https://github.com/iarna))
* [`bbdf4ee`](https://github.com/npm/npm/commit/bbdf4ee0a3cd12be6a2ace255b67d573a72f1f8f)
  [npm/fstream-npm#14](https://github.com/npm/fstream-npm/pull/14)
  `fstream-npm@1.0.7`: Only filter `config.gypi` when it's in the build
  directory.  ([@mscdex](https://github.com/mscdex))
* [`d82ff81`](https://github.com/npm/npm/commit/d82ff81403e906931fac701775723626dcb443b3)
  [npm/fstream-npm#15](https://github.com/npm/fstream-npm/pull/15)
  `fstream-npm@1.0.6`: Stop including directories that happened to have names
  matching whitelisted npm files in npm module tarballs. The most common cause
  was that if you had a README directory then everything in it would be
  included if wanted it or not. ([@taion](https://github.com/taion))

#### DOCUMENTATION FIXES

* [`16361d1`](https://github.com/npm/npm/commit/16361d122f2ff6d1a4729c66153b7c24c698fd19)
  [#10036](https://github.com/npm/npm/pull/10036) Fix typo / over-abbreviation.
  ([@ifdattic](https://github.com/ifdattic))
* [`d1343dd`](https://github.com/npm/npm/commit/d1343dda42f113dc322f95687f5a8c7d71a97c35)
  [#10176](https://github.com/npm/npm/pull/10176) Fix broken link, scopes =>
  scope.  ([@ashleygwilliams](https://github.com/ashleygwilliams))
* [`110663d`](https://github.com/npm/npm/commit/110663d000a3908a4853393d9abae481700cf4dc)
  [#9460](https://github.com/npm/npm/issue/9460) Specifying the default command
  run by "npm start" and the fact that you can pass it arguments.
  ([@JuanCaicedo](https://github.com/JuanCaicedo))

#### DEPENDENCY UPDATES FOR THEIR OWN SAKE

* [`7476d2d`](https://github.com/npm/npm/commit/7476d2d31552a41671c425aa7fcc2844e0381008)
  [npm/npmlog#19](https://github.com/npm/npmlog/pull/19)
  `npmlog@2.0.0`: Make it possible to emit log messages with `error` as the
  prefix.
  ([@bengl](https://github.com/bengl))
* [`6ca7888`](https://github.com/npm/npm/commit/6ca7888862cfe8bf802dc7c66632c102acd94cf5)
  `read-package-json@2.0.2`: Minor cleanups.
  ([@KenanY](https://github.com/KenanY))

### v2.14.9 (2015-10-29):

There's still life in `npm@2`, but for now, enjoy these dependency upgrades!
Also, [@othiym23](https://github.com/othiym23) says hi! _waves_
[@zkat](https://github.com/zkat) has her hands full, and
[@iarna](https://github.com/iarna)'s handling `npm@3`, so I'm dealing with
`npm@2` and the totally nonexistent weird bridge `npm@1.4` LTS release that may
or may not be happening this week.

#### CAN'T STOP WON'T STOP UPDATING THOSE DEPENDENCIES

* [`f52f0cb`](https://github.com/npm/npm/commit/f52f0cb51526314197e9d67619feebbd82a397b7)
  [#10150](https://github.com/npm/npm/issues/10150) `chmodr@1.0.2`: Use
  `fs.lstat()` to check if an entry is a directory, making `chmodr()` work
  properly with NFS mounts on Windows. ([@sheerun](https://github.com/sheerun))
* [`f7011d7`](https://github.com/npm/npm/commit/f7011d7b3b1d9148a6cd8f7b8359d6fe3269a912)
  [#10150](https://github.com/npm/npm/issues/10150) `which@1.2.0`: Additional
  command-line parameters, which is nice but not used by npm.
  ([@isaacs](https://github.com/isaacs))
* [`ebcc0d8`](https://github.com/npm/npm/commit/ebcc0d8629388da0b849bbbad590382cd7268f51)
  [#10150](https://github.com/npm/npm/issues/10150) `minimatch@3.0.0`: Don't
  package browser version. ([@isaacs](https://github.com/isaacs))
* [`8c98dce`](https://github.com/npm/npm/commit/8c98dce5ffe242bafbe92b849e73e8de1803e256)
  [#10150](https://github.com/npm/npm/issues/10150) `fstream-ignore@1.0.3`:
  Upgrade to use `minimatch@3` (for deduping purposes).
  ([@othiym23](https://github.com/othiym23))
* [`db9ef33`](https://github.com/npm/npm/commit/db9ef337c253ecf21c921055bf8742e10d1cb3bb)
  [#10150](https://github.com/npm/npm/issues/10150) `request@2.65.0`:
  Dependency upgrades and a few bug fixes, mostly related to cookie handling.
  ([@simov](https://github.com/simov))

#### DEVDEPENDENCIES TOO, I GUESS, IT'S COOL

* [`dfbf621`](https://github.com/npm/npm/commit/dfbf621afa09c46991249b4f9a995d1823ea7ede)
  [#10150](https://github.com/npm/npm/issues/10150) `tap@2.2.0`: Better
  handling of test order handling (including some test fixes for npm).
  ([@isaacs](https://github.com/isaacs))
* [`cf5ad5a`](https://github.com/npm/npm/commit/cf5ad5a8c88bfd72e30ef8a8d1d3c5508e0b3c23)
  [#10150](https://github.com/npm/npm/issues/10150) `nock@2.16.0`: More
  expectations, documentation, and bug fixes.
  ([@pgte](https://github.com/pgte))

### v2.14.8 (2015-10-08):

#### SLOWLY RECOVERING FROM FEELINGS

OS&F is definitely my favorite convention I've gone to. Y'all should check it
out next year! Rebecca and Kat are back, although Forrest is out at
[&yet conf](http://andyetconf.com/).

This week sees another tiny LTS release with non-code-related patches -- just
CI/release things.

Meanwhile, have you heard? `npm@3` is much faster now! Go upgrade with `npm
install -g npm@latest` and give it a whirl if you haven't already!

#### IF YOU CHANGE CASING ON A FILE, YOU ARE NOT MY FRIEND

Seriously. I love me some case-sensitive filesystems, but a lot of us have to
deal with `git` and its funky support for case normalizing systems. Have mercy
and just don't bother if all you're changing is casing, please? Otherwise, I
have to do this little dance to prevent horrible conflicts.

* [`c3a7b61`](https://github.com/npm/npm/commit/c3a7b619786650a45653c8b55b8741fc7bb5cfda)
  [#9804](https://github.com/npm/npm/pulls/9804) Remove the readme file with
  weird casing.
  ([@zkat](https://github.com/zkat))
* [`f3f619e`](https://github.com/npm/npm/commit/f3f619e06e4be1378dbf286f897b50e9c69c9557)
  [#9804](https://github.com/npm/npm/pulls/9804) Add the readme file back in,
  with desired casing.
  ([@zkat](https://github.com/zkat))

#### IDK. OUR CI DOESN'T EVEN FULLY WORK YET BUT SURE

Either way, it's nice to make sure we're running stuff on the latest Node. `4.2`
is getting released very soon, though (this week?), and that'll be the first
official LTS release!

* [`bd0b9ab`](https://github.com/npm/npm/commit/bd0b9ab6e60a31448794bbd88f94672572c3cb55)
  [#9827](https://github.com/npm/npm/pulls/9827) Add node `4.0` and `4.1` to
  TravisCI
  ([@JaKXz](https://github.com/JaKXz))

### v2.14.7 (2015-10-01):

#### MORE RELEASE STAGGERING?!

Hi all, and greetings from [Open Source & Feelings](http://osfeels.com)!

So we're switching gears a little with how we handle our weekly releases: from
now on, we're going to stagger release weeks between dependency bumps and
regular patches. So, this week, aside from a doc change, we'll be doing only
version bumps. Expect actual patches next week!

#### TOTALLY FOLLOWING THE RULES ALREADY

So I snuck this in, because it's our own [@snopeks](https://github.com/snopeks)'
first contribution to the main `npm` repo. She's been helping with building
support documents for Orgs, and contributed her general intro guide to the new
feature so you can read it with `npm help orgs` right in your terminal!

* [`8324ea0`](https://github.com/npm/npm/commit/8324ea023ace4e08b6b8959ad199e2457af9f9cf)
  [#9761](https://github.com/npm/npm/pull/9761) Added general user guide for
  Orgs.
  ([@snopeks](https://github.com/snopeks))

#### JUST. ONE. MORE.

* [`9a502ca`](https://github.com/npm/npm/commit/9a502ca96e2d43ec75a8f684c9ca33af7e910f0a)
  Use unique package name in tests to work around weird test-state-based
  failures.
  ([@iarna](https://github.com/iarna))

#### OKAY ACTUALLY THE THING I WAS SUPPOSED TO DO

Anyway -- here's your version bump! :)

* [`4aeb94c`](https://github.com/npm/npm/commit/4aeb94c9f0df3f41802cf2e0397a998f3b527c25)
  `request@2.64.0`: No longer defaulting to `application/json` for `json`
  requests. Also some minor doc and packaging patches.
  ([@simov](https://github.com/simov))
  `minimatch@3.0.0`: No longer packaging browser modules.
  ([@isaacs](https://github.com/isaacs))
* [`a18b213`](https://github.com/npm/npm/commit/a18b213e6945a8f5faf882927829ac95f844e2aa)
  `glob@5.0.15`: Upgraded `minimatch` dependency.
  ([@isaacs](https://github.com/isaacs))
* [`9eb64d4`](https://github.com/npm/npm/commit/9eb64e44509519ca9d788502edb2eba4cea5c86b)
  `nock@2.13.0`
  ([@pgte](https://github.com/pgte))

### v2.14.6 (2015-09-24):

#### `¯\_(ツ)_/¯`

Since `2.x` is LTS now, you can expect a slowdown in overall release sizes. On
top of that, we had our all-company-npm-internal-conf thing on Monday and
Tuesday so there wasn't really time to do much at all.

Still, we're bringing you a couple of tiny little changes this week!

* [`7b7da13`](https://github.com/npm/npm/commit/7b7da13c6cdf5eae53c20d5c69afc4c16e6f715d)
  [#9471](https://github.com/npm/npm/pull/9471) When the port for a tarball is
  different than the registry it's in, but the hostname is the same, the
  protocol is now allowed to change, too.
  ([@fastest963](https://github.com/fastest963))
* [`6643ada`](https://github.com/npm/npm/commit/6643adaf9f37f08893e3ad28b797c55a36b2a152)
  `request@2.63.0`: Use `application/json` as the default content type when
  making `json` requests.
  ([@simov](https://github.com/simov))

### v2.14.5 (2015-09-17):

#### NPM IS DEAD. LONG LIVE NPM

That's right folks. As of this week, `npm@next` is `npm@3`, which means it'll be
`npm@latest` next week! There's some really great shiny new things over there,
and you should really take a look.

Many kudos to [@iarna](https://github.com/iarna) for her hard work on `npm@3`!

Don't worry, we'll keep `2.x` around for a while (as LTS), but you won't see
many, if any, new features on this end. From now on, we're going to use
`latest-2` and `next-2` as the dist tags for the `npm@2` branch.

#### OKAY THAT'S FINE CAN I DEPRECATE THINGS NOW?

Yes! Specially if you're using scoped packages. Apparently, deprecating them
never worked, but that should be better now. :)

* [`eca7b24`](https://github.com/npm/npm/commit/eca7b24de9a0090da02a93a69726f5e70ab80543)
  [#9558](https://github.com/npm/npm/issues/9558) Add tests for npm deprecate.
  ([@zkat](https://github.com/zkat))
* [`648fe16`](https://github.com/npm/npm/commit/648fe16157ef0db22395ae056d1dd4b4c1605bf4)
  [#9558](https://github.com/npm/npm/issues/9558) `npm-registry-client@7.0.7`:
  Fixes `npm deprecate` so you can actually deprecate scoped modules now (it
  never worked).
  ([@zkat](https://github.com/zkat))

#### WTF IS `node-waf`

idk. Some old thing. We don't talk about it anymore.

* [`cf1b39f`](https://github.com/npm/npm/commit/cf1b39fc95a9ffad7fba4c2fee705c53b19d1d16)
  [#9584](https://github.com/npm/npm/issues/9584) Fix ancient references to
  `node-waf` in the docs to refer to the `node-gyp` version of things.
  ([@KenanY](https://github.com/KenanY))

#### THE `graceful-fs` AND `node-gyp` SAGA CONTINUES

Last week had some sweeping `graceful-fs` upgrades, and this takes care of one
of the stragglers, as well as bumping `node-gyp`. `node@4` users might be
excited about this, or even `node@<4` users who previously had to cherry-pick a
bunch of patches to get the latest npm working.

* [`e07354f`](https://github.com/npm/npm/commit/e07354f3ff3a6be568fe950f1f825897f72912d8)
  `sha@2.0.1`: Upgraded graceful-fs!
  ([@ForbesLindesay](https://github.com/ForbesLindesay))
* [`83cb6ee`](https://github.com/npm/npm/commit/83cb6ee4045b85e565e9678ca1878877e1dc75bd)
  `node-gyp@3.0.3`
  ([@rvagg](https://github.com/rvagg))

#### DEPS! DEPS! MORE DEPS! OK STOP DEPS

* [`0d60888`](https://github.com/npm/npm/commit/0d608889615a1cb63f5f852337e955053f201aeb)
  `normalize-package-data@2.3.4`: Use an external package to check for built-in
  node modules.
  ([@sindresorhus](https://github.com/sindresorhus))
* [`79b4dac`](https://github.com/npm/npm/commit/79b4dac11f1c2d8ad5489fc3104734e1c10d4793)
  `retry@0.8.0`
  ([@tim-kos](https://github.com/tim-kos))
* [`c164941`](https://github.com/npm/npm/commit/c164941d3c792904d5b126a4fd36eefbe0699f52)
  `request@2.62.0`: node 4 added to build targets. Option initialization issues
  fixed.
  ([@simov](https://github.com/simov))
* [`0fd878a`](https://github.com/npm/npm/commit/0fd878a44d5ae303325808d1f00df4dce7549d50)
  `lru-cache@2.7.0`: Cache serialization support and fixes a cache length bug.
  ([@isaacs](https://github.com/isaacs))
* [`6a7a114`](https://github.com/npm/npm/commit/6a7a114a45b4699995d6e09164fdfd0fa1274591)
  `nock@2.12.0`
  ([@pgte](https://github.com/pgte))
* [`6b25e6d`](https://github.com/npm/npm/commit/6b25e6d2235c11f4444104db4545cb42a0267666)
  `semver@5.0.3`: Removed uglify-js dead code.
  ([@isaacs](https://github.com/isaacs))

### v2.14.4 (2015-09-10):

#### THE GREAT NODEv4 SAGA

So [Node 4 is out now](https://nodejs.org/en/blog/release/v4.0.0/) and that's
going to involve a number of things over in npm land. Most importantly, it's the
last major release that will include the `2.x` branch of npm. That also means
that `2.x` is going to go into LTS mode in the coming weeks -- once `npm@3`
becomes our official `latest` release. You can most likely expect Node 5 to
include `npm@3` by default, whenever that happens. We'll go into more detail
about LTS at that point, as well, so keep your eyes peeled for announcements!

#### NODE IS DEAD. LONG LIVE NODE!

Node 4 being released means that a few things that used to be floating patches
are finally making it right into npm proper. This week, we've got two such
updates, both to dependencies:

* [`505d9e4`](https://github.com/npm/npm/commit/505d9e40c13b8b0bb3f70ee9886f7b73ba569407)
  `node-gyp@3.0.1`: Support for node nightlies and compilation for both node and
  io.js without extra patching
  ([@rvagg](https://github.com/rvagg))

[@thefourtheye](https://github.com/thefourtheye) was kind enough to submit a
*bunch* of PRs to npm's dependencies updating them to `graceful-fs@4.1.2`, which
mainly makes it so we're no longer monkey-patching `fs`. The following are all
updates related to this:

* [`10cb189`](https://github.com/npm/npm/commit/10cb189c773fef804214018d57509cc7a943184b)
  `write-file-atomic@1.1.3`
  ([@thefourtheye](https://github.com/thefourtheye))
* [`edfb80b`](https://github.com/npm/npm/commit/edfb80b39f8cfce9a993f139eb98248001198e09)
  `tar@2.2.1`
  ([@thefourtheye](https://github.com/thefourtheye))
* [`aa6e1ee`](https://github.com/npm/npm/commit/aa6e1eede7d71fa69d7256afdfbaa3406bc39a5b)
  `read-package-json@2.0.1`
  ([@thefourtheye](https://github.com/thefourtheye))
* [`18971a3`](https://github.com/npm/npm/commit/18971a361635ed3958ecd39b63930ae1e56f8612)
  `read-installed@4.0.3`
  ([@thefourtheye](https://github.com/thefourtheye))
* [`a4cba71`](https://github.com/npm/npm/commit/a4cba71bd2532236fda7385bf55e8790cafd4f0a)
  `fstream@1.0.8`
  ([@thefourtheye](https://github.com/thefourtheye))
* [`70a38e2`](https://github.com/npm/npm/commit/70a38e29418951ac61ab6cf269d188074fe8ac3a)
  `fs-write-stream-atomic@1.0.4`
  ([@thefourtheye](https://github.com/thefourtheye))
* [`9cbd20f`](https://github.com/npm/npm/commit/9cbd20f691e37960e4ba12d401abd1069657cb47)
  `fs-vacuum@1.2.7`
  ([@thefourtheye](https://github.com/thefourtheye))

#### OTHER PATCHES

* [`c4dd521`](https://github.com/npm/npm/commit/c4dd5213b2f3283ea0392845e5f78cac4573529e)
  [#9506](https://github.com/npm/npm/issues/9506) Make `npm link` work on
  Windows when using node pre-release/RC releases.
  ([@jon-hall](https://github.com/jon-hall))
* [`b6bc29c`](https://github.com/npm/npm/commit/b6bc29c1401b3d6b570c09cbef1866bdb0436b59)
  [#9544](https://github.com/npm/npm/issues/9549) `process.binding` is being
  deprecated, so our only direct usage has been removed.
  ([@ChALkeR](https://github.com/ChALkeR))

#### MORE DEPENDENCIES!

* [`d940594`](https://github.com/npm/npm/commit/d940594e479a7f012b6dd6952e8ef985ba2a6216)
  `tap@1.4.1`
  ([@isaacs](https://github.com/isaacs))
* [`ee38486`](https://github.com/npm/npm/commit/ee3848669331fd98879a3175789d963543f67ce3)
  `which@1.1.2`: Added tests for Windows-related dead code that was previously
  helping a silent failure happen.  Travis stuff, too.
  ([@isaacs](https://github.com/isaacs))

#### DOC UPDATES

* [`475daf5`](https://github.com/npm/npm/commit/475daf54ad07777938d1d7ee1a3e576961e84510)
  [#9492](https://github.com/npm/npm/issues/9492) Clarify how `.npmignore` and
  `.gitignore` are found and used by npm.
  ([@addaleax](https://github.com/addaleax))
* [`b2c391d`](https://github.com/npm/npm/commit/b2c391d7833249626a6d7650363a83bcc778717a)
  `nopt@3.0.4`: Minor clarifications to docs about how array and errors work.
  ([@zkat](https://github.com/zkat))

### v2.14.3 (2015-09-03):

#### TEAMS AND ORGS STILL BETA. CLI CODE STILL SOLID.

Our closed beta for Teens and Orcs is happening! The web team is hard at work
making sure everything looks pretty and usable and such. Once we fix things
stemming from that beta, you can expect the feature to be available publicly.
Some time after that, it'll even be available for free for FOSS orgs. It'll Be
Done When It's Done™.

#### OH GOOD, I CAN ACTUALLY UPSTREAM NOW

Looks like last week's release foiled our own test suite when trying to upstream
it to Node! Just a friendly reminder that no, `.npmrc` is no longer included
then you pack/release a package! [@othiym23](https://github.com/othiym23) and
[@isaacs](https://github.com/isaacs) managed to suss the really strange test
failures resulting from that, and we've patched it in this release.

* [`01a3428`](https://github.com/npm/npm/commit/01a3428534b754dca89a56fd1e49f55cb22f6f25)
  [#9476](https://github.com/npm/npm/issues/9476) test: Recreate missing
  `.npmrc` files when missing so downstream packagers can run tests on packed
  npm.
  ([@othiym23](https://github.com/othiym23))

#### TALKING ABOUT THE CHANGELOG IN THE CHANGELOG IS LIKE, POMO OR SOMETHING

* [`c1e7a83`](https://github.com/npm/npm/commit/c1e7a83c0ae7aadf01aecc57cf8a0ae2009d4da8)
  [#9431](https://github.com/npm/npm/issues/9431) CHANGELOG: clarify
  windows-related nature of patch
  ([@saper](https://github.com/saper))

#### devDependencies UPDATED

No actual dep updates this week, but we're bumping a couple of devDeps:

* [`8454835`](https://github.com/npm/npm/commit/84548351bfd63e3e305d195abbcad24c6b7c3e8e)
  `tap@1.4.0`: Add `t.contains()` as alias to `t.match()`
  ([@isaacs](https://github.com/isaacs))
* [`13d2216`](https://github.com/npm/npm/commit/13d22161bcdeb6e1ed095d5ba2f77e6abfffa5eb)
  `deep-equal@1.0.1`: Make `null == undefined` in non-strict mode
  ([@isaacs](https://github.com/isaacs))

### v2.14.2 (2015-08-27):

#### GETTING THAT PESKY `preferGlobal` WARNING RIGHT

So apparently the `preferGlobal` option hasn't quite been warning correctly for
some time. But now it should be all better! tl;dr: if you try and install a
dependency with `preferGlobal: true`, and it's _not already_ in your
`package.json`, you'll get a warning that the author would really rather you
install it with `--global`. This should prevent Windows PowerShell from thinking
npm has failed just because of a benign warning.

* [`bbb25f3`](https://github.com/npm/npm/commit/bbb25f30d582f8979168c79233a9f8f840974f90)
  [#8841](https://github.com/npm/npm/issues/8841)
  [#9409](https://github.com/npm/npm/issues/9409) The `preferGlobal`
  warning shouldn't happen if the dependency being installed is listed in
  `devDependencies`. ([@saper](https://github.com/saper))
* [`222fcec`](https://github.com/npm/npm/commit/222fcec85ccd30d35899e5037079fb14625af4e2)
  [#9409](https://github.com/npm/npm/issues/9409) `preferGlobal` now prints a
  warning when there are no dependencies for the current package.
  ([@zkat](https://github.com/zkat))
* [`5cfed6d`](https://github.com/npm/npm/commit/5cfed6d7a1a5f2731688cfc8293b5e43a6355393)
  [#9409](https://github.com/npm/npm/issues/9409) Verify that
  `preferGlobal` is warning as expected (when a `preferGlobal` dependency is
  installed, but isn't listed in either `dependencies` or `devDependencies`).
  ([@zkat](https://github.com/zkat))

#### BUMP +1

* [`eeafce2`](https://github.com/npm/npm/commit/eeafce2d06883c0f51bf403415b6bc5f2647eba3)
  `validate-npm-package-license@3.0.1`: Include additional metadata in parsed license object,
  useful for license checkers. ([@kemitchell](https://github.com/kemitchell))
* [`1502a28`](https://github.com/npm/npm/commit/1502a285f84aa548806b3eafc8889e6288e810f3)
  `normalise-package-data@2.3.2`: Updated to use `validate-npm-package-license@3.0.1`.
  ([@othiym23](https://github.com/othiym23))
* [`cbde823`](https://github.com/npm/npm/commit/cbde8233436bf0ea62a4740869b4990322c20659)
  `init-package-json@1.9.1`: Add a `silent` option to suppress output on writing the
  generated `package.json`. Also, updated to use `validate-npm-package-license@3.0.1`.
  ([@zkat](https://github.com/zkat))
* [`08fda46`](https://github.com/npm/npm/commit/08fda465452b4d77f1ced8050ee3a35a77fc30a5)
  `tar@2.2.0`: Minor improvements. ([@othiym23](https://github.com/othiym23))
* [`dc2f20b`](https://github.com/npm/npm/commit/dc2f20b53fff77203139c863b48da0e959df2ac9)
  `rimraf@2.4.3`: `EPERM` now triggers a delay / retry loop (since Windows throws
  this when things still hold a handle). ([@isaacs](https://github.com/isaacs))
* [`e8acb27`](https://github.com/npm/npm/commit/e8acb273aa67ee0394d0431650e1b2a7d09c8554)
  `read@1.0.7`: Fix licensing ambiguity. ([@isaacs](https://github.com/isaacs))

#### OTHER STUFF THAT'S RELEVANT

* [`73a1ee0`](https://github.com/npm/npm/commit/73a1ee0be90fa1928521b63f28bef83b8ffab61d)
  [#9386](https://github.com/npm/npm/issues/9386) Include additional unignorable files in
  documentation.
  ([@mjhasbach](https://github.com/mjhasbach))
* [`0313e40`](https://github.com/npm/npm/commit/0313e40ee0f757fce8861be590ad668c23d7be53)
  [#9396](https://github.com/npm/npm/issues/9396) Improve the `EISDIR` error
  message returned by npm's error-handling code to give users a better hint of
  what's most likely going on.  Usually, error reports with this error code are
  about people trying to install things without a `package.json`.
  ([@KenanY](https://github.com/KenanY))
* [`2677457`](https://github.com/npm/npm/commit/26774579c739c5951351e58263cf4d6ea3d66ec8)
  [#9360](https://github.com/npm/npm/issues/9360) Make it easier to run
  only _some_ of npm tests with lifecycle scripts via `npm tap test/tap/testname.js`.
  ([@iarna](https://github.com/iarna))

### v2.14.1 (2015-08-20):

#### SECURITY FIX

There are patches for two information leaks of moderate severity in `npm@2.14.1`:

1. In some cases, npm was leaking sensitive credential information into the
   child environment when running package and lifecycle scripts. This could
   lead to packages being published with files (most notably `config.gypi`, a
   file created by `node-gyp` that is a cache of environmental information
   regenerated on every run) containing the bearer tokens used to authenticate
   users to the registry. Users with affected packages have been notified (and
   the affected tokens invalidated), and now npm has been modified to not
   upload files that could contain this information, as well as scrubbing the
   sensitive information out of the environment passed to child scripts.
2. Per-package `.npmrc` files are used by some maintainers as a way to scope
   those packages to a specific registry and its credentials. This is a
   reasonable use case, but by default `.npmrc` was packed into packages,
   leaking those credentials.  npm will no longer include `.npmrc` when packing
   tarballs.

If you maintain packages and believe you may be affected by either
of the above scenarios (especially if you've received a security
notification from npm recently), please upgrade to `npm@2.14.1` as
soon as possible. If you believe you may have inadvertently leaked
your credentials, upgrade to `npm@2.14.1` on the affected machine,
and run `npm logout` and then `npm login`. Your access tokens will be
invalidated, which will eliminate any risk posed by tokens inadvertently
included in published packages. We apologize for the inconvenience this
causes, as well as the oversight that led to the existence of this issue
in the first place.

Huge thanks to [@ChALkeR](https://github.com/ChALkeR) for bringing these
issues to our attention, and for helping us identify affected packages
and maintainers. Thanks also to the Node.js security working group for
their coördination with the team in our response to this issue. We
appreciate everybody's patience and understanding tremendously.

* [`b9474a8`](https://github.com/npm/npm/commit/b9474a843ca55b7c5fac6da33989e8eb39aff8b1)
  `fstream-npm@1.0.5`: Stop publishing build cruft (`config.gypi`) and per-project
  `.npmrc` files to keep local configuration out of published packages.
  ([@othiym23](https://github.com/othiym23))
* [`13c286d`](https://github.com/npm/npm/commit/13c286dbdc3fa8fec4cb79fc4d1ee505c8a41b2e)
  [#9348](https://github.com/npm/npm/issues/9348) Filter "private"
  (underscore-prefixed, even when scoped to a registry) configuration values
  out of child environments. ([@othiym23](https://github.com/othiym23))

#### BETTER WINDOWS INTEGRATION, ONE STEP AT A TIME

* [`e40e71f`](https://github.com/npm/npm/commit/e40e71f2f838a8a42392f44e3eeec04e323ab743)
  [#6412](https://github.com/npm/npm/issues/6412) Improve the search strategy
  used by the npm shims for Windows to prioritize your own local npm installs.
  npm has really needed this tweak for a long time, so hammer on it and let us
  know if you run into issues, but with luck it will Just Work.
  ([@joaocgreis](https://github.com/joaocgreis))
* [`204ebbb`](https://github.com/npm/npm/commit/204ebbb3e0cab696a429a878ceeb4a7e78ec2b94)
  [#8751](https://github.com/npm/npm/issues/8751)
  [#7333](https://github.com/npm/npm/issues/7333) Keep [autorun
  scripts](https://technet.microsoft.com/en-us/sysinternals/bb963902.aspx) from
  interfering with npm package and lifecycle script execution on Windows by
  adding `/d` and `/s` when invoking `cmd.exe`.
  ([@saper](https://github.com/saper))

#### IT SEEMED LIKE AN IDEA AT THE TIME

* [`286f3d9`](https://github.com/npm/npm/commit/286f3d97103812f0fd84b70352addbe899e258f9)
  [#9201](https://github.com/npm/npm/pull/9201) For a while npm was building
  HTML partials for use on [`docs.npmjs.com`](https://docs.npmjs.com), but we
  weren't actually using them. Stop building them, which makes running the full
  test suite and installation process around a third faster.
  ([@isaacs](https://github.com/isaacs))

#### A SINGLE LONELY DEPENDENCY UPGRADE

* [`b343b95`](https://github.com/npm/npm/commit/b343b956ef777e321e4251ddc96ec6d80827d9e2)
  `request@2.61.0`: Bug fixes and keep-alive tweaks.
  ([@simov](https://github.com/simov))

### v2.14.0 (2015-08-13):

#### IT'S HERE! KINDA!

This release adds support for teens and orcs (err, teams and organizations) to
the npm CLI! Note that the web site and registry-side features of this are
still not ready for public consumption.

A beta should be starting in the next couple of weeks, and the features
themselves will become public once all that's done. Keep an eye out for more
news!

All of these changes were done under [`#9011`](https://github.com/npm/npm/pull/9011):

* [`6424170`](https://github.com/npm/npm/commit/6424170fc17c666a6efc090370ec691e0cab1792)
  Added new `npm team` command and subcommands.
  ([@zkat](https://github.com/zkat))
* [`52220d1`](https://github.com/npm/npm/commit/52220d146d474ec29b683bd99c06f75cbd46a9f4)
  Added documentation for new `npm team` command.
  ([@zkat](https://github.com/zkat))
* [`4e66830`](https://github.com/npm/npm/commit/4e668304850d02df8eb27a779fda76fe5de645e7)
  Updated `npm access` to support teams and organizations.
  ([@zkat](https://github.com/zkat))
* [`ea3eb87`](https://github.com/npm/npm/commit/ea3eb8733d9fa09ce34106b1b19fb1a8f95844a5)
  Gussied up docs for `npm access` with new commands.
  ([@zkat](https://github.com/zkat))
* [`6e0b431`](https://github.com/npm/npm/commit/6e0b431c1de5e329c86e57d097aa88ebfedea864)
  Fix up `npm whoami` to make the underlying API usable elsewhere.
  ([@zkat](https://github.com/zkat))
* [`f29c931`](https://github.com/npm/npm/commit/f29c931012ce5ccd69c29d83548f27e443bf7e62)
  `npm-registry-client@7.0.1`: Upgrade `npm-registry-client` API to support
  `team` and `access` calls against the registry.
  ([@zkat](https://github.com/zkat))

#### A FEW EXTRA VERSION BUMPS

* [`c977e12`](https://github.com/npm/npm/commit/c977e12cbfa50c2f52fc807f5cc19ba1cc1b39bf)
  `init-package-json@1.8.0`: Checks for some `npm@3` metadata.
  ([@iarna](https://github.com/iarna))
* [`5c8c9e5`](https://github.com/npm/npm/commit/5c8c9e5ae177ba7d0d298cfa42f3fc7f0271e4ec)
  `columnify@1.5.2`: Updated some dependencies.
  ([@timoxley](https://github.com/timoxley))
* [`5d56742`](https://github.com/npm/npm/commit/5d567425768b75aeab402c817a53d8b2bc60d8de)
  `chownr@1.0.1`: Tests, docs, and minor style nits.
  ([@isaacs](https://github.com/isaacs))

#### ALSO A DOC FIX

* [`846fcc7`](https://github.com/npm/npm/commit/846fcc79b86984b109a97366b0422f995a45f8bf)
  [`#9200`](https://github.com/npm/npm/pull/9200) Remove single quotes
  around semver range, thus making it valid semver.
  ([@KenanY](https://github.com/KenanY))

### v2.13.5 (2015-08-07):

This is another quiet week for the `npm@2` release.
[@zkat](https://github.com/zkat) has been working hard on polishing the CLI
bits of the registry's new feature to support direct management of teams and
organizations, and [@iarna](https://github.com/iarna) continues to work through
the list of issues blocking the general release of `npm@3`, which is looking
more and more solid all the time.

[@othiym23](https://github.com/othiym23) and [@zkat](https://github.com/zkat)
have also been at this week's Node.js / io.js [collaborator
summit](https://github.com/nodejs/summit/tree/master), both as facilitators and
participants. This is a valuable opportunity to get some face time with other
contributors and to work through a bunch of important discussions, but it does
leave us feeling kind of sleepy. Running meetings is hard!

What does that leave for this release? A few of the more tricky bug fixes that
have been sitting around for a little while now, and a couple dependency
upgrades. Nothing too fancy, but most of these were contributed by developers
like _you_, which we think is swell. Thanks!

#### BUG FIXES

* [`d7271b8`](https://github.com/npm/npm/commit/d7271b8226712479cdd339bf85faf7e394923e0d)
  [#4530](https://github.com/npm/npm/issues/4530) The bash completion script
  for npm no longer alters global completion behavior around word breaks.
  ([@whitty](https://github.com/whitty))
* [`c9ce294`](https://github.com/npm/npm/commit/c9ce29415a0a8fc610690b6e9d91b64d6e36cfcc)
  [#7198](https://github.com/npm/npm/issues/7198) When setting up dependencies
  to be shared via `npm link <package>`, only run the lifecycle scripts during
  the original link, not when running `npm link <package>` or `npm install
  --link` against them. ([@murgatroid99](https://github.com/murgatroid99))
* [`422da66`](https://github.com/npm/npm/commit/422da664bd3ce71313da447f170507faf5aac46a)
  [#9108](https://github.com/npm/npm/issues/9108) Clear up minor confusion
  around wording in `bundledDependencies` section of `package.json` docs.
  ([@derekpeterson](https://github.com/derekpeterson))
* [`6b42d99`](https://github.com/npm/npm/commit/6b42d99460885e715772d3487b1c548d2bc8a738)
  [#9146](https://github.com/npm/npm/issues/9146) Include scripts that run for
  `preversion`, `version`, and `postversion` in the section for lifecycle
  scripts rather than the generic `npm run-script` output.
  ([@othiym23](https://github.com/othiym23))

#### NOPE, NOT DONE WITH DEPENDENCY UPDATES

* [`91a48bb`](https://github.com/npm/npm/commit/91a48bb5ef5a990781c86f8b69b8a32cf4fac2d9)
  `chmodr@1.0.1`: Ignore symbolic links when recursively changing mode, just
  like the Unix command. ([@isaacs](https://github.com/isaacs))
* [`4bbc86e`](https://github.com/npm/npm/commit/4bbc86e3825e2eee9a8758ba26bdea0cb6a2581e)
  `nock@2.10.0` ([@pgte](https://github.com/pgte))

### v2.13.4 (2015-07-30):

#### JULY ENDS ON A FAIRLY QUIET NOTE

Hey everyone! I hope you've had a great week. We're having a fairly small
release this week while we wrap up Teams and Orgs (or, as we've taken to calling
it internally, _Teens and Orcs_).

In other exciting news, a bunch of us are gonna be at the [Node.js Collaborator
Summit](https://github.com/nodejs/summit/issues/1), and you can also find us at
[wafflejs](https://wafflejs.com/) on Wednesday. Hopefully we'll be seeing some
of you there. :)

#### THE PATCH!!!

So here it is. The patch. Hope it helps. (Thanks,
[@ktarplee](https://github.com/ktarplee)!)

* [`2e58c48`](https://github.com/npm/npm/commit/2e58c4819e3cafe4ae23ab7f4a520fe09258cfd7)
  [#9033](https://github.com/npm/npm/pull/9033) `npm version` now works on git
  submodules
  ([@ktarplee](https://github.com/ktarplee))

#### OH AND THERE'S A DEV DEPENDENCIES UPDATE

Hooray.

* [`d204683`](https://github.com/npm/npm/commit/d2046839d471322e61e3ceb0f00e78e5c481f967)
  `nock@2.9.1`
  ([@pgte](https://github.com/pgte))

### v2.13.3 (2015-07-23):

#### I'M SAVING THE GOOD JOKES FOR MORE INTERESTING RELEASES

It's pretty hard to outdo last week's release buuuuut~ I promise I'll have a
treat when we release our shiny new **Teams and Organizations** feature! :D
(Coming Soon™). It'll be a real *gem*.

That means it's a pretty low-key release this week. We got some nice
documentation tweaks, a few bugfixes, and other such things, though!

Oh, and a _bunch of version bumps_. Thanks, `semver`!

#### IT'S THE LITTLE THINGS THAT MATTER

* [`2fac6ae`](https://github.com/npm/npm/commit/2fac6aeffefba2934c3db395b525d931599c34d8)
  [#9012](https://github.com/npm/npm/issues/9012) A convenience for releases --
  using the globally-installed npm before now was causing minor annoyances, so
  we just use the exact same npm we're releasing to build the new release.
  ([@zkat](https://github.com/zkat))

#### WHAT DOES THIS BUTTON DO?

There's a couple of doc updates! The last one might be interesting.

* [`4cd3205`](https://github.com/npm/npm/commit/4cd32050c0f89b7f1ae486354fa2c35eea302ba5)
  [#9002](https://github.com/npm/npm/issues/9002) Updated docs to list the
  various files that npm automatically includes and excludes, regardless of
  settings.
  ([@SimenB](https://github.com/SimenB))
* [`cf09e75`](https://github.com/npm/npm/commit/cf09e754931739af32647d667b671e72a4c79081)
  [#9022](https://github.com/npm/npm/issues/9022) Document the `"access"` field
  in `"publishConfig"`. Did you know you don't need to use `--access=public`
  when publishing scoped packages?! Just put it in your `package.json`!
  Go refresh yourself on scopes packages by [checking our docs](https://docs.npmjs.com/getting-started/scoped-packages) on them.
  ([@boennemann](https://github.com/boennemann))
* [`bfd73da`](https://github.com/npm/npm/commit/bfd73da33349cc2afb8278953b2ae16ea95023de)
  [#9013](https://github.com/npm/npm/issues/9013) fixed typo in changelog
  ([@radarhere](https://github.com/radarhere))

#### THE SEMVER MAJOR VERSION APOCALYPSE IS UPON US

Basically, `semver` is up to `@5`, and that meant we needed to go in an update a
bunch of our dependencies manually. `node-gyp` is still pending update, since
it's not ours, though!

* [`9232e58`](https://github.com/npm/npm/commit/9232e58d54c032c23716ef976023d36a42bfdcc9)
  [#8972](https://github.com/npm/npm/issues/8972) `init-package-json@1.7.1`
  ([@othiym23](https://github.com/othiym23))
* [`ba44f6b`](https://github.com/npm/npm/commit/ba44f6b4201a4faee025341b123e372d8f45b6d9)
  [#8972](https://github.com/npm/npm/issues/8972) `normalize-package-data@2.3.1`
  ([@othiym23](https://github.com/othiym23))
* [`3901d3c`](https://github.com/npm/npm/commit/3901d3cf191880bb4420b1d6b8aedbcd8fc26cdf)
  [#8972](https://github.com/npm/npm/issues/8972) `npm-install-checks@1.0.6`
  ([@othiym23](https://github.com/othiym23))
* [`ffcc7dd`](https://github.com/npm/npm/commit/ffcc7dd12f8bb94ff0f64c465c57e460b3f24a24)
  [#8972](https://github.com/npm/npm/issues/8972) `npm-package-arg@4.0.2`
  ([@othiym23](https://github.com/othiym23))
* [`7128f9e`](https://github.com/npm/npm/commit/7128f9ec10c0c8482087511b716dbddb54249626)
  [#8972](https://github.com/npm/npm/issues/8972) `npm-registry-client@6.5.1`
  ([@othiym23](https://github.com/othiym23))
* [`af28911`](https://github.com/npm/npm/commit/af28911ecd54a844f848c6ae41887097d6aa2f3b)
  [#8972](https://github.com/npm/npm/issues/8972) `read-installed@4.0.2`
  ([@othiym23](https://github.com/othiym23))
* [`3cc817a`](https://github.com/npm/npm/commit/3cc817a0f34f698b580ff6ff02308700efc54f7c)
  [#8972](https://github.com/npm/npm/issues/8972) node-gyp needs its own version
  of semver
  ([@othiym23](https://github.com/othiym23))
* [`f98eccc`](https://github.com/npm/npm/commit/f98eccc6e3a6699ca0aa9ecbad93a3b995583871)
  [#8972](https://github.com/npm/npm/issues/8972) `semver@5.0.1`: Stop including
  browser builds.
  ([@isaacs](https://github.com/isaacs))

#### \*BUMP\*

And some other version bumps for good measure.

* [`254ecfb`](https://github.com/npm/npm/commit/254ecfb04f026c2fd16427db01a53600c1892c8b)
  [#8990](https://github.com/npm/npm/issues/8990) `marked-man@0.1.5`: Fixes an
  issue with documentation rendering where backticks in 2nd-level headers would
  break rendering (?!?!)
  ([@steveklabnik](https://github.com/steveklabnik))
* [`79efd79`](https://github.com/npm/npm/commit/79efd79ac216da8cee8636fb2ed926b0196a4eb6)
  `minimatch@2.0.10`: A pattern like `'*.!(x).!(y)'` should not match a name
  like `'a.xyz.yab'`.
  ([@isaacs](https://github.com/isaacs))
* [`39c7dc9`](https://github.com/npm/npm/commit/39c7dc9a4e17cd35a5ed882ba671821c9a900f9e)
  `request@2.60.0`: A few bug fixes and doc updates.
  ([@simov](https://github.com/simov))
* [`72d3c3a`](https://github.com/npm/npm/commit/72d3c3a9e1e461608aa21b14c01a650333330da9)
  `rimraf@2.4.2`: Minor doc and dep updates
  ([@isaacs](https://github.com/isaacs))
* [`7513035`](https://github.com/npm/npm/commit/75130356a06f5f4fbec3786aac9f9f0b36dfe010)
  `nock@2.9.1`
  ([@pgte](https://github.com/pgte))
* [`3d9aa82`](https://github.com/npm/npm/commit/3d9aa82260f0643a32c13d0c1ed16f644b6fd4ab)
  Fixes this thing where Kat decided to save `nock` as a regular dependency ;)
  ([@othiym23](https://github.com/othiym23))

### v2.13.2 (2015-07-16):

#### HOLD ON TO YOUR TENTACLES... IT'S NPM RELEASE TIME!

Kat: Hooray! Full team again, and we've got a pretty small patch  release this
week, about everyone's favorite recurring issue: git URLs!

Rebecca: No Way! Again?

Kat: The ride never ends! In the meantime, there's some fun, exciting work in
the background to get orgs and teams out the door. Keep an eye out for news. :)

Rebecca: And make sure to keep an eye out for patches for the super-fresh
`npm@3`!

#### LET'S GIT INKY

Rebecca: So what's this about another git URL issue?

Kat: Welp, I apparently broke backwards-compatibility on what are actually
invalid `git+https` URLs! So I'm making it work, but we're gonna deprecate URLs
that look like `git+https://user@host:path/is/here`.

Rebecca: What should we use instead?!

Kat: Just do me a solid and use `git+ssh://user@host:path/here` or
`git+https://user@host/absolute/https/path` instead!

* [`769f06e`](https://github.com/npm/npm/commit/769f06e5455d7a9fc738379de2e05868df0dab6f)
  Updated tests for `getResolved` so the URLs are run through
  `normalize-git-url`.
  ([@zkat](https://github.com/zkat))
* [`edbae68`](https://github.com/npm/npm/commit/edbae685bf48971e878ced373d6825fc1891ee47)
  [#8881](https://github.com/npm/npm/issues/8881) Added tests to verify that `git+https:` URLs are handled compatibly.
  ([@zkat](https://github.com/zkat))

#### NEWS FLASH! DOCUMENTATION IMPROVEMENTS!

* [`bad4e014`](https://github.com/npm/npm/commit/bad4e0143cc95754a682f1da543b2b4e196e924b)
  [#8924](https://github.com/npm/npm/pull/8924) Make sure documented default
  values in `lib/cache.js` properly correspond to current code.
  ([@watilde](https://github.com/watilde))
* [`e7a11fd`](https://github.com/npm/npm/commit/e7a11fdf70e333cdfe3dac94a1a30907adb76d59)
  [#8036](https://github.com/npm/npm/issues/8036) Clarify the documentation for
  `.npmrc` to clarify that it's not read at the project level when doing global
  installs.
  ([@espadrine](https://github.com/espadrine))

#### STAY FRESH~

Kat: That's it for npm core changes!

Rebecca: Great! Let's look at the fresh new dependencies, then!

Kat: See you all next week!

Both: Stay Freeesh~

(some cat form of Forrest can be seen snoring in the corner)

* [`bfa1f45`](https://github.com/npm/npm/bfa1f45ee760d05039557d2245b7e3df9fda8def)
  `normalize-git-url@3.0.1`: Fixes url normalization such that `git+https:`
  accepts scp syntax, but get converted into absolute-path `https:` URLs. Also
  fixes scp syntax so you can have absolute paths after the `:`
  (`git@myhost.org:/some/absolute/place.git`)
  ([@zkat](https://github.com/zkat))
* [`6f757d2`](https://github.com/npm/npm/6f757d22b53f91da0bebec6b5d16c1f4dbe130b4)
  `glob@5.0.15`: Better handling of ENOTSUP
  ([@isaacs](https://github.com/isaacs))
* [`0920819`](https://github.com/npm/npm/09208197fb8b0c6d5dbf6bd7f59970cf366de989)
  `node-gyp@2.0.2`: Fixes an issue with long paths on Win32
  ([@TooTallNate](https://github.com/TooTallNate))

### v2.13.1 (2015-07-09):

#### KAUAI WAS NICE. I MISS IT.

But Forrest's still kinda on vacation, and not just mentally, because he's
hanging out with the fine meatbags at CascadiaFest. Enjoy this small bug
release.

#### MAKE OURSELVES HAPPY

* [`40981f2`](https://github.com/npm/npm/commit/40981f2e0c9c12bb003ccf188169afd1d201f5af)
  [#8862](https://github.com/npm/npm/issues/8862) Make the lifecycle's safety
  check work with scoped packages. ([@tcort](https://github.com/tcort))
* [`5125856`](https://github.com/npm/npm/commit/512585622481dbbda9a0306932468d59efaff658)
  [#8855](https://github.com/npm/npm/issues/8855) Make dependency versions of
  `"*"` match `"latest"` when all versions are prerelease.
  ([@iarna](https://github.com/iarna))
* [`22fdc1d`](https://github.com/npm/npm/commit/22fdc1d52602ba7098af978c75fca8f7d1060141)
  Visually emphasize the correct way to write lifecycle scripts.
  ([@josh-egan](https://github.com/josh-egan))

#### MAKE TRAVIS HAPPY

* [`413c3ac`](https://github.com/npm/npm/commit/413c3ac2ab2437f3011c6ca0d1630109ec14e604)
  Use npm's `2.x` branch for testing its `2.x` branch.
  ([@iarna](https://github.com/iarna))
* [`7602f64`](https://github.com/npm/npm/commit/7602f64826f7a465d9f3a20bd87a376d992607e6)
  Don't prompt for GnuPG passphrase in version lifecycle tests.
  ([@othiym23](https://github.com/othiym23))

#### MAKE `npm outdated` HAPPY

* [`d338668`](https://github.com/npm/npm/commit/d338668601d1ebe5247a26237106e80ea8cd7f48)
  [#8796](https://github.com/npm/npm/issues/8796) `fstream-npm@1.0.4`: When packing the
  package tarball, npm no longer crashes for packages with certain combinations of
  `.npmignore` entries, `.gitignore` entries, and lifecycle scripts.
  ([@iarna](https://github.com/iarna))
* [`dbe7c9c`](https://github.com/npm/npm/commit/dbe7c9c74734be870d16dd61b9e7f746123011f6)
  `nock@2.7.0`: Add matching based on query strings.
  ([@othiym23](https://github.com/othiym23))

There are new versions of `strip-ansi` and `ansi-regex`, but npm only uses them
indirectly, so we pushed them down into their dependencies where they can get
updated at their own pace.

* [`06b6ca5`](https://github.com/npm/npm/commit/06b6ca5b5333025f10c8d901628859bd4678e027)
  undeduplicate `ansi-regex` ([@othiym23](https://github.com/othiym23))
* [`b168e33`](https://github.com/npm/npm/commit/b168e33ad46faf47020a45f72ba8cec8c644bdb9)
  undeduplicate `strip-ansi` ([@othiym23](https://github.com/othiym23))

### v2.13.0 (2015-07-02):

#### FORREST IS OUT! LET'S SNEAK IN ALL THE THINGS!

Well, not _everything_. Just a couple of goodies, like the new `npm ping`
command, and the ability to add files to the commits created by `npm version`
with the new version hooks. There's also a couple of bugfixes in `npm` itself
and some of its dependencies. Here we go!

#### YES HELLO THIS IS NPM REGISTRY SORRY NO DOG HERE

Yes, that's right! We now have a dedicated `npm ping` command. It's super simple
and super easy. You ping. We tell you whether you pinged right by saying hello
right back. This should help out folks dealing with things like proxy issues or
other registry-access debugging issues. Give it a shot!

This addresses [#5750](https://github.com/npm/npm/issues/5750), and will help
with the `npm doctor` stuff described in
[#6756](https://github.com/npm/npm/issues/6756).

* [`f1f7a85`](https://github.com/npm/npm/commit/f1f7a85)
  Add ping command to CLI
  ([@michaelnisi](https://github.com/michaelnisi))
* [`8cec629`](https://github.com/npm/npm/commit/8cec629)
  Add ping command to npm-registry-client
  ([@michaelnisi](https://github.com/michaelnisi))
* [`0c0c92d`](https://github.com/npm/npm/0c0c92d)
  Fixed ping command issues (added docs, tests, fixed minor bugs, etc)
  ([@zkat](https://github.com/zkat))

#### I'VE WANTED THIS FOR `version` SINCE LIKE LITERALLY FOREVER AND A DAY

Seriously! This patch lets you add files to the `version` commit before it's
made, So you can add additional metadata files, more automated changes to
`package.json`, or even generate `CHANGELOG.md` automatically pre-commit if
you're into that sort of thing. I'm so happy this is there I can't even. Do you
have other fun usecases for this? Tell
[npmbot (@npmjs)](http://twitter.com/npmjs) about it!

* [`582f170`](https://github.com/npm/npm/commit/582f170)
  [#8620](https://github.com/npm/npm/issues/8620) version: Allow scripts to add
  files to the commit.
  ([@jamestalmage](https://github.com/jamestalmage))

#### ALL YOUR FILE DESCRIPTORS ARE BELONG TO US

We've had problems in the past with things like `EMFILE` errors popping up when
trying to install packages with a bunch of dependencies. Isaac patched up
[`graceful-fs`](https://github.com/isaacs/node-graceful-fs) to handle this case
better, so we should be seeing fewer of those.

* [`022691a`](https://github.com/npm/npm/commit/022691a)
  `graceful-fs@4.1.2`: Updated so we can monkey patch globally.
  ([@isaacs](https://github.com/isaacs))
* [`c9fb0fd`](https://github.com/npm/npm/commit/c9fb0fd)
  Globally monkey-patch graceful-fs. This should fix some errors when installing
  packages with lots of dependencies.
  ([@isaacs](https://github.com/isaacs))

#### READ THE FINE DOCS. THEY'VE IMPROVED

* [`5587d0d`](https://github.com/npm/npm/commit/5587d0d)
  Nice clarification for `directories.bin`
  ([@ujane](https://github.com/ujane))
* [`20673c7`](https://github.com/npm/npm/commit/20673c7)
  Hey, Windows folks! Check out
  [`nvm-windows`](https://github.com/coreybutler/nvm-windows)
  ([@ArtskydJ](https://github.com/ArtskydJ))

#### MORE NUMBERS! MORE VALUE!

* [`5afa2d5`](https://github.com/npm/npm/commit/5afa2d5)
  `validate-npm-package-name@2.2.2`: Documented package name rules in README
  ([@zeusdeux](https://github.com/zeusdeux))
* [`021f4d9`](https://github.com/npm/npm/commit/021f4d9)
  `rimraf@2.4.1`: [#74](https://github.com/isaacs/rimraf/issues/74) Use async
  function for bin (to better handle Window's `EBUSY`)
  ([@isaacs](https://github.com/isaacs))
* [`5223432`](https://github.com/npm/npm/commit/5223432)
  `osenv@0.1.3`: Use `os.homedir()` polyfill for more reliable output. io.js
  added the function and the polyfill does a better job than the prior solution.
  ([@sindresorhus](https://github.com/sindresorhus))
* [`8ebbc90`](https://github.com/npm/npm/commit/8ebbc90)
  `npm-cache-filename@1.0.2`: Make sure different git references get different
  cache folders. This should prevent `foo/bar#v1.0` and `foo/bar#master` from
  sharing the same cache folder.
  ([@tomekwi](https://github.com/tomekwi))
* [`367b854`](https://github.com/npm/npm/commit/367b854)
  `lru-cache@2.6.5`: Minor test/typo changes
  ([@isaacs](https://github.com/isaacs))
* [`9fcae61`](https://github.com/npm/npm/commit/9fcae61)
  `glob@5.0.13`: Tiny doc change + stop firing 'match' events for ignored items.
  ([@isaacs](https://github.com/isaacs))

#### OH AND ONE MORE THING

* [`7827249`](https://github.com/npm/npm/commit/7827249)
  `PeerDependencies` errors now include the package version.
  ([@NickHeiner](https://github.com/NickHeiner))

### v2.12.1 (2015-06-25):

#### HEY WHERE DID EVERYBODY GO

I keep [hearing some commotion](https://github.com/npm/npm/releases/tag/v3.0.0).
Is there something going on? Like, a party or something? Anyway, here's a small
release with at least two significant bug fixes, at least one of which some of
you have been waiting for for quite a while.

#### REMEMBER WHEN I SAID "REMEMBER WHEN I SAID THAT THING ABOUT PERMISSIONS?"?

`npm@2.12.0` has a change that introduces a fix for a permissions problem
whereby the `_locks` directory in the cache directory can up being owned by
root. The fix in 2.12.0 takes care of that problem, but introduces a new
problem for Windows users where npm tries to call `process.getuid()`, which
doesn't exist on Windows. It was easy enough to fix (but more or less
impossible to test, thanks to all the external dependencies involved with
permissions and platforms and whatnot), but as a result, Windows users might
want to skip `npm@2.12.0` and go straight to `npm@2.12.1`. Sorry about that!

* [`7e5da23`](https://github.com/npm/npm/commit/7e5da238ee869201fdb9027c27b79b0f76b440a8)
  When using the new, "fixed" cache directory creator, be extra-careful to not
  call `process.getuid()` on platforms that lack it.
  ([@othiym23](https://github.com/othiym23))

#### WHEW! ALL DONE FIXING GIT FOREVER!

New npm CLI team hero [@zkat](https://github.com/zkat) has finally (FINALLY)
fixed the regression somebody (hi!) introduced a couple months ago whereby git
URLs of the format `git+ssh://user@githost.com:org/repo.git` suddenly stopped
working, and also started being saved (and cached) incorrectly. I am 100% sure
there are absolutely no more bugs in the git caching code at all ever. Mm hm.
Yep. Pretty sure. Maybe. Hmm... I hope.

*Sighs audibly.*

[Let us know](http://github.com/npm/npm/issues/new) if we broke something else
with this fix.

* [`94ca4a7`](https://github.com/npm/npm/commit/94ca4a711619ba8e40ce3d20bc42b13cdb7611b7)
  [#8031](https://github.com/npm/npm/issues/8031) Even though
  `git+ssh://user@githost.com:org/repo.git` isn't a URL, treat it like one for
  the purposes of npm. ([@zkat](https://github.com/zkat))
* [`e7f56e5`](https://github.com/npm/npm/commit/e7f56e5a97fcf1c52d5c5bee71303b0126129815)
  [#8031](https://github.com/npm/npm/issues/8031) `normalize-git-url@2.0.0`:
  Handle git URLs (and URL-like remote refs) in a manner consistent with npm's
  docs. ([@zkat](https://github.com/zkat))

#### YEP, THERE ARE STILL DEPENDENCY UPGRADES

* [`679bf47`](https://github.com/npm/npm/commit/679bf4745ac2cfbb01c9ce273e189807fd04fa33)
  [#40](http://github.com/npm/read-installed/issues/40) `read-installed@4.0.1`:
  Handle prerelease versions in top-level dependencies not in `package.json`
  without marking those packages as invalid.
  ([@benjamn](https://github.com/benjamn))
* [`3a67410`](https://github.com/npm/npm/commit/3a6741068c9119174c920496778aeee870ebdac0)
  `tap@1.3.1` ([@isaacs](https://github.com/isaacs))
* [`151904a`](https://github.com/npm/npm/commit/151904af39dc24567f8c98529a2a64a4dbcc960a)
  `nopt@3.0.3` ([@isaacs](https://github.com/isaacs))

### v2.12.0 (2015-06-18):

#### REMEMBER WHEN I SAID THAT THING ABOUT PERMISSIONS?

About [a million people](https://github.com/npm/npm/issues?utf8=%E2%9C%93&q=is%3Aissue+EACCES+_locks)
have filed issues related to having a tough time using npm after they've run
npm once or twice with sudo. "Don't worry about it!" I said. "We've fixed all
those permissions problems ages ago! Use this one weird trick and you'll never
have to deal with this again!"

Well, uh, if you run npm with root the first time you run npm on a machine, it
turns out that the directory npm uses to store lockfiles ends up being owned by
the wrong user (almost always root), and that can, well, it can cause problems
sometimes. By which I mean every time you run npm without being root it'll barf
with `EACCES` errors. Whoops!

This is an obnoxious regression, and to prevent it from recurring, we've made
it so that the cache, cached git remotes, and the lockfile directories are all
created and maintained using the same utilty module, which not only creates the
relevant paths with the correct permissions, but will fix the permissions on
those directories (if it can) when it notices that they're broken. An `npm
install` run as root ought to be sufficient to fix things up (and if that
doesn't work, first tell us about it, and then run `sudo chown -R $(whoami)
$HOME/.npm`)

Also, I apologize for inadvertently gaslighting any of you by claiming this bug
wasn't actually a bug. I do think we've got this permanently dealt with now,
but I'll be paying extra-close attention to permissions issues related to the
cache for a while.

* [`85d1a53`](https://github.com/npm/npm/commit/85d1a53d7b5e0fc04823187e522ae3711ede61fa)
  Set permissions on lock directory to the owner of the process.
  ([@othiym23](https://github.com/othiym23))

#### I WENT TO NODECONF AND ALL I GOT WAS THIS LOUSY SPDX T-SHIRT

That's not literally true. We spent very little time discussing SPDX,
[@kemitchell](https://github.com/kemitchell) is a champ, and I had a lot of fun
playing drum & bass to a mostly empty Boogie Barn and only ended up with one
moderately severe cold for my pains. Another winner of a NodeConf! (I would
probably wear a SPDX T-shirt if somebody gave me one, though.)

A bunch of us did have a spirited discussion of the basics of open-source
intellectual property, and the convergence of me,
[@kemitchell](https://github.com/kemitchell), and
[@jandrieu](https://github.com/jandrieu) in one place allowed us to hammmer out
a small but significant issue that had been bedeviling early adopters of the
new SPDX expression syntax in `package.json` license fields: how to deal with
packages that are left without a license on purpose.

Refer to [the docs](https://github.com/npm/npm/blob/16a3dd545b10f8a2464e2037506ce39124739b41/doc/files/package.json.md#license)
for the specifics, but the short version is that instead of using
`LicenseRef-LICENSE` for proprietary licenses, you can now use either
`UNLICENSED` if you want to make it clear that you don't _want_ your software
to be licensed (and want npm to stop warning you about this), or `SEE LICENSE
IN <filename>` if there's a license with custom text you want to use. At some
point in the near term, we'll be updating npm to verify that the mentioned
file actually exists, but for now you're all on the honor system.

* [`4827fc7`](https://github.com/npm/npm/commit/4827fc784117c17f35dd9b51b21d1eff6094f661)
  [#8557](https://github.com/npm/npm/issues/8557)
  `normalize-package-data@2.2.1`: Allow `UNLICENSED` and `SEE LICENSE IN
  <filename>` in "license" field of `package.json`.
  ([@kemitchell](https://github.com/kemitchell))
* [`16a3dd5`](https://github.com/npm/npm/commit/16a3dd545b10f8a2464e2037506ce39124739b41)
  [#8557](https://github.com/npm/npm/issues/8557) Document the new accepted
  values for the "license" field.
  ([@kemitchell](https://github.com/kemitchell))
* [`8155311`](https://github.com/npm/npm/commit/81553119350deaf199e79e38e35b52a5c8ad206c)
  [#8557](https://github.com/npm/npm/issues/8557) `init-package-json@1.7.0`:
  Support new "license" field values at init time.
  ([@kemitchell](https://github.com/kemitchell))

#### SMALLISH BUG FIXES

* [`9d8cac9`](https://github.com/npm/npm/commit/9d8cac94a258db648a2b1069b1c8c6529c79d013)
  [#8548](https://github.com/npm/npm/issues/8548) Remove extraneous newline
  from `npm view` output, making it easier to use in shell scripts.
  ([@eush77](https://github.com/eush77))
* [`765fd4b`](https://github.com/npm/npm/commit/765fd4bfca8ea3e2a4a399765b17eec40a3d893d)
  [#8521](https://github.com/npm/npm/issues/8521) When checking for outdated
  packages, or updating packages, raise an error when the registry is
  unreachable instead of silently "succeeding".
  ([@ryantemple](https://github.com/ryantemple))

#### SMALLERISH DOCUMENTATION TWEAKS

* [`5018335`](https://github.com/npm/npm/commit/5018335ce1754a9f771954ecbc1a93acde9b8c0a)
  [#8365](https://github.com/npm/npm/issues/8365) Add details about which git
  environment variables are whitelisted by npm.
  ([@nmalaguti](https://github.com/nmalaguti))
* [`bed9edd`](https://github.com/npm/npm/commit/bed9edddfdcc6d22a80feab33b53e4ef9172ec72)
  [#8554](https://github.com/npm/npm/issues/8554) Fix typo in version docs.
  ([@rainyday](https://github.com/rainyday))

#### WELL, I GUESS THERE ARE MORE DEPENDENCY UPGRADES

* [`7ce2f06`](https://github.com/npm/npm/commit/7ce2f06f6f34d469b1d2e248084d4f3fef10c05e)
  `request@2.58.0`: Refactor tunneling logic, and use `extend` instead of
  abusing `util._extend`. ([@simov](https://github.com/simov))
* [`e6c6195`](https://github.com/npm/npm/commit/e6c61954aad42e20eec49745615c7640b2026a6c)
  `nock@2.6.0`: Refined interception behavior.
  ([@pgte](https://github.com/pgte))
* [`9583cc3`](https://github.com/npm/npm/commit/9583cc3cb192c2fced006927cfba7cd37b588605)
  `fstream-npm@1.0.3`: Ensure that `main` entry in `package.json` is always
  included in the bundled package tarball.
  ([@coderhaoxin](https://github.com/coderhaoxin))
* [`df89493`](https://github.com/npm/npm/commit/df894930f2716adac28740b29b2e863170919990)
  `fstream@1.0.7` ([@isaacs](https://github.com/isaacs))
* [`9744049`](https://github.com/npm/npm/commit/974404934758124aa8ae5b54f7d5257c3bd6b588)
  `dezalgo@1.0.3`: `dezalgo` should be usable in the browser, and can be now
  that `asap` has been upgraded to be browserifiable.
  ([@mvayngrib](https://github.com/mvayngrib))

### v2.11.3 (2015-06-11):

This was a very quiet week. This release was done by
[@iarna](https://github.com/iarna), while the rest of the team hangs out at
NodeConf Adventure!

#### TESTS IN 0.8 FAIL LESS

* [`5b3b3c2`](https://github.com/npm/npm/commit/5b3b3c2)
  [#8491](//github.com/npm/npm/pull/8491)
  Updates a test to use only 0.8 compatible features
  ([@watilde](https://github.com/watilde))

#### THE TREADMILL OF UPDATES NEVER CEASES

* [`9f439da`](https://github.com/npm/npm/commit/9f439da)
  `spdx@0.4.1`: License range updates
  ([@kemitchell](https://github.com/kemitchell))
* [`2dd055b`](https://github.com/npm/npm/commit/2dd055b)
  `normalize-package-data@2.2.1`: Fixes a crashing bug when the package.json
  `scripts` property is not an object.
  ([@iarna](https://github.com/iarna))
* [`e02e85d`](https://github.com/npm/npm/commit/e02e85d)
  `osenv@0.1.2`: Switches to using the `os-tmpdir` module instead of
  `os.tmpdir()` for greater consistency in behavior between node versions.
  ([@iarna](https://github.com/iarna))
* [`a6f0265`](https://github.com/npm/npm/commit/a6f0265)
  `ini@1.3.4` ([@isaacs](https://github.com/isaacs))
* [`7395977`](https://github.com/npm/npm/commit/7395977)
  `rimraf@2.4.0` ([@isaacs](https://github.com/isaacs))

### v2.11.2 (2015-06-04):

Another small release this week, brought to you by the latest addition to the
CLI team, [@zkat](https://github.com/zkat) (Hi, all!)

Mostly small documentation tweaks and version updates. Oh! And `npm outdated`
is actually sorted now. Rejoice!

It's gonna be a while before we get another palindromic version number. Enjoy it
while it lasts. :3

#### QUALITY OF LIFE HAS NEVER BEEN BETTER

* [`31aada4`](https://github.com/npm/npm/commit/31aada4ccc369c0903ff7f233f464955d12c6fe2)
  [#8401](https://github.com/npm/npm/issues/8401) `npm outdated` output is just
  that much nicer to consume now, due to sorting by name.
  ([@watilde](https://github.com/watilde))
* [`458a919`](https://github.com/npm/npm/commit/458a91925d8b20c5e672ba71a86745aad654abaf)
  [#8469](https://github.com/npm/npm/pull/8469) Explicitly set `cwd` for
  `preversion`, `version`, and `postversion` scripts. This makes the scripts
  findable relative to the root dir.
  ([@alexkwolfe](https://github.com/alexkwolfe))
* [`55d6d71`](https://github.com/npm/npm/commit/55d6d71562e979e745c9db88861cc39f99b9f3ec)
  Ensure package name and version are included in display during `npm version`
  lifecycle execution. Gets rid of those little `undefined`s in the console.
  ([@othiym23](https://github.com/othiym23))

#### WORDS HAVE NEVER BEEN QUITE THIS READABLE

* [`3901e49`](https://github.com/npm/npm/commit/3901e4974c800e7f9fba4a5b2ff88da1126d5ef8)
  [#8462](https://github.com/npm/npm/pull/8462) English apparently requires
  correspondence between indefinite articles and attached nouns.
  ([@Enet4](https://github.com/Enet4))
* [`5a744e4`](https://github.com/npm/npm/commit/5a744e4b143ef7b2f50c80a1d96fdae4204d452b)
  [#8421](https://github.com/npm/npm/pull/8421) The effect of `npm prune`'s
  `--production` flag and how to use it have been documented a bit better.
  ([@foiseworth](https://github.com/foiseworth))
* [`eada625`](https://github.com/npm/npm/commit/eada625993485f0a2c5324b06f02bfa0a95ce4bc)
  We've updated our `.mailmap` and `AUTHORS` files to make sure credit is given
  where credit is due. ([@othiym23](https://github.com/othiym23))

#### VERSION NUMBERS HAVE NEVER BEEN BIGGER

* [`c929fd1`](https://github.com/npm/npm/commit/c929fd1d0604b5878ed05706447e078d3e41f5b3)
  `readable-stream@1.1.13`: Manually deduped `v1.1.13` (streams3) to make
  deduping more reliable on `npm@<3`. ([@othiym23](https://github.com/othiym23))
* [`a9b4b78`](https://github.com/npm/npm/commit/a9b4b78dcc85571fd1cdd737903f7f37a5e6a755)
  `request@2.57.0`: Replace dependency on IncomingMessage's `.client` with
  `.socket` as the former was deprecated in io.js 2.2.0.
  ([@othiym23](https://github.com/othiym23))
* [`4b5e557`](https://github.com/npm/npm/commit/4b5e557a23cdefd521ad154111e3d4dcc81f1cdb)
  `abbrev@1.0.7`: Better testing, with coverage.
  ([@othiym23](https://github.com/othiym23))
* [`561affe`](https://github.com/npm/npm/commit/561affee21df9bbea5a47298f2452f533be8f359)
  `semver@4.3.6`: .npmignore added for less cruft, and better testing, with coverage.
  ([@othiym23](https://github.com/othiym23))
* [`60aef3c`](https://github.com/npm/npm/commit/60aef3cf5d84d757752db3eb8ede2cb385469e7b)
  `graceful-fs@3.0.8`: io.js fixes.
  ([@zkat](https://github.com/zkat))
* [`f8bd453`](https://github.com/npm/npm/commit/f8bd453b1a1c46ba7666cb166595e8a011eae443)
  `config-chain@1.1.9`: Added MIT license to package.json
  ([@zkat](https://github.com/zkat))

### v2.11.1 (2015-05-28):

This release brought to you from poolside at the Omni Amelia Island Resort and
JSConf 2015, which is why it's so tiny.

#### CONFERENCE WIFI CAN'T STOP THESE BUG FIXES

* [`cf109a6`](https://github.com/npm/npm/commit/cf109a682f38a059a994da953d5c1b4aaece5e2f)
  [#8381](https://github.com/npm/npm/issues/8381) Documented a subtle gotcha
  with `.npmrc`, which is that it needs to have its permissions set such that
  only the owner can read or write the file.
  ([@colakong](https://github.com/colakong))
* [`180da67`](https://github.com/npm/npm/commit/180da67c9fa53103d625e2f031626c2453c7ebcd)
  [#8365](https://github.com/npm/npm/issues/8365) Git 2.3 adds support for
  `GIT_SSH_COMMAND`, which allows you to pass an explicit git command (with,
  for example, a specific identity passed in on the command line).
    ([@nmalaguti](https://github.com/nmalaguti))

#### MY (VIRGIN) PINA COLADA IS GETTING LOW, BETTER UPGRADE THESE DEPENDENCIES

* [`b72de41`](https://github.com/npm/npm/commit/b72de41c5cc9f0c46d3fa8f062c75bd273641474)
  `node-gyp@2.0.0`: Use a newer version of `gyp`, and generally improve support
  for Visual Studios and Windows.
    ([@TooTallNate](https://github.com/TooTallNate))
* [`8edbe21`](https://github.com/npm/npm/commit/8edbe210af41e8f248f5bb92c72de92f54fda3b1)
  `node-gyp@2.0.1`: Don't crash when Python's version doesn't parse as valid
  semver. ([@TooTallNate](https://github.com/TooTallNate))
* [`ba0e0a8`](https://github.com/npm/npm/commit/ba0e0a845a4f29717aba566b416a27d1a22f5d08)
  `glob@5.0.10`: Add coverage to tests. ([@isaacs](https://github.com/isaacs))
* [`7333701`](https://github.com/npm/npm/commit/7333701b5d4f01673f37d64992c63c4e15864d6d)
  `request@2.56.0`: Bug fixes and dependency upgrades.
  ([@simov](https://github.com/simov))

### v2.11.0 (2015-05-21):

For the first time in a very long time, we've added new events to the life
cycle used by `npm run-script`. Since running `npm version (major|minor|patch)`
is typically the last thing many developers do before publishing their updated
packages, it makes sense to add life cycle hooks to run tests or otherwise
preflight the package before doing a full publish. Thanks, as always, to the
indefatigable [@watilde](https://github.com/watilde) for yet another great
usability improvement for npm!

#### FEATURELETS

* [`b07f7c7`](https://github.com/npm/npm/commit/b07f7c7c1e5021730b3c320f1b3a46e70f8a21ff)
  [#7906](https://github.com/npm/npm/issues/7906)
  Add new [`scripts`](https://github.com/npm/npm/blob/master/doc/misc/npm-scripts.md) to
  allow you to run scripts before and after
  the [`npm version`](https://github.com/npm/npm/blob/master/doc/cli/npm-version.md)
  command has run. This makes it easy to, for instance, require that your
  test suite passes before bumping the version by just adding `"preversion":
  "npm test"` to the scripts section of your `package.json`.
  ([@watilde](https://github.com/watilde))
* [`8a46136`](https://github.com/npm/npm/commit/8a46136f42e416cbadb533bcf89d73d681ed421d)
  [#8185](https://github.com/npm/npm/issues/8185)
  When we get a "not found" error from the registry, we'll now check to see
  if the package name you specified is invalid and if so, give you a better
  error message. ([@thefourtheye](https://github.com/thefourtheye))

#### BUG FIXES

* [`9bcf573`](https://github.com/npm/npm/commit/9bcf5730bd0316f210dafea898afe9103849cea9)
  [#8324](https://github.com/npm/npm/pull/8324) On Windows, when you've configured a
  custom `node-gyp`, run it with node itself instead of using the default open action (which
  is almost never what you want). ([@bangbang93](https://github.com/bangbang93))
* [`1da9b04`](https://github.com/npm/npm/commit/1da9b0411d3416c7fca17d08cbbcfca7ae86e92d)
  [#7195](https://github.com/npm/npm/issues/7195)
  [#7260](https://github.com/npm/npm/issues/7260) `npm-registry-client@6.4.0`:
  (Re-)allow publication of existing mixed-case packages (part 1).
  ([@smikes](https://github.com/smikes))
* [`e926783`](https://github.com/npm/npm/commit/e9267830ab261c751f12723e84d2458ae9238646)
  [#7195](https://github.com/npm/npm/issues/7195)
  [#7260](https://github.com/npm/npm/issues/7260)
  `normalize-package-data@2.2.0`: (Re-)allow publication of existing mixed-case
  packages (part 2). ([@smikes](https://github.com/smikes))

#### DOCUMENTATION IMPROVEMENTS

* [`f62ee05`](https://github.com/npm/npm/commit/f62ee05333b141539a8e851c620dd2e82ff06860)
  [#8314](https://github.com/npm/npm/issues/8314) Update the README to warn
  folks away from using the CLI's internal API. For the love of glob, just use a
  child process to run the CLI! ([@claycarpenter](https://github.com/claycarpenter))
* [`1093921`](https://github.com/npm/npm/commit/1093921c04db41ab46db24a170a634a4b2acd8d9)
  [#8279](https://github.com/npm/npm/pull/8279)
  Update the documentation to note that, yes, you can publish scoped packages to the
  public registry now! ([@mantoni](https://github.com/mantoni))
* [`f87cde5`](https://github.com/npm/npm/commit/f87cde5234a760d3e515ffdaacaed6f5b71dbf44)
  [#8292](https://github.com/npm/npm/pull/8292)
  Fix typo in an example and grammar in the description in
  the [shrinkwrap documentation](https://github.com/npm/npm/blob/master/doc/cli/npm-shrinkwrap.md).
  ([@vshih](https://github.com/vshih))
* [`d3526ce`](https://github.com/npm/npm/commit/d3526ceb09a0c29fdb7d4124536ae09057d033e7)
  Improve the formatting in
  the [shrinkwrap documentation](https://github.com/npm/npm/blob/master/doc/cli/npm-shrinkwrap.md).
  ([@othiym23](https://github.com/othiym23))
* [`19fe6d2`](https://github.com/npm/npm/commit/19fe6d20883e28956ff916fe4dae42d73ee6195b)
  [#8311](https://github.com/npm/npm/pull/8311)
  Update [README.md](https://github.com/npm/npm#readme) to use syntax highlighting in
  its code samples and bits of shell scripts. ([@SimenB](https://github.com/SimenB))

#### DEPENDENCY UPDATES! ALWAYS AND FOREVER!

* [`fc52160`](https://github.com/npm/npm/commit/fc52160d0223226fffe4166f42fdfd3b899b3c1e)
  [#4700](https://github.com/npm/npm/issues/4700) [#5044](https://github.com/npm/npm/issues/5044)
  `init-package-json@1.6.0`: Make entering an invalid version while running `npm init` give
  you an immediate error and prompt you to correct it. ([@watilde](https://github.com/watilde))
* [`738853e`](https://github.com/npm/npm/commit/738853eb1f55636476a2a410c2c04732eec9d51e)
  [#7763](https://github.com/npm/npm/issues/7763) `fs-write-stream-atomic@1.0.3`: Fix a bug
  where errors would not propagate, making error messages unhelpful.
  ([@iarna](https://github.com/iarna))
* [`6d74a2d`](https://github.com/npm/npm/commit/6d74a2d2ac7f92750cf6a2cfafae1af23b569098)
  `npm-package-arg@4.0.1`: Fix tests on windows ([@Bacra](https://github.com)) and with
  more recent `hosted-git-info`. ([@iarna](https://github.com/iarna))
* [`50f7178`](https://github.com/npm/npm/commit/50f717852fbf713ef6cbc4e0a9ab42657decbbbd)
  `hosted-git-info@2.1.4`: Correct spelling in its documentation.
  ([@iarna](https://github.com/iarna))
* [`d7956ca`](https://github.com/npm/npm/commit/d7956ca17c057d5383ff0d3fc5cf6ac2940b034d)
  `glob@5.0.7`: Fix a bug where unusual error conditions could make
  further use of the module fail. ([@isaacs](https://github.com/isaacs))
* [`44f7d74`](https://github.com/npm/npm/commit/44f7d74c5d3181d37da7ea7949c86b344153f8d9)
  `tap@1.1.0`: Update to the most recent tap to get a whole host of bug
  fixes and integration with [coveralls](https://coveralls.io/).
  ([@isaacs](https://github.com/isaacs))
* [`c21e8a8`](https://github.com/npm/npm/commit/c21e8a8d94bcf0ad79dc583ddc53f8366d4813b3)
  `nock@2.2.0` ([@othiym23](https://github.com/othiym23))

#### LICENSE FILES FOR THE LICENSE GOD

* Add missing ISC license file to package ([@kasicka](https://github.com/kasicka)):
    * [`aa9908c`](https://github.com/npm/npm/commit/aa9908c20017729673b9d410b77f9a16b7aae8a4) `realize-package-specifier@3.0.1`
    * [`23a3b1a`](https://github.com/npm/npm/commit/23a3b1a726b9176c70ce0ccf3cd9d25c54429bdf) `fs-vacuum@1.2.6`
    * [`8e04bba`](https://github.com/npm/npm/commit/8e04bba830d4353d84751d21803cd127c96153a7) `dezalgo@1.0.2`
    * [`50f7178`](https://github.com/npm/npm/commit/50f717852fbf713ef6cbc4e0a9ab42657decbbbd) `hosted-git-info@2.1.4`
    * [`6a54917`](https://github.com/npm/npm/commit/6a54917fbd4df995495a95d4b548defd44b77c93) `write-file-atomic@1.1.2`
    * [`971f92c`](https://github.com/npm/npm/commit/971f92c4a4e5514217d1e4db45d1ccf71a60ff19) `async-some@1.0.2`
    * [`67b50b7`](https://github.com/npm/npm/commit/67b50b7667a42bb3340a660eb2e617e1a554d2d4) `normalize-git-url@1.0.1`

#### SPDX LICENSE UPDATES

* Switch license to
  [BSD-2-Clause](http://spdx.org/licenses/BSD-2-Clause.html#licenseText) from
  plain "BSD" ([@isaacs](https://github.com/isaacs)):
    * [`efdb733`](https://github.com/npm/npm/commit/efdb73332eeedcad4c609796929070b62abb37ab) `npm-user-validate@0.1.2`
    * [`e926783`](https://github.com/npm/npm/commit/e9267830ab261c751f12723e84d2458ae9238646) `normalize-package-data@2.2.0`
* Switch license to [ISC](http://spdx.org/licenses/ISC.html#licenseText) from
  [BSD](http://spdx.org/licenses/BSD-2-Clause.html#licenseText)
  ([@isaacs](https://github.com/isaacs)):
    * [`c300956`](https://github.com/npm/npm/commit/c3009565a964f0ead4ac4ab234b1a458e2365f17) `block-stream@0.0.8`
    * [`1de1253`](https://github.com/npm/npm/commit/1de125355765fecd31e682ed0ff9d2edbeac0bb0) `lockfile@1.0.1`
    * [`0d5698a`](https://github.com/npm/npm/commit/0d5698ab132e376c7aec93ae357c274932116220) `osenv@0.1.1`
    * [`2e84921`](https://github.com/npm/npm/commit/2e84921474e1ffb18de9fce4616e73171fa8046d) `abbrev@1.0.6`
    * [`872fac9`](https://github.com/npm/npm/commit/872fac9d10c11607e4d0348c08a683b84e64d30b) `chmodr@0.1.1`
    * [`01eb7f6`](https://github.com/npm/npm/commit/01eb7f60acba584346ad8aae846657899f3b6887) `chownr@0.0.2`
    * [`294336f`](https://github.com/npm/npm/commit/294336f0f31c7b9fe31a50075ed750db6db134d1) `read@1.0.6`
    * [`ebdf6a1`](https://github.com/npm/npm/commit/ebdf6a14d17962cdb7128402c53b452f91d44ca7) `graceful-fs@3.0.7`
* Switch license to [ISC](http://spdx.org/licenses/ISC.html#licenseText) from
  [MIT](http://spdx.org/licenses/MIT.html#licenseText)
  ([@isaacs](https://github.com/isaacs)):
    * [`e5d237f`](https://github.com/npm/npm/commit/e5d237fc0f436dd2a89437ebf8a9632a2e35ccbe) `nopt@3.0.2`
    * [`79fef14`](https://github.com/npm/npm/commit/79fef1421b78f044980f0d1bf0e97039b6992710) `rimraf@2.3.4`
    * [`22527da`](https://github.com/npm/npm/commit/22527da4816e7c2746cdc0317c5fb4a85152d554) `minimatch@2.0.8`
    * [`882ac87`](https://github.com/npm/npm/commit/882ac87a6c4123ca985d7ad4394ea5085e5b0ef5) `lru-cache@2.6.4`
    * [`9d9d015`](https://github.com/npm/npm/commit/9d9d015a2e972f68664dda54fbb204db28b21ede) `npmlog@1.2.1`

### v2.10.1 (2015-05-14):

#### BUG FIXES & DOCUMENTATION TWEAKS

* [`dc77520`](https://github.com/npm/npm/commit/dc7752013ffce13a3d3f13e518a0052c22fc1158)
  When getting back a 404 from a request to a private registry that uses a
  registry path that extends past the root
  (`http://registry.enterprise.co/path/to/registry`), display the name of the
  nonexistent package, rather than the first element in the registry API path.
  Sorry, Artifactory users! ([@hayes](https://github.com/hayes))
* [`f70dea9`](https://github.com/npm/npm/commit/f70dea9b4766f6eaa55012c3e8087e9cb04fd4ce)
  Make clearer that `--registry` can be used on a per-publish basis to push a
  package to a non-default registry. ([@mischkl](https://github.com/mischkl))
* [`a3e26f5`](https://github.com/npm/npm/commit/a3e26f5b4465991a941a325468ab7725670d2a94)
  Did you know that GitHub shortcuts can have commit-ishes included
  (`org/repo#branch`)? They can! ([@iarna](https://github.com/iarna))
* [`0e2c091`](https://github.com/npm/npm/commit/0e2c091a539b61fdc60423b6bbaaf30c24e4b1b8)
  Some errors from `readPackage` were being swallowed, potentially leading to
  invalid package trees on disk. ([@smikes](https://github.com/smikes))

#### DEPENDENCY UPDATES! STILL! MORE! AGAIN!

* [`0b901ad`](https://github.com/npm/npm/commit/0b901ad0811d84dda6ca0755a9adc8d47825edd0)
  `lru-cache@2.6.3`: Removed some cruft from the published package.
  ([@isaacs](https://github.com/isaacs))
* [`d713e0b`](https://github.com/npm/npm/commit/d713e0b14930c563e3fdb6ac6323bae2a8924652)
  `mkdirp@0.5.1`: Made compliant with `standard`, dropped support for Node 0.6,
  added (Travis) support for Node 0.12 and io.js.
  ([@isaacs](https://github.com/isaacs))
* [`a2d6578`](https://github.com/npm/npm/commit/a2d6578b6554c5c9d48fe2006751759f4da57520)
  `glob@1.0.3`: Updated to use `tap@1`. ([@isaacs](https://github.com/isaacs))
* [`64cd1a5`](https://github.com/npm/npm/commit/64cd1a570aaa5f24ccba190948ec9456297c97f5)
  `fstream@ 1.0.6`: Made compliant with [`standard`](http://npm.im/standard)
  (done by [@othiym23](https://github.com/othiym23), and then debugged and
  fixed by [@iarna](https://github.com/iarna)), and license changed to ISC.
  ([@othiym23](https://github.com/othiym23) /
  [@iarna](https://github.com/iarna))
* [`b527a7c`](https://github.com/npm/npm/commit/b527a7c2ba3c4002f443dd2c536ff4ff41a38b86)
  `which@1.1.1`: Callers can pass in their own `PATH` instead of relying on
  `process.env`. ([@isaacs](https://github.com/isaacs))

### v2.10.0 (2015-05-8):

#### THE IMPLICATIONS ARE MORE PROFOUND THAN THEY APPEAR

If you've done much development in The Enterprise®™, you know that keeping
track of software licenses is far more important than one might expect / hope /
fear. Tracking licenses is a hassle, and while many (if not most) of us have
(reluctantly) gotten around to setting a license to use by default with all our
new projects (even if it's just WTFPL), that's about as far as most of us think
about it. In big enterprise shops, ensuring that projects don't inadvertently
use software with unacceptably encumbered licenses is serious business, and
developers spend a surprising (and appalling) amount of time ensuring that
licensing is covered by writing automated checkers and other license auditing
tools.

The Linux Foundation has been working on a machine-parseable syntax for license
expressions in the form of [SPDX](https://spdx.org/), an appropriately
enterprisey acronym. IP attorney and JavaScript culture hero [Kyle
Mitchell](http://kemitchell.com/) has put a considerable amount of effort into
bringing SPDX to JavaScript and Node. He's written
[`spdx.js`](https://github.com/kemitchell/spdx.js), a JavaScript SPDX
expression parser, and has integrated it into npm in a few different ways.

For you as a user of npm, this means:

* npm now has proper support for dual licensing in `package.json`, due to
  SPDX's compound expression syntax. Run `npm help package.json` for details.
* npm will warn you if the `package.json` for your project is either missing a
  `"license"` field, or if the value of that field isn't a valid SPDX
  expression (pro tip: `"BSD"` becomes `"BSD-2-Clause"` in SPDX (unless you
  really want one of its variants); `"MIT"` and `"ISC"` are fine as-is; the
  [full list](https://github.com/shinnn/spdx-license-ids/blob/master/spdx-license-ids.json)
  is its own package).
* `npm init` now demands that you use a valid SPDX expression when using it
  interactively (pro tip: I mostly use `npm init -y`, having previously run
  `npm config set init.license=MIT` / `npm config set init.author.email=foo` /
  `npm config set init.author.name=me`).
* The documentation for `package.json` has been updated to tell you how to use
  the `"license"` field properly with SPDX.

In general, this shouldn't be a big deal for anybody other than people trying
to run their own automated license validators, but in the long run, if
everybody switches to this format, many people's lives will be made much
simpler. I think this is an important improvement for npm and am very thankful
to Kyle for taking the lead on this. Also, even if you think all of this is
completely stupid, just [choose a license](http://en.wikipedia.org/wiki/License-free_software)
anyway. Future you will thank past you someday, unless you are
[djb](http://cr.yp.to/), in which case you are djb, and more power to you.

* [`8669f7d`](https://github.com/npm/npm/commit/8669f7d88c472ccdd60e140106ac43cca636a648)
  [#8179](https://github.com/npm/npm/issues/8179) Document how to use SPDX in
  `license` stanzas in `package.json`, including how to migrate from old busted
  license declaration arrays to fancy new compound-license clauses.
  ([@kemitchell](https://github.com/kemitchell))
* [`98ad98c`](https://github.com/npm/npm/commit/98ad98cb11f3d3ba29a488ef1ab050b066d9c7f6)
  [#8197](https://github.com/npm/npm/issues/8197) `init-package-json@1.5.0`
  Ensure that packages bootstrapped with `npm init` use an SPDX-compliant
  license expression. ([@kemitchell](https://github.com/kemitchell))
* [`2ad3905`](https://github.com/npm/npm/commit/2ad3905e9139b0be2b22accf707b814469de813e)
  [#8197](https://github.com/npm/npm/issues/8197)
  `normalize-package-data@2.1.0`: Warn when a package is missing a license
  declaration, or using a license expression that isn't valid SPDX.
  ([@kemitchell](https://github.com/kemitchell))
* [`127bb73`](https://github.com/npm/npm/commit/127bb73ccccc59a1267851c702d8ebd3f3a97e81)
  [#8197](https://github.com/npm/npm/issues/8197) `tar@2.1.1`: Switch from
  `BSD` to `ISC` for license, where the latter is valid SPDX.
  ([@othiym23](https://github.com/othiym23))
* [`e9a933a`](https://github.com/npm/npm/commit/e9a933a9148180d9d799f99f4154f5110ff2cace)
  [#8197](https://github.com/npm/npm/issues/8197) `once@1.3.2`: Switch from
  `BSD` to `ISC` for license, where the latter is valid SPDX.
  ([@othiym23](https://github.com/othiym23))
* [`412401f`](https://github.com/npm/npm/commit/412401fb6a19b18f3e02d97a24d4dafed650c186)
  [#8197](https://github.com/npm/npm/issues/8197) `semver@4.3.4`: Switch from
  `BSD` to `ISC` for license, where the latter is valid SPDX.
  ([@othiym23](https://github.com/othiym23))

As a corollary to the previous changes, I've put some work into making `npm
install` spew out fewer pointless warnings about missing values in transitive
dependencies. From now on, npm will only warn you about missing READMEs,
license fields, and the like for top-level projects (including packages you
directly install into your application, but we may relax that eventually).

Practically _nobody_ liked having those warnings displayed for child
dependencies, for the simple reason that there was very little that anybody
could _do_ about those warnings, unless they happened to be the maintainers of
those dependencies themselves. Since many, many projects don't have
SPDX-compliant licenses, the number of warnings reached a level where they ran
the risk of turning into a block of visual noise that developers (read: me, and
probably you) would ignore forever.

So I fixed it. If you still want to see the messages about child dependencies,
they're still there, but have been pushed down a logging level to `info`. You
can display them by running `npm install -d` or `npm install --loglevel=info`.

* [`eb18245`](https://github.com/npm/npm/commit/eb18245f55fb4cd62a36867744bcd1b7be0a33e2)
  Only warn on normalization errors for top-level dependencies. Transitive
  dependency validation warnings are logged at `info` level.
  ([@othiym23](https://github.com/othiym23))

#### BUG FIXES

* [`e40e809`](https://github.com/npm/npm/commit/e40e8095d2bc9fa4eb8f01aa22067e0068fa8a54)
  `tap@1.0.1`: TAP: The Next Generation. Fix up many tests to they work
  properly with the new major version of `node-tap`. Look at all the colors!
  ([@isaacs](https://github.com/isaacs))
* [`f9314e9`](https://github.com/npm/npm/commit/f9314e97d26532c0ef2b03e98f3ed300b7cd5026)
  `nock@1.9.0`: Minor tweaks and bug fixes. ([@pgte](https://github.com/pgte))
* [`45c2b1a`](https://github.com/npm/npm/commit/45c2b1aaa051733fa352074994ae6e569fd51e8b)
  [#8187](https://github.com/npm/npm/issues/8187) `npm ls` wasn't properly
  recognizing dependencies installed from GitHub repositories as git
  dependencies, and so wasn't displaying them as such.
  ([@zornme](https://github.com/zornme))
* [`1ab57c3`](https://github.com/npm/npm/commit/1ab57c38116c0403965c92bf60121f0f251433e4)
  In some cases, `npm help` was using something that looked like a regular
  expression where a glob pattern should be used, and vice versa.
  ([@isaacs](https://github.com/isaacs))

### v2.9.1 (2015-04-30):

#### WOW! MORE GIT FIXES! YOU LOVE THOSE!

The first item below is actually a pretty big deal, as it fixes (with a
one-word change and a much, much longer test case (thanks again,
[@iarna](https://github.com/iarna))) a regression that's been around for months
now. If you're depending on multiple branches of a single git dependency in a
single project, you probably want to check out `npm@2.9.1` and verify that
things (again?) work correctly in your project.

* [`178a6ad`](https://github.com/npm/npm/commit/178a6ad540215820d16217465a5f220d8c95a313)
  [#7202](https://github.com/npm/npm/issues/7202) When caching git
  dependencies, do so by the whole URL, including the branch name, so that if a
  single application depends on multiple branches from the same repository (in
  practice, multiple version tags), every install is of the correct version,
  instead of reusing whichever branch the caching process happened to check out
  first.  ([@iarna](https://github.com/iarna))
* [`63b79cc`](https://github.com/npm/npm/commit/63b79ccde092a9cb3b1f34abe43e1d2ba69c0dbf)
  [#8084](https://github.com/npm/npm/issues/8084) Ensure that Bitbucket,
  GitHub, and Gitlab dependencies are installed the same way as non-hosted git
  dependencies, fixing `npm install --link`.
  ([@laiso](https://github.com/laiso))

#### DOCUMENTATION FIXES AND TWEAKS

These changes may seem simple and small (except Lin's fix to the package name
restrictions, which was more an egregious oversight on our part), but cleaner
documentation makes npm significantly more pleasant to use. I really appreciate
all the typo fixes, clarifications, and formatting tweaks people send us, and
am delighted that we get so many of these pull requests. Thanks, everybody!

* [`ca478dc`](https://github.com/npm/npm/commit/ca478dcaa29b8f07cd6fe515a3c4518166819291)
  [#8137](https://github.com/npm/npm/issues/8137) Somehow, we had failed to
  clearly document the full restrictions on package names.
  [@linclark](https://github.com/linclark) has now fixed that, although we will
  take with us to our graves the reasons why the maximum package name length is 214
  characters (well, OK, it was that that was the longest name in the registry
  when we decided to put a cap on the name length).
  ([@linclark](https://github.com/linclark))
* [`b574076`](https://github.com/npm/npm/commit/b5740767c320c1eff3576a8d63952534a0fbb936)
  [#8079](https://github.com/npm/npm/issues/8079) Make the `npm shrinkwrap`
  documentation use code formatting for examples consistently. It would be
  great to do this for more commands HINT HINT.
  ([@RichardLitt](https://github.com/RichardLitt))
* [`1ff636e`](https://github.com/npm/npm/commit/1ff636e2db3852a53e38c866fed7eafdacd307fc)
  [#8105](https://github.com/npm/npm/issues/8105) Document that the global
  `npmrc` goes in `$PREFIX/etc/npmrc`, instead of `$PREFIX/npmrc`.
  ([@anttti](https://github.com/anttti))
* [`c3f2f7c`](https://github.com/npm/npm/commit/c3f2f7c299342e1c1eccc55a976a63c607f51621)
  [#8127](https://github.com/npm/npm/issues/8127) Document how to use `npm run
  build` directly (hint: it's different from `npm build`!).
  ([@mikemaccana](https://github.com/mikemaccana))
* [`873e467`](https://github.com/npm/npm/commit/873e46757e1986761b15353f94580a071adcb383)
  [#8069](https://github.com/npm/npm/issues/8069) Take the old, dead npm
  mailing list address out of `package.json`. It seems that people don't have
  much trouble figuring out how to report errors to npm.
  ([@robertkowalski](https://github.com/robertkowalski))

#### ENROBUSTIFICATIONMENT

* [`5abfc9c`](https://github.com/npm/npm/commit/5abfc9c9017da714e47a3aece750836b4f9af6a9)
  [#7973](https://github.com/npm/npm/issues/7973) `npm run-script` completion
  will only suggest run scripts, instead of including dependencies. If for some
  reason you still wanted it to suggest dependencies, let us know.
  ([@mantoni](https://github.com/mantoni))
* [`4b564f0`](https://github.com/npm/npm/commit/4b564f0ce979dc74c09604f4d46fd25a2ee63804)
  [#8081](https://github.com/npm/npm/issues/8081) Use `osenv` to parse the
  environment's `PATH` in a platform-neutral way.
  ([@watilde](https://github.com/watilde))
* [`a4b6238`](https://github.com/npm/npm/commit/a4b62387b41848818973eeed056fd5c6570274f3)
  [#8094](https://github.com/npm/npm/issues/8094) When we refactored the
  configuration code to split out checking for IPv4 local addresses, we
  inadvertently completely broke it by failing to return the values. In
  addition, just the call to `os.getInterfaces()` could throw on systems where
  querying the network configuration requires elevated privileges (e.g. Amazon
  Lambda). Add the return, and trap errors so they don't cause npm to explode.
  Thanks to [@mhart](https://github.com/mhart) for bringing this to our
  attention! ([@othiym23](https://github.com/othiym23))

#### DEPENDENCY UPDATES WAIT FOR NO SOPHONT

* [`000cd8b`](https://github.com/npm/npm/commit/000cd8b52104942ac3404f0ad0651d82f573da37)
  `rimraf@2.3.3`: More informative assertions on argument validation failure.
  ([@isaacs](https://github.com/isaacs))
* [`530a2e3`](https://github.com/npm/npm/commit/530a2e369128270f3e098f0e9be061533003b0eb)
  `lru-cache@2.6.2`: Revert to old key access-time behavior, as it was correct
  all along. ([@isaacs](https://github.com/isaacs))
* [`d88958c`](https://github.com/npm/npm/commit/d88958ca02ce81b027b9919aec539d0145875a59)
  `minimatch@2.0.7`: Feature detection and test improvements.
  ([@isaacs](https://github.com/isaacs))
* [`3fa39e4`](https://github.com/npm/npm/commit/3fa39e4d492609d5d045033896dcd99f7b875329)
  `nock@1.7.1` ([@pgte](https://github.com/pgte))

### v2.9.0 (2015-04-23):

This week was kind of a breather to concentrate on fixing up the tests on the
`multi-stage` branch, and not mess with git issues for a little while.
Unfortunately, There are now enough severe git issues that we'll probably have
to spend another couple weeks tackling them. In the meantime, enjoy these two
small features. They're just enough to qualify for a semver-minor bump:

#### NANOFEATURES

* [`2799322`](https://github.com/npm/npm/commit/279932298ce5b589c5eea9439ac40b88b99c6a4a)
  [#7426](https://github.com/npm/npm/issues/7426) Include local modules in `npm
  outdated` and `npm update`.  ([@ArnaudRinquin](https://github.com/ArnaudRinquin))
* [`2114862`](https://github.com/npm/npm/commit/21148620fa03a582f4ec436bb16bd472664f2737)
  [#8014](https://github.com/npm/npm/issues/8014) The prefix used before the
  version on version tags is now configurable via `tag-version-prefix`. Be
  careful with this one and read the docs before using it.
  ([@kkragenbrink](https://github.com/kkragenbrink))

#### OTHER MINOR TWEAKS

* [`18ce0ec`](https://github.com/npm/npm/commit/18ce0ecd2d94ad3af01e997f1396515892dd363c)
  [#3032](https://github.com/npm/npm/issues/3032) `npm unpublish` will now use
  the registry set in `package.json`, just like `npm publish`. This only
  applies, for now, when unpublishing the entire package, as unpublishing a
  single version requires the name be included on the command line and
  therefore doesn't read from `package.json`. ([@watilde](https://github.com/watilde))
* [`9ad2100`](https://github.com/npm/npm/commit/9ad210042242e51d52b2a8b633d8e59248f5faa4)
  [#8008](https://github.com/npm/npm/issues/8008) Once again, when considering
  what to install on `npm install`, include `devDependencies`.
  ([@smikes](https://github.com/smikes))
* [`5466260`](https://github.com/npm/npm/commit/546626059909dca1906454e820ca4e315c1795bd)
  [#8003](https://github.com/npm/npm/issues/8003) Clarify the documentation
  around scopes to make it easier to understand how they support private
  packages. ([@smikes](https://github.com/smikes))

#### DEPENDENCIES WILL NOT STOP UNTIL YOU ARE VERY SLEEPY

* [`faf65a7`](https://github.com/npm/npm/commit/faf65a7bbb2fad13216f64ed8f1243bafe743f97)
  `init-package-json@1.4.2`: If there are multiple validation errors and
  warnings, ensure they all get displayed (includes a rad new way of testing
  `init-package-json` contributed by
  [@michaelnisi](https://github.com/michaelnisi)).
  ([@MisumiRize](https://github.com/MisumiRize))
* [`7f10f38`](https://github.com/npm/npm/commit/7f10f38d29a8423d7cde8103fa7b64ac728da1e0)
  `editor@1.0.0`: `1.0.0` is literally more than `0.1.0` (no change aside from
  version number). ([@substack](https://github.com/substack))
* [`4979af3`](https://github.com/npm/npm/commit/4979af3fcae5a3962383b7fdad3162381e62eefe)
  [#6805](https://github.com/npm/npm/issues/6805) `npm-registry-client@6.3.3`:
  Decode scoped package names sent by the registry so they look nicer.
  ([@mmalecki](https://github.com/mmalecki))

### v2.8.4 (2015-04-16):

This is the fourth release of npm this week, so it's mostly just landing a few
small outstanding PRs on dependencies and some tiny documentation tweaks.
`npm@2.8.3` is where the real action is.

* [`ee2bd77`](https://github.com/npm/npm/commit/ee2bd77f3c64d38735d1d31028224a5c40422a9b)
  [#7983](https://github.com/npm/npm/issues/7983) `tar@2.1.0`: Better error
  reporting in corrupted tar files, and add support for the `fromBase` flag
  (rescued from the dustbin of history by
  [@deanmarano](https://github.com/deanmarano)).
  ([@othiym23](https://github.com/othiym23))
* [`d8eee6c`](https://github.com/npm/npm/commit/d8eee6cf9d2ff7aca68dfaed2de76824a3e0d9af)
  `init-package-json@1.4.1`: Add support for a default author, and only add
  scope to a package name once. ([@othiym23](https://github.com/othiym23))
* [`4fc5d98`](https://github.com/npm/npm/commit/4fc5d98b785f601c60d4dc0a2c8674f0cccf6262)
  `lru-cache@2.6.1`: Small tweaks to cache value aging and entry counting that
  are irrelevant to npm. ([@isaacs](https://github.com/isaacs))
* [`1fe5840`](https://github.com/npm/npm/commit/1fe584089f5bef133de5518aa26eaf6064be2bf7)
  [#7946](https://github.com/npm/npm/issues/7946) Make `npm init` text
  friendlier. ([@sandfox](https://github.com/sandfox))

### v2.8.3 (2015-04-15):

#### TWO SMALL GIT TWEAKS

This is the last of a set of releases intended to ensure npm's git support is
robust enough that we can stop working on it for a while. These fixes are
small, but prevent a common crasher and clear up one of the more confusing
error messages coming out of npm when working with repositories hosted on git.

* [`387f889`](https://github.com/npm/npm/commit/387f889c0e8fb617d9cc9a42ed0a3ec49424ab5d)
  [#7961](https://github.com/npm/npm/issues/7961) Ensure that hosted git SSH
  URLs always have a valid protocol when stored in `resolved` fields in
  `npm-shrinkwrap.json`. ([@othiym23](https://github.com/othiym23))
* [`394c2f5`](https://github.com/npm/npm/commit/394c2f5a1227232c0baf42fbba1402aafe0d6ffb)
  Switch the order in which hosted Git providers are checked to `git:`,
  `git+https:`, then `git+ssh:` (from `git:`, `git+ssh:`, then `git+https:`) in
  an effort to go from most to least likely to succeed, to make for less
  confusing error message. ([@othiym23](https://github.com/othiym23))

### v2.8.2 (2015-04-14):

#### PEACE IN OUR TIME

npm has been having an issue with CouchDB's web server since the release
of io.js and Node.js 0.12.0 that has consumed a huge amount of my time
to little visible effect. Sam Mikes picked up the thread from me, and
after a [_lot_ of effort](https://github.com/npm/npm/issues/7699#issuecomment-93091111)
figured out that ultimately there are probably a couple problems with
the new HTTP Agent keep-alive handling in new versions of Node. In
addition, `npm-registry-client` was gratuitously sending a body along
with a GET request which was triggering the bugs. Sam removed about 10 bytes from
one file in `npm-registry-client`, and this problem, which has been bugging us for months,
completely went away.

In conclusion, Sam Mikes is great, and anybody using a private registry
hosted on CouchDB should thank him for his hard work. Also, thanks to
the community at large for pitching in on this bug, which has been
around for months now.

* [`431c3bf`](https://github.com/npm/npm/commit/431c3bf6cdec50f9f0c735f478cb2f3f337d3313)
  [#7699](https://github.com/npm/npm/issues/7699) `npm-registry-client@6.3.2`:
  Don't send body with HTTP GET requests when logging in.
  ([@smikes](https://github.com/smikes))

### v2.8.1 (2015-04-12):

#### CORRECTION: NPM'S GIT INTEGRATION IS DOING OKAY

A [helpful bug report](https://github.com/npm/npm/issues/7872#issuecomment-91809553)
led to another round of changes to
[`hosted-git-info`](https://github.com/npm/hosted-git-info/commit/827163c74531b69985d1ede7abced4861e7b0cd4),
some additional test-writing, and a bunch of hands-on testing against actual
private repositories. While the complexity of npm's git dependency handling is
nearly fractal (because npm is very complex, and git is even more complex),
it's feeling way more solid than it has for a while. We think this is a
substantial improvement over what we had before, so give `npm@2.8.1` a shot if
you have particularly complex git use cases and
[let us know](https://github.com/npm/npm/issues/new) how it goes.

(NOTE: These changes mostly affect cloning and saving references to packages
hosted in git repositories, and don't address some known issues with things
like lifecycle scripts not being run on npm dependencies. Work continues on
other issues that affect parity between git and npm registry packages.)

* [`66377c6`](https://github.com/npm/npm/commit/66377c6ece2cf4d53d9a618b7d9824e1452bc293)
  [#7872](https://github.com/npm/npm/issues/7872) `hosted-git-info@2.1.2`: Pass
  through credentials embedded in SSH and HTTPs git URLs.
  ([@othiym23](https://github.com/othiym23))
* [`15efe12`](https://github.com/npm/npm/commit/15efe124753257728a0ddc64074fa5a4b9c2eb30)
  [#7872](https://github.com/npm/npm/issues/7872) Use the new version of
  `hosted-git-info` to pass along credentials embedded in git URLs. Test it.
  Test it a lot. ([@othiym23](https://github.com/othiym23))

#### SCOPED DEPENDENCIES AND PEER DEPENDENCIES: NOT QUITE REESE'S

Big thanks to [@ewie](https://github.com/ewie) for identifying an issue with
how npm was handling `peerDependencies` that were implicitly installed from the
`package.json` files of scoped dependencies. This
[will be a moot point](https://github.com/npm/npm/issues/6565#issuecomment-74971689)
with the release of `npm@3`, but until then, it's important that
`peerDependency` auto-installation work as expected.

* [`b027319`](https://github.com/npm/npm/commit/b0273190c71eba14395ddfdd1d9f7ba625297523)
  [#7920](https://github.com/npm/npm/issues/7920) Scoped packages with
  `peerDependencies` were installing the `peerDependencies` into the wrong
  directory. ([@ewie](https://github.com/ewie))
* [`649e31a`](https://github.com/npm/npm/commit/649e31ae4fd02568bae5dc6b4ea783431ce3d63e)
  [#7920](https://github.com/npm/npm/issues/7920) Test `peerDependency`
  installs involving scoped packages using `npm-package-arg` instead of simple
  path tests, for consistency. ([@othiym23](https://github.com/othiym23))

#### MAKING IT EASIER TO WRITE NPM TESTS, VERSION 0.0.1

[@iarna](https://github.com/iarna) and I
([@othiym23](https://github.com/othiym23)) have been discussing a
[candidate plan](https://github.com/npm/npm/wiki/rewriting-npm's-tests:-a-plan-maybe)
for improving npm's test suite, with the goal of making it easier for new
contributors to get involved with npm by reducing the learning curve
necessary to be able to write good tests for proposed changes. This is the
first substantial piece of that effort. Here's what the commit message for
[`ed7e249`](https://github.com/npm/npm/commit/ed7e249d50444312cd266942ce3b89e1ca049bdf)
had to say about this work:

> It's too difficult for npm contributors to figure out what the conventional
> style is for tests. Part of the problem is that the documentation in
> CONTRIBUTING.md is inadequate, but another important factor is that the tests
> themselves are written in a variety of styles.  One of the most notable
> examples of this is the fact that many tests use fixture directories to store
> precooked test scenarios and package.json files.
>
> This had some negative consequences:
>
>   * tests weren't idempotent
>   * subtle dependencies between tests existed
>   * new tests get written in this deprecated style because it's not
>     obvious that the style is out of favor
>   * it's hard to figure out why a lot of those directories existed,
>     because they served a variety of purposes, so it was difficult to
>     tell when it was safe to remove them
>
> All in all, the fixture directories were a major source of technical debt, and
> cleaning them up, while time-consuming, makes the whole test suite much more
> approachable, and makes it more likely that new tests written by outside
> contributors will follow a conventional style. To support that, all of the
> tests touched by this changed were cleaned up to pass the `standard` style
> checker.

And here's a little extra context from a comment I left on [#7929](https://github.com/npm/npm/issues/7929):

> One of the other things that encouraged me was looking at this
> [presentation on technical debt](http://www.slideshare.net/nnja/pycon-2015-technical-debt-the-monster-in-your-closet)
> from Pycon 2015, especially slide 53, which I interpreted in terms of
> difficulty getting new contributors to submit patches to an OSS project like
> npm. npm has a long ways to go, but I feel good about this change.

* [`ed7e249`](https://github.com/npm/npm/commit/ed7e249d50444312cd266942ce3b89e1ca049bdf)
  [#7929](https://github.com/npm/npm/issues/7929) Eliminate fixture directories
  from `test/tap`, leaving each test self-contained.
  ([@othiym23](https://github.com/othiym23))
* [`4928d30`](https://github.com/npm/npm/commit/4928d30140821c63e03fffed73f8d88ebdc43710)
  [#7929](https://github.com/npm/npm/issues/7929) Move fixture files from
  `test/tap/*` to `test/fixtures`. ([@othiym23](https://github.com/othiym23))
* [`e925deb`](https://github.com/npm/npm/commit/e925debca91092a814c1a00933babc3a8cf975be)
  [#7929](https://github.com/npm/npm/issues/7929) Tweak the run scripts to stop
  slaughtering the CPU on doc rebuild.
  ([@othiym23](https://github.com/othiym23))
* [`65bf7cf`](https://github.com/npm/npm/commit/65bf7cffaf91c426b676c47529eee796f8b8b75c)
  [#7923](https://github.com/npm/npm/issues/7923) Use an alias of scripts and
  run-scripts in `npm run test-all` ([@watilde](https://github.com/watilde))
* [`756a3fb`](https://github.com/npm/npm/commit/756a3fbb852a2469afe706635ed88d22c37743e5)
  [#7923](https://github.com/npm/npm/issues/7923) Sync timeout time of `npm
  run-script test-all` to be the same as `test` and `tap` scripts.
  ([@watilde](https://github.com/watilde))
* [`8299b5f`](https://github.com/npm/npm/commit/8299b5fb6373354a7fbaab6f333863758812ae90)
  Set a timeout for tap tests for `npm run-script test-all`.
  ([@othiym23](https://github.com/othiym23))

#### THE EVER-BEATING DRUM OF DEPENDENCY UPDATES

* [`d90d0b9`](https://github.com/npm/npm/commit/d90d0b992acbf62fd5d68debf9d1dbd6cfa20804)
  [#7924](https://github.com/npm/npm/issues/7924) Remove `child-process-close`,
  as it was included for Node 0.6 compatibility, and npm no longer supports
  0.6. ([@robertkowalski](https://github.com/robertkowalski))
* [`16427c1`](https://github.com/npm/npm/commit/16427c1f3ea3d71ee753c62eb4c2663c7b32b84f)
  `lru-cache@2.5.2`: More accurate updating of expiry times when `maxAge` is
  set. ([@isaacs](https://github.com/isaacs))
* [`03cce83`](https://github.com/npm/npm/commit/03cce83b64344a9e0fe036dce214f4d68cfcc9e7)
  `nock@1.6.0`: Mocked network error handling.
  ([@pgte](https://github.com/pgte))
* [`f93b1f0`](https://github.com/npm/npm/commit/f93b1f0b7eb5d1b8a7967e837bbd756db1091d00)
  `glob@5.0.5`: Use `path-is-absolute` polyfill, allowing newer Node.js and
  io.js versions to use `path.isAbsolute()`.
  ([@sindresorhus](https://github.com/sindresorhus))
* [`a70d694`](https://github.com/npm/npm/commit/a70d69495a6e96997e64855d9e749d943ee6d64f)
  `request@2.55.0`: Bug fixes and simplification.
  ([@simov](https://github.com/simov))
* [`2aecc6f`](https://github.com/npm/npm/commit/2aecc6f4083526feeb14615b4e5484edc66175b5)
  `columnify@1.5.1`: Switch to using babel from 6to5.
  ([@timoxley](https://github.com/timoxley))

### v2.8.0 (2015-04-09):

#### WE WILL NEVER BE DONE FIXING NPM'S GIT SUPPORT

If you look at [the last release's release
notes](https://github.com/npm/npm/blob/master/CHANGELOG.md#git-mean-git-tuff-git-all-the-way-away-from-my-stuff),
you will note that they confidently assert that it's perfectly OK to force all
GitHub URLs through the same `git:` -> `git+ssh:` fallback flow for cloning. It
turns out that many users depend on `git+https:` URLs in their build
environments because they use GitHub auth tokens instead of SSH keys. Also, in
some cases you just want to be able to explicitly say how a given dependency
should be cloned from GitHub.

Because of the way we resolved the inconsistency in GitHub shorthand handling
[before](https://github.com/npm/npm/blob/master/CHANGELOG.md#bug-fixes-1), this
turned out to be difficult to work around. So instead of hacking around it, we
completely redid how git is handled within npm and its attendant packages.
Again. This time, we changed things so that `normalize-package-data` and
`read-package-json` leave more of the git logic to npm itself, which makes
handling shorthand syntax consistently much easier, and also allows users to
resume using explicit, fully-qualified git URLs without npm messing with them.

Here's a summary of what's changed:

* Instead of converting the GitHub shorthand syntax to a `git+ssh:`, `git:`, or
  `git+https:` URL and saving that, save the shorthand itself to
  `package.json`.
* If presented with shortcuts, try cloning via the git protocol, SSH, and HTTPS
  (in that order).
* No longer prompt for credentials -- it didn't work right with the spinner,
  and wasn't guaranteed to work anyway. We may experiment with doing this a
  better way in the future. Users can override this by setting `GIT_ASKPASS` in
  their environment if they want to experiment with interactive cloning, but
  should also set `--no-spin` on the npm command line (or run `npm config set
  spin=false`).
* **EXPERIMENTAL FEATURE**: Add support for `github:`, `gist:`, `bitbucket:`,
  and `gitlab:` shorthand prefixes. GitHub shortcuts will continue to be
  normalized to `org/repo` instead of being saved as `github:org/repo`, but
  `gitlab:`, `gist:`, and `bitbucket:` prefixes will be used on the command
  line and from `package.json`. BE CAREFUL WITH THIS. `package.json` files
  published with the new shorthand syntax can _only_ be read by `npm@2.8.0` and
  later, and this feature is mostly meant for playing around with it. If you
  want to save git dependencies in a form that older versions of npm can read,
  use `--save-exact`, which will save the git URL and resolved commit hash of
  the head of the branch in a manner similar to the way that `--save-exact`
  pins versions for registry dependencies.  This is documented (so check `npm
  help install` for details), but we're not going to make a lot of noise about
  it until it has a chance to bake in a little more.

It is [@othiym23](https://github.com/othiym23)'s sincere hope that this will
resolve all of the inconsistencies users were seeing with GitHub and git-hosted
packages, but given the level of change here, that may just be a fond wish.
Extra testing of this change is requested.

* [`6b0f588`](https://github.com/npm/npm/commit/6b0f58877f37df9904490ffbaaad33862bd36dce)
  [#7867](https://github.com/npm/npm/issues/7867) Use git shorthand and git
  URLs as presented by user. Support new `hosted-git-info` shortcut syntax.
  Save shorthand in `package.json`. Try cloning via `git:`, `git+ssh:`, and
  `git+https:`, in that order, when supported by the underlying hosting
  provider. ([@othiym23](https://github.com/othiym23))
* [`75d4267`](https://github.com/npm/npm/commit/75d426787869d54ca7400408f562f971b34649ef)
  [#7867](https://github.com/npm/npm/issues/7867) Document new GitHub, GitHub
  gist, Bitbucket, and GitLab shorthand syntax.
  ([@othiym23](https://github.com/othiym23))
* [`7d92c75`](https://github.com/npm/npm/commit/7d92c7592998d90ec883fa989ca74f04ec1b93de)
  [#7867](https://github.com/npm/npm/issues/7867) When `--save-exact` is used
  with git shorthand or URLs, save the fully-resolved URL, with branch name
  resolved to the exact hash for the commit checked out.
  ([@othiym23](https://github.com/othiym23))
* [`9220e59`](https://github.com/npm/npm/commit/9220e59f8def8c82c6d331a39ba29ad4c44e3a9b)
  [#7867](https://github.com/npm/npm/issues/7867) Ensure that non-prefixed and
  non-normalized GitHub shortcuts are saved to `package.json`.
  ([@othiym23](https://github.com/othiym23))
* [`dd398e9`](https://github.com/npm/npm/commit/dd398e98a8eba27eeba84378200da3d078fdf980)
  [#7867](https://github.com/npm/npm/issues/7867) `hosted-git-info@2.1.1`:
  Ensure that `gist:` shorthand survives being round-tripped through
  `package.json`. ([@othiym23](https://github.com/othiym23))
* [`33d1420`](https://github.com/npm/npm/commit/33d1420bf2f629332fceb2ac7e174e63ac48f96a)
  [#7867](https://github.com/npm/npm/issues/7867) `hosted-git-info@2.1.0`: Add
  support for auth embedded directly in git URLs.
  ([@othiym23](https://github.com/othiym23))
* [`23a1d5a`](https://github.com/npm/npm/commit/23a1d5a540e8db27f5cd0245de7c3694e2bddad1)
  [#7867](https://github.com/npm/npm/issues/7867) `hosted-git-info@2.0.2`: Make
  it possible to determine in which form a hosted git URL was passed.
  ([@iarna](https://github.com/iarna))
* [`eaf75ac`](https://github.com/npm/npm/commit/eaf75acb718611ad5cfb360084ec86938d9c66c5)
  [#7867](https://github.com/npm/npm/issues/7867)
  `normalize-package-data@2.0.0`: Normalize GitHub specifiers so they pass
  through shortcut syntax and preserve explicit URLs.
  ([@iarna](https://github.com/iarna))
* [`95e0535`](https://github.com/npm/npm/commit/95e0535e365e0aca49c634dd2061a0369b0475f1)
  [#7867](https://github.com/npm/npm/issues/7867) `npm-package-arg@4.0.0`: Add
  git URL and shortcut to hosted git spec and use `hosted-git-info@2.0.2`.
  ([@iarna](https://github.com/iarna))
* [`a808926`](https://github.com/npm/npm/commit/a8089268d5f3d57f42dbaba02ff6437da5121191)
  [#7867](https://github.com/npm/npm/issues/7867)
  `realize-package-specifier@3.0.0`: Use `npm-package-arg@4.0.0` and test
  shortcut specifier behavior. ([@iarna](https://github.com/iarna))
* [`6dd1e03`](https://github.com/npm/npm/commit/6dd1e039bddf8cf5383343f91d84bc5d78acd083)
  [#7867](https://github.com/npm/npm/issues/7867) `init-package-json@1.4.0`:
  Allow dependency on `read-package-json@2.0.0`.
  ([@iarna](https://github.com/iarna))
* [`63254bb`](https://github.com/npm/npm/commit/63254bb6358f66752aca6aa1a275271b3ae03f7c)
  [#7867](https://github.com/npm/npm/issues/7867) `read-installed@4.0.0`: Use
  `read-package-json@2.0.0`. ([@iarna](https://github.com/iarna))
* [`254b887`](https://github.com/npm/npm/commit/254b8871f5a173bb464cc5b0ace460c7878b8097)
  [#7867](https://github.com/npm/npm/issues/7867) `read-package-json@2.0.0`:
  Use `normalize-package-data@2.0.0`. ([@iarna](https://github.com/iarna))
* [`0b9f8be`](https://github.com/npm/npm/commit/0b9f8be62fe5252abe54d49e36a696f4816c2eca)
  [#7867](https://github.com/npm/npm/issues/7867) `npm-registry-client@6.3.0`:
  Mark compatibility with `normalize-package-data@2.0.0` and
  `npm-package-arg@4.0.0`. ([@iarna](https://github.com/iarna))
* [`f40ecaa`](https://github.com/npm/npm/commit/f40ecaad68f77abc50eb6f5b224e31dec3d250fc)
  [#7867](https://github.com/npm/npm/issues/7867) Extract a common method to
  use when cloning git repos for testing.
  ([@othiym23](https://github.com/othiym23))

#### TEST FIXES FOR NODE 0.8

npm continues to [get closer](https://github.com/npm/npm/issues/7842) to being
completely green on Travis for Node 0.8.

* [`26d36e9`](https://github.com/npm/npm/commit/26d36e9cf0eca69fe1863d2ea536c28555b9e8de)
  [#7842](https://github.com/npm/npm/issues/7842) When spawning child
  processes, map exit code 127 to ENOENT so Node 0.8 handles child process
  failures the same as later versions.
  ([@SonicHedgehog](https://github.com/SonicHedgehog))
* [`54cd895`](https://github.com/npm/npm/commit/54cd8956ea783f96749e46597d8c2cb9397c5d5f)
  [#7842](https://github.com/npm/npm/issues/7842) Node 0.8 requires -e with -p
  when evaluating snippets; fix test.
  ([@SonicHedgehog](https://github.com/SonicHedgehog))

#### SMALL FIX AND DOC TWEAK

* [`20e9003`](https://github.com/npm/npm/commit/20e90031b847e9f7c7168f3dad8b1e526f9a2586)
  `tar@2.0.1`: Fix regression where relative symbolic links within an
  extraction root that pointed within an extraction root would get normalized
  to absolute symbolic links. ([@isaacs](https://github.com/isaacs))
* [`2ef8898`](https://github.com/npm/npm/commit/2ef88989c41bee1578570bb2172c90ede129dbd1)
  [#7879](https://github.com/npm/npm/issues/7879) Better document that `npm
  publish --tag=foo` will not set `latest` to that version.
  ([@linclark](https://github.com/linclark))

### v2.7.6 (2015-04-02):

#### GIT MEAN, GIT TUFF, GIT ALL THE WAY AWAY FROM MY STUFF

Part of the reason that we're reluctant to take patches to how npm deals with
git dependencies is that every time we touch the git support, something breaks.
The last few releases are a case in point. `npm@2.7.4` completely broke
installing private modules from GitHub, and `npm@2.7.5` fixed them at the cost
of logging a misleading error message that caused many people to believe that
their dependencies hadn't been successfully installed when they actually had
been.

This all started from a desire to ensure that GitHub shortcut syntax is being
handled correctly.  The correct behavior is for npm to try to clone all
dependencies on GitHub (whether they're specified with the GitHub
`organization/repository` shortcut syntax or not) via the plain `git:` protocol
first, and to fall back to using `git+ssh:` if `git:` doesn't work. Previously,
sometimes npm would use `git:` and `git+ssh:` in some cases (most notably when
using GitHub shortcut syntax on the command line), and use `git+https:` in
others (when the GitHub shortcut syntax was present in `package.json`). This
led to subtle and hard-to-understand inconsistencies, and we're glad that as of
`npm@2.7.6`, we've finally gotten things to where they were before we started,
only slightly more consistent overall.

We are now going to go back to our policy of being extremely reluctant to touch
the code that handles Git dependencies.

* [`b747593`](https://github.com/npm/npm/commit/b7475936f473f029e6a027ba1b16277523747d0b)
  [#7630](https://github.com/npm/npm/issues/7630) Don't automatically log all
  git failures as errors. `maybeGithub` needs to be able to fail without
  logging to support its fallback logic.
  ([@othiym23](https://github.com/othiym23))
* [`cd67a0d`](https://github.com/npm/npm/commit/cd67a0db07891d20871822696c26692c8a84866a)
  [#7829](https://github.com/npm/npm/issues/7829) When fetching a git remote
  URL, handle failures gracefully (without assuming standard output exists).
  ([@othiym23](https://github.com/othiym23))
* [`637c7d1`](https://github.com/npm/npm/commit/637c7d1411fe07f409cf91f2e65fd70685cb253c)
  [#7829](https://github.com/npm/npm/issues/7829) When fetching a git remote
  URL, handle failures gracefully (without assuming standard _error_ exists).
  ([@othiym23](https://github.com/othiym23))

#### OTHER SIGNIFICANT FIXES

* [`78005eb`](https://github.com/npm/npm/commit/78005ebb6f4103c20f077669c3929b7ea46a4c0d)
  [#7743](https://github.com/npm/npm/issues/7743) Always quote arguments passed
  to `npm run-script`. This allows build systems and the like to safely escape
  glob patterns passed as arguments to `run-scripts` with `npm run-script
  <script> -- <arguments>`. This is a tricky change to test, and may be
  reverted or moved to `npm@3` if it turns out it breaks things for users.
  ([@mantoni](https://github.com/mantoni))
* [`da015ee`](https://github.com/npm/npm/commit/da015eee45f6daf384598151d06a9b57ffce136e)
  [#7074](https://github.com/npm/npm/issues/7074) `read-package-json@1.3.3`:
  `read-package-json` no longer caches `package.json` files, which trades a
  very small performance loss for the elimination of a large class of really
  annoying race conditions. See [#7074](https://github.com/npm/npm/issues/7074)
  for the grisly details. ([@othiym23](https://github.com/othiym23))
* [`dd20f57`](https://github.com/npm/npm/commit/dd20f5755291b9433f0d298ee0eead22cda6db36)
  `init-package-json@1.3.2`: Only add the `@` to scoped package names if it's
  not already there when reading from the filesystem
  ([@watilde](https://github.com/watilde)), and support inline validation of
  package names ([@michaelnisi](https://github.com/michaelnisi)).

#### SMALL FIXES AND DEPENDENCY UPGRADES

* [`1f380f6`](https://github.com/npm/npm/commit/1f380f66c1e944b8ffbf096fa94d09e931626e12)
  [#7820](https://github.com/npm/npm/issues/7820) `are-we-there-yet@1.0.4`: Use
  `readable-stream` instead of built-in `stream` module to better support
  Node.js 0.8.x. ([@SonicHedgehog](https://github.com/SonicHedgehog))
* [`d380188`](https://github.com/npm/npm/commit/d380188e161be31f5a4f53947de6bc28df4732d8)
  `semver@4.3.3`: Don't throw on `semver.parse(null)`, and parse numeric
  version strings more robustly. ([@isaacs](https://github.com/isaacs))
* [`01d9964`](https://github.com/npm/npm/commit/01d99649265f921e1c61cf406613e7042bcea008)
  `nock@1.4.0`: This change may need to be rolled back, or rolled forward,
  because [nock depends on
  `setImmediate`](https://github.com/npm/npm/issues/7842), which causes tests
  to fail when run with Node.js 0.8. ([@othiym23](https://github.com/othiym23))
* [`91f5cb1`](https://github.com/npm/npm/commit/91f5cb1fb91520fbe25a4da5b80848ed540b9ad3)
  [#7791](https://github.com/npm/npm/issues/7791) Fix brackets in npmconf so
  that `loaded` is set correctly.
  ([@charmander](https://github.com/charmander))
* [`1349e27`](https://github.com/npm/npm/commit/1349e27c936a8b0fc9f6440a6d6404ef3b19c587)
  [#7818](https://github.com/npm/npm/issues/7818) Update `README.md` to point
  out that the install script now lives on https://www.npmjs.com.
  ([@weisjohn](https://github.com/weisjohn))

### v2.7.5 (2015-03-26):

#### SECURITY FIXES

* [`300834e`](https://github.com/npm/npm/commit/300834e91a4e2a95fb7fb59c309e7c3fc91d2312)
  `tar@2.0.0`: Normalize symbolic links that point to targets outside the
  extraction root. This prevents packages containing symbolic links from
  overwriting targets outside the expected paths for a package. Thanks to [Tim
  Cuthbertson](http://gfxmonk.net/) and the team at [Lift
  Security](https://liftsecurity.io/) for working with the npm team to identify
  this issue. ([@othiym23](https://github.com/othiym23))
* [`0dc6875`](https://github.com/npm/npm/commit/0dc68757cffd5397c280bc71365d106523a5a052)
  `semver@4.3.2`: Package versions can be no more than 256 characters long.
  This prevents a situation in which parsing the version number can use
  exponentially more time and memory to parse, leading to a potential denial of
  service. Thanks to Adam Baldwin at Lift Security for bringing this to our
  attention.  ([@isaacs](https://github.com/isaacs))

#### BUG FIXES

* [`5811468`](https://github.com/npm/npm/commit/5811468e104ccb6b26b8715dff390d68daa10066)
  [#7713](https://github.com/npm/npm/issues/7713) Add a test for `npm link` and
  `npm link <package>`. ([@watilde](https://github.com/watilde))
* [`3cf3b0c`](https://github.com/npm/npm/commit/3cf3b0c8fddb6b66f969969feebea85fabd0360b)
  [#7713](https://github.com/npm/npm/issues/7713) Only use absolute symbolic
  links when `npm link`ing. ([@hokaccha](https://github.com/hokaccha))
* [`f35aa93`](https://github.com/npm/npm/commit/f35aa933e136228a89e3fcfdebe8c7cc4f1e7c00)
  [#7443](https://github.com/npm/npm/issues/7443) Keep relative URLs when
  hitting search endpoint. ([@othiym23](https://github.com/othiym23))
* [`eab6184`](https://github.com/npm/npm/commit/eab618425c51e3aa4416da28dcd8ca4ba63aec41)
  [#7766](https://github.com/npm/npm/issues/7766) One last tweak to ensure that
  GitHub shortcuts work with private repositories.
  ([@iarna](https://github.com/iarna))
* [`5d7f704`](https://github.com/npm/npm/commit/5d7f704823f5f92ddd7ff3e7dd2b8bcc66c73005)
  [#7656](https://github.com/npm/npm/issues/7656) Don't try to load a deleted
  CA file, allowing the `cafile` config to be changed.
  ([@KenanY](https://github.com/KenanY))
* [`a840a13`](https://github.com/npm/npm/commit/a840a13bbf0330157536381ea8e58d0bd93b4c05)
  [#7746](https://github.com/npm/npm/issues/7746) Only fix up URL paths when
  there are paths to fix up. ([@othiym23](https://github.com/othiym23))

#### DEPENDENCY UPDATES

* [`94df809`](https://github.com/npm/npm/commit/94df8095985bf5ba9d8db99dc445d05dac136aaf)
  `request@2.54.0`: Fixes for Node.js 0.12 and io.js.
  ([@simov](https://github.com/simov))
* [`98a13ea`](https://github.com/npm/npm/commit/98a13eafdf098b53069ad15297008fcab9c61653)
  `opener@1.4.1`: Deal with `start` on Windows more conventionally.
  ([@domenic](https://github.com/domenic))
* [`c2417c7`](https://github.com/npm/npm/commit/c2417c7702459a446f07d43ca3c4e99bde7fe9d6)
  `require-inject@1.2.0`: Add installGlobally to bypass cleanups.
  ([@iarna](https://github.com/iarna))

#### DOCUMENTATION FIXES

* [`f87c728`](https://github.com/npm/npm/commit/f87c728f8732c9e977c0dc2060c0610649e79155)
  [#7696](https://github.com/npm/npm/issues/7696) Months and minutes were
  swapped in doc-build.sh ([@MeddahJ](https://github.com/MeddahJ))
* [`4e216b2`](https://github.com/npm/npm/commit/4e216b29b30463f06afe6e3c645e205da5f50922)
  [#7752](https://github.com/npm/npm/issues/7752) Update string examples to be
  properly quoted. ([@snuggs](https://github.com/snuggs))
* [`402f52a`](https://github.com/npm/npm/commit/402f52ab201efa348feb87cad753fc4b91e8a3fb)
  [#7635](https://github.com/npm/npm/issues/7635) Clarify Windows installation
  instructions. ([@msikma](https://github.com/msikma))
* [`c910399`](https://github.com/npm/npm/commit/c910399ecfd8db49fe4496dd26887765a8aed20f)
  small typo fix to `CHANGELOG.md` ([@e-jigsaw](https://github.com/e-jigsaw))

### v2.7.4 (2015-03-20):

#### BUG FIXES

* [`fe1bc38`](https://github.com/npm/npm/commit/fe1bc387a14475e373557de669e03d9d006d3173)
  [#7672](https://github.com/npm/npm/issues/7672) `npm-registry-client@3.1.2`:
  Fix client-side certificate handling by correcting property name.
  ([@atamon](https://github.com/atamon))
* [`3ce3cc2`](https://github.com/npm/npm/commit/3ce3cc242fc345bca6820185a4f5a013c5bc1944)
  [#7635](https://github.com/npm/npm/issues/7635) `fstream-npm@1.0.2`: Raise a
  more descriptive error when `bundledDependencies` isn't an array.
  ([@KenanY](https://github.com/KenanY))
* [`3a12723`](https://github.com/npm/npm/commit/3a127235076a1f00bc8befba56c024c6d0e7f477)
  [#7661](https://github.com/npm/npm/issues/7661) Allow setting `--registry` on
  the command line to trump the mapped registry for `--scope`.
  ([@othiym23](https://github.com/othiym23))
* [`89ce829`](https://github.com/npm/npm/commit/89ce829a00b526d0518f5cd855c323bffe182af0)
  [#7630](https://github.com/npm/npm/issues/7630) `hosted-git-info@1.5.3`: Part
  3 of ensuring that GitHub shorthand is handled consistently.
  ([@othiym23](https://github.com/othiym23))
* [`63313eb`](https://github.com/npm/npm/commit/63313eb0c37891c355546fd1093010c8a0c3cd81)
  [#7630](https://github.com/npm/npm/issues/7630)
  `realize-package-specifier@2.2.0`: Part 2 of ensuring that GitHub shorthand
  is handled consistently. ([@othiym23](https://github.com/othiym23))
* [`3ed41bf`](https://github.com/npm/npm/commit/3ed41bf64a1bb752bb3155c74dd6ffbbd28c89c9)
  [#7630](https://github.com/npm/npm/issues/7630) `npm-package-arg@3.1.1`: Part
  1 of ensuring that GitHub shorthand is handled consistently.
  ([@othiym23](https://github.com/othiym23))

#### DEPENDENCY UPDATES

* [`6a498c6`](https://github.com/npm/npm/commit/6a498c6aaa00611a0a1ea405255900c327103f8b)
  `npm-registry-couchapp@2.6.7`: Ensure that npm continues to work with new
  registry architecture. ([@bcoe](https://github.com/bcoe))
* [`bd72c47`](https://github.com/npm/npm/commit/bd72c47ce8c58e287d496902c11845c8fea420d6)
  `glob@5.0.3`: Updated to latest version.
  ([@isaacs](https://github.com/isaacs))
* [`4bfbaa2`](https://github.com/npm/npm/commit/4bfbaa2d8b9dc7067d999de8f55676db3a4f4196)
  `npmlog@1.2.0`: Getting up to date with latest version (but not using any of
  the new features). ([@othiym23](https://github.com/othiym23))

#### A NEW REGRESSION TEST

* [`3703b0b`](https://github.com/npm/npm/commit/3703b0b87c127a64649bdbfc3bc697ebccc4aa24)
  Add regression test for `npm version` to ensure `message` property in config
  continues to be honored. ([@dannyfritz](https://github.com/dannyfritz))

### v2.7.3 (2015-03-16):

#### HAHA WHOOPS LIL SHINKWRAP ISSUE THERE LOL

* [`1549106`](https://github.com/npm/npm/commit/1549106f518000633915686f5f1ccc6afcf77f8f)
  [#7641](https://github.com/npm/npm/issues/7641) Due to 448efd0, running `npm
  shrinkwrap --dev` caused production dependencies to no longer be included in
  `npm-shrinkwrap.json`. Whoopsie! ([@othiym23](https://github.com/othiym23))

### v2.7.2 (2015-03-12):

#### NPM GASTROENTEROLOGY

* [`fb0ac26`](https://github.com/npm/npm/commit/fb0ac26eecdd76f6eaa4a96a865b7c6f52ce5aa5)
  [#7579](https://github.com/npm/npm/issues/7579) Only block removing files and
  links when we're sure npm isn't responsible for them. This change is hard to
  summarize, because if things are working correctly you should never see it,
  but if you want more context, just [go read the commit
  message](https://github.com/npm/npm/commit/fb0ac26eecdd76f6eaa4a96a865b7c6f52ce5aa5),
  which lays it all out. ([@othiym23](https://github.com/othiym23))
* [`051c473`](https://github.com/npm/npm/commit/051c4738486a826300f205b71590781ce7744f01)
  [#7552](https://github.com/npm/npm/issues/7552) `bundledDependencies` are now
  properly included in the installation context. This is another fantastically
  hard-to-summarize bug, and once again, I encourage you to [read the commit
  message](https://github.com/npm/npm/commit/051c4738486a826300f205b71590781ce7744f01)
  if you're curious about the details. The snappy takeaway is that this
  unbreaks many use cases for `ember-cli`. ([@othiym23](https://github.com/othiym23))

#### LESS DRAMATIC CHANGES

* [`fcd9247`](https://github.com/npm/npm/commit/fcd92476f3a9092f6f8c83a19a24fe63b206edcd)
  [#7597](https://github.com/npm/npm/issues/7597) Awk varies pretty
  dramatically from platform to platform, so use Perl to generate the AUTHORS
  list instead. ([@KenanY](https://github.com/KenanY))
* [`721b17a`](https://github.com/npm/npm/commit/721b17a31690bec074eb8763d823d6de63406005)
  [#7598](https://github.com/npm/npm/issues/7598) `npm install --save` really
  isn't experimental anymore. ([@RichardLitt](https://github.com/RichardLitt))

#### DEPENDENCY REFRESH

* [`a91f2c7`](https://github.com/npm/npm/commit/a91f2c7c9a5183d9cde7aae040ebd9ccdf104be7)
  [#7559](https://github.com/npm/npm/issues/7559) `node-gyp@1.0.3` Switch
  `node-gyp` to use `stdio` instead of `customFds` so it stops printing a
  deprecation warning every time you build a native dependency.
  ([@jeffbski](https://github.com/jeffbski))
* [`0c85db7`](https://github.com/npm/npm/commit/0c85db7f0dde41762411e40a029153e6a65ef483)
  `rimraf@2.3.2`: Globbing now deals with paths containing valid glob
  metacharacters better. ([@isaacs](https://github.com/isaacs))
* [`d14588e`](https://github.com/npm/npm/commit/d14588ed09b032c4c770e34b4c0f2436f5fccf6e)
  `minimatch@2.0.4`: Bug fixes. ([@isaacs](https://github.com/isaacs))
* [`aa9952e`](https://github.com/npm/npm/commit/aa9952e8270a6c1b7f97e579875dd6e3aa22abfd)
  `graceful-fs@3.0.6`: Bug fixes. ([@isaacs](https://github.com/isaacs))

### v2.7.1 (2015-03-05):

#### GITSANITY

* [`6823807`](https://github.com/npm/npm/commit/6823807bba6c00228a724e1205ae90d67df0adad)
  [#7121](https://github.com/npm/npm/issues/7121) `npm install --save` for Git
  dependencies saves the URL passed in, instead of the temporary directory used
  to clone the remote repo. Fixes using Git dependencies when shrinkwrapping.
  In the process, rewrote the Git dependency caching code. Again. No more
  single-letter variable names, and a much clearer workflow.
  ([@othiym23](https://github.com/othiym23))
* [`c8258f3`](https://github.com/npm/npm/commit/c8258f31365b045e5fcf15b865a363abbc3be616)
  [#7486](https://github.com/npm/npm/issues/7486) When installing Git remotes,
  the caching code was passing in the function `gitEnv` instead of the results
  of invoking it. ([@functino](https://github.com/functino))
* [`c618eed`](https://github.com/npm/npm/commit/c618eeda3e321fd454d77c476b53a0330f2344cc)
  [#2556](https://github.com/npm/npm/issues/2556) Make it possible to install
  Git dependencies when using `--link` by not linking just the Git
  dependencies. ([@smikes](https://github.com/smikes))

#### WHY DID THIS TAKE SO LONG.

* [`abdd040`](https://github.com/npm/npm/commit/abdd040da90932535472f593d5433a67ee074801)
  `read-package-json@1.3.2`: Provide more helpful error messages when JSON
  parse errors are encountered by using a more forgiving JSON parser than
  JSON.parse. ([@smikes](https://github.com/smikes))

#### BUGS & TWEAKS

* [`c56cfcd`](https://github.com/npm/npm/commit/c56cfcd79cd8ab4ccd06d2c03d7e04030d576683)
  [#7525](https://github.com/npm/npm/issues/7525) `npm dedupe` handles scoped
  packages. ([@KidkArolis](https://github.com/KidkArolis))
* [`1b8ba74`](https://github.com/npm/npm/commit/1b8ba7426393cbae2c76ad2c35953782d4401871)
  [#7531](https://github.com/npm/npm/issues/7531) `npm stars` and `npm whoami`
  will no longer send the registry the error text saying you need to log in as
  your username.  ([@othiym23](https://github.com/othiym23))
* [`6de1e91`](https://github.com/npm/npm/commit/6de1e91116a5105dfa75126532b9083d8672e034)
  [#6441](https://github.com/npm/npm/issues/6441) Prevent needless reinstalls
  by only updating packages when the current version isn't the same as the
  version returned as `wanted` by `npm outdated`.
  ([@othiym23](https://github.com/othiym23))
* [`2abc3ee`](https://github.com/npm/npm/commit/2abc3ee08f0cabc4e7bfd7b973c0b59dc44715ff)
  Add `npm upgrade` as an alias for `npm update`.
  ([@othiym23](https://github.com/othiym23))
* [`bcd4722`](https://github.com/npm/npm/commit/bcd47224e18884191a5d0057c2b2fff83ac8206e)
  [#7508](https://github.com/npm/npm/issues/7508) FreeBSD uses `EAI_FAIL`
  instead of `ENOTFOUND`. ([@othiym23](https://github.com/othiym23))
* [`21c1ac4`](https://github.com/npm/npm/commit/21c1ac41280f0716a208cde14025a2ad5ef61fed)
  [#7507](https://github.com/npm/npm/issues/7507) Update support URL in generic
  error handler to `https:` from `http:`.
  ([@watilde](https://github.com/watilde))
* [`b6bd99a`](https://github.com/npm/npm/commit/b6bd99a73f575545fbbaef95c12237c47dd32561)
  [#7492](https://github.com/npm/npm/issues/7492) On install, the
  `package.json` `engineStrict` deprecation only warns for the current package.
  ([@othiym23](https://github.com/othiym23))
* [`4ef1412`](https://github.com/npm/npm/commit/4ef1412d0061239da2b1c4460ed6db37cc9ded27)
  [#7075](https://github.com/npm/npm/issues/7075) If you try to tag a release
  as a valid semver range, `npm publish` and `npm tag` will error early instead
  of proceeding. ([@smikes](https://github.com/smikes))
* [`ad53d0f`](https://github.com/npm/npm/commit/ad53d0f666125d9f50d661b54901c6e5bab4d603)
  Use `rimraf` in npm build script because Windows doesn't know what rm is.
  ([@othiym23](https://github.com/othiym23))
* [`8885c4d`](https://github.com/npm/npm/commit/8885c4dfb618f2838930b5c5149abea300a762d6)
  `rimraf@2.3.1`: Better Windows support.
  ([@isaacs](https://github.com/isaacs))
* [`8885c4d`](https://github.com/npm/npm/commit/8885c4dfb618f2838930b5c5149abea300a762d6)
  `glob@4.4.2`: Handle bad symlinks properly.
  ([@isaacs](https://github.com/isaacs))

###E TYPSO & CLARFIICATIONS

dId yuo know that submiting fxies for doc tpyos is an exclelent way to get
strated contriburting to a new open-saurce porject?

* [`42c605c`](https://github.com/npm/npm/commit/42c605c7b401f603c32ea70427e1a7666adeafd9)
  Fix typo in `CHANGELOG.md` ([@adrianblynch](https://github.com/adrianblynch))
* [`c9bd58d`](https://github.com/npm/npm/commit/c9bd58dd637b9c41441023584a13e3818d5db336)
  Add note about `node_modules/.bin` being added to the path in `npm
  run-script`. ([@quarterto](https://github.com/quarterto))
* [`903bdd1`](https://github.com/npm/npm/commit/903bdd105b205d6e45d3a2ab83eea8e4071e9aeb)
  Matt Ranney confused the world when he renamed `node-redis` to `redis`. "The
  world" includes npm's documentation.
  ([@RichardLitt](https://github.com/RichardLitt))
* [`dea9bb2`](https://github.com/npm/npm/commit/dea9bb2319183fe54bf4d173d8533d46d2c6611c)
  Fix typo in contributor link. ([@watilde](https://github.com/watilde))
* [`1226ca9`](https://github.com/npm/npm/commit/1226ca98d4d7650cc3ba16bf7ac62e44820f3bfa)
  Properly close code block in npm-install.md.
  ([@olizilla](https://github.com/olizilla))

### v2.7.0 (2015-02-26):

#### SOMETIMES SEMVER MEANS "SUBJECTIVE-EMPATHETIC VERSIONING"

For a very long time (maybe forever?), the documentation for `npm run-script`
has said that `npm restart` will only call `npm stop` and `npm start` when
there is no command defined as `npm restart` in `package.json`. The problem
with this documentation is that `npm run-script` was apparently never wired up
to actually work this way.

Until now.

If the patch below were landed on its own, free of context, it would be a
breaking change. But, since the "new" behavior is how the documentation claims
this feature has always worked, I'm classifying it as a patch-level bug fix. I
apologize in advance if this breaks anybody's deployment scripts, and if it
turns out to be a significant regression in practice, we can revert this change
and move it to `npm@3`, which is allowed to make breaking changes due to being
a new major version of semver.

* [`2f6a1df`](https://github.com/npm/npm/commit/2f6a1df3e1e3e0a3bc4abb69e40f59a64204e7aa)
  [#1999](https://github.com/npm/npm/issues/1999) Only run `stop` and `start`
  scripts (plus their pre- and post- scripts) when there's no `restart` script
  defined. This makes it easier to support graceful restarts of services
  managed by npm.  ([@watilde](https://github.com/watilde) /
  [@scien](https://github.com/scien))

#### A SMALL FEATURE WITH BIG IMPLICATIONS

* [`145af65`](https://github.com/npm/npm/commit/145af6587f45de135cc876be2027ed818ed4ca6a)
  [#4887](https://github.com/npm/npm/issues/4887) Replace calls to the
  `node-gyp` script bundled with npm by passing the
  `--node-gyp=/path/to/node-gyp` option to npm. Swap in `pangyp` or a version
  of `node-gyp` modified to work better with io.js without having to touch
  npm's code!  ([@ackalker](https://github.com/ackalker))

#### [@WATILDE'S](https://github.com/watilde) NPM USABILITY CORNER

Following `npm@2.6.1`'s unexpected fix of many of the issues with `npm update
-g` simply by making `--depth=0` the default for `npm outdated`, friend of npm
[@watilde](https://github.com/watilde) has made several modest changes to npm's
behavior that together justify bumping npm's minor version, as well as making
npm significantly more pleasant to use:

* [`448efd0`](https://github.com/npm/npm/commit/448efd0eaa6f97af0889bf47efc543a1ea2f8d7e)
  [#2853](https://github.com/npm/npm/issues/2853) Add support for `--dev` and
  `--prod` to `npm ls`, so that you can list only the trees of production or
  development dependencies, as desired.
  ([@watilde](https://github.com/watilde))
* [`a0a8777`](https://github.com/npm/npm/commit/a0a87777af8bee180e4e9321699f050c29ed5ac4)
  [#7463](https://github.com/npm/npm/issues/7463) Split the list printed by
  `npm run-script` into lifecycle scripts and scripts directly invoked via `npm
  run-script`. ([@watilde](https://github.com/watilde))
* [`a5edc17`](https://github.com/npm/npm/commit/a5edc17d5ef1435b468a445156a4a109df80f92b)
  [#6749](https://github.com/npm/npm/issues/6749) `init-package-json@1.3.1`:
  Support for passing scopes to `npm init` so packages are initialized as part
  of that scope / organization / team. ([@watilde](https://github.com/watilde))

#### SMALLER FEATURES AND FIXES

It turns out that quite a few pull requests had piled up on npm's issue
tracker, and they included some nice small features and fixes:

* [`f33e8b8`](https://github.com/npm/npm/commit/f33e8b8ff2de094071c5976be95e35110cf2ab1a)
  [#7354](https://github.com/npm/npm/issues/7354) Add `--if-present` flag to
  allow e.g. CI systems to call (semi-) standard build tasks defined in
  `package.json`, but don't raise an error if no such script is defined.
  ([@jussi-kalliokoski](https://github.com/jussi-kalliokoski))
* [`7bf85cc`](https://github.com/npm/npm/commit/7bf85cc372ab5698593b01e139c383fa62c92516)
  [#4005](https://github.com/npm/npm/issues/4005)
  [#6248](https://github.com/npm/npm/issues/6248) Globally unlink a package
  when `npm rm` / `npm unlink` is called with no arguments.
  ([@isaacs](https://github.com/isaacs))
* [`a2e04bd`](https://github.com/npm/npm/commit/a2e04bd921feab8f9e40a27e180ca9308eb709d7)
  [#7294](https://github.com/npm/npm/issues/7294) Ensure that when depending on
  `git+<proto>` URLs, npm doesn't keep tacking additional `git+` prefixes onto
  the front. ([@twhid](https://github.com/twhid))
* [`0f87f5e`](https://github.com/npm/npm/commit/0f87f5ed28960d962f34977953561d22983da4f9)
  [#6422](https://github.com/npm/npm/issues/6422) When depending on GitHub
  private repositories, make sure we construct the Git URLS correctly.
  ([@othiym23](https://github.com/othiym23))
* [`50f461d`](https://github.com/npm/npm/commit/50f461d248c4d22e881a9535dccc1d57d994dbc7)
  [#4595](https://github.com/npm/npm/issues/4595) Support finding compressed
  manpages. It's still up to the system to figure out how to display them,
  though. ([@pshevtsov](https://github.com/pshevtsov))
* [`44da664`](https://github.com/npm/npm/commit/44da66456b530c049ff50953f78368460df87461)
  [#7465](https://github.com/npm/npm/issues/7465) When calling git, log the
  **full** command, with all arguments, on error.
  ([@thriqon](https://github.com/thriqon))
* [`9748d5c`](https://github.com/npm/npm/commit/9748d5cd195d0269b32caf45129a93d29359a796)
  Add parent to error on `ETARGET` error.
  ([@davglass](https://github.com/davglass))
* [`37038d7`](https://github.com/npm/npm/commit/37038d7db47a986001f77ac17b3e164000fc8ff3)
  [#4663](https://github.com/npm/npm/issues/4663) Remove hackaround for Linux
  tests, as it's evidently no longer necessary.
  ([@mmalecki](https://github.com/mmalecki))
* [`d7b7853`](https://github.com/npm/npm/commit/d7b785393dffce93bb70317fbc039a6428ca37c5)
  [#2612](https://github.com/npm/npm/issues/2612) Add support for path
  completion on `npm install`, which narrows completion to only directories
  containing `package.json` files. ([@deestan](https://github.com/deestan))
* [`628fcdb`](https://github.com/npm/npm/commit/628fcdb0be4e14c0312085a50dc2ae01dc713fa6)
  Remove all command completion calls to `-/short`, because it's been removed
  from the primary registry for quite some time, and is generally a poor idea
  on any registry with more than a few hundred packages.
  ([@othiym23](https://github.com/othiym23))
* [`3f6061d`](https://github.com/npm/npm/commit/3f6061d75650441ee690472d1fa9c8dd7a7b1b28)
  [#6659](https://github.com/npm/npm/issues/6659) Instead of removing zsh
  completion global, make it a local instead.
  ([@othiym23](https://github.com/othiym23))

#### DOCUMENTATION TWEAKS

* [`5bc70e6`](https://github.com/npm/npm/commit/5bc70e6cfb3598da433806c6f447fc94c8e1d35d)
  [#7417](https://github.com/npm/npm/issues/7417) Provide concrete examples of
  how the new `npm update` defaults work in practice, tied to actual test
  cases. Everyone interested in using `npm update -g` now that it's been fixed
  should read these documents, as should anyone interested in writing
  documentation for npm. ([@smikes](https://github.com/smikes))
* [`8ac6f21`](https://github.com/npm/npm/commit/8ac6f2123a6af13dc9447fad96ec9cb583c45a71)
  [#6543](https://github.com/npm/npm/issues/6543) Clarify `npm-scripts`
  warnings to de-emphasize dangers of using `install` scripts.
  ([@zeke](https://github.com/zeke))
* [`ebe3b37`](https://github.com/npm/npm/commit/ebe3b37098efdada41dcc4c52a291e29296ea242)
  [#6711](https://github.com/npm/npm/issues/6711) Note that git tagging of
  versions can be disabled via `--no-git-tag-verson`.
  ([@smikes](https://github.com/smikes))
* [`2ef5771`](https://github.com/npm/npm/commit/2ef5771632006e6cee8cf17f836c0f98ab494bd1)
  [#6711](https://github.com/npm/npm/issues/6711) Document `git-tag-version`
  configuration option. ([@KenanY](https://github.com/KenanY))
* [`95e59b2`](https://github.com/npm/npm/commit/95e59b287c9517780318e145371a859e8ebb2d20)
  Document that `NODE_ENV=production` behaves analogously to `--production` on
  `npm install`. ([@stefaneg](https://github.com/stefaneg))
* [`687117a`](https://github.com/npm/npm/commit/687117a5bcd6a838cd1532ea7020ec6fcf0c33c0)
  [#7463](https://github.com/npm/npm/issues/7463) Document the new script
  grouping behavior in the man page for `npm run-script`.
  ([@othiym23](https://github.com/othiym23))
* [`536b2b6`](https://github.com/npm/npm/commit/536b2b6f55c349247b3e79b5d11b4c033ef5a3df)
  Rescue one of the the disabled tests and make it work properly.
  ([@smikes](https://github.com/smikes))

#### DEPENDENCY UPDATES

* [`89fc6a4`](https://github.com/npm/npm/commit/89fc6a4e7ff8c524675fcc14493ca0a1e3a76d38)
  `which@1.0.9`: Test for being run as root, as well as the current user.
  ([@isaacs](https://github.com/isaacs))
* [`5d0612f`](https://github.com/npm/npm/commit/5d0612f31e226cba32a05351c47b055c0ab6c557)
  `glob@4.4.1`: Better error message to explain why calling sync glob with a
  callback results in an error. ([@isaacs](https://github.com/isaacs))
* [`64b07f6`](https://github.com/npm/npm/commit/64b07f6caf6cb07e4102f1e4e5f2ff2b944e452e)
  `tap@0.7.1`: More accurate counts of pending & skipped tests.
  ([@rmg](https://github.com/rmg))
* [`8fda451`](https://github.com/npm/npm/commit/8fda45195dae1d6f792be556abe87f7763fab09b)
  `semver@4.3.1`: Make official the fact that `node-semver` has moved from
  [@isaacs](https://github.com/isaacs)'s organization to
  [@npm](https://github.com/npm)'s. ([@isaacs](https://github.com/isaacs))

### v2.6.1 (2015-02-19):

* [`8b98f0e`](https://github.com/npm/npm/commit/8b98f0e709d77a8616c944aebd48ab726f726f76)
  [#4471](https://github.com/npm/npm/issues/4471) `npm outdated` (and only `npm
  outdated`) now defaults to `--depth=0`. See the [docs for
  `--depth`](https://github.com/npm/npm/blob/82f484672adb1a3caf526a8a48832789495bb43d/doc/misc/npm-config.md#depth)
  for the mildly confusing details. ([@smikes](https://github.com/smikes))
* [`aa79194`](https://github.com/npm/npm/commit/aa791942a9f3c8af6a650edec72a675deb7a7c6e)
  [#6565](https://github.com/npm/npm/issues/6565) Tweak `peerDependency`
  deprecation warning to include which peer dependency on which package is
  going to need to change. ([@othiym23](https://github.com/othiym23))
* [`5fa067f`](https://github.com/npm/npm/commit/5fa067fd47682ac3cdb12a2b009d8ca59b05f992)
  [#7171](https://github.com/npm/npm/issues/7171) Tweak `engineStrict`
  deprecation warning to include which `package.json` is using it.
  ([@othiym23](https://github.com/othiym23))
* [`0fe0caa`](https://github.com/npm/npm/commit/0fe0caa7eddb7acdacbe5ee81ceabaca27175c78)
  `glob@4.4.0`: Glob patterns can now ignore matches.
  ([@isaacs](https://github.com/isaacs))

### v2.6.0 (2015-02-12):

#### A LONG-AWAITED GUEST

* [`38c4825`](https://github.com/npm/npm/commit/38c48254d3d217b4babf5027cb39492be4052fc2)
  [#5068](https://github.com/npm/npm/issues/5068) Add new logout command, and
  make it do something useful on both bearer-based and basic-based authed
  clients. ([@othiym23](https://github.com/othiym23))
* [`4bf0f5d`](https://github.com/npm/npm/commit/4bf0f5d56c33649124b486e016ba4a620c105c1c)
  `npm-registry-client@6.1.1`: Support new `logout` endpoint to invalidate
  token for sessions. ([@othiym23](https://github.com/othiym23))

#### DEPRECATIONS

* [`c8e08e6`](https://github.com/npm/npm/commit/c8e08e6d91f4016c80f572aac5a2080df0f78098)
  [#6565](https://github.com/npm/npm/issues/6565) Warn that `peerDependency`
  behavior is changing and add a note to the docs.
  ([@othiym23](https://github.com/othiym23))
* [`7c81a5f`](https://github.com/npm/npm/commit/7c81a5f5f058941f635a92f22641ea68e79b60db)
  [#7171](https://github.com/npm/npm/issues/7171) Warn that `engineStrict` in
  `package.json` will be going away in the next major version of npm (coming
  soon!) ([@othiym23](https://github.com/othiym23))

#### BUG FIXES & TWEAKS

* [`add5890`](https://github.com/npm/npm/commit/add5890ce447dabf120b907a85f715df1e065f44)
  [#4668](https://github.com/npm/npm/issues/4668) `read-package-json@1.3.1`:
  Warn when a `bin` symbolic link is a dangling reference.
  ([@nicks](https://github.com/nicks))
* [`4b42071`](https://github.com/npm/npm/commit/4b420714dfb84338d85def78c30bd665e32d72c1)
  `semver@4.3.0`: Add functions to extract parts of the version triple, fix a
  typo. ([@isaacs](https://github.com/isaacs))
* [`a9aff38`](https://github.com/npm/npm/commit/a9aff38719918486fc381d67ad3371c475632ff7)
  Use full path for man pages as the symbolic link source, instead of just the
  file name. ([@bengl](https://github.com/bengl))
* [`6fd0fbd`](https://github.com/npm/npm/commit/6fd0fbd8a0347fd47cb7ee0064e0902a2f8a087c)
  [#7233](https://github.com/npm/npm/issues/7233) Ensure `globalconfig` path
  exists before trying to edit it. ([@ljharb](https://github.com/ljharb))
* [`a0a2620`](https://github.com/npm/npm/commit/a0a262047647d9e2690cebe5a89e6a0dd33202bb)
  `ini@1.3.3`: Allow embedded, quoted equals signs in ini field names.
  ([@isaacs](https://github.com/isaacs))

Also typos and other documentation issues were addressed by
[@rutsky](https://github.com/rutsky), [@imurchie](https://github.com/imurchie),
[@marcin-wosinek](https://github.com/marcin-wosinek),
[@marr](https://github.com/marr), [@amZotti](https://github.com/amZotti), and
[@karlhorky](https://github.com/karlhorky). Thank you, everyone!

### v2.5.1 (2015-02-06):

This release doesn't look like much, but considerable effort went into ensuring
that npm's tests will pass on io.js 1.1.0 and Node 0.11.16 / 0.12.0 on both OS
X and Linux.

**NOTE:** there are no actual changes to npm's code in `npm@2.5.1`. Only test
code (and the upgrade of `request` to the latest version) has changed.

#### `npm-registry-mock@1.0.0`:

* [`0e8d473`](https://github.com/npm/npm/commit/0e8d4736a1cbdda41ae8eba8a02c7ff7ce80c2ff)
  [#7281](https://github.com/npm/npm/issues/7281) `npm-registry-mock@1.0.0`:
  Clean up API, set `connection: close`.
  ([@robertkowalski](https://github.com/robertkowalski))
* [`4707bba`](https://github.com/npm/npm/commit/4707bba7d44dfab85cc45c2ecafa9c1601ba2e9a)
  Further update tests to work with `npm-registry-mock@1.0.0`.
  ([@othiym23](https://github.com/othiym23))
* [`41a0f89`](https://github.com/npm/npm/commit/41a0f8959d4e02af9661588afa7d2b4543cc21b6)
  Got rid of completely gratuitous global config manipulation in tests.
  ([@othiym23](https://github.com/othiym23))

#### MINOR DEPENDENCY TWEAK

* [`a4c7af9`](https://github.com/npm/npm/commit/a4c7af9c692f250c0fd017397ed9514fc263b752)
  `request@2.53.0`: Tweaks to tunneling proxy behavior.
  ([@nylen](https://github.com/nylen))

### v2.5.0 (2015-01-29):

#### SMALL FEATURE I HAVE ALREADY USED TO MAINTAIN NPM ITSELF

* [`9d61e96`](https://github.com/npm/npm/commit/9d61e96fb1f48687a85c211e4e0cd44c7f95a38e)
  `npm outdated --long` now includes a column showing the type of dependency.
  ([@watilde](https://github.com/watilde))

#### BUG FIXES & TWEAKS

* [`fec4c96`](https://github.com/npm/npm/commit/fec4c967ee235030bf31393e8605e9e2811f4a39)
  Allow `--no-proxy` to override `HTTP_PROXY` setting in environment.
  ([@othiym23](https://github.com/othiym23))
* [`589acb9`](https://github.com/npm/npm/commit/589acb9714f395c2ad0d98cb0ac4236f1842d2cc)
  Only set `access` when publshing when it's explicitly set.
  ([@othiym23](https://github.com/othiym23))
* [`1027087`](https://github.com/npm/npm/commit/102708704c8c4f0ea99775d38f8d1efecf584940)
  Add script and `Makefile` stanza to update AUTHORS.
  ([@KenanY](https://github.com/KenanY))
* [`eeff04d`](https://github.com/npm/npm/commit/eeff04da7979a0181becd36b8777d607e7aa1787)
  Add `NPMOPTS` to top-level install in `Makefile` to override `userconfig`.
  ([@aredridel](https://github.com/aredridel))
* [`0d17328`](https://github.com/npm/npm/commit/0d173287336650606d4c91818bb7bcfb0c5d57a1)
  `fstream@1.0.4`: Run chown only when necessary.
  ([@silkentrance](https://github.com/silkentrance))
* [`9aa4622`](https://github.com/npm/npm/commit/9aa46226ee63b9e183fd49fc72d9bdb0fae9605e)
  `columnify@1.4.1`: ES6ified! ([@timoxley](https://github.com/timoxley))
* [`51b2fd1`](https://github.com/npm/npm/commit/51b2fd1974e38b825ac5ca4a852ab3c4142624cc)
  Update default version in `docs/npm-config.md`.
  ([@lucthev](https://github.com/lucthev))

#### `npm-registry-client@6.0.7`:

* [`f9313a0`](https://github.com/npm/npm/commit/f9313a066c9889a0ee898d8a35676e40b8101e7f)
  [#7226](https://github.com/npm/npm/issues/7226) Ensure that all request
  settings are copied onto the agent.
  ([@othiym23](https://github.com/othiym23))
* [`e186f6e`](https://github.com/npm/npm/commit/e186f6e7cfeb4db9c94d7375638f0b2f0d472947)
  Only set `access` on publish when it differs from the norm.
  ([@othiym23](https://github.com/othiym23))
* [`f9313a0`](https://github.com/npm/npm/commit/f9313a066c9889a0ee898d8a35676e40b8101e7f)
  Allow overriding request's environment-based proxy handling.
  ([@othiym23](https://github.com/othiym23))
* [`f9313a0`](https://github.com/npm/npm/commit/f9313a066c9889a0ee898d8a35676e40b8101e7f)
  Properly handle retry failures on fetch.
  ([@othiym23](https://github.com/othiym23))

### v2.4.1 (2015-01-23):

![bridge that doesn't meet in the middle](http://www.static-18.themodernnomad.com/wp-content/uploads/2011/08/bridge-fail.jpg)

Let's accentuate the positive: the `dist-tag` endpoints for `npm dist-tag
{add,rm,ls}` are now live on the public npm registry.

* [`f70272b`](https://github.com/npm/npm/commit/f70272bed7d77032d1e21553371dd5662fef32f2)
  `npm-registry-client@6.0.3`: Properly escape JSON tag version strings and
  filter `_etag` from CouchDB docs. ([@othiym23](https://github.com/othiym23))

### v2.4.0 (2015-01-22):

#### REGISTRY 2: ACCESS AND DIST-TAGS

NOTE: This week's registry-2 commands are leading the implementation on
registry.npmjs.org a little bit, so some of the following may not work for
another week or so. Also note that `npm access` has documentation and
subcommands that are not yet finished, because they depend on incompletely
specified registry API endpoints. Things are coming together very quickly,
though, so expect the missing pieces to be filled in the coming weeks.

* [`c963eb2`](https://github.com/npm/npm/commit/c963eb295cf766921b1680f4a71fd0ed3e1bcad8)
  [#7181](https://github.com/npm/npm/issues/7181) NEW `npm access public` and
  `npm access restricted`: Toggle visibility of scoped packages.
  ([@othiym23](https://github.com/othiym23))
* [`dc51810`](https://github.com/npm/npm/commit/dc51810e08c0f104259146c9c035d255de4f7d1d)
  [#6243](https://github.com/npm/npm/issues/6243) /
  [#6854](https://github.com/npm/npm/issues/6854) NEW `npm dist-tags`: Directly
  manage `dist-tags` on packages. Most notably, `dist-tags` can now be deleted.
  ([@othiym23](https://github.com/othiym23))
* [`4c7c132`](https://github.com/npm/npm/commit/4c7c132a6b8305dca2974943226c39c0cdc64ff9)
  [#7181](https://github.com/npm/npm/issues/7181) /
  [#6854](https://github.com/npm/npm/issues/6854) `npm-registry-client@6.0.1`:
  Add new `access` and `dist-tags` endpoints
  ([@othiym23](https://github.com/othiym23))

#### NOT EXACTLY SELF-DEPRECATING

* [`10d5c77`](https://github.com/npm/npm/commit/10d5c77653487f15759ac7de262a97e9c655240c)
  [#6274](https://github.com/npm/npm/issues/6274) Deprecate `npm tag` in favor
  of `npm dist-tag`. ([@othiym23](https://github.com/othiym23))

#### BUG FIX AND TINY FEATURE

* [`29a6ef3`](https://github.com/npm/npm/commit/29a6ef38ef86ac318c5d9ea4bee28ce614672fa6)
  [#6850](https://github.com/npm/npm/issues/6850) Be smarter about determining
  base of file deletion when unbuilding. ([@phated](https://github.com/phated))
* [`4ad01ea`](https://github.com/npm/npm/commit/4ad01ea2930a7a1cf88be121cc5ce9eba40c6807)
  `init-package-json@1.2.0`: Support `--save-exact` in `npm init`.
  ([@gustavnikolaj](https://github.com/gustavnikolaj))

### v2.3.0 (2015-01-15):

#### REGISTRY 2: OH MY STARS! WHO AM I?

* [`e662a60`](https://github.com/npm/npm/commit/e662a60e2f9a542effd8e72279d4622fe514415e)
  The new `whoami` endpoint might not return a value.
  ([@othiym23](https://github.com/othiym23))
* [`c2cccd4`](https://github.com/npm/npm/commit/c2cccd4bbc65885239ed646eb510155f7b8af13d)
  `npm-registry-client@5.0.0`: Includes the following fine changes
  ([@othiym23](https://github.com/othiym23)):
  * [`ba6b73e`](https://github.com/npm/npm-registry-client/commit/ba6b73e351027246c228622014e4441412409bad)
    [#92](https://github.com/npm/npm-registry-client/issues/92) BREAKING CHANGE:
    Move `/whoami` endpoint out of the package namespace (to `/-/whoami`).
    ([@othiym23](https://github.com/othiym23))
  * [`3b174b7`](https://github.com/npm/npm-registry-client/commit/3b174b75c0c9ea77e298e6bb664fb499824ecc7c)
    [#93](https://github.com/npm/npm-registry-client/issues/93) Registries based
    on token-based auth can now offer starring.
    ([@bcoe](https://github.com/bcoe))
  * [`4701a29`](https://github.com/npm/npm-registry-client/commit/4701a29bcda41bc14aa91f361dd0d576e24677d7)
    Fix HTTP[S] connection keep-alive on Node 0.11 / io.js 1.0.
    ([@fengmk2](https://github.com/fengmk2))

#### BETTER REGISTRY METADATA CACHING

* [`98e1e10`](https://github.com/npm/npm/commit/98e1e1080df1f2cab16ed68035603950ea3d2d48)
  [#6791](https://github.com/npm/npm/issues/6791) Add caching based on
  Last-Modified / If-Modified-Since headers. Includes this
  `npm-registry-client@5.0.0` change ([@lxe](https://github.com/lxe)):
  * [`07bc335`](https://github.com/npm/npm-registry-client/commit/07bc33502b93554cd7539bfcce37d6e2d5404cd0)
    [#86](https://github.com/npm/npm-registry-client/issues/86) Add Last-Modified
    / If-Modified-Since cache header handling. ([@lxe](https://github.com/lxe))

#### HOW MUCH IS THAT WINDOWS IN THE DOGGY?

* [`706d49a`](https://github.com/npm/npm/commit/706d49ab45521360fce1a68779b8de899015d8c2)
  [#7107](https://github.com/npm/npm/issues/7107) `getCacheStat` passes a stub
  stat on Windows. ([@rmg](https://github.com/rmg))
* [`5fce278`](https://github.com/npm/npm/commit/5fce278a688a1cb79183e012bde40b089c2e97a4)
  [#5267](https://github.com/npm/npm/issues/5267) Use `%COMSPEC%` when set on
  Windows. ([@edmorley](https://github.com/edmorley))
* [`cc2e099`](https://github.com/npm/npm/commit/cc2e09912ce2f91567c485422e4e797c4deb9842)
  [#7083](https://github.com/npm/npm/issues/7083) Ensure Git cache prefix
  exists before repo clone on Windows.
  ([@othiym23](https://github.com/othiym23))

#### THRILLING BUG FIXES

* [`c6fb430`](https://github.com/npm/npm/commit/c6fb430e55672b3caf87d25cbd2aeeebc449e2f2)
  [#4197](https://github.com/npm/npm/issues/4197) Report `umask` as a 0-padded
  octal literal. ([@smikes](https://github.com/smikes))
* [`209713e`](https://github.com/npm/npm/commit/209713ebd4b77da11ce27d90c3346f78d760ba52)
  [#4197](https://github.com/npm/npm/issues/4197) `umask@1.1.0`: Properly
  handle `umask`s (i.e. not decimal numbers).
  ([@smikes](https://github.com/smikes))
* [`9eac0a1`](https://github.com/npm/npm/commit/9eac0a14488c5979ebde4c17881c8cd74f395069)
  Make the example for bin links non-destructive.
  ([@KevinSheedy](https://github.com/KevinSheedy))
* [`6338bcf`](https://github.com/npm/npm/commit/6338bcfcd9cd1b0cc48b051dae764dc436ab5332)
  `glob@4.3.5`: " -> ', for some reason. ([@isaacs](https://github.com/isaacs))

### v2.2.0 (2015-01-08):

* [`88c531d`](https://github.com/npm/npm/commit/88c531d1c0b3aced8f2a09632db01b5635e7226a)
  [#7056](https://github.com/npm/npm/issues/7056) version doesn't need a
  package.json. ([@othiym23](https://github.com/othiym23))
* [`2656c19`](https://github.com/npm/npm/commit/2656c19f6b915c3173acc3b6f184cc321563da5f)
  [#7095](https://github.com/npm/npm/issues/7095) Link to npm website instead
  of registry. ([@konklone](https://github.com/konklone))
* [`c76b801`](https://github.com/npm/npm/commit/c76b8013bf1758587565822626171b76cb465c9e)
  [#7067](https://github.com/npm/npm/issues/7067) Obfuscate secrets, including
  nerfed URLs. ([@smikes](https://github.com/smikes))
* [`17f66ce`](https://github.com/npm/npm/commit/17f66ceb1bd421084e4ae82a6b66634a6e272929)
  [#6849](https://github.com/npm/npm/issues/6849) Explain the tag workflow more
  clearly. ([@smikes](https://github.com/smikes))
* [`e309df6`](https://github.com/npm/npm/commit/e309df642de33d10d6dffadaa8a5d214a924d0dc)
  [#7096](https://github.com/npm/npm/issues/7096) Really, `npm update -g` is
  almost always a terrible idea. ([@smikes](https://github.com/smikes))
* [`acf287d`](https://github.com/npm/npm/commit/acf287d2547c8a0a8871652c164019261b666d55)
  [#6999](https://github.com/npm/npm/issues/6999) `npm run-script env`: add a
  new default script that will print out environment values.
  ([@gcb](https://github.com/gcb))
* [`560c009`](https://github.com/npm/npm/commit/560c00945d4dec926cd29193e336f137c7f3f951)
  [#6745](https://github.com/npm/npm/issues/6745) Document `npm update --dev`.
  ([@smikes](https://github.com/smikes))
* [`226a677`](https://github.com/npm/npm/commit/226a6776a1a9e28570485623b8adc2ec4b041335)
  [#7046](https://github.com/npm/npm/issues/7046) We have never been the Node
  package manager. ([@linclark](https://github.com/linclark))
* [`38eef22`](https://github.com/npm/npm/commit/38eef2248f03bb8ab04cae1833e2a228fb887f3c)
  `npm-install-checks@1.0.5`: Compatibility with npmlog@^1.
  ([@iarna](https://github.com/iarna))

### v2.1.18 (2015-01-01):

* [`bf8640b`](https://github.com/npm/npm/commit/bf8640b0395b5dff71260a0cede7efc699a7bcf5)
  [#7044](https://github.com/npm/npm/issues/7044) Document `.npmignore` syntax.
  ([@zeke](https://github.com/zeke))

### v2.1.17 (2014-12-25):

merry npm xmas

Working with [@phated](https://github.com/phated), I discovered that npm still
had some lingering race conditions around how it handles Git dependencies. The
following changes were intended to remedy to these issues. Thanks to
[@phated](https://github.com/phated) for all his help getting to the bottom of
these.

* [`bdf1c84`](https://github.com/npm/npm/commit/bdf1c8483f5c4ad79b712db12d73276e15883923)
  [#7006](https://github.com/npm/npm/issues/7006) Only `chown` template and
  top-level Git cache directories. ([@othiym23](https://github.com/othiym23))
* [`581a72d`](https://github.com/npm/npm/commit/581a72da18f35ec87edef6255adf4ef4714a478c)
  [#7006](https://github.com/npm/npm/issues/7006) Map Git remote inflighting to
  clone paths rather than Git URLs. ([@othiym23](https://github.com/othiym23))
* [`1c48d08`](https://github.com/npm/npm/commit/1c48d08dea31a11ac11a285cac598a482481cade)
  [#7009](https://github.com/npm/npm/issues/7009) `normalize-git-url@1.0.0`:
  Normalize Git URLs while caching. ([@othiym23](https://github.com/othiym23))
* [`5423cf0`](https://github.com/npm/npm/commit/5423cf0be8ff2b76bfff7c8e780e5f261235a86a)
  [#7009](https://github.com/npm/npm/issues/7009) Pack tarballs to their final
  locations atomically. ([@othiym23](https://github.com/othiym23))
* [`7f6557f`](https://github.com/npm/npm/commit/7f6557ff317469ee4a87c542ff9a991e74ce9f38)
  [#7009](https://github.com/npm/npm/issues/7009) Inflight local directory
  packing, just to be safe. ([@othiym23](https://github.com/othiym23))

Other changes:

* [`1c491e6`](https://github.com/npm/npm/commit/1c491e65d70af013e8d5ac008d6d9762d6d91793)
  [#6991](https://github.com/npm/npm/issues/6991) `npm version`: fix regression
  in dirty-checking behavior ([@rlidwka](https://github.com/rlidwka))
* [`55ceb2b`](https://github.com/npm/npm/commit/55ceb2b08ff8a0f56b94cc972ca15d7862e8733c)
  [#1991](https://github.com/npm/npm/issues/1991) modify docs to reflect actual
  `npm restart` behavior ([@smikes](https://github.com/smikes))
* [`fb8e31b`](https://github.com/npm/npm/commit/fb8e31b95476a50bda35a665a99eec8a5d25a4db)
  [#6982](https://github.com/npm/npm/issues/6982) when doing registry
  operations, ensure registry URL always ends with `/`
  ([@othiym23](https://github.com/othiym23))
* [`5bcba65`](https://github.com/npm/npm/commit/5bcba65bed2678ffe80fb596f72abe9871d131c8)
  pull whitelisted Git environment variables out into a named constant
  ([@othiym23](https://github.com/othiym23))
* [`be04bbd`](https://github.com/npm/npm/commit/be04bbdc52ebfc820cd939df2f7d79fe87067747)
  [#7000](https://github.com/npm/npm/issues/7000) No longer install badly-named
  manpage files, and log an error when trying to uninstall them.
  ([@othiym23](https://github.com/othiym23))
* [`6b7c5ec`](https://github.com/npm/npm/commit/6b7c5eca6b65e1247d0e51f6400cf2637ac880ce)
  [#7011](https://github.com/npm/npm/issues/7011) Send auth for tarball fetches
  for packages in `npm-shrinkwrap.json` from private registries.
    ([@othiym23](https://github.com/othiym23))
* [`9b9de06`](https://github.com/npm/npm/commit/9b9de06a99893b40aa23f0335726dec6df7979db)
  `glob@4.3.2`: Better handling of trailing slashes.
  ([@isaacs](https://github.com/isaacs))
* [`030f3c7`](https://github.com/npm/npm/commit/030f3c7450b8ce124a19073bfbae0948a0a1a02c)
  `semver@4.2.0`: Diffing between version strings.
  ([@isaacs](https://github.com/isaacs))

### v2.1.16 (2014-12-22):

* [`a4e4e33`](https://github.com/npm/npm/commit/a4e4e33edb35c68813f04bf42bdf933a6f727bcd)
  [#6987](https://github.com/npm/npm/issues/6987) `read-installed@3.1.5`: fixed
  a regression where a new / empty package would cause read-installed to throw.
  ([@othiym23](https://github.com/othiym23) /
  [@pgilad](https://github.com/pgilad))

### v2.1.15 (2014-12-18):

* [`e5a2dee`](https://github.com/npm/npm/commit/e5a2dee47c74f26c56fee5998545b97497e830c8)
  [#6951](https://github.com/npm/npm/issues/6951) `fs-vacuum@1.2.5`: Use
  `path-is-inside` for better Windows normalization.
  ([@othiym23](https://github.com/othiym23))
* [`ac6167c`](https://github.com/npm/npm/commit/ac6167c2b9432939c57296f7ddd11ad5f8f918b2)
  [#6955](https://github.com/npm/npm/issues/6955) Call `path.normalize` in
  `lib/utils/gently-rm.js` for better Windows normalization.
  ([@ben-page](https://github.com/ben-page))
* [`c625d71`](https://github.com/npm/npm/commit/c625d714795e3b5badd847945e2401adfad5a196)
  [#6964](https://github.com/npm/npm/issues/6964) Clarify CA configuration
  docs. ([@jeffjo](https://github.com/jeffjo))
* [`58b8cb5`](https://github.com/npm/npm/commit/58b8cb5cdf26a854358b7c2ab636572dba9bac16)
  [#6950](https://github.com/npm/npm/issues/6950) Fix documentation typos.
  ([@martinvd](https://github.com/martinvd))
* [`7c1299d`](https://github.com/npm/npm/commit/7c1299d00538ea998684a1903a4091eafc63b7f1)
  [#6909](https://github.com/npm/npm/issues/6909) Remove confusing mention of
  rubygems `~>` semver operator. ([@mjtko](https://github.com/mjtko))
* [`7dfdcc6`](https://github.com/npm/npm/commit/7dfdcc6debd8ef1fc52a2b508997d15887aad824)
  [#6909](https://github.com/npm/npm/issues/6909) `semver@4.1.1`: Synchronize
  documentation with PR [#6909](https://github.com/npm/npm/issues/6909)
  ([@othiym23](https://github.com/othiym23))
* [`adfddf3`](https://github.com/npm/npm/commit/adfddf3b682e0ae08e4b59d87c1b380dd651c572)
  [#6925](https://github.com/npm/npm/issues/6925) Correct typo in
  `doc/api/npm-ls.md` ([@oddurs](https://github.com/oddurs))
* [`f5c534b`](https://github.com/npm/npm/commit/f5c534b711ab173129baf366c4f08d68f6117333)
  [#6920](https://github.com/npm/npm/issues/6920) Remove recommendation to run
  as root from `README.md`.
  ([@robertkowalski](https://github.com/robertkowalski))
* [`3ef4459`](https://github.com/npm/npm/commit/3ef445922cd39f25b992d91bd22c4d367882ea22)
  [#6920](https://github.com/npm/npm/issues/6920) `npm-@googlegroups.com` has
  gone the way of all things. That means it's gone.
  ([@robertkowalski](https://github.com/robertkowalski))

### v2.1.14 (2014-12-13):

* [`cf7aeae`](https://github.com/npm/npm/commit/cf7aeae3c3a24e48d3de4006fa082f0c6040922a)
  [#6923](https://github.com/npm/npm/issues/6923) Overaggressive link update
  for new website broke node-gyp. ([@othiym23](https://github.com/othiym23))

### v2.1.13 (2014-12-11):

* [`cbb890e`](https://github.com/npm/npm/commit/cbb890eeacc0501ba1b8c6955f1c829c8af9f486)
  [#6897](https://github.com/npm/npm/issues/6897) npm is a nice package manager
  that runs server-side JavaScript. ([@othiym23](https://github.com/othiym23))
* [`d9043c3`](https://github.com/npm/npm/commit/d9043c3b8d7450c3cb9ca795028c0e1c05377820)
  [#6893](https://github.com/npm/npm/issues/6893) Remove erroneous docs about
  preupdate / update / postupdate lifecycle scripts, which have never existed.
  ([@devTristan](https://github.com/devTristan))
* [`c5df4d0`](https://github.com/npm/npm/commit/c5df4d0d683cd3506808d1cd1acebff02a8b82db)
  [#6884](https://github.com/npm/npm/issues/6884) Update npmjs.org to npmjs.com
  in docs. ([@linclark](https://github.com/linclark))
* [`cb6ff8d`](https://github.com/npm/npm/commit/cb6ff8dace1b439851701d4784d2d719c22ca7a7)
  [#6879](https://github.com/npm/npm/issues/6879) npm version: Update
  shrinkwrap post-check. ([@othiym23](https://github.com/othiym23))
* [`2a340bd`](https://github.com/npm/npm/commit/2a340bdd548c6449468281e1444a032812bff677)
  [#6868](https://github.com/npm/npm/issues/6868) Use magic numbers instead of
  regexps to distinguish tarballs from other things.
  ([@daxxog](https://github.com/daxxog))
* [`f1c8bdb`](https://github.com/npm/npm/commit/f1c8bdb3f6b753d0600597e12346bdc3a34cb9c1)
  [#6861](https://github.com/npm/npm/issues/6861) `npm-registry-client@4.0.5`:
  Distinguish between error properties that are part of the response and error
  strings that should be returned to the user.
  ([@disrvptor](https://github.com/disrvptor))
* [`d3a1b63`](https://github.com/npm/npm/commit/d3a1b6397fddef04b5198ca89d36d720aeb05eb6)
  [#6762](https://github.com/npm/npm/issues/6762) Make `npm outdated` ignore
  private packages. ([@KenanY](https://github.com/KenanY))
* [`16d8542`](https://github.com/npm/npm/commit/16d854283ca5bcdb0cb2812fc5745d841652b952)
  install.sh: Drop support for node < 0.8, remove engines bits.
  ([@isaacs](https://github.com/isaacs))
* [`b9c6046`](https://github.com/npm/npm/commit/b9c60466d5b713b1dc2947da14a5dfe42352e029)
  `init-package-json@1.1.3`: ([@terinstock](https://github.com/terinstock))
  noticed that `init.license` configuration doesn't stick. Make sure that
  dashed defaults don't trump dotted parameters.
  ([@othiym23](https://github.com/othiym23))
* [`b6d6acf`](https://github.com/npm/npm/commit/b6d6acfc02c8887f78067931babab8f7c5180fed)
  `which@1.0.8`: No longer use graceful-fs for some reason.
  ([@isaacs](https://github.com/isaacs))
* [`d39f673`](https://github.com/npm/npm/commit/d39f673caf08a90fb2bb001d79c98062d2cd05f4)
  `request@2.51.0`: Incorporate bug fixes. ([@nylen](https://github.com/nylen))
* [`c7ad727`](https://github.com/npm/npm/commit/c7ad7279cc879930ec58ccc62fa642e621ecb65c)
  `columnify@1.3.2`: Incorporate bug fixes.
  ([@timoxley](https://github.com/timoxley))

### v2.1.12 (2014-12-04):

* [`e5b1e44`](https://github.com/npm/npm/commit/e5b1e448bb4a9d6eae4ba0f67b1d3c2cea8ed383)
  add alias verison=version ([@isaacs](https://github.com/isaacs))
* [`5eed7bd`](https://github.com/npm/npm/commit/5eed7bddbd7bb92a44c4193c93e8529500c558e6)
  `request@2.49.0` ([@nylen](https://github.com/nylen))
* [`e72f81d`](https://github.com/npm/npm/commit/e72f81d8412540ae7d1e0edcc37c11bcb8169051)
  `glob@4.3.1` / `minimatch@2.0.1` ([@isaacs](https://github.com/isaacs))
* [`b8dcc36`](https://github.com/npm/npm/commit/b8dcc3637b5b71933b97162b7aff1b1a622c13e2)
  `graceful-fs@3.0.5` ([@isaacs](https://github.com/isaacs))

### v2.1.11 (2014-11-27):

* [`4861d28`](https://github.com/npm/npm/commit/4861d28ad0ebd959fe6bc15b9c9a50fcabe57f55)
  `which@1.0.7`: License update. ([@isaacs](https://github.com/isaacs))
* [`30a2ea8`](https://github.com/npm/npm/commit/30a2ea80c891d384b31a1cf28665bba4271915bd)
  `ini@1.3.2`: License update. ([@isaacs](https://github.com/isaacs))
* [`6a4ea05`](https://github.com/npm/npm/commit/6a4ea054f6ddf52fc58842ba2046564b04c5c0e2)
  `fstream@1.0.3`: Propagate error events to downstream streams.
  ([@gfxmonk](https://github.com/gfxmonk))
* [`a558695`](https://github.com/npm/npm/commit/a5586954f1c18df7c96137e0a79f41a69e7a884e)
  `tar@1.0.3`: Don't extract broken files, propagate `drain` event.
  ([@gfxmonk](https://github.com/gfxmonk))
* [`989624e`](https://github.com/npm/npm/commit/989624e8321f87734c1b1272fc2f646e7af1f81c)
  [#6767](https://github.com/npm/npm/issues/6767) Actually pass parameters when
  adding git repo to cache under Windows.
  ([@othiym23](https://github.com/othiym23))
* [`657af73`](https://github.com/npm/npm/commit/657af7308f7d6cd2f81389fcf0d762252acaf1ce)
  [#6774](https://github.com/npm/npm/issues/6774) When verifying paths on
  unbuild, resolve both source and target as symlinks.
  ([@hokaccha](https://github.com/hokaccha))
* [`fd19c40`](https://github.com/npm/npm/commit/fd19c4046414494f9647a6991c00f8406a939929)
  [#6713](https://github.com/npm/npm/issues/6713)
  `realize-package-specifier@1.3.0`: Make it so that `npm install foo@1` work
  when a file named `1` exists. ([@iarna](https://github.com/iarna))
* [`c8ac37a`](https://github.com/npm/npm/commit/c8ac37a470491b2ed28514536e2e198494638c79)
  `npm-registry-client@4.0.4`: Fix regression in failed fetch retries.
  ([@othiym23](https://github.com/othiym23))

### v2.1.10 (2014-11-20):

* [`756f3d4`](https://github.com/npm/npm/commit/756f3d40fe18bc02bc93afe17016dfcc266c4b6b)
  [#6735](https://github.com/npm/npm/issues/6735) Log "already built" messages
  at info, not error. ([@smikes](https://github.com/smikes))
* [`1b7330d`](https://github.com/npm/npm/commit/1b7330dafba3bbba171f74f1e58b261cb1b9301e)
  [#6729](https://github.com/npm/npm/issues/6729) `npm-registry-client@4.0.3`:
  GitHub won't redirect you through an HTML page to a compressed tarball if you
  don't tell it you accept JSON responses.
  ([@KenanY](https://github.com/KenanY))
* [`d9c7857`](https://github.com/npm/npm/commit/d9c7857be02dacd274e55bf6d430d90d91509d53)
  [#6506](https://github.com/npm/npm/issues/6506)
  `readdir-scoped-modules@1.0.1`: Use `graceful-fs` so the whole dependency
  tree gets read,  even in case of `EMFILE`.
  ([@sakana](https://github.com/sakana))
* [`3a085be`](https://github.com/npm/npm/commit/3a085be158ace8f1e4395e69f8c102d3dea00c5f)
  Grammar fix in docs. ([@icylace](https://github.com/icylace))
* [`3f8e2ff`](https://github.com/npm/npm/commit/3f8e2ff8342d327d6f1375437ecf4bd945dc360f)
  Did you know that npm has a Code of Conduct? Add a link to it to
  CONTRIBUTING.md. ([@isaacs](https://github.com/isaacs))
* [`319ccf6`](https://github.com/npm/npm/commit/319ccf633289e06e57a80d74c39706899348674c)
  `glob@4.2.1`: Performance tuning. ([@isaacs](https://github.com/isaacs))
* [`835f046`](https://github.com/npm/npm/commit/835f046e7568c33e81a0b48c84cff965024d8b8a)
  `readable-stream@1.0.33`: Bug fixes. ([@rvagg](https://github.com/rvagg))
* [`a34c38d`](https://github.com/npm/npm/commit/a34c38d0732fb246d11f2a776d2ad0d8db654338)
  `request@2.48.0`: Bug fixes. ([@nylen](https://github.com/nylen))

### v2.1.9 (2014-11-13):

* [`eed9f61`](https://github.com/npm/npm/commit/eed9f6101963364acffc59d7194fc1655180e80c)
  [#6542](https://github.com/npm/npm/issues/6542) `npm owner add / remove` now
  works properly with scoped packages
  ([@othiym23](https://github.com/othiym23))
* [`cd25973`](https://github.com/npm/npm/commit/cd25973825aa5315b7ebf26227bd32bd6be5533f)
  [#6548](https://github.com/npm/npm/issues/6548) using sudo won't leave the
  cache's git directories with bad permissions
  ([@othiym23](https://github.com/othiym23))
* [`56930ab`](https://github.com/npm/npm/commit/56930abcae6a6ea41f1b75e23765c61259cef2dd)
  fixed irregular `npm cache ls` output (yes, that's a thing)
  ([@othiym23](https://github.com/othiym23))
* [`740f483`](https://github.com/npm/npm/commit/740f483db6ec872b453065842da080a646c3600a)
  legacy tests no longer poison user's own cache
  ([@othiym23](https://github.com/othiym23))
* [`ce37f14`](https://github.com/npm/npm/commit/ce37f142a487023747a9086335618638ebca4372)
  [#6169](https://github.com/npm/npm/issues/6169) add terse output similar to
  `npm publish / unpublish` for `npm owner add / remove`
  ([@KenanY](https://github.com/KenanY))
* [`bf2b8a6`](https://github.com/npm/npm/commit/bf2b8a66d7188900bf1e957c052b893948b67e0e)
  [#6680](https://github.com/npm/npm/issues/6680) pass auth credentials to
  registry when downloading search index
  ([@terinjokes](https://github.com/terinjokes))
* [`00ecb61`](https://github.com/npm/npm/commit/00ecb6101422984696929f602e14da186f9f669c)
  [#6400](https://github.com/npm/npm/issues/6400) `.npmignore` is respected for
  git repos on cache / pack / publish
  ([@othiym23](https://github.com/othiym23))
* [`d1b3a9e`](https://github.com/npm/npm/commit/d1b3a9ec5e2b6d52765ba5da5afb08dba41c49c1)
  [#6311](https://github.com/npm/npm/issues/6311) `npm ls -l --depth=0` no
  longer prints phantom duplicate children
  ([@othiym23](https://github.com/othiym23))
* [`07c5f34`](https://github.com/npm/npm/commit/07c5f34e45c9b18c348ed53b5763b1c5d4325740)
  [#6690](https://github.com/npm/npm/issues/6690) `uid-number@0.0.6`: clarify
  confusing names in error-handling code ([@isaacs](https://github.com/isaacs))
* [`1ac9be9`](https://github.com/npm/npm/commit/1ac9be9f3bab816211d72d13cb05b5587878a586)
  [#6684](https://github.com/npm/npm/issues/6684) `npm init`: don't report
  write if canceled ([@smikes](https://github.com/smikes))
* [`7bb207d`](https://github.com/npm/npm/commit/7bb207d1d6592a9cffc986871e4b671575363c2f)
  [#5754](https://github.com/npm/npm/issues/5754) never remove app directories
  on failed install ([@othiym23](https://github.com/othiym23))
* [`705ce60`](https://github.com/npm/npm/commit/705ce601e7b9c5428353e02ebb30cb76c1991fdd)
  [#5754](https://github.com/npm/npm/issues/5754) `fs-vacuum@1.2.2`: don't
  throw when another fs task writes to a directory being vacuumed
  ([@othiym23](https://github.com/othiym23))
* [`1b650f4`](https://github.com/npm/npm/commit/1b650f4f217c413a2ffb96e1701beb5aa67a0de2)
  [#6255](https://github.com/npm/npm/issues/6255) ensure that order credentials
  are used from `.npmrc` doesn't regress
  ([@othiym23](https://github.com/othiym23))
* [`9bb2c34`](https://github.com/npm/npm/commit/9bb2c3435cedef40b45d3e9bd7a8edfb8cbe7209)
  [#6644](https://github.com/npm/npm/issues/6644) `warn` rather than `info` on
  fetch failure ([@othiym23](https://github.com/othiym23))
* [`e34a7b6`](https://github.com/npm/npm/commit/e34a7b6b7371b1893a062f627ae8e168546d7264)
  [#6524](https://github.com/npm/npm/issues/6524) `npm-registry-client@4.0.2`:
  proxy via `request` more transparently
  ([@othiym23](https://github.com/othiym23))
* [`40afd6a`](https://github.com/npm/npm/commit/40afd6aaf34c11a10e80ec87b115fb2bb907e3bd)
  [#6524](https://github.com/npm/npm/issues/6524) push proxy settings into
  `request` ([@tauren](https://github.com/tauren))

### v2.1.8 (2014-11-06):

* [`063d843`](https://github.com/npm/npm/commit/063d843965f9f0bfa5732d7c2d6f5aa37a8260a2)
  npm version now updates version in npm-shrinkwrap.json
  ([@faiq](https://github.com/faiq))
* [`3f53cd7`](https://github.com/npm/npm/commit/3f53cd795f8a600e904a97f215ba5b5a9989d9dd)
  [#6559](https://github.com/npm/npm/issues/6559) save local dependencies in
  npm-shrinkwrap.json ([@Torsph](https://github.com/Torsph))
* [`e249262`](https://github.com/npm/npm/commit/e24926268b2d2220910bc81cce6d3b2e08d94eb1)
  npm-faq.md: mention scoped pkgs in namespace Q
  ([@smikes](https://github.com/smikes))
* [`6b06ec4`](https://github.com/npm/npm/commit/6b06ec4ef5da490bdca1512fa7f12490245c192b)
  [#6642](https://github.com/npm/npm/issues/6642) `init-package-json@1.1.2`:
  Handle both `init-author-name` and `init.author.name`.
  ([@othiym23](https://github.com/othiym23))
* [`9cb334c`](https://github.com/npm/npm/commit/9cb334c8a895a55461aac18791babae779309a0e)
  [#6409](https://github.com/npm/npm/issues/6409) document commit-ish with
  GitHub URLs ([@smikes](https://github.com/smikes))
* [`0aefae9`](https://github.com/npm/npm/commit/0aefae9bc2598a4b7a3ee7bb2306b42e3e12bb28)
  [#2959](https://github.com/npm/npm/issues/2959) npm run no longer fails
  silently ([@flipside](https://github.com/flipside))
* [`e007a2c`](https://github.com/npm/npm/commit/e007a2c1e4fac1759fa61ac6e78c6b83b2417d11)
  [#3908](https://github.com/npm/npm/issues/3908) include command in spawn
  errors ([@smikes](https://github.com/smikes))

### v2.1.7 (2014-10-30):

* [`6750b05`](https://github.com/npm/npm/commit/6750b05dcba20d8990a672957ec56c48f97e241a)
  [#6398](https://github.com/npm/npm/issues/6398) `npm-registry-client@4.0.0`:
  consistent API, handle relative registry paths, use auth more consistently
  ([@othiym23](https://github.com/othiym23))
* [`7719cfd`](https://github.com/npm/npm/commit/7719cfdd8b204dfeccc41289707ea58b4d608905)
  [#6560](https://github.com/npm/npm/issues/6560) use new npm-registry-client
  API ([@othiym23](https://github.com/othiym23))
* [`ed61971`](https://github.com/npm/npm/commit/ed619714c93718b6c1922b8c286f4b6cd2b97c80)
  move caching of search metadata from `npm-registry-client` to npm itself
  ([@othiym23](https://github.com/othiym23))
* [`3457041`](https://github.com/npm/npm/commit/34570414cd528debeb22943873440594d7f47abf)
  handle caching of metadata independently from `npm-registry-client`
  ([@othiym23](https://github.com/othiym23))
* [`20a331c`](https://github.com/npm/npm/commit/20a331ced6a52faac6ec242e3ffdf28bcd447c40)
  [#6538](https://github.com/npm/npm/issues/6538) map registry URLs to
  credentials more safely ([@indexzero](https://github.com/indexzero))
* [`4072e97`](https://github.com/npm/npm/commit/4072e97856bf1e7affb38333d080c172767eea27)
  [#6589](https://github.com/npm/npm/issues/6589) `npm-registry-client@4.0.1`:
  allow publishing of packages with names identical to built-in Node modules
  ([@feross](https://github.com/feross))
* [`254f0e4`](https://github.com/npm/npm/commit/254f0e4adaf2c56e9df25c7343c43b0b0804a3b5)
  `tar@1.0.2`: better error-handling ([@runk](https://github.com/runk))
* [`73ee2aa`](https://github.com/npm/npm/commit/73ee2aa4f1a47e43fe7cf4317a5446875f7521fa)
  `request@2.47.0` ([@mikeal](https://github.com/mikeal))

### v2.1.6 (2014-10-23):

* [`681b398`](https://github.com/npm/npm/commit/681b3987a18e7aba0aaf78c91a23c7cc0ab82ce8)
  [#6523](https://github.com/npm/npm/issues/6523) fix default `logelevel` doc
  ([@KenanY](https://github.com/KenanY))
* [`80b368f`](https://github.com/npm/npm/commit/80b368ffd786d4d008734b56c4a6fe12d2cb2926)
  [#6528](https://github.com/npm/npm/issues/6528) `npm version` should work in
  a git directory without git ([@terinjokes](https://github.com/terinjokes))
* [`5f5f9e4`](https://github.com/npm/npm/commit/5f5f9e4ddf544c2da6adf3f8c885238b0e745076)
  [#6483](https://github.com/npm/npm/issues/6483) `init-package-json@1.1.1`:
  Properly pick up default values from environment variables.
  ([@othiym23](https://github.com/othiym23))
* [`a114870`](https://github.com/npm/npm/commit/a1148702f53f82d49606b2e4dac7581261fff442)
  perl 5.18.x doesn't like -pi without filenames
  ([@othiym23](https://github.com/othiym23))
* [`de5ba00`](https://github.com/npm/npm/commit/de5ba007a48db876eb5bfb6156435f3512d58977)
  `request@2.46.0`: Tests and cleanup.
  ([@othiym23](https://github.com/othiym23))
* [`76933f1`](https://github.com/npm/npm/commit/76933f169f17b5273b32e924a7b392d5729931a7)
  `fstream-npm@1.0.1`: Always include `LICENSE[.*]`, `LICENCE[.*]`,
  `CHANGES[.*]`, `CHANGELOG[.*]`, and `HISTORY[.*]`.
  ([@jonathanong](https://github.com/jonathanong))

### v2.1.5 (2014-10-16):

* [`6a14b23`](https://github.com/npm/npm/commit/6a14b232a0e34158bd95bb25c607167be995c204)
  [#6397](https://github.com/npm/npm/issues/6397) Defactor npmconf back into
  npm. ([@othiym23](https://github.com/othiym23))
* [`4000e33`](https://github.com/npm/npm/commit/4000e3333a76ca4844681efa8737cfac24b7c2c8)
  [#6323](https://github.com/npm/npm/issues/6323) Install `peerDependencies`
  from top. ([@othiym23](https://github.com/othiym23))
* [`5d119ae`](https://github.com/npm/npm/commit/5d119ae246f27353b14ff063559d1ba8c616bb89)
  [#6498](https://github.com/npm/npm/issues/6498) Better error messages on
  malformed `.npmrc` properties. ([@nicks](https://github.com/nicks))
* [`ae18efb`](https://github.com/npm/npm/commit/ae18efb65fed427b1ef18e4862885bf60b87b92e)
  [#6093](https://github.com/npm/npm/issues/6093) Replace instances of 'hash'
  with 'object' in documentation. ([@zeke](https://github.com/zeke))
* [`53108b2`](https://github.com/npm/npm/commit/53108b276fec5f97a38250933a2768d58b6928da)
  [#1558](https://github.com/npm/npm/issues/1558) Clarify how local paths
  should be used. ([@KenanY](https://github.com/KenanY))
* [`344fa1a`](https://github.com/npm/npm/commit/344fa1a219ac8867022df3dc58a47636dde8a242)
  [#6488](https://github.com/npm/npm/issues/6488) Work around bug in marked.
  ([@othiym23](https://github.com/othiym23))

OUTDATED DEPENDENCY CLEANUP JAMBOREE

* [`60c2942`](https://github.com/npm/npm/commit/60c2942e13655d9ecdf6e0f1f97f10cb71a75255)
  `realize-package-specifier@1.2.0`: Handle names and rawSpecs more
  consistently. ([@iarna](https://github.com/iarna))
* [`1b5c95f`](https://github.com/npm/npm/commit/1b5c95fbda77b87342bd48c5ecac5b1fd571ccfe)
  `sha@1.3.0`: Change line endings?
  ([@ForbesLindesay](https://github.com/ForbesLindesay))
* [`d7dee3f`](https://github.com/npm/npm/commit/d7dee3f3f7d9e7c2061a4ecb4dd93e3e4bfe4f2e)
  `request@2.45.0`: Dependency updates, better proxy support, better compressed
  response handling, lots of 'use strict'.
  ([@mikeal](https://github.com/mikeal))
* [`3d75180`](https://github.com/npm/npm/commit/3d75180c2cc79fa3adfa0e4cb783a27192189a65)
  `opener@1.4.0`: Added gratuitous return.
  ([@Domenic](https://github.com/Domenic))
* [`8e2703f`](https://github.com/npm/npm/commit/8e2703f78d280d1edeb749e257dda1f288bad6e3)
  `retry@0.6.1` / `npm-registry-client@3.2.4`: Change of ownership.
  ([@tim-kos](https://github.com/tim-kos))
* [`c87b00f`](https://github.com/npm/npm/commit/c87b00f82f92434ee77831915012c77a6c244c39)
  `once@1.3.1`: Wrap once with wrappy. ([@isaacs](https://github.com/isaacs))
* [`01ec790`](https://github.com/npm/npm/commit/01ec790fd47def56eda6abb3b8d809093e8f493f)
  `npm-user-validate@0.1.1`: Correct repository URL.
  ([@robertkowalski](https://github.com/robertkowalski))
* [`389e52c`](https://github.com/npm/npm/commit/389e52c2d94c818ca8935ccdcf392994fec564a2)
  `glob@4.0.6`: Now absolutely requires `graceful-fs`.
  ([@isaacs](https://github.com/isaacs))
* [`e15ab15`](https://github.com/npm/npm/commit/e15ab15a27a8f14cf0d9dc6f11dee452080378a0)
  `ini@1.3.0`: Tighten up whitespace handling.
  ([@isaacs](https://github.com/isaacs))
* [`7610f3e`](https://github.com/npm/npm/commit/7610f3e62e699292ece081bfd33084d436e3246d)
  `archy@1.0.0` ([@substack](https://github.com/substack))
* [`9c13149`](https://github.com/npm/npm/commit/9c1314985e513e20ffa3ea0ca333ba2ab78299c9)
  `semver@4.1.0`: Add support for prerelease identifiers.
  ([@bromanko](https://github.com/bromanko))
* [`f096c25`](https://github.com/npm/npm/commit/f096c250441b031d758f03afbe8d2321f94c7703)
  `graceful-fs@3.0.4`: Add a bunch of additional tests, skip the unfortunate
  complications of `graceful-fs@3.0.3`. ([@isaacs](https://github.com/isaacs))

### v2.1.4 (2014-10-09):

* [`3aeb440`](https://github.com/npm/npm/commit/3aeb4401444fad83cc7a8d11bf2507658afa5248)
  [#6442](https://github.com/npm/npm/issues/6442) proxying git needs `GIT_SSL_CAINFO`
  ([@wmertens](https://github.com/wmertens))
* [`a8da8d6`](https://github.com/npm/npm/commit/a8da8d6e0cd56d97728c0b76b51604ee06ef6264)
  [#6413](https://github.com/npm/npm/issues/6413) write builtin config on any
  global npm install ([@isaacs](https://github.com/isaacs))
* [`9e4d632`](https://github.com/npm/npm/commit/9e4d632c0142ba55df07d624667738b8727336fc)
  [#6343](https://github.com/npm/npm/issues/6343) don't pass run arguments to
  pre & post scripts ([@TheLudd](https://github.com/TheLudd))
* [`d831b1f`](https://github.com/npm/npm/commit/d831b1f7ca1a9921ea5b394e39b7130ecbc6d7b4)
  [#6399](https://github.com/npm/npm/issues/6399) race condition: inflight
  installs, prevent `peerDependency` problems
  ([@othiym23](https://github.com/othiym23))
* [`82b775d`](https://github.com/npm/npm/commit/82b775d6ff34c4beb6c70b2344d491a9f2026577)
  [#6384](https://github.com/npm/npm/issues/6384) race condition: inflight
  caching by URL rather than semver range
  ([@othiym23](https://github.com/othiym23))
* [`7bee042`](https://github.com/npm/npm/commit/7bee0429066fedcc9e6e962c043eb740b3792809)
  `inflight@1.0.4`: callback can take arbitrary number of parameters
  ([@othiym23](https://github.com/othiym23))
* [`3bff494`](https://github.com/npm/npm/commit/3bff494f4abf17d6d7e0e4a3a76cf7421ecec35a)
  [#5195](https://github.com/npm/npm/issues/5195) fixed regex color regression
  for `npm search` ([@chrismeyersfsu](https://github.com/chrismeyersfsu))
* [`33ba2d5`](https://github.com/npm/npm/commit/33ba2d585160a0a2a322cb76c4cd989acadcc984)
  [#6387](https://github.com/npm/npm/issues/6387) allow `npm view global` if
  package is specified ([@evanlucas](https://github.com/evanlucas))
* [`99c4cfc`](https://github.com/npm/npm/commit/99c4cfceed413396d952cf05f4e3c710f9682c23)
  [#6388](https://github.com/npm/npm/issues/6388) npm-publish →
  npm-developers(7) ([@kennydude](https://github.com/kennydude))

TEST CLEANUP EXTRAVAGANZA:

* [`8d6bfcb`](https://github.com/npm/npm/commit/8d6bfcb88408f5885a2a67409854c43e5c3a23f6)
  tap tests run with no system-wide side effects
  ([@chrismeyersfsu](https://github.com/chrismeyersfsu))
* [`7a1472f`](https://github.com/npm/npm/commit/7a1472fbdbe99956ad19f629e7eb1cc07ba026ef)
  added npm cache cleanup script
  ([@chrismeyersfsu](https://github.com/chrismeyersfsu))
* [`0ce6a37`](https://github.com/npm/npm/commit/0ce6a3752fa9119298df15671254db6bc1d8e64c)
  stripped out dead test code (othiym23)
* replace spawn with common.npm (@chrismeyersfsu):
    * [`0dcd614`](https://github.com/npm/npm/commit/0dcd61446335eaf541bf5f2d5186ec1419f86a42)
      test/tap/cache-shasum-fork.js
    * [`97f861c`](https://github.com/npm/npm/commit/97f861c967606a7e51e3d5047cf805d9d1adea5a)
      test/tap/false_name.js
    * [`d01b3de`](https://github.com/npm/npm/commit/d01b3de6ce03f25bbf3db97bfcd3cc85830d6801)
      test/tap/git-cache-locking.js
    * [`7b63016`](https://github.com/npm/npm/commit/7b63016778124c6728d6bd89a045c841ae3900b6)
      test/tap/pack-scoped.js
    * [`c877553`](https://github.com/npm/npm/commit/c877553265c39673e03f0a97972f692af81a595d)
      test/tap/scripts-whitespace-windows.js
    * [`df98525`](https://github.com/npm/npm/commit/df98525331e964131299d457173c697cfb3d95b9)
      test/tap/prepublish.js
    * [`99c4cfc`](https://github.com/npm/npm/commit/99c4cfceed413396d952cf05f4e3c710f9682c23)
      test/tap/prune.js

### v2.1.3 (2014-10-02):

BREAKING CHANGE FOR THE SQRT(i) PEOPLE ACTUALLY USING `npm submodule`:

* [`1e64473`](https://github.com/npm/npm/commit/1e6447360207f45ad6188e5780fdf4517de6e23d)
  `rm -rf npm submodule` command, which has been broken since the Carter
  Administration ([@isaacs](https://github.com/isaacs))

BREAKING CHANGE IF YOU ARE FOR SOME REASON STILL USING NODE 0.6 AND YOU SHOULD
NOT BE DOING THAT CAN YOU NOT:

* [`3e431f9`](https://github.com/npm/npm/commit/3e431f9d6884acb4cde8bcb8a0b122a76b33ee1d)
  [joyent/node#8492](https://github.com/joyent/node/issues/8492) bye bye
  customFds, hello stdio ([@othiym23](https://github.com/othiym23))

Other changes:

* [`ea607a8`](https://github.com/npm/npm/commit/ea607a8a20e891ad38eed11b5ce2c3c0a65484b9)
  [#6372](https://github.com/npm/npm/issues/6372) noisily error (without
  aborting) on multi-{install,build} ([@othiym23](https://github.com/othiym23))
* [`3ee2799`](https://github.com/npm/npm/commit/3ee2799b629fd079d2db21d7e8f25fa7fa1660d0)
  [#6372](https://github.com/npm/npm/issues/6372) only make cache creation
  requests in flight ([@othiym23](https://github.com/othiym23))
* [`1a90ec2`](https://github.com/npm/npm/commit/1a90ec2f2cfbefc8becc6ef0c480e5edacc8a4cb)
  [#6372](https://github.com/npm/npm/issues/6372) wait to put Git URLs in
  flight until normalized ([@othiym23](https://github.com/othiym23))
* [`664795b`](https://github.com/npm/npm/commit/664795bb7d8da7142417b3f4ef5986db3a394071)
  [#6372](https://github.com/npm/npm/issues/6372) log what is and isn't in
  flight ([@othiym23](https://github.com/othiym23))
* [`00ef580`](https://github.com/npm/npm/commit/00ef58025a1f52dfabf2c4dc3898621d16a6e062)
  `inflight@1.0.3`: fix largely theoretical race condition, because we really
  really hate race conditions ([@isaacs](https://github.com/isaacs))
* [`1cde465`](https://github.com/npm/npm/commit/1cde4658d897ae0f93ff1d65b258e1571b391182)
  [#6363](https://github.com/npm/npm/issues/6363)
  `realize-package-specifier@1.1.0`: handle local dependencies better
  ([@iarna](https://github.com/iarna))
* [`86f084c`](https://github.com/npm/npm/commit/86f084c6c6d7935cd85d72d9d94b8784c914d51e)
  `realize-package-specifier@1.0.2`: dependency realization! in its own module!
  ([@iarna](https://github.com/iarna))
* [`553d830`](https://github.com/npm/npm/commit/553d830334552b83606b6bebefd821c9ea71e964)
  `npm-package-arg@2.1.3`: simplified semver, better tests
  ([@iarna](https://github.com/iarna))
* [`bec9b61`](https://github.com/npm/npm/commit/bec9b61a316c19f5240657594f0905a92a474352)
  `readable-stream@1.0.32`: for some reason
  ([@rvagg](https://github.com/rvagg))
* [`ff08ec5`](https://github.com/npm/npm/commit/ff08ec5f6d717bdbd559de0b2ede769306a9a763)
  `dezalgo@1.0.1`: use wrappy for instrumentability
  ([@isaacs](https://github.com/isaacs))

### v2.1.2 (2014-09-29):

* [`a1aa20e`](https://github.com/npm/npm/commit/a1aa20e44bb8285c6be1e7fa63b9da920e3a70ed)
  [#6282](https://github.com/npm/npm/issues/6282)
  `normalize-package-data@1.0.3`: don't prune bundledDependencies
  ([@isaacs](https://github.com/isaacs))
* [`a1f5fe1`](https://github.com/npm/npm/commit/a1f5fe1005043ce20a06e8b17a3e201aa3215357)
  move locks back into cache, now path-aware
  ([@othiym23](https://github.com/othiym23))
* [`a432c4b`](https://github.com/npm/npm/commit/a432c4b48c881294d6d79b5f41c2e1c16ad15a8a)
  convert lib/utils/tar.js to use atomic streams
  ([@othiym23](https://github.com/othiym23))
* [`b8c3c74`](https://github.com/npm/npm/commit/b8c3c74a3c963564233204161cc263e0912c930b)
  `fs-write-stream-atomic@1.0.2`: Now works with streams1 fs.WriteStreams.
  ([@isaacs](https://github.com/isaacs))
* [`c7ab76f`](https://github.com/npm/npm/commit/c7ab76f44cce5f42add5e3ba879bd10e7e00c3e6)
  logging cleanup ([@othiym23](https://github.com/othiym23))
* [`4b2d95d`](https://github.com/npm/npm/commit/4b2d95d0641435b09d047ae5cb2226f292bf38f0)
  [#6329](https://github.com/npm/npm/issues/6329) efficiently validate tmp
  tarballs safely ([@othiym23](https://github.com/othiym23))

### v2.1.1 (2014-09-26):

* [`563225d`](https://github.com/npm/npm/commit/563225d813ea4c12f46d4f7821ac7f76ba8ee2d6)
  [#6318](https://github.com/npm/npm/issues/6318) clean up locking; prefix
  lockfile with "." ([@othiym23](https://github.com/othiym23))
* [`c7f30e4`](https://github.com/npm/npm/commit/c7f30e4550fea882d31fcd4a55b681cd30713c44)
  [#6318](https://github.com/npm/npm/issues/6318) remove locking code around
  tarball packing and unpacking ([@othiym23](https://github.com/othiym23))

### v2.1.0 (2014-09-25):

NEW FEATURE:

* [`3635601`](https://github.com/npm/npm/commit/36356011b6f2e6a5a81490e85a0a44eb27199dd7)
  [#5520](https://github.com/npm/npm/issues/5520) Add `'npm view .'`.
  ([@evanlucas](https://github.com/evanlucas))

Other changes:

* [`f24b552`](https://github.com/npm/npm/commit/f24b552b596d0627549cdd7c2d68fcf9006ea50a)
  [#6294](https://github.com/npm/npm/issues/6294) Lock cache → lock cache
  target. ([@othiym23](https://github.com/othiym23))
* [`ad54450`](https://github.com/npm/npm/commit/ad54450104f94c82c501138b4eee488ce3a4555e)
  [#6296](https://github.com/npm/npm/issues/6296) Ensure that npm-debug.log
  file is created when rollbacks are done.
  ([@isaacs](https://github.com/isaacs))
* [`6810071`](https://github.com/npm/npm/commit/681007155a40ac9d165293bd6ec5d8a1423ccfca)
  docs: Default loglevel "http" → "warn".
  ([@othiym23](https://github.com/othiym23))
* [`35ac89a`](https://github.com/npm/npm/commit/35ac89a940f23db875e882ce2888208395130336)
  Skip installation of installed scoped packages.
  ([@timoxley](https://github.com/timoxley))
* [`e468527`](https://github.com/npm/npm/commit/e468527256ec599892b9b88d61205e061d1ab735)
  Ensure cleanup executes for scripts-whitespace-windows test.
  ([@timoxley](https://github.com/timoxley))
* [`ef9101b`](https://github.com/npm/npm/commit/ef9101b7f346797749415086956a0394528a12c4)
  Ensure cleanup executes for packed-scope test.
  ([@timoxley](https://github.com/timoxley))
* [`69b4d18`](https://github.com/npm/npm/commit/69b4d18cdbc2ae04c9afaffbd273b436a394f398)
  `fs-write-stream-atomic@1.0.1`: Fix a race condition in our race-condition
  fixer. ([@isaacs](https://github.com/isaacs))
* [`26b17ff`](https://github.com/npm/npm/commit/26b17ff2e3b21ee26c6fdbecc8273520cff45718)
  [#6272](https://github.com/npm/npm/issues/6272) `npmconf` decides what the
  default prefix is. ([@othiym23](https://github.com/othiym23))
* [`846faca`](https://github.com/npm/npm/commit/846facacc6427dafcf5756dcd36d9036539938de)
  Fix development dependency is preferred over dependency.
  ([@andersjanmyr](https://github.com/andersjanmyr))
* [`9d1a9db`](https://github.com/npm/npm/commit/9d1a9db3af5adc48a7158a5a053eeb89ee41a0e7)
  [#3265](https://github.com/npm/npm/issues/3265) Re-apply a71615a. Fixes
  [#3265](https://github.com/npm/npm/issues/3265) again, with a test!
  ([@glasser](https://github.com/glasser))
* [`1d41db0`](https://github.com/npm/npm/commit/1d41db0b2744a7bd50971c35cc060ea0600fb4bf)
  `marked-man@0.1.4`: Fixes formatting of synopsis blocks in man docs.
  ([@kapouer](https://github.com/kapouer))
* [`a623da0`](https://github.com/npm/npm/commit/a623da01bea1b2d3f3a18b9117cfd2d8e3cbdd77)
  [#5867](https://github.com/npm/npm/issues/5867) Specify dummy git template
  dir when cloning to prevent copying hooks.
  ([@boneskull](https://github.com/boneskull))

### v2.0.2 (2014-09-19):

* [`42c872b`](https://github.com/npm/npm/commit/42c872b32cadc0e555638fc78eab3a38a04401d8)
  [#5920](https://github.com/npm/npm/issues/5920)
  `fs-write-stream-atomic@1.0.0` ([@isaacs](https://github.com/isaacs))
* [`6784767`](https://github.com/npm/npm/commit/6784767fe15e28b44c81a1d4bb1738c642a65d78)
  [#5920](https://github.com/npm/npm/issues/5920) make all write streams atomic
  ([@isaacs](https://github.com/isaacs))
* [`f6fac00`](https://github.com/npm/npm/commit/f6fac000dd98ebdd5ea1d5921175735d463d328b)
  [#5920](https://github.com/npm/npm/issues/5920) barf on 0-length cached
  tarballs ([@isaacs](https://github.com/isaacs))
* [`3b37592`](https://github.com/npm/npm/commit/3b37592a92ea98336505189ae8ca29248b0589f4)
  `write-file-atomic@1.1.0`: use graceful-fs
  ([@iarna](https://github.com/iarna))

### v2.0.1 (2014-09-18):

* [`74c5ab0`](https://github.com/npm/npm/commit/74c5ab0a676793c6dc19a3fd5fe149f85fecb261)
  [#6201](https://github.com/npm/npm/issues/6201) `npmconf@2.1.0`: scope
  always-auth to registry URI ([@othiym23](https://github.com/othiym23))
* [`774b127`](https://github.com/npm/npm/commit/774b127da1dd6fefe2f1299e73505d9146f00294)
  [#6201](https://github.com/npm/npm/issues/6201) `npm-registry-client@3.2.2`:
  use scoped always-auth settings ([@othiym23](https://github.com/othiym23))
* [`f2d2190`](https://github.com/npm/npm/commit/f2d2190aa365d22378d03afab0da13f95614a583)
  [#6201](https://github.com/npm/npm/issues/6201) support saving
  `--always-auth` when logging in ([@othiym23](https://github.com/othiym23))
* [`17c941a`](https://github.com/npm/npm/commit/17c941a2d583210fe97ed47e2968d94ce9f774ba)
  [#6163](https://github.com/npm/npm/issues/6163) use `write-file-atomic`
  instead of `fs.writeFile()` ([@fiws](https://github.com/fiws))
* [`fb5724f`](https://github.com/npm/npm/commit/fb5724fd98e1509c939693568df83d11417ea337)
  [#5925](https://github.com/npm/npm/issues/5925) `npm init -f`: allow `npm
  init` to run without prompting
  ([@michaelnisi](https://github.com/michaelnisi))
* [`b706d63`](https://github.com/npm/npm/commit/b706d637d5965dbf8f7ce07dc5c4bc80887f30d8)
  [#3059](https://github.com/npm/npm/issues/3059) disable prepublish when
  running `npm install --production`
  ([@jussi-kalliokoski](https://github.com/jussi-kalliokoski))
* [`119f068`](https://github.com/npm/npm/commit/119f068eae2a36fa8b9c9ca557c70377792243a4)
  attach the node version used when publishing a package to its registry
  metadata ([@othiym23](https://github.com/othiym23))
* [`8fe0081`](https://github.com/npm/npm/commit/8fe008181665519c2ac201ee432a3ece9798c31f)
  seriously, don't use `npm -g update npm`
  ([@thomblake](https://github.com/thomblake))
* [`ea5b3d4`](https://github.com/npm/npm/commit/ea5b3d446b86dcabb0dbc6dba374d3039342ecb3)
  `request@2.44.0` ([@othiym23](https://github.com/othiym23))

### v2.0.0 (2014-09-12):

BREAKING CHANGES:

* [`4378a17`](https://github.com/npm/npm/commit/4378a17db340404a725ffe2eb75c9936f1612670)
  `semver@4.0.0`: prerelease versions no longer show up in ranges; `^0.x.y`
  behaves the way it did in `semver@2` rather than `semver@3`; docs have been
  reorganized for comprehensibility ([@isaacs](https://github.com/isaacs))
* [`c6ddb64`](https://github.com/npm/npm/commit/c6ddb6462fe32bf3a27b2c4a62a032a92e982429)
  npm now assumes that node is newer than 0.6
  ([@isaacs](https://github.com/isaacs))

Other changes:

* [`ea515c3`](https://github.com/npm/npm/commit/ea515c3b858bf493a7b87fa4cdc2110a0d9cef7f)
  [#6043](https://github.com/npm/npm/issues/6043) `slide@1.1.6`: wait until all
  callbacks have finished before proceeding
  ([@othiym23](https://github.com/othiym23))
* [`0b0a59d`](https://github.com/npm/npm/commit/0b0a59d504f20f424294b1590ace73a7464f0378)
  [#6043](https://github.com/npm/npm/issues/6043) defer rollbacks until just
  before the CLI exits ([@isaacs](https://github.com/isaacs))
* [`a11c88b`](https://github.com/npm/npm/commit/a11c88bdb1488b87d8dcac69df9a55a7a91184b6)
  [#6175](https://github.com/npm/npm/issues/6175) pack scoped packages
  correctly ([@othiym23](https://github.com/othiym23))
* [`e4e48e0`](https://github.com/npm/npm/commit/e4e48e037d4e95fdb6acec80b04c5c6eaee59970)
  [#6121](https://github.com/npm/npm/issues/6121) `read-installed@3.1.2`: don't
  mark linked dev dependencies as extraneous
  ([@isaacs](https://github.com/isaacs))
* [`d673e41`](https://github.com/npm/npm/commit/d673e4185d43362c2b2a91acbca8c057e7303c7b)
  `cmd-shim@2.0.1`: depend on `graceful-fs` directly
  ([@ForbesLindesay](https://github.com/ForbesLindesay))
* [`9d54d45`](https://github.com/npm/npm/commit/9d54d45e602d595bdab7eae09b9fa1dc46370147)
  `npm-registry-couchapp@2.5.3`: make tests more reliable on Travis
  ([@iarna](https://github.com/iarna))
* [`673d738`](https://github.com/npm/npm/commit/673d738c6142c3d043dcee0b7aa02c9831a2e0ca)
  ensure permissions are set correctly in cache when running as root
  ([@isaacs](https://github.com/isaacs))
* [`6e6a5fb`](https://github.com/npm/npm/commit/6e6a5fb74af10fd345411df4e121e554e2e3f33e)
  prepare for upgrade to `node-semver@4.0.0`
  ([@isaacs](https://github.com/isaacs))
* [`ab8dd87`](https://github.com/npm/npm/commit/ab8dd87b943262f5996744e8d4cc30cc9358b7d7)
  swap out `ronn` for `marked-man@0.1.3` ([@isaacs](https://github.com/isaacs))
* [`803da54`](https://github.com/npm/npm/commit/803da5404d5a0b7c9defa3fe7fa0f2d16a2b19d3)
  `npm-registry-client@3.2.0`: prepare for `node-semver@4.0.0` and include more
  error information ([@isaacs](https://github.com/isaacs))
* [`4af0e71`](https://github.com/npm/npm/commit/4af0e7134f5757c3d456d83e8349224a4ba12660)
  make default error display less scary ([@isaacs](https://github.com/isaacs))
* [`4fd9e79`](https://github.com/npm/npm/commit/4fd9e7901a15abff7a3dd478d99ce239b9580bca)
  `npm-registry-client@3.2.1`: handle errors returned by the registry much,
  much better ([@othiym23](https://github.com/othiym23))
* [`ca791e2`](https://github.com/npm/npm/commit/ca791e27e97e51c1dd491bff6622ac90b54c3e23)
  restore a long (always?) missing pass for deduping
  ([@othiym23](https://github.com/othiym23))
* [`ca0ef0e`](https://github.com/npm/npm/commit/ca0ef0e99bbdeccf28d550d0296baa4cb5e7ece2)
  correctly interpret relative paths for local dependencies
  ([@othiym23](https://github.com/othiym23))
* [`5eb8db2`](https://github.com/npm/npm/commit/5eb8db2c370eeb4cd34f6e8dc6a935e4ea325621)
  `npm-package-arg@2.1.2`: support git+file:// URLs for local bare repos
  ([@othiym23](https://github.com/othiym23))
* [`860a185`](https://github.com/npm/npm/commit/860a185c43646aca84cb93d1c05e2266045c316b)
  tweak docs to no longer advocate checking in `node_modules`
  ([@hunterloftis](https://github.com/hunterloftis))
* [`80e9033`](https://github.com/npm/npm/commit/80e9033c40e373775e35c674faa6c1948661782b)
  add links to nodejs.org downloads to docs
  ([@meetar](https://github.com/meetar))

### v2.0.0-beta.3 (2014-09-04):

* [`fa79413`](https://github.com/npm/npm/commit/fa794138bec8edb7b88639db25ee9c010d2f4c2b)
  [#6119](https://github.com/npm/npm/issues/6119) fall back to registry installs
  if package.json is missing in a local directory ([@iarna](https://github.com/iarna))
* [`16073e2`](https://github.com/npm/npm/commit/16073e2d8ae035961c4c189b602d4aacc6d6b387)
  `npm-package-arg@2.1.0`: support file URIs as local specs
  ([@othiym23](https://github.com/othiym23))
* [`9164acb`](https://github.com/npm/npm/commit/9164acbdee28956fa816ce5e473c559395ae4ec2)
  `github-url-from-username-repo@1.0.2`: don't match strings that are already
  URIs ([@othiym23](https://github.com/othiym23))
* [`4067d6b`](https://github.com/npm/npm/commit/4067d6bf303a69be13f3af4b19cf4fee1b0d3e12)
  [#5629](https://github.com/npm/npm/issues/5629) support saving of local packages
  in `package.json` ([@dylang](https://github.com/dylang))
* [`1b2ffdf`](https://github.com/npm/npm/commit/1b2ffdf359a8c897a78f91fc5a5d535c97aaec97)
  [#6097](https://github.com/npm/npm/issues/6097) document scoped packages
  ([@seldo](https://github.com/seldo))
* [`0a67d53`](https://github.com/npm/npm/commit/0a67d536067c4808a594d81288d34c0f7e97e105)
  [#6007](https://github.com/npm/npm/issues/6007) `request@2.42.0`: properly
  set headers on proxy requests ([@isaacs](https://github.com/isaacs))
* [`9bac6b8`](https://github.com/npm/npm/commit/9bac6b860b674d24251bb7b8ba412fdb26cbc836)
  `npmconf@2.0.8`: disallow semver ranges in tag configuration
  ([@isaacs](https://github.com/isaacs))
* [`d2d4d7c`](https://github.com/npm/npm/commit/d2d4d7cd3c32f91a87ffa11fe464d524029011c3)
  [#6082](https://github.com/npm/npm/issues/6082) don't allow tagging with a
  semver range as the tag name ([@isaacs](https://github.com/isaacs))

### v2.0.0-beta.2 (2014-08-29):

SPECIAL LABOR DAY WEEKEND RELEASE PARTY WOOO

* [`ed207e8`](https://github.com/npm/npm/commit/ed207e88019de3150037048df6267024566e1093)
  `npm-registry-client@3.1.7`: Clean up auth logic and improve logging around
  auth decisions. Also error on trying to change a user document without
  writing to it. ([@othiym23](https://github.com/othiym23))
* [`66c7423`](https://github.com/npm/npm/commit/66c7423b7fb07a326b83c83727879410d43c439f)
  `npmconf@2.0.7`: support -C as an alias for --prefix
  ([@isaacs](https://github.com/isaacs))
* [`0dc6a07`](https://github.com/npm/npm/commit/0dc6a07c778071c94c2251429c7d107e88a45095)
  [#6059](https://github.com/npm/npm/issues/6059) run commands in prefix, not
  cwd ([@isaacs](https://github.com/isaacs))
* [`65d2179`](https://github.com/npm/npm/commit/65d2179af96737eb9038eaa24a293a62184aaa13)
  `github-url-from-username-repo@1.0.1`: part 3 handle slashes in branch names
  ([@robertkowalski](https://github.com/robertkowalski))
* [`e8d75d0`](https://github.com/npm/npm/commit/e8d75d0d9f148ce2b3e8f7671fa281945bac363d)
  [#6057](https://github.com/npm/npm/issues/6057) `read-installed@3.1.1`:
  properly handle extraneous dev dependencies of required dependencies
  ([@othiym23](https://github.com/othiym23))
* [`0602f70`](https://github.com/npm/npm/commit/0602f708f070d524ad41573afd4c57171cab21ad)
  [#6064](https://github.com/npm/npm/issues/6064) ls: do not show deps of
  extraneous deps ([@isaacs](https://github.com/isaacs))

### v2.0.0-beta.1 (2014-08-28):

* [`78a1fc1`](https://github.com/npm/npm/commit/78a1fc12307a0cbdbc944775ed831b876ee65855)
  `github-url-from-git@1.4.0`: add support for git+https and git+ssh
  ([@stefanbuck](https://github.com/stefanbuck))
* [`bf247ed`](https://github.com/npm/npm/commit/bf247edf5429c6b3ec4d4cb798fa0eb0a9c19fc1)
  `columnify@1.2.1` ([@othiym23](https://github.com/othiym23))
* [`4bbe682`](https://github.com/npm/npm/commit/4bbe682a6d4eabcd23f892932308c9f228bf4de3)
  `cmd-shim@2.0.0`: upgrade to graceful-fs 3
  ([@ForbesLindesay](https://github.com/ForbesLindesay))
* [`ae1d590`](https://github.com/npm/npm/commit/ae1d590bdfc2476a4ed446e760fea88686e3ae05)
  `npm-package-arg@2.0.4`: accept slashes in branch names
  ([@thealphanerd](https://github.com/thealphanerd))
* [`b2f51ae`](https://github.com/npm/npm/commit/b2f51aecadf585711e145b6516f99e7c05f53614)
  `semver@3.0.1`: semver.clean() is cleaner
  ([@isaacs](https://github.com/isaacs))
* [`1d041a8`](https://github.com/npm/npm/commit/1d041a8a5ebd5bf6cecafab2072d4ec07823adab)
  `github-url-from-username-repo@1.0.0`: accept slashes in branch names
  ([@robertkowalski](https://github.com/robertkowalski))
* [`02c85d5`](https://github.com/npm/npm/commit/02c85d592c4058e5d9eafb0be36b6743ae631998)
  `async-some@1.0.1` ([@othiym23](https://github.com/othiym23))
* [`5af493e`](https://github.com/npm/npm/commit/5af493efa8a463cd1acc4a9a394699e2c0793b9c)
  ensure lifecycle spawn errors caught properly
  ([@isaacs](https://github.com/isaacs))
* [`60fe012`](https://github.com/npm/npm/commit/60fe012fac9570d6c72554cdf34a6fa95bf0f0a6)
  `npmconf@2.0.6`: init.version defaults to 1.0.0
  ([@isaacs](https://github.com/isaacs))
* [`b4c717b`](https://github.com/npm/npm/commit/b4c717bbf58fb6a0d64ad229036c79a184297ee2)
  `npm-registry-client@3.1.4`: properly encode % in passwords
  ([@isaacs](https://github.com/isaacs))
* [`7b55f44`](https://github.com/npm/npm/commit/7b55f44420252baeb3f30da437d22956315c31c9)
  doc: Fix 'npm help index' ([@isaacs](https://github.com/isaacs))

### v2.0.0-beta.0 (2014-08-21):

* [`685f8be`](https://github.com/npm/npm/commit/685f8be1f2770cc75fd0e519a8d7aac72735a270)
  `npm-registry-client@3.1.3`: Print the notification header returned by the
  registry, and make sure status codes are printed without gratuitous quotes
  around them. ([@isaacs](https://github.com/isaacs) /
  [@othiym23](https://github.com/othiym23))
* [`a8cb676`](https://github.com/npm/npm/commit/a8cb676aef0561eaf04487d2719672b097392c85)
  [#5900](https://github.com/npm/npm/issues/5900) remove `npm` from its own
  `engines` field in `package.json`. None of us remember why it was there.
  ([@timoxley](https://github.com/timoxley))
* [`6c47201`](https://github.com/npm/npm/commit/6c47201a7d071e8bf091b36933daf4199cc98e80)
  [#5752](https://github.com/npm/npm/issues/5752),
  [#6013](https://github.com/npm/npm/issues/6013) save git URLs correctly in
  `_resolved` fields ([@isaacs](https://github.com/isaacs))
* [`e4e1223`](https://github.com/npm/npm/commit/e4e1223a91c37688ba3378e1fc9d5ae045654d00)
  [#5936](https://github.com/npm/npm/issues/5936) document the use of tags in
  `package.json` ([@KenanY](https://github.com/KenanY))
* [`c92b8d4`](https://github.com/npm/npm/commit/c92b8d4db7bde2a501da5b7d612684de1d629a42)
  [#6004](https://github.com/npm/npm/issues/6004) manually installed scoped
  packages are tracked correctly ([@dead](https://github.com/dead)-horse)
* [`21ca0aa`](https://github.com/npm/npm/commit/21ca0aaacbcfe2b89b0a439d914da0cae62de550)
  [#5945](https://github.com/npm/npm/issues/5945) link scoped packages
  correctly ([@dead](https://github.com/dead)-horse)
* [`16bead7`](https://github.com/npm/npm/commit/16bead7f2c82aec35b83ff0ec04df051ba456764)
  [#5958](https://github.com/npm/npm/issues/5958) ensure that file streams work
  in all versions of node ([@dead](https://github.com/dead)-horse)
* [`dbf0cab`](https://github.com/npm/npm/commit/dbf0cab29d0db43ac95e4b5a1fbdea1e0af75f10)
  you can now pass quoted args to `npm run-script`
  ([@bcoe](https://github.com/bcoe))
* [`0583874`](https://github.com/npm/npm/commit/05838743f01ccb8d2432b3858d66847002fb62df)
  `tar@1.0.1`: Add test for removing an extract target immediately after
  unpacking.
  ([@isaacs](https://github.com/isaacs))
* [`cdf3b04`](https://github.com/npm/npm/commit/cdf3b0428bc0b0183fb41dcde9e34e8f42c5e3a7)
  `lockfile@1.0.0`: Fix incorrect interaction between `wait`, `stale`, and
  `retries` options. Part 2 of race condition leading to `ENOENT`
  ([@isaacs](https://github.com/isaacs))
  errors.
* [`22d72a8`](https://github.com/npm/npm/commit/22d72a87a9e1a9ab56d9585397f63551887d9125)
  `fstream@1.0.2`: Fix a double-finish call which can result in excess FS
  operations after the `close` event. Part 1 of race condition leading to
  `ENOENT` errors.
  ([@isaacs](https://github.com/isaacs))

### v2.0.0-alpha.7 (2014-08-14):

* [`f23f1d8`](https://github.com/npm/npm/commit/f23f1d8e8f86ec1b7ab8dad68250bccaa67d61b1)
  doc: update version doc to include `pre-*` increment args
  ([@isaacs](https://github.com/isaacs))
* [`b6bb746`](https://github.com/npm/npm/commit/b6bb7461824d4dc1c0936f46bd7929b5cd597986)
  build: add 'make tag' to tag current release as latest
  ([@isaacs](https://github.com/isaacs))
* [`27c4bb6`](https://github.com/npm/npm/commit/27c4bb606e46e5eaf604b19fe8477bc6567f8b2e)
  build: publish with `--tag=v1.4-next` ([@isaacs](https://github.com/isaacs))
* [`cff66c3`](https://github.com/npm/npm/commit/cff66c3bf2850880058ebe2a26655dafd002495e)
  build: add script to output `v1.4-next` publish tag
  ([@isaacs](https://github.com/isaacs))
* [`22abec8`](https://github.com/npm/npm/commit/22abec8833474879ac49b9604c103bc845dad779)
  build: remove outdated `docpublish` make target
  ([@isaacs](https://github.com/isaacs))
* [`1be4de5`](https://github.com/npm/npm/commit/1be4de51c3976db8564f72b00d50384c921f0917)
  build: remove `unpublish` step from `make publish`
  ([@isaacs](https://github.com/isaacs))
* [`e429e20`](https://github.com/npm/npm/commit/e429e2011f4d78e398f2461bca3e5a9a146fbd0c)
  doc: add new changelog ([@othiym23](https://github.com/othiym23))
* [`9243d20`](https://github.com/npm/npm/commit/9243d207896ea307082256604c10817f7c318d68)
  lifecycle: test lifecycle path modification
  ([@isaacs](https://github.com/isaacs))
* [`021770b`](https://github.com/npm/npm/commit/021770b9cb07451509f0a44afff6c106311d8cf6)
  lifecycle: BREAKING CHANGE do not add the directory containing node executable
  ([@chulkilee](https://github.com/chulkilee))
* [`1d5c41d`](https://github.com/npm/npm/commit/1d5c41dd0d757bce8b87f10c4135f04ece55aeb9)
  install: rename .gitignore when unpacking foreign tarballs
  ([@isaacs](https://github.com/isaacs))
* [`9aac267`](https://github.com/npm/npm/commit/9aac2670a73423544d92b27cc301990a16a9563b)
  cache: detect non-gzipped tar files more reliably
  ([@isaacs](https://github.com/isaacs))
* [`3f24755`](https://github.com/npm/npm/commit/3f24755c8fce3c7ab11ed1dc632cc40d7ef42f62)
  `readdir-scoped-modules@1.0.0` ([@isaacs](https://github.com/isaacs))
* [`151cd2f`](https://github.com/npm/npm/commit/151cd2ff87b8ac2fc9ea366bc9b7f766dc5b9684)
  `read-installed@3.1.0` ([@isaacs](https://github.com/isaacs))
* [`f5a9434`](https://github.com/npm/npm/commit/f5a94343a8ebe4a8cd987320b55137aef53fb3fd)
  test: fix Travis timeouts ([@dylang](https://github.com/dylang))
* [`126cafc`](https://github.com/npm/npm/commit/126cafcc6706814c88af3042f2ffff408747bff4)
  `npm-registry-couchapp@2.5.0` ([@othiym23](https://github.com/othiym23))

### v2.0.0-alpha.6 (2014-08-07):

BREAKING CHANGE:

* [`ea547e2`](https://github.com/npm/npm/commit/ea547e2) Bump semver to
  version 3: `^0.x.y` is now functionally the same as `=0.x.y`.
  ([@isaacs](https://github.com/isaacs))

Other changes:

* [`d987707`](https://github.com/npm/npm/commit/d987707) move fetch into
  npm-registry-client ([@othiym23](https://github.com/othiym23))
* [`9b318e2`](https://github.com/npm/npm/commit/9b318e2) `read-installed@3.0.0`
  ([@isaacs](https://github.com/isaacs))
* [`9d73de7`](https://github.com/npm/npm/commit/9d73de7) remove unnecessary
  mkdirps ([@isaacs](https://github.com/isaacs))
* [`33ccd13`](https://github.com/npm/npm/commit/33ccd13) Don't squash execute
  perms in `_git-remotes/` dir ([@adammeadows](https://github.com/adammeadows))
* [`48fd233`](https://github.com/npm/npm/commit/48fd233) `npm-package-arg@2.0.1`
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
