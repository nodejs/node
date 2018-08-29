## v6.4.0 (2018-09-08):

### NEW FEATURES

* [`6e9f04b0b`](https://github.com/npm/cli/commit/6e9f04b0baed007169d4e0c341f097cf133debf7)
  [npm/cli#8](https://github.com/npm/cli/pull/8)
  Search for authentication token defined by environment variables by preventing
  the translation layer from env variable to npm option from breaking
  `:_authToken`.
  ([@mkhl](https://github.com/mkhl))
* [`84bfd23e7`](https://github.com/npm/cli/commit/84bfd23e7d6434d30595594723a6e1976e84b022)
  [npm/cli#35](https://github.com/npm/cli/pull/35)
  Stop filtering out non-IPv4 addresses from `local-addrs`, making npm actually
  use IPv6 addresses when it must.
  ([@valentin2105](https://github.com/valentin2105))
* [`792c8c709`](https://github.com/npm/cli/commit/792c8c709dc7a445687aa0c8cba5c50bc4ed83fd)
  [npm/cli#31](https://github.com/npm/cli/pull/31)
  configurable audit level for non-zero exit
  `npm audit` currently exits with exit code 1 if any vulnerabilities are found of any level.
  Add a flag of `--audit-level` to `npm audit` to allow it to pass if only vulnerabilities below a certain level are found.
  Example: `npm audit --audit-level=high` will exit with 0 if only low or moderate level vulns are detected.
  ([@lennym](https://github.com/lennym))

### BUGFIXES

* [`d81146181`](https://github.com/npm/cli/commit/d8114618137bb5b9a52a86711bb8dc18bfc8e60c)
  [npm/cli#32](https://github.com/npm/cli/pull/32)
  Don't check for updates to npm when we are updating npm itself.
  ([@olore](https://github.com/olore))

### DEPENDENCY UPDATES

A very special dependency update event! Since the [release of
`node-gyp@3.8.0`](https://github.com/nodejs/node-gyp/pull/1521), an awkward
version conflict that was preventing `request` from begin flattened was
resolved. This means two things:

1. We've cut down the npm tarball size by another 200kb, to 4.6MB
2. `npm audit` now shows no vulnerabilities for npm itself!

Thanks, [@rvagg](https://github.com/rvagg)!

* [`866d776c2`](https://github.com/npm/cli/commit/866d776c27f80a71309389aaab42825b2a0916f6)
  `request@2.87.0`
  ([@simov](https://github.com/simov))
* [`f861c2b57`](https://github.com/npm/cli/commit/f861c2b579a9d4feae1653222afcefdd4f0e978f)
  `node-gyp@3.8.0`
  ([@rvagg](https://github.com/rvagg))
* [`32e6947c6`](https://github.com/npm/cli/commit/32e6947c60db865257a0ebc2f7e754fedf7a6fc9)
  [npm/cli#39](https://github.com/npm/cli/pull/39)
  `colors@1.1.2`:
  REVERT REVERT, newer versions of this library are broken and print ansi
  codes even when disabled.
  ([@iarna](https://github.com/iarna))
* [`beb96b92c`](https://github.com/npm/cli/commit/beb96b92caf061611e3faafc7ca10e77084ec335)
  `libcipm@2.0.1`
  ([@zkat](https://github.com/zkat))
* [`348fc91ad`](https://github.com/npm/cli/commit/348fc91ad223ff91cd7bcf233018ea1d979a2af1)
  `validate-npm-package-license@3.0.4`: Fixes errors with empty or string-only
  license fields.
  ([@Gudahtt](https://github.com/Gudahtt))
* [`e57d34575`](https://github.com/npm/cli/commit/e57d3457547ef464828fc6f82ae4750f3e511550)
  `iferr@1.0.2`
  ([@shesek](https://github.com/shesek))
* [`46f1c6ad4`](https://github.com/npm/cli/commit/46f1c6ad4b2fd5b0d7ec879b76b76a70a3a2595c)
  `tar@4.4.6`
  ([@isaacs](https://github.com/isaacs))
* [`50df1bf69`](https://github.com/npm/cli/commit/50df1bf691e205b9f13e0fff0d51a68772c40561)
  `hosted-git-info@2.7.1`
  ([@iarna](https://github.com/iarna))
  ([@Erveon](https://github.com/Erveon))
  ([@huochunpeng](https://github.com/huochunpeng))

### DOCUMENTATION

* [`af98e76ed`](https://github.com/npm/cli/commit/af98e76ed96af780b544962aa575585b3fa17b9a)
  [npm/cli#34](https://github.com/npm/cli/pull/34)
  Remove `npm publish` from list of commands not affected by `--dry-run`.
  ([@joebowbeer](https://github.com/joebowbeer))
* [`e2b0f0921`](https://github.com/npm/cli/commit/e2b0f092193c08c00f12a6168ad2bd9d6e16f8ce)
  [npm/cli#36](https://github.com/npm/cli/pull/36)
  Tweak formatting in repository field examples.
  ([@noahbenham](https://github.com/noahbenham))
* [`e2346e770`](https://github.com/npm/cli/commit/e2346e7702acccefe6d711168c2b0e0e272e194a)
  [npm/cli#14](https://github.com/npm/cli/pull/14)
  Used `process.env` examples to make accessing certain `npm run-scripts`
  environment variables more clear.
  ([@mwarger](https://github.com/mwarger))

## v6.3.0 (2018-08-01):

This is basically the same as the prerelease, but two dependencies have been
bumped due to bugs that had been around for a while.

* [`0a22be42e`](https://github.com/npm/cli/commit/0a22be42eb0d40cd0bd87e68c9e28fc9d72c0e19)
  `figgy-pudding@3.2.0`
  ([@zkat](https://github.com/zkat))
* [`0096f6997`](https://github.com/npm/cli/commit/0096f69978d2f40b170b28096f269b0b0008a692)
  `cacache@11.1.0`
  ([@zkat](https://github.com/zkat))

## v6.3.0-next.0 (2018-07-25):

### NEW FEATURES

* [`ad0dd226f`](https://github.com/npm/cli/commit/ad0dd226fb97a33dcf41787ae7ff282803fb66f2)
  [npm/cli#26](https://github.com/npm/cli/pull/26)
  `npm version` now supports a `--preid` option to specify the preid for
  prereleases. For example, `npm version premajor --preid rc` will tag a version
  like `2.0.0-rc.0`.
  ([@dwilches](https://github.com/dwilches))

### MESSAGING IMPROVEMENTS

* [`c1dad1e99`](https://github.com/npm/cli/commit/c1dad1e994827f2eab7a13c0f6454f4e4c22ebc2)
  [npm/cli#6](https://github.com/npm/cli/pull/6)
  Make `npm audit fix` message provide better instructions for vulnerabilities
  that require manual review.
  ([@bradsk88](https://github.com/bradsk88))
* [`15c1130fe`](https://github.com/npm/cli/commit/15c1130fe81961706667d845aad7a5a1f70369f3)
  Fix missing colon next to tarball url in new `npm view` output.
  ([@zkat](https://github.com/zkat))
* [`21cf0ab68`](https://github.com/npm/cli/commit/21cf0ab68cf528d5244ae664133ef400bdcfbdb6)
  [npm/cli#24](https://github.com/npm/cli/pull/24)
  Use the defaut OTP explanation everywhere except when the context is
  "OTP-aware" (like when setting double-authentication). This improves the
  overall CLI messaging when prompting for an OTP code.
  ([@jdeniau](https://github.com/jdeniau))

### MISC

* [`a9ac8712d`](https://github.com/npm/cli/commit/a9ac8712dfafcb31a4e3deca24ddb92ff75e942d)
  [npm/cli#21](https://github.com/npm/cli/pull/21)
  Use the extracted `stringify-package` package.
  ([@dpogue](https://github.com/dpogue))
* [`9db15408c`](https://github.com/npm/cli/commit/9db15408c60be788667cafc787116555507dc433)
  [npm/cli#27](https://github.com/npm/cli/pull/27)
  `wrappy` was previously added to dependencies in order to flatten it, but we
  no longer do legacy-style for npm itself, so it has been removed from
  `package.json`.
  ([@rickschubert](https://github.com/rickschubert))

### DOCUMENTATION

* [`3242baf08`](https://github.com/npm/cli/commit/3242baf0880d1cdc0e20b546d3c1da952e474444)
  [npm/cli#13](https://github.com/npm/cli/pull/13)
  Update more dead links in README.md.
  ([@u32i64](https://github.com/u32i64))
* [`06580877b`](https://github.com/npm/cli/commit/06580877b6023643ec780c19d84fbe120fe5425c)
  [npm/cli#19](https://github.com/npm/cli/pull/19)
  Update links in docs' `index.html` to refer to new bug/PR URLs.
  ([@watilde](https://github.com/watilde))
* [`ca03013c2`](https://github.com/npm/cli/commit/ca03013c23ff38e12902e9569a61265c2d613738)
  [npm/cli#15](https://github.com/npm/cli/pull/15)
  Fix some typos in file-specifiers docs.
  ([@Mstrodl](https://github.com/Mstrodl))
* [`4f39f79bc`](https://github.com/npm/cli/commit/4f39f79bcacef11bf2f98d09730bc94d0379789b)
  [npm/cli#16](https://github.com/npm/cli/pull/16)
  Fix some typos in file-specifiers and package-lock docs.
  ([@watilde](https://github.com/watilde))
* [`35e51f79d`](https://github.com/npm/cli/commit/35e51f79d1a285964aad44f550811aa9f9a72cd8)
  [npm/cli#18](https://github.com/npm/cli/pull/18)
  Update build status badge url in README.
  ([@watilde](https://github.com/watilde))
* [`a67db5607`](https://github.com/npm/cli/commit/a67db5607ba2052b4ea44f66657f98b758fb4786)
  [npm/cli#17](https://github.com/npm/cli/pull/17/)
  Replace TROUBLESHOOTING.md with [posts in
  npm.community](https://npm.community/c/support/troubleshooting).
  ([@watilde](https://github.com/watilde))
* [`e115f9de6`](https://github.com/npm/cli/commit/e115f9de65bf53711266152fc715a5012f7d3462)
  [npm/cli#7](https://github.com/npm/cli/pull/7)
  Use https URLs in documentation when appropriate. Happy [Not Secure Day](https://arstechnica.com/gadgets/2018/07/todays-the-day-that-chrome-brands-plain-old-http-as-not-secure/)!
  ([@XhmikosR](https://github.com/XhmikosR))

## v6.2.0 (2018-07-13):

In case you missed it, [we
moved!](https://blog.npmjs.org/post/175587538995/announcing-npmcommunity). We
look forward to seeing future PRs landing in
[npm/cli](https://github.com/npm/cli) in the future, and we'll be chatting with
you all in [npm.community](https://npm.community). Go check it out!

This final release of `npm@6.2.0` includes a couple of features that weren't
quite ready on time but that we'd still like to include. Enjoy!

### FEATURES

* [`244b18380`](https://github.com/npm/npm/commit/244b18380ee55950b13c293722771130dbad70de)
  [#20554](https://github.com/npm/npm/pull/20554)
  Add support for tab-separated output for `npm audit` data with the
  `--parseable` flag.
  ([@luislobo](https://github.com/luislobo))
* [`7984206e2`](https://github.com/npm/npm/commit/7984206e2f41b8d8361229cde88d68f0c96ed0b8)
  [#12697](https://github.com/npm/npm/pull/12697)
  Add new `sign-git-commit` config to control whether the git commit itself gets
  signed, or just the tag (which is the default).
  ([@tribou](https://github.com/tribou))

### FIXES

* [`4c32413a5`](https://github.com/npm/npm/commit/4c32413a5b42e18a34afb078cf00eed60f08e4ff)
  [#19418](https://github.com/npm/npm/pull/19418)
  Do not use `SET` to fetch the env in git-bash or Cygwin.
  ([@gucong3000](https://github.com/gucong3000))

### DEPENDENCY BUMPS

* [`d9b2712a6`](https://github.com/npm/npm/commit/d9b2712a670e5e78334e83f89a5ed49616f1f3d3)
  `request@2.81.0`: Downgraded to allow better deduplication. This does
  introduce a bunch of `hoek`-related audit reports, but they don't affect npm
  itself so we consider it safe. We'll upgrade `request` again once `node-gyp`
  unpins it.
  ([@simov](https://github.com/simov))
* [`2ac48f863`](https://github.com/npm/npm/commit/2ac48f863f90166b2bbf2021ed4cc04343d2503c)
  `node-gyp@3.7.0`
  ([@MylesBorins](https://github.com/MylesBorins))
* [`8dc6d7640`](https://github.com/npm/npm/commit/8dc6d76408f83ba35bda77a2ac1bdbde01937349)
  `cli-table3@0.5.0`: `cli-table2` is unmaintained and required `lodash`. With
  this dependency bump, we've removed `lodash` from our tree, which cut back
  tarball size by another 300kb.
  ([@Turbo87](https://github.com/Turbo87))
* [`90c759fee`](https://github.com/npm/npm/commit/90c759fee6055cf61cf6709432a5e6eae6278096)
  `npm-audit-report@1.3.1`
  ([@zkat](https://github.com/zkat))
* [`4231a0a1e`](https://github.com/npm/npm/commit/4231a0a1eb2be13931c3b71eba38c0709644302c)
  Add `cli-table3` to bundleDeps.
  ([@iarna](https://github.com/iarna))
* [`322d9c2f1`](https://github.com/npm/npm/commit/322d9c2f107fd82a4cbe2f9d7774cea5fbf41b8d)
  Make `standard` happy.
  ([@iarna](https://github.com/iarna))

### DOCS

* [`5724983ea`](https://github.com/npm/npm/commit/5724983ea8f153fb122f9c0ccab6094a26dfc631)
  [#21165](https://github.com/npm/npm/pull/21165)
  Fix some markdown formatting in npm-disputes.md.
  ([@hchiam](https://github.com/hchiam))
* [`738178315`](https://github.com/npm/npm/commit/738178315fe48e463028657ea7ae541c3d63d171)
  [#20920](https://github.com/npm/npm/pull/20920)
  Explicitly state that republishing an unpublished package requires a 72h
  waiting period.
  ([@gmattie](https://github.com/gmattie))
* [`f0a372b07`](https://github.com/npm/npm/commit/f0a372b074cc43ee0e1be28dbbcef0d556b3b36c)
  Replace references to the old repo or issue tracker. We're at npm/cli now!
  ([@zkat](https://github.com/zkat))

## v6.2.0-next.1 (2018-07-05):

This is a quick patch to the release to fix an issue that was preventing users
from installing `npm@next`.

* [`ecdcbd745`](https://github.com/npm/npm/commit/ecdcbd745ae1edd9bdd102dc3845a7bc76e1c5fb)
  [#21129](https://github.com/npm/npm/pull/21129)
  Remove postinstall script that depended on source files, thus preventing
  `npm@next` from being installable from the registry.
  ([@zkat](https://github.com/zkat))

## v6.2.0-next.0 (2018-06-28):

### NEW FEATURES

* [`ce0793358`](https://github.com/npm/npm/commit/ce07933588ec2da1cc1980f93bdaa485d6028ae2)
  [#20750](https://github.com/npm/npm/pull/20750)
  You can now disable the update notifier entirely by using
  `--no-update-notifier` or setting it in your config with `npm config set
  update-notifier false`.
  ([@travi](https://github.com/travi))
* [`d2ad776f6`](https://github.com/npm/npm/commit/d2ad776f6dcd92ae3937465736dcbca171131343)
  [#20879](https://github.com/npm/npm/pull/20879)
  When `npm run-script <script>` fails due to a typo or missing script, npm will
  now do a "did you mean?..." for scripts that do exist.
  ([@watilde](https://github.com/watilde))

### BUGFIXES

* [`8f033d72d`](https://github.com/npm/npm/commit/8f033d72da3e84a9dbbabe3a768693817af99912)
  [#20948](https://github.com/npm/npm/pull/20948)
  Fix the regular expression matching in `xcode_emulation` in `node-gyp` to also
  handle version numbers with multiple-digit major versions which would
  otherwise break under use of XCode 10.
  ([@Trott](https://github.com/Trott))
* [`c8ba7573a`](https://github.com/npm/npm/commit/c8ba7573a4ea95789f674ce038762d6a77a8b047)
  Stop trying to hoist/dedupe bundles dependencies.
  ([@iarna](https://github.com/iarna))
* [`cd698f068`](https://github.com/npm/npm/commit/cd698f06840b7c9407ac802efa96d16464722a7d)
  [#20762](https://github.com/npm/npm/pull/20762)
  Add synopsis to brief help for `npm audit` and suppress trailing newline.
  ([@wyardley](https://github.com/wyardley))
* [`6808ee3bd`](https://github.com/npm/npm/commit/6808ee3bd59560b1334a18aa6c6e0120094b03c0)
  [#20881](https://github.com/npm/npm/pull/20881)
  Exclude /.github directory from npm tarball.
  ([@styfle](https://github.com/styfle))
* [`177cbb476`](https://github.com/npm/npm/commit/177cbb4762c1402bfcbf0636c4bc4905fd684fc1)
  [#21105](https://github.com/npm/npm/pull/21105)
  Add suggestion to use a temporary cache instead of `npm cache clear --force`.
  ([@karanjthakkar](https://github.com/karanjthakkar))

### DOCS

* [`7ba3fca00`](https://github.com/npm/npm/commit/7ba3fca00554b884eb47f2ed661693faf2630b27)
  [#20855](https://github.com/npm/npm/pull/20855)
  Direct people to npm.community instead of the GitHub issue tracker on error.
  ([@zkat](https://github.com/zkat))
* [`88efbf6b0`](https://github.com/npm/npm/commit/88efbf6b0b403c5107556ff9e1bb7787a410d14d)
  [#20859](https://github.com/npm/npm/pull/20859)
  Fix typo in registry docs.
  ([@strugee](https://github.com/strugee))
* [`61bf827ae`](https://github.com/npm/npm/commit/61bf827aea6f98bba08a54e60137d4df637788f9)
  [#20947](https://github.com/npm/npm/pull/20947)
  Fixed a small grammar error in the README.
  ([@bitsol](https://github.com/bitsol))
* [`f5230c90a`](https://github.com/npm/npm/commit/f5230c90afef40f445bf148cbb16d6129a2dcc19)
  [#21018](https://github.com/npm/npm/pull/21018)
  Small typo fix in CONTRIBUTING.md.
  ([@reggi](https://github.com/reggi))
* [`833efe4b2`](https://github.com/npm/npm/commit/833efe4b2abcef58806f823d77ab8bb8f4f781c6)
  [#20986](https://github.com/npm/npm/pull/20986)
  Document current structure/expectations around package tarballs.
  ([@Maximaximum](https://github.com/Maximaximum))
* [`9fc0dc4f5`](https://github.com/npm/npm/commit/9fc0dc4f58d728bac6a8db7143d04863d7b653db)
  [#21019](https://github.com/npm/npm/pull/21019)
  Clarify behavior of `npm link ../path` shorthand.
  ([@davidgilbertson](https://github.com/davidgilbertson))
* [`3924c72d0`](https://github.com/npm/npm/commit/3924c72d06b9216ac2b6a9d951fd565a1d5eda89)
  [#21064](https://github.com/npm/npm/pull/21064)
  Add missing "if"
  ([@roblourens](https://github.com/roblourens))

### DEPENDENCY SHUFFLE!

We did some reshuffling and moving around of npm's own dependencies. This
significantly reduces the total bundle size of the npm pack, from 8MB to 4.8MB
for the distributed tarball! We also moved around what we actually commit to the
repo as far as devDeps go.

* [`0483f5c5d`](https://github.com/npm/npm/commit/0483f5c5deaf18c968a128657923103e49f4e67a)
  Flatten and dedupe our dependencies!
  ([@iarna](https://github.com/iarna))
* [`ef9fa1ceb`](https://github.com/npm/npm/commit/ef9fa1ceb5f9d175fd453138b1a26d45a5071dfd)
  Remove unused direct dependency `ansi-regex`.
  ([@iarna](https://github.com/iarna))
* [`0d14b0bc5`](https://github.com/npm/npm/commit/0d14b0bc59812f4e33798194e11ffacbea3c0493)
  Reshuffle ansi-regex for better deduping.
  ([@iarna](https://github.com/iarna))
* [`68a101859`](https://github.com/npm/npm/commit/68a101859b2b6f78b2e7c3a936492acdb15f7c4a)
  Reshuffle strip-ansi for better deduping.
  ([@iarna](https://github.com/iarna))
* [`0d5251f97`](https://github.com/npm/npm/commit/0d5251f97dc8b8b143064869e530d465c757ffbb)
  Reshuffle is-fullwidth-code-point for better deduping.
  ([@iarna](https://github.com/iarna))
* [`2d0886632`](https://github.com/npm/npm/commit/2d08866327013522fc5fbe61ed872b8f30e92775)
  Add fake-registry, npm-registry-mock replacement.
  ([@iarna](https://github.com/iarna))

### DEPENDENCIES

* [`8cff8eea7`](https://github.com/npm/npm/commit/8cff8eea75dc34c9c1897a7a6f65d7232bb0c64c)
  `tar@4.4.3`
  ([@zkat](https://github.com/zkat))
* [`bfc4f873b`](https://github.com/npm/npm/commit/bfc4f873bd056b7e3aee389eda4ecd8a2e175923)
  `pacote@8.1.6`
  ([@zkat](https://github.com/zkat))
* [`532096163`](https://github.com/npm/npm/commit/53209616329119be8fcc29db86a43cc8cf73454d)
  `libcipm@2.0.0`
  ([@zkat](https://github.com/zkat))
* [`4a512771b`](https://github.com/npm/npm/commit/4a512771b67aa06505a0df002a9027c16a238c71)
  `request@2.87.0`
  ([@iarna](https://github.com/iarna))
* [`b7cc48dee`](https://github.com/npm/npm/commit/b7cc48deee45da1feab49aa1dd4d92e33c9bcac8)
  `which@1.3.1`
  ([@iarna](https://github.com/iarna))
* [`bae657c28`](https://github.com/npm/npm/commit/bae657c280f6ea8e677509a9576e1b47c65c5441)
  `tar@4.4.4`
  ([@iarna](https://github.com/iarna))
* [`3d46e5c4e`](https://github.com/npm/npm/commit/3d46e5c4e3c5fecd9bf05a7425a16f2e8ad5c833)
  `JSONStream@1.3.3`
  ([@iarna](https://github.com/iarna))
* [`d0a905daf`](https://github.com/npm/npm/commit/d0a905dafc7e3fcd304e8053acbe3da40ba22554)
  `is-cidr@2.0.6`
  ([@iarna](https://github.com/iarna))
* [`4fc1f815f`](https://github.com/npm/npm/commit/4fc1f815fec5a7f6f057cf305e01d4126331d1f2)
  `marked@0.4.0`
  ([@iarna](https://github.com/iarna))
* [`f72202944`](https://github.com/npm/npm/commit/f722029441a088d03df94bdfdeeec51cfd318659)
  `tap@12.0.1`
  ([@iarna](https://github.com/iarna))
* [`bdce96eb3`](https://github.com/npm/npm/commit/bdce96eb3c30fcff873aa3f1190e8ae4928d690b)
  `npm-profile@3.0.2`
  ([@iarna](https://github.com/iarna))
* [`fe4240e85`](https://github.com/npm/npm/commit/fe4240e852144770bf76d7b1952056ca5baa63cf)
  `uuid@3.3.2`
  ([@zkat](https://github.com/zkat))

## v6.1.0 (2018-05-17):

### FIX WRITE AFTER END ERROR

First introduced in 5.8.0, this finally puts to bed errors where you would
occasionally see `Error: write after end at MiniPass.write`.

* [`171f3182f`](https://github.com/npm/npm/commit/171f3182f32686f2f94ea7d4b08035427e0b826e)
  [node-tar#180](https://github.com/npm/node-tar/issues/180)
  [npm.community#35](https://npm.community/t/write-after-end-when-installing-packages-with-5-8-and-later/35)
  `pacote@8.1.5`: Fix write-after-end errors.
  ([@zkat](https://github.com/zkat))

### DETECT CHANGES IN GIT SPECIFIERS

* [`0e1726c03`](https://github.com/npm/npm/commit/0e1726c0350a02d5a60f5fddb1e69c247538625e)
  We can now determine if the commitid of a git dependency in the lockfile is derived
  from the specifier in the package.json and if it isn't we now trigger an update for it.
  ([@iarna](https://github.com/iarna))

### OTHER BUGS

* [`442d2484f`](https://github.com/npm/npm/commit/442d2484f686e3a371b07f8473a17708f84d9603)
  [`2f0c88351`](https://github.com/npm/npm/commit/2f0c883519f17c94411dd1d9877c5666f260c12f)
  [`631d30a34`](https://github.com/npm/npm/commit/631d30a340f5805aed6e83f47a577ca4125599b2)
  When requesting the update of a direct dependency that was also a
  transitive dependency to a version incompatible with the transitive
  requirement and you had a lock-file but did not have a `node_modules`
  folder then npm would fail to provide a new copy of the transitive
  dependency, resulting in an invalid lock-file that could not self heal.
  ([@iarna](https://github.com/iarna))
* [`be5dd0f49`](https://github.com/npm/npm/commit/be5dd0f496ec1485b1ea3094c479dfc17bd50d82)
  [#20715](https://github.com/npm/npm/pull/20715)
  Cleanup output of `npm ci` summary report.
  ([@legodude17](https://github.com/legodude17))
* [`98ffe4adb`](https://github.com/npm/npm/commit/98ffe4adb55a6f4459271856de2e27e95ee63375)
  Node.js now has a test that scans for things that look like conflict
  markers in source code.  This was triggering false positives on a fixture in a test
  of npm's ability to heal lockfiles with conflicts in them.
  ([@iarna](https://github.com/iarna))

### DEPENDENCY UPDATES

* [`3f2e306b8`](https://github.com/npm/npm/commit/3f2e306b884a027df03f64524beb8658ce1772cb)
  Using `npm audit fix`, replace some transitive dependencies with security
  issues with versions that don't have any.
  ([@iarna](https://github.com/iarna))
* [`1d07134e0`](https://github.com/npm/npm/commit/1d07134e0b157f7484a20ce6987ff57951842954)
  `tar@4.4.1`:
  Dropping to 4.4.1 from 4.4.2 due to https://github.com/npm/node-tar/issues/183
  ([@zkat](https://github.com/zkat))


## v6.1.0-next.0 (2018-05-17):

Look at that! A feature bump! `npm@6` was super-exciting not just because it
used a bigger number than ever before, but also because it included a super
shiny new command: `npm audit`. Well, we've kept working on it since then and
have some really nice improvements for it. You can expect more of them, and the
occasional fix, in the next few releases as more users start playing with it and
we get more feedback about what y'all would like to see from something like
this.

I, for one, have started running it (and the new subcommand...) in all my
projects, and it's one of those things that I don't know how I ever functioned
-without- it! This will make a world of difference to so many people as far as
making the npm ecosystem a higher-quality, safer commons for all of us.

This is also a good time to remind y'all that we have a new [RFCs
repository](https://github.com/npm/rfcs), along with a new process for them.
This repo is open to anyone's RFCs, and has already received some great ideas
about where we can take the CLI (and, to a certain extent, the registry). It's a
great place to get feedback, and completely replaces feature requests in the
main repo, so we won't be accepting feature requests there at all anymore. Check
it out if you have something you'd like to suggest, or if you want to keep track
of what the future might look like!

### NEW FEATURE: `npm audit fix`

This is the biggie with this release! `npm audit fix` does exactly what it says
on the tin. It takes all the actionable reports from your `npm audit` and runs
the installs automatically for you, so you don't have to try to do all that
mechanical work yourself!

Note that by default, `npm audit fix` will stick to semver-compatible changes,
so you should be able to safely run it on most projects and carry on with your
day without having to track down what breaking changes were included. If you
want your (toplevel) dependencies to accept semver-major bumps as well, you can
use `npm audit fix --force` and it'll toss those in, as well. Since it's running
the npm installer under the hood, it also supports `--production` and
`--only=dev` flags, as well as things like `--dry-run`, `--json`, and
`--package-lock-only`, if you want more control over what it does.

Give it a whirl and tell us what you think! See `npm help audit` for full docs!

* [`3800a660d`](https://github.com/npm/npm/commit/3800a660d99ca45c0175061dbe087520db2f54b7)
  Add `npm audit fix` subcommand to automatically fix detected vulnerabilities.
  ([@zkat](https://github.com/zkat))

### OTHER NEW `audit` FEATURES

* [`1854b1c7f`](https://github.com/npm/npm/commit/1854b1c7f09afceb49627e539a086d8a3565601c)
  [#20568](https://github.com/npm/npm/pull/20568)
  Add support for `npm audit --json` to print the report in JSON format.
  ([@finnp](https://github.com/finnp))
* [`85b86169d`](https://github.com/npm/npm/commit/85b86169d9d0423f50893d2ed0c7274183255abe)
  [#20570](https://github.com/npm/npm/pull/20570)
  Include number of audited packages in `npm install` summary output.
  ([@zkat](https://github.com/zkat))
* [`957cbe275`](https://github.com/npm/npm/commit/957cbe27542d30c33e58e7e6f2f04eeb64baf5cd)
  `npm-audit-report@1.2.1`:
  Overhaul audit install and detail output format. The new format is terser and
  fits more closely into the visual style of the CLI, while still providing you
  with the important bits of information you need. They also include a bit more
  detail on the footer about what actions you can take!
  ([@zkat](https://github.com/zkat))

### NEW FEATURE: GIT DEPS AND `npm init <pkg>`!

Another exciting change that came with `npm@6` was the new `npm init` command
that allows for community-authored generators. That means you can, for example,
do `npm init react-app` and it'll one-off download, install, and run
[`create-react-app`](https://npm.im/create-react-app) for you, without requiring
or keeping around any global installs. That is, it basically just calls out to
[`npx`](https://npm.im/npx).

The first version of this command only really supported registry dependencies,
but now, [@jdalton](https://github.com/jdalton) went ahead and extended this
feature so you can use hosted git dependencies, and their shorthands.

So go ahead and do `npm init facebook/create-react-app` and it'll grab the
package from the github repo now! Or you can use it with a private github
repository to maintain your organizational scaffolding tools or whatnot. ✨

* [`483e01180`](https://github.com/npm/npm/commit/483e011803af82e63085ef41b7acce5b22aa791c)
  [#20403](https://github.com/npm/npm/pull/20403)
  Add support for hosted git packages to `npm init <name>`.
  ([@jdalton](https://github.com/jdalton))

### BUGFIXES

* [`a41c0393c`](https://github.com/npm/npm/commit/a41c0393cba710761a15612c6c85c9ef2396e65f)
  [#20538](https://github.com/npm/npm/pull/20538)
  Make the new `npm view` work when the license field is an object instead of a
  string.
  ([@zkat](https://github.com/zkat))
* [`eb7522073`](https://github.com/npm/npm/commit/eb75220739302126c94583cc65a5ff12b441e3c6)
  [#20582](https://github.com/npm/npm/pull/20582)
  Add support for environments (like Docker) where the expected binary for
  opening external URLs is not available.
  ([@bcoe](https://github.com/bcoe))
* [`212266529`](https://github.com/npm/npm/commit/212266529ae72056bf0876e2cff4b8ba01d09d0f)
  [#20536](https://github.com/npm/npm/pull/20536)
  Fix a spurious colon in the new update notifier message and add support for
  the npm canary.
  ([@zkat](https://github.com/zkat))
* [`5ee1384d0`](https://github.com/npm/npm/commit/5ee1384d02c3f11949d7a26ec6322488476babe6)
  [#20597](https://github.com/npm/npm/pull/20597)
  Infer a version range when a `package.json` has a dist-tag instead of a
  version range in one of its dependency specs. Previously, this would cause
  dependencies to be flagged as invalid.
  ([@zkat](https://github.com/zkat))
* [`4fa68ae41`](https://github.com/npm/npm/commit/4fa68ae41324293e59584ca6cf0ac24b3e0825bb)
  [#20585](https://github.com/npm/npm/pull/20585)
  Make sure scoped bundled deps are shown in the new publish preview, too.
  ([@zkat](https://github.com/zkat))
* [`1f3ee6b7e`](https://github.com/npm/npm/commit/1f3ee6b7e1b36b52bdedeb9241296d4e66561d48)
  `cacache@11.0.2`:
  Stop dropping `size` from metadata on `npm cache verify`.
  ([@jfmartinez](https://github.com/jfmartinez))
* [`91ef93691`](https://github.com/npm/npm/commit/91ef93691a9d6ce7c016fefdf7da97854ca2b2ca)
  [#20513](https://github.com/npm/npm/pull/20513)
  Fix nested command aliases.
  ([@mmermerkaya](https://github.com/mmermerkaya))
* [`18b2b3cf7`](https://github.com/npm/npm/commit/18b2b3cf71a438648ced1bd13faecfb50c71e979)
  `npm-lifecycle@2.0.3`:
  Make sure different versions of the `Path` env var on Windows all get
  `node_modules/.bin` prepended when running lifecycle scripts.
  ([@laggingreflex](https://github.com/laggingreflex))

### DOCUMENTATION

* [`a91d87072`](https://github.com/npm/npm/commit/a91d87072f292564e58dcab508b5a8c6702b9aae)
  [#20550](https://github.com/npm/npm/pull/20550)
  Update required node versions in README.
  ([@legodude17](https://github.com/legodude17))
* [`bf3cfa7b8`](https://github.com/npm/npm/commit/bf3cfa7b8b351714c4ec621e1a5867c8450c6fff)
  Pull in changelogs from the last `npm@5` release.
  ([@iarna](https://github.com/iarna))
* [`b2f14b14c`](https://github.com/npm/npm/commit/b2f14b14ca25203c2317ac2c47366acb50d46e69)
  [#20629](https://github.com/npm/npm/pull/20629)
  Make tone in `publishConfig` docs more neutral.
  ([@jeremyckahn](https://github.com/jeremyckahn))

### DEPENDENCY BUMPS

* [`5fca4eae8`](https://github.com/npm/npm/commit/5fca4eae8a62a7049b1ae06aa0bbffdc6e0ad6cc)
  `byte-size@4.0.3`
  ([@75lb](https://github.com/75lb))
* [`d9ef3fba7`](https://github.com/npm/npm/commit/d9ef3fba79f87c470889a6921a91f7cdcafa32b9)
  `lru-cache@4.1.3`
  ([@isaacs](https://github.com/isaacs))
* [`f1baf011a`](https://github.com/npm/npm/commit/f1baf011a0d164f8dc8aa6cd31e89225e3872e3b)
  `request@2.86.0`
  ([@simonv](https://github.com/simonv))
* [`005fa5420`](https://github.com/npm/npm/commit/005fa542072f09a83f77a9d62c5e53b8f6309371)
  `require-inject@1.4.3`
  ([@iarna](https://github.com/iarna))
* [`1becdf09a`](https://github.com/npm/npm/commit/1becdf09a2f19716726c88e9a2342e1e056cfc71)
  `tap@11.1.5`
  ([@isaacs](https://github.com/isaacs))

## v6.0.1 (2018-05-09):

### AUDIT SHOULDN'T WAIT FOREVER

This will likely be reduced further with the goal that the audit process
shouldn't noticibly slow down your builds regardless of your network
situation.

* [`3dcc240db`](https://github.com/npm/npm/commit/3dcc240dba5258532990534f1bd8a25d1698b0bf)
  Timeout audit requests eventually.
  ([@iarna](https://github.com/iarna))

### Looking forward

We're still a way from having node@11, so now's a good time to ensure we
don't warn about being used with it.

* [`ed1aebf55`](https://github.com/npm/npm/commit/ed1aebf55)
  Allow node@11, when it comes.
  ([@iarna](https://github.com/iarna))

## v6.0.1-next.0 (2018-05-03):

### CTRL-C OUT DURING PACKAGE EXTRACTION AS MUCH AS YOU WANT!

* [`b267bbbb9`](https://github.com/npm/npm/commit/b267bbbb9ddd551e3dbd162cc2597be041b9382c)
  [npm/lockfile#29](https://github.com/npm/lockfile/pull/29)
  `lockfile@1.0.4`:
  Switches to `signal-exit` to detect abnormal exits and remove locks.
  ([@Redsandro](https://github.com/Redsandro))

### SHRONKWRAPS AND LACKFILES

If a published modules had legacy `npm-shrinkwrap.json` we were saving
ordinary registry dependencies (`name@version`) to your `package-lock.json`
as `https://` URLs instead of versions.

* [`89102c0d9`](https://github.com/npm/npm/commit/89102c0d995c3d707ff2b56995a97a1610f8b532)
  When saving the lock-file compute how the dependency is being required instead of using
  `_resolved` in the `package.json`.  This fixes the bug that was converting
  registry dependencies into `https://` dependencies.
  ([@iarna](https://github.com/iarna))
* [`676f1239a`](https://github.com/npm/npm/commit/676f1239ab337ff967741895dbe3a6b6349467b6)
  When encountering a `https://` URL in our lockfiles that point at our default registry, extract
  the version and use them as registry dependencies.  This lets us heal
  `package-lock.json` files produced by 6.0.0
  ([@iarna](https://github.com/iarna))

### AUDIT AUDIT EVERYWHERE

You can't use it _quite_ yet, but we do have a few last moment patches to `npm audit` to make
it even better when it is turned on!

* [`b2e4f48f5`](https://github.com/npm/npm/commit/b2e4f48f5c07b8ebc94a46ce01a810dd5d6cd20c)
  Make sure we hide stream errors on background audit submissions. Previously some classes
  of error could end up being displayed (harmlessly) during installs.
  ([@iarna](https://github.com/iarna))
* [`1fe0c7fea`](https://github.com/npm/npm/commit/1fe0c7fea226e592c96b8ab22fd9435e200420e9)
  Include session and scope in requests (as we do in other requests to the registry).
  ([@iarna](https://github.com/iarna))
* [`d04656461`](https://github.com/npm/npm/commit/d046564614639c37e7984fff127c79a8ddcc0c92)
  Exit with non-zero status when vulnerabilities are found. So you can have `npm audit` as a test or prepublish step!
  ([@iarna](https://github.com/iarna))
* [`fcdbcbacc`](https://github.com/npm/npm/commit/fcdbcbacc16d96a8696dde4b6d7c1cba77828337)
  Verify lockfile integrity before running. You'd get an error either way, but this way it's
  faster and can give you more concrete instructions on how to fix it.
  ([@iarna](https://github.com/iarna))
* [`2ac8edd42`](https://github.com/npm/npm/commit/2ac8edd4248f2393b35896f0300b530e7666bb0e)
  Refuse to run in global mode. Audits require a lockfile and globals don't have one. Yet.
  ([@iarna](https://github.com/iarna))

### DOCUMENTATION IMPROVEMENTS

* [`b7fca1084`](https://github.com/npm/npm/commit/b7fca1084b0be6f8b87ec0807c6daf91dbc3060a)
  [#20407](https://github.com/npm/npm/pull/20407)
  Update the lock-file spec doc to mention that we now generate the from field for `git`-type dependencies.
  ([@watilde](https://github.com/watilde))
* [`7a6555e61`](https://github.com/npm/npm/commit/7a6555e618e4b8459609b7847a9e17de2d4fa36e)
  [#20408](https://github.com/npm/npm/pull/20408)
  Describe what the colors in outdated mean.
  ([@teameh](https://github.com/teameh))

### DEPENDENCY UPDATES

* [`5e56b3209`](https://github.com/npm/npm/commit/5e56b3209c4719e3c4d7f0d9346dfca3881a5d34)
  `npm-audit-report@1.0.8`
  ([@evilpacket](https://github.com/evilpacket))
* [`58a0b31b4`](https://github.com/npm/npm/commit/58a0b31b43245692b4de0f1e798fcaf71f8b7c31)
  `lock-verify@2.0.2`
  ([@iarna](https://github.com/iarna))
* [`e7a8c364f`](https://github.com/npm/npm/commit/e7a8c364f3146ffb94357d8dd7f643e5563e2f2b)
  [zkat/pacote#148](https://github.com/zkat/pacote/pull/148)
  `pacote@8.1.1`
  ([@redonkulus](https://github.com/redonkulus))
* [`46c0090a5`](https://github.com/npm/npm/commit/46c0090a517526dfec9b1b6483ff640227f0cd10)
  `tar@4.4.2`
  ([@isaacs](https://github.com/isaacs))
* [`8a16db3e3`](https://github.com/npm/npm/commit/8a16db3e39715301fd085a8f4c80ae836f0ec714)
  `update-notifier@2.5.0`
  ([@alexccl](https://github.com/alexccl))
* [`696375903`](https://github.com/npm/npm/commit/6963759032fe955c1404d362e14f458d633c9444)
  `safe-buffer@5.1.2`
  ([@feross](https://github.com/feross))
* [`c949eb26a`](https://github.com/npm/npm/commit/c949eb26ab6c0f307e75a546f342bb2ec0403dcf)
  `query-string@6.1.0`
  ([@sindresorhus](https://github.com/sindresorhus))

## v6.0.0 (2018-04-20):

Hey y'all! Here's another `npm@6` release -- with `node@10` around the corner,
this might well be the last prerelease before we tag `6.0.0`! There's two major
features included with this release, along with a few miscellaneous fixes and
changes.

### EXTENDED `npm init` SCAFFOLDING

Thanks to the wonderful efforts of [@jdalton](https://github.com/jdalton) of
lodash fame, `npm init` can now be used to invoke custom scaffolding tools!

You can now do things like `npm init react-app` or `npm init esm` to scaffold an
npm package by running `create-react-app` and `create-esm`, respectively. This
also adds an `npm create` alias, to correspond to Yarn's `yarn create` feature,
which inspired this.

* [`008a83642`](https://github.com/npm/npm/commit/008a83642e04360e461f56da74b5557d5248a726) [`ed81d1426`](https://github.com/npm/npm/commit/ed81d1426776bcac47492cabef43f65e1d4ab536) [`833046e45`](https://github.com/npm/npm/commit/833046e45fe25f75daffd55caf25599a9f98c148)
  [#20303](https://github.com/npm/npm/pull/20303)
  Add an `npm init` feature that calls out to `npx` when invoked with positional
  arguments.  ([@jdalton](https://github.com/jdalton))

### DEPENDENCY AUDITING

This version of npm adds a new command, `npm audit`, which will run a security
audit of your project's dependency tree and notify you about any actions you may
need to take.

The registry-side services required for this command to work will be available
on the main npm registry in the coming weeks. Until then, you won't get much out
of trying to use this on the CLI.

As part of this change, the npm CLI now sends scrubbed and cryptographically
anonymized metadata about your dependency tree to your configured registry, to
allow notifying you about the existence of critical security flaws. For details
about how the CLI protects your privacy when it shares this metadata, see `npm
help audit`, or [read the docs for `npm audit`
online](https://github.com/npm/npm/blob/release-next/doc/cli/npm-audit.md). You
can disable this altogether by doing `npm config set audit false`, but will no
longer benefit from the service.

* [`f4bc648ea`](https://github.com/npm/npm/commit/f4bc648ea7b19d63cc9878c9da2cb1312f6ce152)
  [#20389](https://github.com/npm/npm/pull/20389)
  `npm-registry-fetch@1.1.0`
  ([@iarna](https://github.com/iarna))
* [`594d16987`](https://github.com/npm/npm/commit/594d16987465014d573c51a49bba6886cc19f8e8)
  [#20389](https://github.com/npm/npm/pull/20389)
  `npm-audit-report@1.0.5`
  ([@iarna](https://github.com/iarna))
* [`8c77dde74`](https://github.com/npm/npm/commit/8c77dde74a9d8f9007667cd1732c3329e0d52617) [`1d8ac2492`](https://github.com/npm/npm/commit/1d8ac2492196c4752b2e41b23d5ddc92780aaa24) [`552ff6d64`](https://github.com/npm/npm/commit/552ff6d64a5e3bcecb33b2a861c49a3396adad6d) [`09c734803`](https://github.com/npm/npm/commit/09c73480329e75e44fb8e55ca522f798be68d448)
  [#20389](https://github.com/npm/npm/pull/20389)
  Add new `npm audit` command.
  ([@iarna](https://github.com/iarna))
* [`be393a290`](https://github.com/npm/npm/commit/be393a290a5207dc75d3d70a32973afb3322306c)
  [#20389](https://github.com/npm/npm/pull/20389)
  Temporarily suppress git metadata till there's an opt-in.
  ([@iarna](https://github.com/iarna))
* [`8e713344f`](https://github.com/npm/npm/commit/8e713344f6e0828ddfb7733df20d75e95a5382d8)
  [#20389](https://github.com/npm/npm/pull/20389)
  Document the new command.
  ([@iarna](https://github.com/iarna))
*
  [#20389](https://github.com/npm/npm/pull/20389)
  Default audit to off when running the npm test suite itself.
  ([@iarna](https://github.com/iarna))

### MORE `package-lock.json` FORMAT CHANGES?!

* [`820f74ae2`](https://github.com/npm/npm/commit/820f74ae22b7feb875232d46901cc34e9ba995d6)
  [#20384](https://github.com/npm/npm/pull/20384)
  Add `from` field back into package-lock for git dependencies. This will give
  npm the information it needs to figure out whether git deps are valid,
  specially when running with legacy install metadata or in
  `--package-lock-only` mode when there's no `node_modules`. This should help
  remove a significant amount of git-related churn on the lock-file.
  ([@zkat](https://github.com/zkat))

### BUGFIXES

* [`9d5d0a18a`](https://github.com/npm/npm/commit/9d5d0a18a5458655275056156b5aa001140ae4d7)
  [#20358](https://github.com/npm/npm/pull/20358)
  `npm install-test` (aka `npm it`) will no longer generate `package-lock.json`
  when running with `--no-package-lock` or `package-lock=false`.
  ([@raymondfeng](https://github.com/raymondfeng))
* [`e4ed976e2`](https://github.com/npm/npm/commit/e4ed976e20b7d1114c920a9dc9faf351f89a31c9)
  [`2facb35fb`](https://github.com/npm/npm/commit/2facb35fbfbbc415e693d350b67413a66ff96204)
  [`9c1eb945b`](https://github.com/npm/npm/commit/9c1eb945be566e24cbbbf186b0437bdec4be53fc)
  [#20390](https://github.com/npm/npm/pull/20390)
  Fix a scenario where a git dependency had a comittish associated with it
  that was not a complete commitid.  `npm` would never consider that entry
  in the `package.json` as matching the entry in the `package-lock.json` and
  this resulted in inappropriate pruning or reinstallation of git
  dependencies.  This has been addressed in two ways, first, the addition of the
  `from` field as described in [#20384](https://github.com/npm/npm/pull/20384) means
  we can exactly match the `package.json`. Second, when that's missing (when working with
  older `package-lock.json` files), we assume that the match is ok.  (If
  it's not, we'll fix it up when a real installation is done.)
  ([@iarna](https://github.com/iarna))


### DEPENDENCIES

* [`1c1f89b73`](https://github.com/npm/npm/commit/1c1f89b7319b2eef6adee2530c4619ac1c0d83cf)
  `libnpx@10.2.0`
  ([@zkat](https://github.com/zkat))
* [`242d8a647`](https://github.com/npm/npm/commit/242d8a6478b725778c00be8ba3dc85f367006a61)
  `pacote@8.1.0`
  ([@zkat](https://github.com/zkat))

### DOCS

* [`a1c77d614`](https://github.com/npm/npm/commit/a1c77d614adb4fe6769631b646b817fd490d239c)
  [#20331](https://github.com/npm/npm/pull/20331)
  Fix broken link to 'private-modules' page. The redirect went away when the new
  npm website went up, but the new URL is better anyway.
  ([@vipranarayan14](https://github.com/vipranarayan14))
* [`ad7a5962d`](https://github.com/npm/npm/commit/ad7a5962d758efcbcfbd9fda9a3d8b38ddbf89a1)
  [#20279](https://github.com/npm/npm/pull/20279)
  Document the `--if-present` option for `npm run-script`.
  ([@aleclarson](https://github.com/aleclarson))

## v6.0.0-next.1 (2018-04-12):

### NEW FEATURES

* [`a9e722118`](https://github.com/npm/npm/commit/a9e7221181dc88e14820d0677acccf0648ac3c5a)
  [#20256](https://github.com/npm/npm/pull/20256)
  Add support for managing npm webhooks.  This brings over functionality
  previously provided by the [`wombat`](https://www.npmjs.com/package/wombat) CLI.
  ([@zkat](https://github.com/zkat))
* [`8a1a64203`](https://github.com/npm/npm/commit/8a1a64203cca3f30999ea9e160eb63662478dcee)
  [#20126](https://github.com/npm/npm/pull/20126)
  Add `npm cit` command that's equivalent of `npm ci && npm t` that's equivalent of `npm it`.
  ([@SimenB](https://github.com/SimenB))
* [`fe867aaf1`](https://github.com/npm/npm/commit/fe867aaf19e924322fe58ed0cf0a570297a96559)
  [`49d18b4d8`](https://github.com/npm/npm/commit/49d18b4d87d8050024f8c5d7a0f61fc2514917b1)
  [`ff6b31f77`](https://github.com/npm/npm/commit/ff6b31f775f532bb8748e8ef85911ffb35a8c646)
  [`78eab3cda`](https://github.com/npm/npm/commit/78eab3cdab6876728798f876d569badfc74ce68f)
  The `requires` field in your lock-file will be upgraded to use ranges from
  versions on your first use of npm.
  ([@iarna](https://github.com/iarna))
* [`cf4d7b4de`](https://github.com/npm/npm/commit/cf4d7b4de6fa241a656e58f662af0f8d7cd57d21)
  [#20257](https://github.com/npm/npm/pull/20257)
  Add shasum and integrity to the new `npm view` output.
  ([@zkat](https://github.com/zkat))

### BUG FIXES

* [`685764308`](https://github.com/npm/npm/commit/685764308e05ff0ddb9943b22ca77b3a56d5c026)
  Fix a bug where OTPs passed in via the commandline would have leading
  zeros deleted resulted in authentication failures.
  ([@iarna](https://github.com/iarna))
* [`8f3faa323`](https://github.com/npm/npm/commit/8f3faa3234b2d2fcd2cb05712a80c3e4133c8f45)
  [`6800f76ff`](https://github.com/npm/npm/commit/6800f76ffcd674742ba8944f11f6b0aa55f4b612)
  [`ec90c06c7`](https://github.com/npm/npm/commit/ec90c06c78134eb2618612ac72288054825ea941)
  [`825b5d2c6`](https://github.com/npm/npm/commit/825b5d2c60e620da5459d9dc13d4f911294a7ec2)
  [`4785f13fb`](https://github.com/npm/npm/commit/4785f13fb69f33a8c624ecc8a2be5c5d0d7c94fc)
  [`bd16485f5`](https://github.com/npm/npm/commit/bd16485f5b3087625e13773f7251d66547d6807d)
  Restore the ability to bundle dependencies that are uninstallable from the
  registry.  This also eliminates needless registry lookups for bundled
  dependencies.

  Fixed a bug where attempting to install a dependency that is bundled
  inside another module without reinstalling that module would result in
  ENOENT errors.
  ([@iarna](https://github.com/iarna))
* [`429498a8c`](https://github.com/npm/npm/commit/429498a8c8d4414bf242be6a3f3a08f9a2adcdf9)
  [#20029](https://github.com/npm/npm/pull/20029)
  Allow packages with non-registry specifiers to follow the fast path that
  the we use with the lock-file for registry specifiers. This will improve install time
  especially when operating only on the package-lock (`--package-lock-only`).
  ([@zkat](https://github.com/zkat))

  Fix the a bug where `npm i --only=prod` could remove development
  dependencies from lock-file.
  ([@iarna](https://github.com/iarna))
* [`834b46ff4`](https://github.com/npm/npm/commit/834b46ff48ade4ab4e557566c10e83199d8778c6)
  [#20122](https://github.com/npm/npm/pull/20122)
  Improve the update-notifier messaging (borrowing ideas from pnpm) and
  eliminate false positives.
  ([@zkat](https://github.com/zkat))
* [`f9de7ef3a`](https://github.com/npm/npm/commit/f9de7ef3a1089ceb2610cd27bbd4b4bc2979c4de)
  [#20154](https://github.com/npm/npm/pull/20154)
  Let version succeed when `package-lock.json` is gitignored.
  ([@nwoltman](https://github.com/nwoltman))
* [`f8ec52073`](https://github.com/npm/npm/commit/f8ec520732bda687bc58d9da0873dadb2d65ca96)
  [#20212](https://github.com/npm/npm/pull/20212)
  Ensure that we only create an `etc` directory if we are actually going to write files to it.
  ([@buddydvd](https://github.com/buddydvd))
* [`ab489b753`](https://github.com/npm/npm/commit/ab489b75362348f412c002cf795a31dea6420ef0)
  [#20140](https://github.com/npm/npm/pull/20140)
  Note in documentation that `package-lock.json` version gets touched by `npm version`.
  ([@srl295](https://github.com/srl295))
* [`857c2138d`](https://github.com/npm/npm/commit/857c2138dae768ea9798782baa916b1840ab13e8)
  [#20032](https://github.com/npm/npm/pull/20032)
  Fix bug where unauthenticated errors would get reported as both 404s and
  401s, i.e. `npm ERR!  404 Registry returned 401`.  In these cases the error
  message will now be much more informative.
  ([@iarna](https://github.com/iarna))
* [`d2d290bca`](https://github.com/npm/npm/commit/d2d290bcaa85e44a4b08cc40cb4791dd4f81dfc4)
  [#20082](https://github.com/npm/npm/pull/20082)
  Allow optional @ prefix on scope with `npm team` commands for parity with other commands.
  ([@bcoe](https://github.com/bcoe))
* [`b5babf0a9`](https://github.com/npm/npm/commit/b5babf0a9aa1e47fad8a07cc83245bd510842047)
  [#19580](https://github.com/npm/npm/pull/19580)
  Improve messaging when two-factor authentication is required while publishing.
  ([@jdeniau](https://github.com/jdeniau))
* [`471ee1c5b`](https://github.com/npm/npm/commit/471ee1c5b58631fe2e936e32480f3f5ed6438536)
  [`0da38b7b4`](https://github.com/npm/npm/commit/0da38b7b4aff0464c60ad12e0253fd389efd5086)
  Fix a bug where optional status of a dependency was not being saved to
  the package-lock on the initial install.
  ([@iarna](https://github.com/iarna))
* [`b3f98d8ba`](https://github.com/npm/npm/commit/b3f98d8ba242a7238f0f9a90ceea840b7b7070af)
  [`9dea95e31`](https://github.com/npm/npm/commit/9dea95e319169647bea967e732ae4c8212608f53)
  Ensure that `--no-optional` does not remove optional dependencies from the lock-file.
  ([@iarna](https://github.com/iarna))

### MISCELLANEOUS

* [`ec6b12099`](https://github.com/npm/npm/commit/ec6b120995c9c1d17ff84bf0217ba5741365af2d)
  Exclude all tests from the published version of npm itself.
  ([@iarna](https://github.com/iarna))

### DEPENDENCY UPDATES

* [`73dc97455`](https://github.com/npm/npm/commit/73dc974555217207fb384e39d049da19be2f79ba)
  [zkat/cipm#46](https://github.com/zkat/cipm/pull/46)
  `libcipm@1.6.2`:
  Detect binding.gyp for default install lifecycle. Let's `npm ci` work on projects that
  have their own C code.
  ([@caleblloyd](https://github.com/caleblloyd))
* [`77c3f7a00`](https://github.com/npm/npm/commit/77c3f7a0091f689661f61182cd361465e2d695d5)
  `iferr@1.0.0`
* [`dce733e37`](https://github.com/npm/npm/commit/dce733e37687c21cb1a658f06197c609ac39c793)
  [zkat/json-parse-better-errors#1](https://github.com/zkat/json-parse-better-errors/pull/1)
  `json-parse-better-errors@1.0.2`
  ([@Hoishin](https://github.com/Hoishin))
* [`c52765ff3`](https://github.com/npm/npm/commit/c52765ff32d195842133baf146d647760eb8d0cd)
  `readable-stream@2.3.6`
  ([@mcollina](https://github.com/mcollina))
* [`e160adf9f`](https://github.com/npm/npm/commit/e160adf9fce09f226f66e0892cc3fa45f254b5e8)
  `update-notifier@2.4.0`
  ([@sindersorhus](https://github.com/sindersorhus))
* [`9a9d7809e`](https://github.com/npm/npm/commit/9a9d7809e30d1add21b760804be4a829e3c7e39e)
  `marked@0.3.1`
  ([@joshbruce](https://github.com/joshbruce))
* [`f2fbd8577`](https://github.com/npm/npm/commit/f2fbd857797cf5c12a68a6fb0ff0609d373198b3)
  [#20256](https://github.com/npm/npm/pull/20256)
  `figgy-pudding@2.0.1`
  ([@zkat](https://github.com/zkat))
* [`44972d53d`](https://github.com/npm/npm/commit/44972d53df2e0f0cc22d527ac88045066205dbbf)
  [#20256](https://github.com/npm/npm/pull/20256)
  `libnpmhook@3.0.0`
  ([@zkat](https://github.com/zkat))
* [`cfe562c58`](https://github.com/npm/npm/commit/cfe562c5803db08a8d88957828a2cd1cc51a8dd5)
  [#20276](https://github.com/npm/npm/pull/20276)
  `node-gyp@3.6.2`
* [`3c0bbcb8e`](https://github.com/npm/npm/commit/3c0bbcb8e5440a3b90fabcce85d7a1d31e2ecbe7)
  [zkat/npx#172](https://github.com/zkat/npx/pull/172)
  `libnpx@10.1.1`
  ([@jdalton](https://github.com/jdalton))
* [`0573d91e5`](https://github.com/npm/npm/commit/0573d91e57c068635a3ad4187b9792afd7b5e22f)
  [zkat/cacache#128](https://github.com/zkat/cacache/pull/128)
  `cacache@11.0.1`
  ([@zkat](https://github.com/zkat))
* [`396afa99f`](https://github.com/npm/npm/commit/396afa99f61561424866d5c8dd7aedd6f91d611a)
  `figgy-pudding@3.1.0`
  ([@zkat](https://github.com/zkat))
* [`e7f869c36`](https://github.com/npm/npm/commit/e7f869c36ec1dacb630e5ab749eb3bb466193f01)
  `pacote@8.0.0`
  ([@zkat](https://github.com/zkat))
* [`77dac72df`](https://github.com/npm/npm/commit/77dac72dfdb6add66ec859a949b1d2d788a379b7)
  `ssri@6.0.0`
  ([@zkat](https://github.com/zkat))
* [`0b802f2a0`](https://github.com/npm/npm/commit/0b802f2a0bfa15c6af8074ebf9347f07bccdbcc7)
  `retry@0.12.0`
  ([@iarna](https://github.com/iarna))
* [`4781b64bc`](https://github.com/npm/npm/commit/4781b64bcc47d4e7fb7025fd6517cde044f6b5e1)
  `libnpmhook@4.0.1`
  ([@zkat](https://github.com/zkat))
* [`7bdbaeea6`](https://github.com/npm/npm/commit/7bdbaeea61853280f00c8443a3b2d6e6b893ada9)
  `npm-package-arg@6.1.0`
  ([@zkat](https://github.com/zkat))
* [`5f2bf4222`](https://github.com/npm/npm/commit/5f2bf4222004117eb38c44ace961bd15a779fd66)
  `read-package-tree@5.2.1`
  ([@zkat](https://github.com/zkat))

## v6.0.0-0 (2018-03-23):

Sometimes major releases are a big splash, sometimes they're something
smaller.  This is the latter kind.  That said, we expect to keep this in
release candidate status until Node 10 ships at the end of April.  There
will likely be a few more features for the 6.0.0 release line between now
and then.  We do expect to have a bigger one later this year though, so keep
an eye out for `npm@7`!

### *BREAKING* AVOID DEPRECATED

When selecting versions to install, we now avoid deprecated versions if
possible. For example:

```
Module: example
Versions:
1.0.0
1.1.0
1.1.2
1.1.3 (deprecated)
1.2.0 (latest)
```

If you ask `npm` to install `example@~1.1.0`, `npm` will now give you `1.1.2`.

By contrast, if you installed `example@~1.1.3` then you'd get `1.1.3`, as
it's the only version that can match the range.

* [`78bebc0ce`](https://github.com/npm/npm/commit/78bebc0cedc4ce75c974c47b61791e6ca1ccfd7e)
  [#20151](https://github.com/npm/npm/pull/20151)
  Skip deprecated versions when possible.
  ([@zkat](https://github.com/zkat))

### *BREAKING* UPDATE AND OUTDATED

When `npm install` is finding a version to install, it first checks to see
if the specifier you requested matches the `latest` tag.  If it doesn't,
then it looks for the highest version that does.  This means you can do
release candidates on tags other than `latest` and users won't see them
unless they ask for them.  Promoting them is as easy as setting the `latest`
tag to point at them.

Historically `npm update` and `npm outdated` worked differently.  They just
looked for the most recent thing that matched the semver range, disregarding
the `latest` tag. We're changing it to match `npm install`'s behavior.

* [`3aaa6ef42`](https://github.com/npm/npm/commit/3aaa6ef427b7a34ebc49cd656e188b5befc22bae)
  Make update and outdated respect latest interaction with semver as install does.
  ([@iarna](https://github.com/iarna))
* [`e5fbbd2c9`](https://github.com/npm/npm/commit/e5fbbd2c999ab9c7ec15b30d8b4eb596d614c715)
  `npm-pick-manifest@2.1.0`
  ([@iarna](https://github.com/iarna))

### PLUS ONE SMALLER PATCH

Technically this is a bug fix, but the change in behavior is enough of an
edge case that I held off on bringing it in until a major version.

When we extract a binary and it starts with a shebang (or "hash bang"), that
is, something like:

```
#!/usr/bin/env node
```

If the file has Windows line endings we strip them off of the first line.
The reason for this is that shebangs are only used in Unix-like environments
and the files with them can't be run if the shebang has a Windows line ending.

Previously we converted ALL line endings from Windows to Unix.  With this
patch we only convert the line with the shebang.  (Node.js works just fine
with either set of line endings.)

* [`814658371`](https://github.com/npm/npm/commit/814658371bc7b820b23bc138e2b90499d5dda7b1)
  [`7265198eb`](https://github.com/npm/npm/commit/7265198ebb32d35937f4ff484b0167870725b054)
  `bin-links@1.1.2`:
  Only rewrite the CR after a shebang (if any) when fixing up CR/LFs.
  ([@iarna](https://github.com/iarna))

### *BREAKING* SUPPORTED NODE VERSIONS

Per our supported Node.js policy, we're dropping support for both Node 4 and
Node 7, which are no longer supported by the Node.js project.

* [`077cbe917`](https://github.com/npm/npm/commit/077cbe917930ed9a0c066e10934d540e1edb6245)
  Drop support for Node 4 and Node 7.
  ([@iarna](https://github.com/iarna))

### DEPENDENCIES

* [`478fbe2d0`](https://github.com/npm/npm/commit/478fbe2d0bce1534b1867e0b80310863cfacc01a)
  `iferr@1.0.0`
* [`b18d88178`](https://github.com/npm/npm/commit/b18d88178a4cf333afd896245a7850f2f5fb740b)
  `query-string@6.0.0`
* [`e02fa7497`](https://github.com/npm/npm/commit/e02fa7497f89623dc155debd0143aa54994ace74)
  `is-cidr@2.0.5`
* [`c8f8564be`](https://github.com/npm/npm/commit/c8f8564be6f644e202fccd9e3de01d64f346d870)
  [`311e55512`](https://github.com/npm/npm/commit/311e5551243d67bf9f0d168322378061339ecff8)
  `standard@11.0.1`
