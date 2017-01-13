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
  ([@Kat Marchán](https://github.com/Kat Marchán))
* [`85b0174`](https://github.com/npm/npm/commit/85b0174ba9842e8e89f3c33d009e4b4a9e877c7d)
  `request@2.79.0`
  ([@Kat Marchán](https://github.com/Kat Marchán))
* [`9664d36`](https://github.com/npm/npm/commit/9664d36653503247737630440bc2ff657de965c3)
  `tap@8.0.1`
  ([@Kat Marchán](https://github.com/Kat Marchán))

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
