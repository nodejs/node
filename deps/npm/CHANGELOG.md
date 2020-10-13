## v7.0.0 (2020-10-12)

### BUG FIXES

* [`7bcdb3636`](https://github.com/npm/cli/commit/7bcdb3636e29291b9c722fe03a8450859dcb5b4f)
  [#1949](https://github.com/npm/cli/pull/1949) fix: ensure `publishConfig`
  is passed through ([@nlf](https://github.com/nlf))
* [`97978462e`](https://github.com/npm/cli/commit/97978462e9050261e4ce2549e71fe94a48796577)
  fix: patch `config.js` to remove duplicate vals
  ([@darcyclarke](https://github.com/darcyclarke))

### DOCUMENTION

* [`60769d757`](https://github.com/npm/cli/commit/60769d757859c88e2cceab66975f182a47822816)
  [#1911](https://github.com/npm/cli/pull/1911) docs: v7 npm-install
  refresh ([@ruyadorno](https://github.com/ruyadorno))
* [`08de49042`](https://github.com/npm/cli/commit/08de4904255742cbf7477a20bdeebe82f283a406)
  [#1938](https://github.com/npm/cli/pull/1938) docs: v7 using npm config
  updates ([@ruyadorno](https://github.com/ruyadorno))

### DEPENDENCIES

* [`15366a1cf`](https://github.com/npm/cli/commit/15366a1cf0073327b90ac7eb977ff8a73b52cc62)
  `npm-registry-fetch@8.1.5`
* [`f04a74140`](https://github.com/npm/cli/commit/f04a74140bf65db36be3c379e0eb20dd6db3cc5c)
  `init-package-json@2.0.0`
    * [`1de21dce0`](https://github.com/npm/cli/commit/1de21dce0e56874203a789ce33124a4fc4d3b15f)
      fix: support dot-separated aliases defined in a `.npmrc` ini files
      for `init-*` configs ([@ruyadorno](https://github.com/ruyadorno))
* [`a67275cd9`](https://github.com/npm/cli/commit/a67275cd9a75fa05ee3d3265832d0a015b14e81c)
  `eslint@7.11.0`
* [`6fb83b78d`](https://github.com/npm/cli/commit/6fb83b78db09adfafd7cbd4b926e77802c4993e4)
  `hosted-git-info@3.0.6`
* [`1ca30cc9b`](https://github.com/npm/cli/commit/1ca30cc9b8e7edc2043c1f848855f19781729dc9)
  `libnpmfund@1.0.0`
* [`28a2d2ba4`](https://github.com/npm/cli/commit/28a2d2ba4a63808614f5d98685a64531e3198b93)
  `@npmcli/arborist@1.0.0`
    * [npm/rfcs#239](https://github.com/npm/rfcs/pull/239) Improve handling
      of conflicting `peerDependencies` in transitive dependencies, so that
      `--force` will always accept a best effort override, and
      `--strict-peer-deps` will fail faster on conflicts.
* [`9306c6833`](https://github.com/npm/cli/commit/9306c6833e2e77675e0cfddd569b6b54a8bcf172)
  `libnpmfund@1.0.1`
* [`fafb348ef`](https://github.com/npm/cli/commit/fafb348ef976116d47ada238beb258d5db5758a7)
  `npm-package-arg@8.1.0`
* [`365f2e756`](https://github.com/npm/cli/commit/365f2e7565d0cfde858a43d894a77fb3c6338bb7)
  `read-package-json@3.0.0`

## v7.0.0-rc.4 (2020-10-09)

* [`09b456f2d`](https://github.com/npm/cli/commit/09b456f2d776e2757956d2b9869febd1e01a1076)
  `@npmcli/config@1.2.1`
    * [#1919](https://github.com/npm/cli/pull/1919)
      exposes `npm_config_user_agent` env variable
      ([@nlf](https://github.com/nlf))
* [`e859fba9e`](https://github.com/npm/cli/commit/e859fba9e7c267b0587b7d22da72e33f3e8f906b)
  [#1936](https://github.com/npm/cli/pull/1936)
  fix npx for non-interactive shells
  ([@nlf](https://github.com/nlf))
* [`9320b8e4f`](https://github.com/npm/cli/commit/9320b8e4f0e0338ea95e970ec9bbf0704def64b8)
  [#1906](https://github.com/npm/cli/pull/1906)
  restore old npx behavior of running existing bins first
  ([@nlf](https://github.com/nlf))
* [`7bd47ca2c`](https://github.com/npm/cli/commit/7bd47ca2c718df0a7d809f1992b7a87eece3f6dc)
  `@npmcli/arborist@0.0.33`
    * fixed handling of invalid package.json file
* [`02737453b`](https://github.com/npm/cli/commit/02737453bc2363daeef8c4e4b7d239e2299029b2)
  `make-fetch-happen@8.0.10`
    * do not calculate integrity values of http errors

## v7.0.0-rc.3 (2020-10-06)

* [`d816c2efa`](https://github.com/npm/cli/commit/d816c2efae41930cbdf4fff8657e0adc450d1dd4)
  [`c8f0d5457`](https://github.com/npm/cli/commit/c8f0d5457dd913b425987ae30a611d4eb9e84b7d)
  [`d48086d0d`](https://github.com/npm/cli/commit/d48086d0d3e006e76f364fb2c62b406a97ce8f68)
  [`f34595f2e`](https://github.com/npm/cli/commit/f34595f2e5814a929049aca0349ce418a7f400c6)
  [#1902](https://github.com/npm/cli/pull/1902)
  tests for several commands
  ([@nlf](https://github.com/nlf))
* [`6d49207db`](https://github.com/npm/cli/commit/6d49207dbc5d66f91f4f462f05dd8916046e3a7b)
  [#1903](https://github.com/npm/cli/pull/1903)
  Revert "Remove unused npx binary"
  ([@MylesBorins](https://github.com/MylesBorins))
* [`138dfc202`](https://github.com/npm/cli/commit/138dfc202f401d2d93b4b5d2499799be6eb4ff0b)
  set executable permissions on bins that node installer uses
* [`b06d68078`](https://github.com/npm/cli/commit/b06d68078830cc2446b1e51553db10e87591865b)
  `@npmcli/arborist@0.0.32`
    * Do not remove `node_modules` folders from Workspaces when
      `loadActual` races with `buildIdealTree` ([@ruyadorno](https://github.com/ruyadorno))
* [`2509e3a1b`](https://github.com/npm/cli/commit/2509e3a1bf76289062f1f6f06eee184df386054b)
  `uuid@8.3.1`

## v7.0.0-rc.2 (2020-10-02)

* [`6de81a013`](https://github.com/npm/cli/commit/6de81a013833e0961abdec6f7c1ad50b63faaae6)
  `@npmcli/run-script@1.7.2`
    * Fix regression running 'install' scripts when package.json does not
    contain a scripts object

## v7.0.0-rc.1 (2020-10-02)

* [`281a7f39a`](https://github.com/npm/cli/commit/281a7f39ac314bd7657ce2bcd7918b21eee99210)
  `@npmcli/arborist@0.0.31`
    * Allow `npm update` to update bundled root dependencies
    * Only do implicit node-gyp build for gyp files named `binding.gyp`
* [`384f5ec47`](https://github.com/npm/cli/commit/384f5ec47091eed66c2a47f2c98df3ba7506ec9f)
  update minipass-fetch to fix many 'cb() never called' errors
* [`7b1e75906`](https://github.com/npm/cli/commit/7b1e75906351bd73cde2f745ccaf63b9ad7de435)
  `@npmcli/run-script@1.7.1`
    * Only do implicit node-gyp build for gyp files named `binding.gyp`
* [`c20e2f0c7`](https://github.com/npm/cli/commit/c20e2f0c7766a04f999fdc64faad29277904c2d3)
  [#1892](https://github.com/npm/cli/pull/1892)
  Support `--omit` options in npm outdated

## v7.0.0-rc.0 (2020-10-01)

* [`3b417055c`](https://github.com/npm/cli/commit/3b417055cf07c4ef8e4c5063f00d3c24b5f5cbd2)
  [#1859](https://github.com/npm/cli/pull/1859)
  fix `proxy` and `https-proxy` config support
  ([@badeggg](https://github.com/badeggg))
* [`dd7d7a284`](https://github.com/npm/cli/commit/dd7d7a284d5150d1804d0cd5a85519c86adf3bc2)
  `@npmcli/arborist@0.0.30`
    * [#1849](https://github.com/npm/cli/issues/1849) Do not drop peer/dev
      dep while saving if both set
    * Do not install or build if there is a global top bin conflict
    * Default to building node-gyp dependencies
* [`40c17e12c`](https://github.com/npm/cli/commit/40c17e12c5a734c37b88692e36221ac974c0c63d)
  `cli-table3@0.6.0`
* [`47a8ca1d7`](https://github.com/npm/cli/commit/47a8ca1d72f0f0835b45cfb2c4fb8ab1218dc14a)
  `byte-size@7.0.0`
* [`81073f99a`](https://github.com/npm/cli/commit/81073f99a93b680e3ca08b8f099e9aca2aaf50be)
  `eslint@7.10.0`
* [`67793abd4`](https://github.com/npm/cli/commit/67793abd4abdf315816b6266ddb045289f607b03)
  `eslint-plugin-import@2.22.1`
* [`a27e8d006`](https://github.com/npm/cli/commit/a27e8d00664e5d4c3e81d664253a10176adb39c8)
  `is-cidr@4.0.2`
* [`893fed45e`](https://github.com/npm/cli/commit/893fed45e2272ef764ebf927c659fcf5e7b355b3)
  `marked-man@0.7.0`
* [`bc20e0c8a`](https://github.com/npm/cli/commit/bc20e0c8ae30a202c72af88586ab9c167dff980a)
  `rimraf@3.0.2`
* [`a2b8fd3c1`](https://github.com/npm/cli/commit/a2b8fd3c153ecca55cb2d60654fff207688532ab)
  `uuid@8.3.0`
* [`ee4c85b87`](https://github.com/npm/cli/commit/ee4c85b878410143644460c3860ab706a8c925e0)
  `write-file-atomic@3.0.3`
* [`4bdad5fdf`](https://github.com/npm/cli/commit/4bdad5fdf6ef387e2159b529ba4652f0221433b5)
  `bin-links@2.2.1`
* [`c394937ec`](https://github.com/npm/cli/commit/c394937ec1911cd17ec42c8fc74773047d47322c)
  `@npmcli/run-script@1.7.0`
    * Default to building node-gyp dependencies and projects
* remove many unused dependencies
  ([@ruyadorno](https://github.com/ruyadorno))
    * [`558e9781a`](https://github.com/npm/cli/commit/558e9781ada06b66be4d2d5d0f7e763f645eda25)
      deep-equal
    * [`2aa9a1f8a`](https://github.com/npm/cli/commit/2aa9a1f8a5773b9a960b14b51c8111fb964bc9ae)
      request
    * [`d77594e52`](https://github.com/npm/cli/commit/d77594e52f2f7d65d45347f542f48e4dbb6d2f26)
      npm-registry-couchapp
    * [`8ec84d9f6`](https://github.com/npm/cli/commit/8ec84d9f691686da67bd14c2728472c94ab3b955)
      tacks
    * [`a07b421f7`](https://github.com/npm/cli/commit/a07b421f708c13d8239e7283ad89611b24b23d0a)
      lincesee
    * [`41126e165`](https://github.com/npm/cli/commit/41126e165d3d5625a55e140b84fdd02052520146)
      npm-cache-filename
    * [`130da51b5`](https://github.com/npm/cli/commit/130da51b553e550584f31e2a8a961f4338f2a0cd)
      npm-registry-mock
    * [`b355af486`](https://github.com/npm/cli/commit/b355af48696bb5001c6d2b938974d9ab9f5e2360)
      sprintf-js
    * [`721c0a873`](https://github.com/npm/cli/commit/721c0a8736f3cd0a0e75e0b89518a431553843c6)
      uid-number
    * [`9c920e5f5`](https://github.com/npm/cli/commit/9c920e5f584e4d912aabc6e412693f7142242a89)
      umask
    * [`aae1c38bb`](https://github.com/npm/cli/commit/aae1c38bbb983cf40e9b3df012b18bebba5e5400)
      config-chain
    * [`450845eac`](https://github.com/npm/cli/commit/450845eaceb7e178c8ec7867a67e5cc948986904)
      find-npm-prefix
    * [`963d542d3`](https://github.com/npm/cli/commit/963d542d385c7fe26830a885fe40d96010d01862)
      has-unicode
    * [`cad9cbc70`](https://github.com/npm/cli/commit/cad9cbc70561c8638ed6e56286f753693f411000)
      infer-owner
    * [`3ae02914d`](https://github.com/npm/cli/commit/3ae02914d49f3302d25c85d2242096bb2291f9f4)
      lockfile
    * [`7bc474d7c`](https://github.com/npm/cli/commit/7bc474d7cb2e2e083fd8358d0648d7c5fb43707f)
      once
    * [`5c5e0099a`](https://github.com/npm/cli/commit/5c5e0099a4708ec84da3d2e427e16c4a9cfe3c8a)
      retry
    * [`cfaddd334`](https://github.com/npm/cli/commit/cfaddd334b8b1eddcefa3cd2a9b823ec140271a4)
      sha
    * [`3a978ffc7`](https://github.com/npm/cli/commit/3a978ffc7fddd6802c81996a5710b2efd15edc11)
      slide

## v7.0.0-beta.13 (2020-09-29)

* [`405e051f7`](https://github.com/npm/cli/commit/405e051f724a2e79844f78f8ea9ba019fdc513aa)
  Fix EBADPLATFORM error message
  ([@#1876](https://github.com/#1876))
* [`e4d911d21`](https://github.com/npm/cli/commit/e4d911d219899c0fdc12f8951b7d70e0887909f8)
  `@npmcli/arborist@0.0.28`
    * fix: workspaces install entering an infinite loop
    * Save provided range if not a subset of savePrefix
    * package-lock.json custom indentation
    * Check engine and platform when building ideal tree
* [`90550b2e0`](https://github.com/npm/cli/commit/90550b2e023e7638134e91c80ed96828afb41539)
  [#1853](https://github.com/npm/cli/pull/1853)
  test coverage and refactor for token command
  ([@nlf](https://github.com/nlf))
* [`2715220c9`](https://github.com/npm/cli/commit/2715220c9b5d3f325e65e95bae2b5af8a485a579)
  [#1858](https://github.com/npm/cli/pull/1858)
  [#1813](https://github.com/npm/cli/issues/1813)
  do not include omitted optional dependencies in install output
  ([@ruyadorno](https://github.com/ruyadorno))
* [`e225ddcf8`](https://github.com/npm/cli/commit/e225ddcf8d74a6b1cfb24ec49e37e3f5d06e5151)
  [#1862](https://github.com/npm/cli/pull/1862)
  [#1861](https://github.com/npm/cli/issues/1861)
  respect depth when running `npm ls <pkg>`
  ([@ruyadorno](https://github.com/ruyadorno))
* [`2469ae515`](https://github.com/npm/cli/commit/2469ae5153fa4114a72684376a1b226aa07edf81)
  [#1870](https://github.com/npm/cli/pull/1870)
  [#1780](https://github.com/npm/cli/issues/1780)
  Add 'fetch-timeout' config
  ([@isaacs](https://github.com/isaacs))
* [`52114b75e`](https://github.com/npm/cli/commit/52114b75e83db8a5e08f23889cce41c89af9eb93)
  [#1871](https://github.com/npm/cli/pull/1871)
  fix `npm ls` for linked dependencies
  ([@ruyadorno](https://github.com/ruyadorno))
* [`9981211c0`](https://github.com/npm/cli/commit/9981211c070ce2b1e34d30223d12bd275adcacf5)
  [#1857](https://github.com/npm/cli/pull/1857)
  [#1703](https://github.com/npm/cli/issues/1703)
  fix `npm outdated` parsing invalid specs
  ([@ruyadorno](https://github.com/ruyadorno))

## v7.0.0-beta.12 (2020-09-22)

* [`24f3a5448`](https://github.com/npm/cli/commit/24f3a5448f021ad603046dfb9fd97ed66bd63bba)
  [#1811](https://github.com/npm/cli/issues/1811)
  npm ci should never save package.json or lockfile
  ([@isaacs](https://github.com/isaacs))
* [`5e780a5f0`](https://github.com/npm/cli/commit/5e780a5f067476c1d207173fc9249faf9eaac0c2)
  remove unused spec parameter, assign error code
  ([@nlf](https://github.com/nlf))
* [`f019a248a`](https://github.com/npm/cli/commit/f019a248a67e8c46dbe41bf31f4818c5ca2138bf)
  Remove unused npx binary
  ([@isaacs](https://github.com/isaacs))
* [`db157b3ce`](https://github.com/npm/cli/commit/db157b3ceb46327ca2089604d5f4fc9de391584e)
  `@npmcli/arborist@0.0.27`
    * Resolve race condition with conflicting bin links in local installs
    * [#1812](https://github.com/npm/cli/issues/1812) Log engine mismatches more usefully
    * [#1814](https://github.com/npm/cli/issues/1814) Do not loop trying to resolve dependencies that fail to load
    * [npm/rfcs#224](https://github.com/npm/rfcs/pull/224) Do not automatically install optional peer dependencies
    * Add the `strictPeerDeps` option, defaulting to `false`
    * fix forwarding configs to resolve pkg spec when adding new deps
* [`b3a50d275`](https://github.com/npm/cli/commit/b3a50d27501e47c61b52c3cc4de99ff4e4641efe)
  [#1846](https://github.com/npm/cli/pull/1846)
  `@npmcli/run-script@1.6.0`
    * This updates node-gyp to v7, allowing us to deduplicate a lot of significant dependencies.
* [`a1d375f6b`](https://github.com/npm/cli/commit/a1d375f6b0ee358be41110a49acc1c9fdb775fbe)
  [#1819](https://github.com/npm/cli/pull/1819)
  Add `--strict-peer-deps` option
  ([@isaacs](https://github.com/isaacs))
* [`5837a4843`](https://github.com/npm/cli/commit/5837a4843ab1f19fb62f60151f522ca0fa5449ae)
  [#1699](https://github.com/npm/cli/pull/1699)
  Use allow/deny list in docs
  ([@luciomartinez](https://github.com/luciomartinez))

## v7.0.0-beta.11 (2020-09-16)

* [`63005f4a9`](https://github.com/npm/cli/commit/63005f4a98d55786fda46f3bbb3feab044d078df)
  [#1639](https://github.com/npm/cli/issues/1639)
  npm view should not output extra newline ([@MylesBorins](https://github.com/MylesBorins))
* [`3743a42c8`](https://github.com/npm/cli/commit/3743a42c854d9ea7e333d7ff86d206a4b079a352)
  [#1750](https://github.com/npm/cli/pull/1750)
  add outdated tests ([@claudiahdz](https://github.com/claudiahdz))
* [`2019abdf1`](https://github.com/npm/cli/commit/2019abdf159eb13c9fb3a2bd2f35897a8f52b0d9)
  [#1786](https://github.com/npm/cli/pull/1786)
  add lib/link.js tests ([@ruyadorno](https://github.com/ruyadorno))
* [`2f8d11968`](https://github.com/npm/cli/commit/2f8d11968607a74c8def3c05266049bee5e313eb)
  `@npmcli/arborist@0.0.25`
    * add meta vulnerability calculator for faster audits
    * changed parsing specs to be relative to cwd
    * fix logging script execution
    * fix properly following resolved symlinks
    * fix package.json dependencies order
* [`49b2bf5a7`](https://github.com/npm/cli/commit/49b2bf5a798b49d52166744088a80b8a39ccaeb6)
  `@npmcli/config@1.1.8`
    * fix unkown envs to be passed through
    * fix setting correct globalPrefix on load
* [`f9aac351d`](https://github.com/npm/cli/commit/f9aac351dd36a19d14e1f951a2e8e20b41545822)
  `libnpmversion@1.0.5`
    * fix git ignored lockfiles

## v7.0.0-beta.10 (2020-09-08)

* [`7418970f0`](https://github.com/npm/cli/commit/7418970f03229dd2bce7973b99b981779aee6916)
  Improve output of dependency node explanations
* [`5e49bdaa3`](https://github.com/npm/cli/commit/5e49bdaa34e29dbd25c687f8e6881747a86b7435)
  [#1776](https://github.com/npm/cli/pull/1776) Add 'npm explain' command

## v7.0.0-beta.9 (2020-09-04)

* [`ef8f5676b`](https://github.com/npm/cli/commit/ef8f5676b1c90dcf44256b8ed1f61ddb6277c23a)
  [#1757](https://github.com/npm/cli/pull/1757)
  view: always fetch fullMetadata, and preferOnline
* [`ac5aa709a`](https://github.com/npm/cli/commit/ac5aa709a8609ec2beb7a8c60b3bde18f882f4e8)
  [#1758](https://github.com/npm/cli/pull/1758)
  fix scope config
* [`a36e2537f`](https://github.com/npm/cli/commit/a36e2537fd4c81df53fb6de01900beb9fa4fa0aa)
  outdated: don't throw on non-version/tag/range dep
* [`371f0f062`](https://github.com/npm/cli/commit/371f0f06215ad8caf598c20e3d0d38ff597531e9)
  `@npmcli/arborist@0.0.20`

  * Provide explanation objects for `ERESOLVE` errors
  * Support overriding certain classes of `ERESOLVE` errors with `--force`
  * Detect changes to package.json requiring package-lock dependency flag
    re-evaluation

* [`2a4e2e9ef`](https://github.com/npm/cli/commit/2a4e2e9efecb7f86147e5071c59cfc2461a5a7f5)
  [#1761](https://github.com/npm/cli/pull/1761)
  Explain `ERESOLVE` errors
* [`8e3e83bd4`](https://github.com/npm/cli/commit/8e3e83bd4f816bfed0efb8266985143ee9b94b86)
  `@npmcli/arborist@0.0.21`

    * Remove bin links on prune
    * Remove unnecessary tree walk for workspace projects
    * Install workspaces on update:true

* [`d6b134fd9`](https://github.com/npm/cli/commit/d6b134fd9005d911343831270615f80dfead7e3d)
  [#1738](https://github.com/npm/cli/pull/1738)
  [#1734](https://github.com/npm/cli/pull/1734)
  fix package spec parsing during cache add process
  ([@mjeanroy](https://github.com/mjeanroy))
* [`f105eb833`](https://github.com/npm/cli/commit/f105eb8333fa3300c5b47464b129c1b0057ed7bf)
  `npm-audit-report@2.1.4`:

    * Do not crash on cyclical meta-vulnerability references

* [`03a9f569b`](https://github.com/npm/cli/commit/03a9f569b5121a173f14711980db297d4a04ac6b)
  `opener@1.5.2`
* [`5616a23b4`](https://github.com/npm/cli/commit/5616a23b4b868d19aa100a6d86d781cc9bfd94f7)
  `@npmcli/git@2.0.4`

    * Support `.git` files, so that git worktrees are respected

## v7.0.0-beta.8 (2020-09-01)

* [`834e62a0e`](https://github.com/npm/cli/commit/834e62a0e5b76e97cfe9ea3d3188661579ebc874)
    * fix: npm ls extraneous workspaces
    * `@npmcli/arborist@0.0.19`
* [`758b02358`](https://github.com/npm/cli/commit/758b02358613591ea877e26fcdb76e5b1d40f892)
  [#1739](https://github.com/npm/cli/pull/1739)
  add full install options to npm exec
  ([@ruyadorno](https://github.com/ruyadorno))
* [`2ee7c8a98`](https://github.com/npm/cli/commit/2ee7c8a98cf01225a3c9ac247139243f868e2e03)
  `@npmcli/config@1.1.7`
  ([@ruyadorno](https://github.com/ruyadorno))

## v7.0.0-beta.7 (2020-08-25)

* [`b38f68acd`](https://github.com/npm/cli/commit/b38f68acd9292b7432c936db3b6d2d12e896f45d)
  ensure `npm-command` HTTP header is sent properly
* [`9f200abb9`](https://github.com/npm/cli/commit/9f200abb94ea2127d9a104c225159b1b7080c82c)
  Properly exit with error status code
* [`aa0152b58`](https://github.com/npm/cli/commit/aa0152b58f34f8cdae05be63853c6e0ace03236a)
  [#1719](https://github.com/npm/cli/pull/1719) Detect CI properly
* [`50f9740ca`](https://github.com/npm/cli/commit/50f9740ca8000b1c4bd3155bf1bc3d58fb6f0e20)
  [#1717](https://github.com/npm/cli/pull/1717) fund with multiple funding
  sources ([@ruyadorno](https://github.com/ruyadorno))
* [`3a63ecb6f`](https://github.com/npm/cli/commit/3a63ecb6f6a0b235660f73a3ffa329b1f131b0c3)
  [#1718](https://github.com/npm/cli/pull/1718)
  [RFC-0029](https://github.com/npm/rfcs/blob/latest/accepted/0029-add-ability-to-skip-hooks.md)
  add ability to skip pre/post hooks to `npm run-script` by using
  `--ignore-scripts` ([@ruyadorno](https://github.com/ruyadorno))

## v7.0.0-beta.6 (2020-08-21)

* [`707207bdd`](https://github.com/npm/cli/commit/707207bddb2900d6f7a57ff864cef26cda75a71a)
  add `@npmcli/config` dependency

* [`5cb9a1d4d`](https://github.com/npm/cli/commit/5cb9a1d4d985aaa8e988c51fe5ae7f7ed3602811)
  [#1688](https://github.com/npm/cli/pull/1688) use `@npmcli/config` for
  configuration ([@isaacs](https://github.com/isaacs))

* [`a4295f5db`](https://github.com/npm/cli/commit/a4295f5db7667e8cc6b83abdad168619ad31a12f)
  `npm-registry-fetch@8.1.4`:

    * Redact passwords from HTTP logs

* [`a5a6a516d`](https://github.com/npm/cli/commit/a5a6a516d16828c1375eaf41d04468d919df4a57)
  `json-parse-even-better-errors@2.3.0`:

    * Adds support for indentation/newline formatting preservation

* [`a14054558`](https://github.com/npm/cli/commit/a1405455843db1b14938596303b29fb3ad4f90f0)
  `read-package-json-fast@1.2.1`:

    * Adds support for indentation/newline formatting preservation

* [`f8603c8af`](https://github.com/npm/cli/commit/f8603c8affefc342d81c109e4676d498a8359b78)
  `libnpmversion@1.0.4`:

    * Adds support for indentation/newline formatting preservation

* [`9891fa71c`](https://github.com/npm/cli/commit/9891fa71c88f425bef8d881c3795e5823d732e1f)
  `read-package-json@2.1.2`:

    * Adds support for indentation/newline formatting preservation

* [`b44768aac`](https://github.com/npm/cli/commit/b44768aace0e9c938ebd6d05a5de1cc4368e2d7d)
  [#1662](https://github.com/npm/cli/issues/1662)
  [#1693](https://github.com/npm/cli/issues/1693)
  [#1690](https://github.com/npm/cli/issues/1690)
  `@npmcli/arborist@0.0.17`:

    * Load root project `package.json` when running loadVirtual.
    * Fetch metadata from registry when loading tree from outdated
      package-lock.json file.  This avoids a situation where a lockfile or
      shrinkwrap from npm v5 would result in deleting dependencies on
      install.
    * Preserve `package.json` and `package-lock.json` formatting in all
      places where these files are written.

* [`281da6fdc`](https://github.com/npm/cli/commit/281da6fdcda3fb3860b73ed35daa234ad228c363)
  `tar@6.0.5`

* [`1faa5b33d`](https://github.com/npm/cli/commit/1faa5b33dcc6d7e4eba1c0d85ad30cf0c237c9e1)
  [#1655](https://github.com/npm/cli/issues/1655) show usage when
  `help-search` finds no results

* [`10fcff73a`](https://github.com/npm/cli/commit/10fcff73a3381ea5e6dcb03888679ae4b501d2f0)
  [#1695](https://github.com/npm/cli/issues/1695) fix `pulseWhileDone`
  promise handling

* [`88e4241c5`](https://github.com/npm/cli/commit/88e4241c5d4f512a4e2b09d26fcdcc7f877e65ed)
  [#1698](https://github.com/npm/cli/pull/1698) add lib/logout.js unit
  tests ([@ruyadorno](https://github.com/ruyadorno))

## v7.0.0-beta.5 (2020-08-18)

* [`b718b0e28`](https://github.com/npm/cli/commit/b718b0e2844d9244cc63667f62ccf81864cc1092)
  [#1657](https://github.com/npm/cli/pull/1657) display multiple versions
  when using `--json` with `npm view`
  ([@claudiahdz](https://github.com/claudiahdz))
* [`9e7cc42f6`](https://github.com/npm/cli/commit/9e7cc42f687b479d96d222b61f76b2a30c7e6507)
  [#1071](https://github.com/npm/cli/pull/1071) migrate from `meant` to
  `leven` ([@jamesgeorge007](https://github.com/jamesgeorge007))
* [`85027f40c`](https://github.com/npm/cli/commit/85027f40ca5237bd750a5633104d12bcc248551c)
  [#1664](https://github.com/npm/cli/pull/1664) refactor and add tests for
  `npm adduser` ([@ruyadorno](https://github.com/ruyadorno))
* [`6e03e5583`](https://github.com/npm/cli/commit/6e03e55833d50fd0f5b7824ed14b7e2b14f70eaf)
  [#1672](https://github.com/npm/cli/pull/1672) refactor and add tests for
  `npm audit` ([@claudiahdz](https://github.com/claudiahdz))

## v7.0.0-beta.4 (2020-08-11)

Replace some environment variables that were excluded.  This implements the
[amendment to RFC0021](https://github.com/npm/rfcs/pull/183).

* [`631142f4a`](https://github.com/npm/cli/commit/631142f4a13959fbe02dc115fb6efa55a3368795)
  `@npmcli/run-script@1.5.0`
* [`da95386ae`](https://github.com/npm/cli/commit/da95386aedb3f0c0cc51761bfa750b64ac0eabc9)
  [#1650](https://github.com/npm/cli/pull/1650)
  [#1652](https://github.com/npm/cli/pull/1652)
  include booleans, skip already-set envs

## v7.0.0-beta.3 (2020-08-10)

Bring back support for `npm audit --production`, fix a minor `npm version`
annoyance, and track down a very serious issue where a project could be
blown away when it matches a meta-dep in the tree.

* [`5fb217701`](https://github.com/npm/cli/commit/5fb217701c060e37a3fb4a2e985f80fb015157b9)
  [#1641](https://github.com/npm/cli/issues/1641) `@npmcli/arborist@0.0.15`
* [`3598fe1f2`](https://github.com/npm/cli/commit/3598fe1f2dfe6c55221bbac8aaf21feab74a936a)
  `@npmcli/arborist@0.0.16` Add support for `npm audit --production`
* [`8ba2aeaee`](https://github.com/npm/cli/commit/8ba2aeaeeb77718cb06fe577fdd56dcdcbfe9c52)
  `libnpmversion@1.0.3`

## v7.0.0-beta.2 (2020-08-07)

New notification style for updates, and a working doctor.

* [`cf2819210`](https://github.com/npm/cli/commit/cf2819210327952696346486002239f9fc184a3e)
  [#1622](https://github.com/npm/cli/pull/1622)
  Improve abbrevs for install and help
* [`d062b2c02`](https://github.com/npm/cli/commit/d062b2c02a4d6d5f1a274aa8eb9c5969ca6253db)
  new npm-specific update-notifier implementation
* [`f6d468a3b`](https://github.com/npm/cli/commit/f6d468a3b4bef0b3cc134065d776969869fca51e)
  update doctor command
* [`b8b4d77af`](https://github.com/npm/cli/commit/b8b4d77af836f8c49832dda29a0de1b3c2d39233)
  [#1638](https://github.com/npm/cli/pull/1638)
  Direct users to our GitHub issues instead of npm.community

## v7.0.0-beta.1 (2020-08-05)

Fix some issues found in the beta pubish process, and initial attempts to
use npm v7 with [citgm](https://github.com/nodejs/citgm/).

* [`2c305e8b7`](https://github.com/npm/cli/commit/2c305e8b7bfa28b07812df74f46983fad2cb85b6)
  output generated tarball filename
* [`0808328c9`](https://github.com/npm/cli/commit/0808328c93d9cd975837eeb53202ce3844e1cf70)
  pack: set correct filename for scoped packages
  ([@isaacs](https://github.com/isaacs))
* [`cf27df035`](https://github.com/npm/cli/commit/cf27df035cfba4f859d14859229bb90841b8fda6)
  `@npmcli/arborist@0.0.14` ([@isaacs](https://github.com/isaacs))

## v7.0.0-beta.0 (2020-08-04)

Major refactoring and overhaul of, well, pretty much everything.  Almost
all dependencies have been updated, many have been removed, and the entire
`Installer` class is moved into
[`@npmcli/arborist`](http://npm.im/@npmcli/arborist).

### Some High-level Changes and Improvements

- You can install GitHub pull requests by adding `#pull/<number>` to the
  git url.  So it'd be something like `npm install
  github:user/project#pull/123` to install PR number 123 of the
  `user/project` git repo.  You can of course also use this in
  dependencies, or anywhere else dependency specifiers are found.
- Initial Workspaces support is added.  If you `npm install` in a project
  with a `workspaces` declaration, npm will install all your sub-projects'
  dependencies as well, and link everything up proper.
- `npm exec` is added, to run any arbitrary command as if it was an npm
  script.  This is sort of like `npx`, which is also ported to use `npm
  exec` under the hood.
- `npm audit` output is tightened up, and prettified.  Audit can also now
  fix a few more classes of problems, sends far less data over the wire,
  and doesn't place blame on the wrong maintainers.  (Technically this is a
  breaking change if you depend on the specific audit output, but it's
  also a big improvement!)
- `npm install` got faster.  Like a lot faster.  "So fast you'll think it's
  broken" faster.  `npm ls` got even fasterer.  A lot of stuff sped up, is
  what we're saying.
- Support has been dropped for Node.js versions less than v10.

### On the "Breaking" in "Breaking Changes"

The Semantic Versioning specification precisely defines what constitutes a
"breaking" change.  In a nutshell, it's any change that causes a you to
change _your_ code in order to start using _our_ code.  We hasten to point
this out, because a "breaking change" does not mean that something about
the update is "broken", necessarily.

We're sure that some things likely _are_ broken in this beta, because beta
software, and a healthy pessimism about things.  But nothing is "broken" on
purpose here, and if you find a bug, we'd love for you to [let us
know](https://github.com/npm/cli/issues).

### Known Issues, and What's Missing From This Beta (Why Not GA?)

It's beta software!

#### Tests

We have not yet gotten to 100% test coverage of the npm CLI codebase.  As
such, there are almost certainly bugs lying in wait.  We _do_ have 100%
test coverage of most of the commands, and all recently-updated
dependencies in the npm stack, so it's certainly more well-tested than any
version of npm before.

#### Docs

The documentation is incorrect and out of date in most places.  Prior to a
GA release, we'll be going through all of our documentation with a
fine-toothed comb to minimize the lies that it tells.

#### Error Messaging

There are a few cases where this release will just say something failed,
and not give you as much help as we'd like.  We know, and we'll fix that
prior to the GA 7.0.0 release.

In particular, if you install a project that has conflicting
`peerDependencies` in the tree, it'll just say "Unable to resolve package
tree".  Prior to GA release, it'll tell you how to fix it.  (For the time
being, just run it again with `--legacy-peer-deps`, and that'll make it
operate like npm v6.)

#### Audit Issue

There is a known performance issue in some cases that we've identified
where `npm audit` can spin wildly out of control like a dancer gripped by a
fever, heating up your laptop with fires of passion and CPU work.  This
happens when a vulnerability is in a tree with a _lot_ of cross-linked
dependencies that all depend on one another.

We have a fix for it, but if you run into this issue, you can run with
`--no-audit` to tell npm to chill out a little bit.

That's about it!  It's ready to use, and you should try it out.

Now on to the list of **BREAKING CHANGES**!

### Programmatic Usage

- [RFC
  20](https://github.com/npm/rfcs/blob/latest/accepted/0020-npm-option-handling.md)
  The CLI and its dependencies no longer use the `figgy-pudding` library
  for configs.  Configuration is done using a flat plain old JavaScript
  object.
- The `lib/fetch-package-metadata.js` module is removed.  Use
  [`pacote`](http://npm.im/pacote) to fetch package metadata.
- [`@npmcli/arborist`](http://npm.im/@npmcli/arborist) should be used to do
  most things programmatically involving dependency trees.
- The `onload-script` option is no longer supported.
- The `log-stream` option is no longer supported.
- `npm.load()` MUST be called with two arguments (the parsed cli options
  and a callback).
- `npm.root` alias for `npm.dir` removed.
- The `package.json` in npm now defines an `exports` field, making it no
  longer possible to `require()` npm's internal modules.  (This was always
  a bad idea, but now it won't work.)

### All Registry Interactions

The following affect all commands that contact the npm registry.

- `referer` header no longer sent
- `npm-command` header added

### All Lifecycle Scripts

The environment for lifecycle scripts (eg, build scripts, `npm test`, etc.)
has changed.

- [RFC
  21](https://github.com/npm/rfcs/blob/latest/accepted/0021-reduce-lifecycle-script-environment.md)
  Environment no longer includes `npm_package_*` fields, or `npm_config_*`
  fields for default configs.  `npm_package_json`, `npm_package_integrity`,
  `npm_package_resolved`, and `npm_command` environment variables added.

    (NB: this [will change a bit prior to a `v7.0.0` GA
    release](https://github.com/npm/rfcs/pull/183))

- [RFC
  22](https://github.com/npm/rfcs/blob/latest/accepted/0022-quieter-install-scripts.md)
  Scripts run during the normal course of installation are silenced unless
  they exit in error (ie, with a signal or non-zero exit status code), and
  are for a non-optional dependency.

- [RFC
  24](https://github.com/npm/rfcs/blob/latest/accepted/0024-npm-run-traverse-directory-tree.md)
  `PATH` environment variable includes all `node_modules/.bin` folders,
  even if found outside of an existing `node_modules` folder hierarchy.

- The `user`, `group`, `uid`, `gid`, and `unsafe-perms` configurations are
  no longer relevant.  When npm is run as root, scripts are always run with
  the effective `uid` and `gid` of the working directory owner.

- Commands that just run a single script (`npm test`, `npm start`, `npm
  stop`, and `npm restart`) will now run their script even if
  `--ignore-scripts` is set.  Prior to the GA v7.0.0 release, [they will
  _not_ run the pre/post scripts](https://github.com/npm/rfcs/pull/185),
  however.  (So, it'll be possible to run `npm test --ignore-scripts` to
  run your test but not your linter, for example.)

### npx

The `npx` binary was rewritten in npm v7, and the standalone `npx` package
deprecated when v7.0.0 hits GA.  `npx` uses the new `npm exec` command
instead of a separate argument parser and install process, with some
affordances to maintain backwards compatibility with the arguments it
accepted in previous versions.

This resulted in some shifts in its functionality:

- Any `npm` config value may be provided.
- To prevent security and user-experience problems from mistyping package
  names, `npx` prompts before installing anything.  Suppress this
  prompt with the `-y` or `--yes` option.
- The `--no-install` option is deprecated, and will be converted to `--no`.
- Shell fallback functionality is removed, as it is not advisable.
- The `-p` argument is a shorthand for `--parseable` in npm, but shorthand
  for `--package` in npx.  This is maintained, but only for the `npx`
  executable.  (Ie, running `npm exec -p foo` will be different from
  running `npx -p foo`.)
- The `--ignore-existing` option is removed.  Locally installed bins are
  always present in the executed process `PATH`.
- The `--npm` option is removed.  `npx` will always use the `npm` it ships
  with.
- The `--node-arg` and `-n` options are removed.
- The `--always-spawn` option is redundant, and thus removed.
- The `--shell` option is replaced with `--script-shell`, but maintained
  in the `npx` executable for backwards compatibility.

We do intend to continue supporting the `npx` that npm ships; just not the
`npm install -g npx` library that is out in the wild today.

### Files On Disk

- [RFC
  13](https://github.com/npm/rfcs/blob/latest/accepted/0013-no-package-json-_fields.md)
  Installed `package.json` files no longer are mutated to include extra
  metadata.  (This extra metadata is stored in the lockfile.)
- `package-lock.json` is updated to a newer format, using
  `"lockfileVersion": 2`.  This format is backwards-compatible with npm CLI
  versions using `"lockfileVersion": 1`, but older npm clients will print a
  warning about the version mismatch.
- `yarn.lock` files used as source of package metadata and resolution
  guidance, if available.  (Prior to v7, they were ignored.)

### Dependency Resolution

These changes affect `install`, `ci`, `install-test`, `install-ci-test`,
`update`, `prune`, `dedupe`, `uninstall`, `link`, and `audit fix`.

- [RFC
  25](https://github.com/npm/rfcs/blob/latest/accepted/0025-install-peer-deps.md)
  `peerDependencies` are installed by default.  This behavior can be
  disabled by setting the `legacy-peer-deps` configuration flag.

    **BREAKING CHANGE**: this can cause some packages to not be
    installable, if they have unresolveable peer dependency conflicts.
    While the correct solution is to fix the conflict, this was not forced
    upon users for several years, and some have come to rely on this lack
    of correctness.  Use the `--legacy-peer-deps` config flag if impacted.

- [RFC
  23](https://github.com/npm/rfcs/blob/latest/accepted/0023-acceptDependencies.md)
  Support for `acceptDependencies` is added.  This can result in dependency
  resolutions that previous versions of npm will incorrectly flag as invalid.

- Git dependencies on known git hosts (GitHub, BitBucket, etc.) will
  always attempt to fetch package contents from the relevant tarball CDNs
  if possible, falling back to `git+ssh` for private packages.  `resolved`
  value in `package-lock.json` will always reflect the `git+ssh` url value.
  Saved value in `package.json` dependencies will always reflect the
  canonical shorthand value.

- Support for the `--link` flag (to install a link to a globall-installed
  copy of a module if present, otherwise install locally) has been removed.
  Local installs are always local, and `npm link <pkg>` must be used
  explicitly if desired.

- Installing a dependency with the same name as the root project no longer
  requires `--force`.  (That is, the `ENOSELF` error is removed.)

### Workspaces

- [RFC
  26](https://github.com/npm/rfcs/blob/latest/accepted/0026-workspaces.md)
  First phase of `workspaces` support is added.  This changes npm's
  behavior when a root project's `package.json` file contains a
  `workspaces` field.

### `npm update`

- [RFC
  19](https://github.com/npm/rfcs/blob/latest/accepted/0019-remove-update-depth-option.md)
  Update all dependencies when `npm update` is run without any arguments.
  As it is no longer relevant, `--depth` config flag removed from `npm
  update`.

### `npm outdated`

- [RFC
  27](https://github.com/npm/rfcs/blob/latest/accepted/0027-remove-depth-outdated.md)
  Remove `--depth` config from `npm outdated`.  Only top-level dependencies
  are shown, unless `--all` config option is set.

### `npm adduser`, `npm login`

- The `--sso` options are deprecated, and will print a warning.

### `npm audit`

- Output and data structure is significantly refactored to call attention
  to issues, identify classes of fixes not previously available, and
  remove extraneous data not used for any purpose.

    **BREAKING CHANGE**: Any tools consuming the output of `npm audit` will
    almost certainly need to be updated, as this has changed significantly,
    both in the readable and `--json` output styles.

### `npm dedupe`

- Performs a full dependency tree reification to disk.  As a result, `npm
  dedupe` can cause missing or invalid packages to be installed or updated,
  though it will only do this if required by the stated dependency
  semantics.

- Note that the `--prefer-dedupe` flag has been added, so that you may
  install in a maximally deduplicated state from the outset.

### `npm fund`

- Human readable output updated, reinstating depth level to the printed
  output.

### `npm ls`

- Extraneous dependencies are listed based on their location in the
  `node_modules` tree.
- `npm ls` only prints the first level of dependencies by default.  You can
  make it print more of the tree by using `--depth=<n>` to set a specific
  depth, or `--all` to print all of them.

### `npm pack`, `npm publish`

- Generated gzipped tarballs no longer contain the zlib OS indicator.  As a
  result, they are truly dependent _only_ on the contents of the package,
  and fully reproducible.  However, anyone relying on this byte to identify
  the operating system of a package's creation may no longer rely on it.

### `npm rebuild`

- Runs package installation scripts as well as re-creating links to bins.
  Properly respects the `--ignore-scripts` and `--bin-links=false`
  configuration options.

### `npm build`, `npm unbuild`

- These two internal commands were removed, as they are no longer needed.

### `npm test`

- When no test is specified, will fail with `missing script: test` rather
  than injecting a synthetic `echo 'Error: no test specified'` test script
  into the `package.json` data.

## Credits

Huge thanks to the people who wrote code for this update, as well as our
group of dedicated Open RFC call participants.  Your participation has
contributed immeasurably to the quality and design of npm.
