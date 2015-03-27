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
  to clone the remote repo. Fixes using Git dependencies when shrinkwwapping.
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
  adding git repo to cach under Windows.
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

### v1.4.28 (2014-09-12):

* [`f4540b6`](https://github.com/npm/npm/commit/f4540b6537a87e653d7495a9ddcf72949fdd4d14)
  [#6043](https://github.com/npm/npm/issues/6043) defer rollbacks until just
  before the CLI exits ([@isaacs](https://github.com/isaacs))
* [`1eabfd5`](https://github.com/npm/npm/commit/1eabfd5c03f33c2bd28823714ff02059eeee3899)
  [#6043](https://github.com/npm/npm/issues/6043) `slide@1.1.6`: wait until all
  callbacks have finished before proceeding
  ([@othiym23](https://github.com/othiym23))

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

### v1.4.27 (2014-09-04):

* [`4cf3c8f`](https://github.com/npm/npm/commit/4cf3c8fd78c9e2693a5f899f50c28f4823c88e2e)
  [#6007](https://github.com/npm/npm/issues/6007) request@2.42.0: properly set
  headers on proxy requests ([@isaacs](https://github.com/isaacs))
* [`403cb52`](https://github.com/npm/npm/commit/403cb526be1472bb7545fa8e62d4976382cdbbe5)
  [#6055](https://github.com/npm/npm/issues/6055) npmconf@1.1.8: restore
  case-insensitivity of environmental config
  ([@iarna](https://github.com/iarna))

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
  [#5525](https://github.com/npm/npm/issues/5525) ([@bmeck](https://github.com/bmeck))
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
