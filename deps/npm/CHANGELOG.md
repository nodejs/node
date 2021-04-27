## v7.11.1 (2021-04-23)

### DEPENDENCIES

* [`7925cca24`](https://github.com/npm/cli/commit/7925cca24543d9e1a8297844b3e53e11057643ef)
  `pacote@11.3.3`:
  * fix(registry): normalize manfest
* [`b61eac693`](https://github.com/npm/cli/commit/b61eac693df82c52b955e6c18ec4dcf4cedea8a3)
  [#3130](https://github.com/npm/cli/issues/3130)
  `@npmcli/config@2.2.0`
* [`c74e67fc6`](https://github.com/npm/cli/commit/c74e67fc6572bb001d74c7486c05d211a0e03de8)
  [#3130](https://github.com/npm/cli/issues/3130)
  `npm-registry-fetch@10.1.1`

### DOCUMENTATION

* [`efdd7dd44`](https://github.com/npm/cli/commit/efdd7dd4427a0ee856c18aab1df2d3d30a307997)
  Remove unused and incorrectly documented `--always-auth` config definition
  ([@isaacs](https://github.com/isaacs))

## v7.11.0 (2021-04-22)

### FEATURES

* [`4c1f16d2c`](https://github.com/npm/cli/commit/4c1f16d2c29a7a56c19b97f2820e6305a6075083)
  [#3095](https://github.com/npm/cli/issues/3095)
  feat(init): add workspaces support
  ([@ruyadorno](https://github.com/ruyadorno))

### BUG FIXES

* [`42ca59eee`](https://github.com/npm/cli/commit/42ca59eeedd3e402aa1c606941f7f52864e6039b)
  [#3086](https://github.com/npm/cli/issues/3086)
  fix(ls): do not exit with error when all problems are extraneous deps
  ([@nlf](https://github.com/nlf))
* [`2aecec591`](https://github.com/npm/cli/commit/2aecec591df6866e27d0b17dc49cef8f7d738d77)
  [#2724](https://github.com/npm/cli/issues/2724)
  [#3119](https://github.com/npm/cli/issues/3119)
  fix(ls): make --long work when missing deps
  ([@ruyadorno](https://github.com/ruyadorno))
* [`42e0587a9`](https://github.com/npm/cli/commit/42e0587a9ea6940a5d5be5903370ad1113feef21)
  [#3115](https://github.com/npm/cli/issues/3115)
  fix(pack): refuse to pack invalid packument
  ([@wraithgar](https://github.com/wraithgar))
* [`1c4eff7b5`](https://github.com/npm/cli/commit/1c4eff7b513b8e84876818ede014d3ab19d203c6)
  [#3126](https://github.com/npm/cli/issues/3126)
  fix(logout): use isBasicAuth attribute
  ([@wraithgar](https://github.com/wraithgar))
### DOCUMENTATION


* [`c93f1c39e`](https://github.com/npm/cli/commit/c93f1c39e326feff0857712a10ef6183fbafe1ab)
  [#3101](https://github.com/npm/cli/issues/3101)
  chore(docs): update view docs
  ([@wraithgar](https://github.com/wraithgar))
* [`c4ff4bc11`](https://github.com/npm/cli/commit/c4ff4bc113c3a5b6ee5d74ab0b1adee95169ed32)
  [npm/statusboard#313](https://github.com/npm/statusboard/issues/313)
  [#3109](https://github.com/npm/cli/issues/3109)
  fix(usage): fix refs to ws shorthand
  ([@ruyadorno](https://github.com/ruyadorno))


### DEPENDENCIES

* [`83166ebcc`](https://github.com/npm/cli/commit/83166ebcc4ba5e3bf215f08151437d96637f4f33)
  `npm-registry-fetch@10.1.0`
    * feat(auth): set isBasicAuth
* [`e02bda6da`](https://github.com/npm/cli/commit/e02bda6da68b8e8f490bf270cb5d6adec81685ea)
  `npm-registry-fetch@10.0.0`
    * feat(auth) load/send based on URI, not registry
* [`a0382deba`](https://github.com/npm/cli/commit/a0382deba346b09834e75db89e1fd4527f1f07dd)
  `@npmcli/run-script@1.8.5`
    * fix: windows ComSpec env variable name
* [`7f82ef5a8`](https://github.com/npm/cli/commit/7f82ef5a84d70e28983ed43ba1d8aced0fb4ba45)
  `pacote@11.3.2`
* [`35e49b94f`](https://github.com/npm/cli/commit/35e49b94fba478a63df6cc9b62816eafe5f1fbdd)
  `@npmcli/arborist@2.4.0`
* [`95faf8ce6`](https://github.com/npm/cli/commit/95faf8ce6c007082a02c160977da194c08ee9d82)
  `libnpmaccess@4.0.2`
* [`17fffc0e4`](https://github.com/npm/cli/commit/17fffc0e42b2a9e7b84691093e45ba511906cbfa)
  `libnpmhook@6.0.2`
* [`1b5a213aa`](https://github.com/npm/cli/commit/1b5a213aaf39652661ba72ba2e8751f049b170fb)
  `libnpmorg@2.0.2`
* [`9f83e6484`](https://github.com/npm/cli/commit/9f83e6484aa163d066f318df42ec89c8234b614e)
  `libnpmpublish@4.0.1`
* [`251f788c5`](https://github.com/npm/cli/commit/251f788c554a198ab42682453fa5504f8abe93fe)
  `libnpmsearch@3.1.1`
* [`35873a989`](https://github.com/npm/cli/commit/35873a989fe67041ddcf30a0a278ed77ace5ee3c)
  `libnpmteam@2.0.3`
* [`23e12b4d8`](https://github.com/npm/cli/commit/23e12b4d8f63d765a48036e7bb08f53319c73304)
  `npm-profile@5.0.3`

## v7.10.0 (2021-04-15)

### FEATURES

* [`f9b639eb6`](https://github.com/npm/cli/commit/f9b639eb6c504ded6cdd59e83e26a392bfe81e5d)
  [#3052](https://github.com/npm/cli/issues/3052)
  feat(bugs): fall back to email if provided
  ([@Yash-Singh1](https://github.com/Yash-Singh1))
* [`8c9e24778`](https://github.com/npm/cli/commit/8c9e24778db867cb3148bc247c7e321639aa9f58)
  [#3055](https://github.com/npm/cli/issues/3055)
  feat(version): add workspace support
  ([@wraithgar](https://github.com/wraithgar))

### DEPENDENCIES

* [`f1e6743a6`](https://github.com/npm/cli/commit/f1e6743a6e8e32ddad6d1964eb05d17e6c50a456)
  `libnpmversion@1.2.0`
    * feat(retrieve-tag): retrieve unannotated git tags
    * fix(retrieve-tag): use semver to look for semver
* [`3b476a24c`](https://github.com/npm/cli/commit/3b476a24cf0b2823fdf92505b84bddde4fcc8b14)
  `@npmcl/git@2.0.8`
    * fix(git): do not use shell when calling git
* [`dfcd0c1e2`](https://github.com/npm/cli/commit/dfcd0c1e2331c1f4b6573466b50505772eddaf22)
  [#3069](https://github.com/npm/cli/issues/3069)
  `tap@15.0.2`

### DOCUMENTATION

* [`90b61eda9`](https://github.com/npm/cli/commit/90b61eda9b41af108ed69fc0c43a522a92745047)
  [#3053](https://github.com/npm/cli/issues/3053)
  fix(contributing.md): explicitely outline dep updates
  ([@darcyclarke](https://github.com/darcyclarke))

## v7.9.0 (2021-04-08)

### FEATURES

* [`1f3e88eba`](https://github.com/npm/cli/commit/1f3e88ebaf4901d8f9f07b43404d824fef7e5ff5)
  [#3032](https://github.com/npm/cli/issues/3032)
  feat(dist-tag): add workspace support
  ([@nlf](https://github.com/nlf))
* [`6e31df4e7`](https://github.com/npm/cli/commit/6e31df4e7957337962fd3d93e495931e3592bb9e)
  [#3033](https://github.com/npm/cli/issues/3033)
  feat(pack): add workspace support
  ([@wraithgar](https://github.com/wraithgar))

### DEPENDENCIES

* [`ba4f7fea8`](https://github.com/npm/cli/commit/ba4f7fea8fca8e3509469a218f094fe69095888b)
  `licensee@8.2.0`

## v7.8.0 (2021-04-01)

### FEATURES


* [`8bcc5d73f`](https://github.com/npm/cli/commit/8bcc5d73f35434e781ff56419dd7f0c380efd072)
  [#2972](https://github.com/npm/cli/issues/2972)
  feat(workspaces): add repo and docs
  ([@wraithgar](https://github.com/wraithgar))
* [`ec520ce32`](https://github.com/npm/cli/commit/ec520ce32d5e834a32ebd58491df4200e01ce690)
  [#2998](https://github.com/npm/cli/issues/2998)
  feat(set-script): implement workspaces
* [`32717a60e`](https://github.com/npm/cli/commit/32717a60eb55fcf8c7e5016223bfee78a6daba0e)
  [#3001](https://github.com/npm/cli/issues/3001)
  feat(view): add workspace support
  ([@wraithgar](https://github.com/wraithgar))
* [`7b177e43f`](https://github.com/npm/cli/commit/7b177e43f3bfb558bcd8723cdb2166a3df19647a)
  [#3014](https://github.com/npm/cli/issues/3014)
  feat(config): add 'envExport' flag
  ([@isaacs](https://github.com/isaacs))

### BUG FIXES

* [`4c4252348`](https://github.com/npm/cli/commit/4c4252348c538246e1072421d65f4558dc948080)
  [#3016](https://github.com/npm/cli/issues/3016)
  fix(usage): specify the key each time for multiples
  ([@isaacs](https://github.com/isaacs))
* [`9237d375b`](https://github.com/npm/cli/commit/9237d375b0b7d34c7dc5ba70aec7f616f4133732)
  [#3013](https://github.com/npm/cli/issues/3013)
  fix(docs): add workspaces configuration
  ([@wraithgar](https://github.com/wraithgar))
* [`cb6eb0d20`](https://github.com/npm/cli/commit/cb6eb0d206b7e2f63d5c7a7a17bea4aed1b9f2bf)
  [#3015](https://github.com/npm/cli/issues/3015)
  fix(ERESOLVE): better errors when current is missing
  ([@isaacs](https://github.com/isaacs))

### DEPENDENCIES

* [`61da39beb`](https://github.com/npm/cli/commit/61da39beb5373320e2b591b61ecd6596eeaba6ed)
  `@npmcli/config@2.1.0`
  * feat(config): add support for envExport:false
* [`fb095a708`](https://github.com/npm/cli/commit/fb095a708a1f930bbd0195446ac611b82bfeff14)
  `@npmcli/arborist@2.3.0`:
  * [#2896](https://github.com/npm/cli/issues/2896) Provide currentEdge in
  ERESOLVE if known, and address self-linking edge case.
  * Add/remove dependencies to/from workspaces when set, not root project
  * Only reify the portions of the dependency graph identified by the
  `workspace` configuration value.
  * Do not recursively `chown` the project root path.

## v7.7.6 (2021-03-29)

### BUG FIXES

* [`9dd2ed518`](https://github.com/npm/cli/commit/9dd2ed5189b6f283094664e9e192cf1598ec3f79)
  fix empty newline printed to stderr
  ([@ruyadorno](https://github.com/ruyadorno))
* [`9d391462a`](https://github.com/npm/cli/commit/9d391462a25f637219501e2430ef1f7b89710816)
  [#2973](https://github.com/npm/cli/issues/2973)
  fix spelling in workspaces.md file
  ([@sethomas](https://github.com/sethomas))
* [`4b100249a`](https://github.com/npm/cli/commit/4b100249a6cad67e002186816e64817313b636c7)
  [#2979](https://github.com/npm/cli/issues/2979)
  change 'maxsockets' default value back to 15
  ([@wallrat](https://github.com/wallrat))

### DEPENDENCIES

* [`a28f89572`](https://github.com/npm/cli/commit/a28f89572a708cced69cc938f877eaa969dbad9e)
  `libnpmversion@1.1.0`
    * fix reading `script-shell` config on `npm version` lifecycle scripts
* [`03734c29e`](https://github.com/npm/cli/commit/03734c29e00191d17f164d1c0e75d9f228268842)
  `npm-packlist@2.1.5`
    * fix packaging `bundledDependencies`
* [`80ce2a019`](https://github.com/npm/cli/commit/80ce2a019526632b01b70e1c75c42608dc160332)
  `@npmcli/metavuln-calculator@1.1.1`
    * fix error auditing package documents with missing dependencies

## v7.7.5 (2021-03-25)

### BUG FIXES

* [`95ba87622`](https://github.com/npm/cli/commit/95ba87622e00d68270eda9e071b19737718fca16)
  [#2949](https://github.com/npm/cli/issues/2949)
  fix handling manual indexes in `npm help`
  ([@dmchurch](https://github.com/dmchurch))
* [`59cf37962`](https://github.com/npm/cli/commit/59cf37962a2286e0f7d3bd37fa9c8bc3bac94218)
  [#2958](https://github.com/npm/cli/issues/2958)
  always set `npm.command` to canonical command name
  ([@isaacs](https://github.com/isaacs))
* [`1415b4bde`](https://github.com/npm/cli/commit/1415b4bdeeaabb6e0ba12b6b1b0cc56502bd64ab)
  [#2964](https://github.com/npm/cli/issues/2964)
  fix(config): properly translate user-agent
  ([@wraithgar](https://github.com/wraithgar))
* [`59271936d`](https://github.com/npm/cli/commit/59271936d90fbd6956a41967119f578c0ba63db9)
  [#2965](https://github.com/npm/cli/issues/2965)
  fix(config): tie save-exact/save-prefix together
  ([@wraithgar](https://github.com/wraithgar))

### TESTS

* [`97b415287`](https://github.com/npm/cli/commit/97b41528739460b2e9e72e09000aded412418cb2)
  [#2959](https://github.com/npm/cli/issues/2959)
  add smoke tests
  ([@ruyadorno](https://github.com/ruyadorno))

## v7.7.4 (2021-03-24)

### BUG FIXES

* [`200bee74b`](https://github.com/npm/cli/commit/200bee74b31a738687446b7b535cac67b1c582fd)
  [#2951](https://github.com/npm/cli/issues/2951)
  fix(config): accept explicit `production=false`
  ([@wraithgar](https://github.com/wraithgar))
* [`7b45e9df6`](https://github.com/npm/cli/commit/7b45e9df6102c7bd6e403d1fdc9939581c38f546)
  [#2950](https://github.com/npm/cli/issues/2950)
  warn if using workspaces config options in `npm config`
  ([@ruyadorno](https://github.com/ruyadorno))

## v7.7.3 (2021-03-24)

### BUG FIXES

* [`c76f04ac2`](https://github.com/npm/cli/commit/c76f04ac28ddf2ae4df4b3ce0aec684a118de1b5)
  [#2925](https://github.com/npm/cli/issues/2925)
  fix(set-script): add completion
  ([@Yash-Singh1](https://github.com/Yash-Singh1))
* [`0379eab69`](https://github.com/npm/cli/commit/0379eab698b78ae4aa89bbe2043607f420e52f11)
  [#2929](https://github.com/npm/cli/issues/2929)
  fix(install): ignore auditLevel
  `npm install` should not be affected by the `auditLevel` config, as the
  results of audit do not change its exit status.
  ([@wraithgar](https://github.com/wraithgar))
* [`98efadeb4`](https://github.com/npm/cli/commit/98efadeb4b2ae9289f14ed6f42a169230faf7239)
  [#2923](https://github.com/npm/cli/issues/2923)
  fix(audit-level): add `info` audit level
  This is a valid level but wasn't configured to be allowed.
  Also added this param to the usage output for `npm audit`
  ([@wraithgar](https://github.com/wraithgar))
* [`e8d2adcf4`](https://github.com/npm/cli/commit/e8d2adcf40ad63030f844c9aa44c6d16e2146797)
  [#2945](https://github.com/npm/cli/issues/2945)
  config should not error when workspaces are configured
  ([@nlf](https://github.com/nlf))
* [`aba2bc623`](https://github.com/npm/cli/commit/aba2bc623ea99e563b1b15b81dbb4ba94f86fe4c)
  [#2944](https://github.com/npm/cli/issues/2944)
  fix(progress): re-add progress bar to reify
  The logger was no longer in flatOptions, we pass it in explicitly now
  ([@wraithgar](https://github.com/wraithgar))
* [`877b4ed29`](https://github.com/npm/cli/commit/877b4ed2925c97b5249a4d33575420dda64f7339)
  [#2946](https://github.com/npm/cli/issues/2946)
  fix(flatOptions): re-add `_auth`
  This was not being added to flatOptions, and things like
  `npm-registry-fetch` are looking for it.
  ([@wraithgar](https://github.com/wraithgar))

## v7.7.2 (2021-03-24)

### BUG FIXES
* [`a4df2b98d`](https://github.com/npm/cli/commit/a4df2b98d89429b19cd29b5fc895cdbfc0a6bd78)
  [#2942](https://github.com/npm/cli/issues/2942)
  Restore --dev flag, unify --omit flatteners
  ([@isaacs](https://github.com/isaacs))

### DEPENDENCIES
* [`2cbfaac0e`](https://github.com/npm/cli/commit/2cbfaac0ecd5810316f6d76168ed9618bd11bf3a)
  `hosted-git-info@4.0.2`
    * [#83](https://github.com/npm/hosted-git-info/pull/83) Do not parse
        urls for gitlab
  ([@nlf](https://github.com/nlf))

## v7.7.1 (2021-03-24)

### BUG FIXES

* [`543b0e39b`](https://github.com/npm/cli/commit/543b0e39bcb94fc408804b01ca9c0d7b960b2681)
  [#2930](https://github.com/npm/cli/issues/2930)
  fix(uninstall): use correct local prefix
  ([@jameschensmith](https://github.com/jameschensmith))
* [`dce4960ef`](https://github.com/npm/cli/commit/dce4960ef6d52af128affe7755b2ca72de913b6c)
  [#2932](https://github.com/npm/cli/issues/2932)
  fix(config): flatten savePrefix properly
  ([@wraithgar](https://github.com/wraithgar))

## v7.7.0 (2021-03-23)

### FEATURES

* [`33c4189f9`](https://github.com/npm/cli/commit/33c4189f939aebdfaf85ea419e6ea01d0977b79d)
  [#2864](https://github.com/npm/cli/issues/2864)
  add `npm run-script` workspaces support
  ([@ruyadorno](https://github.com/ruyadorno))
* [`e1b3b318f`](https://github.com/npm/cli/commit/e1b3b318f095a7e1a7cc4b131907de4955275d9d)
  [#2886](https://github.com/npm/cli/issues/2886)
  add `npm exec` workspaces support
  ([@ruyadorno](https://github.com/ruyadorno))
* [`41facf643`](https://github.com/npm/cli/commit/41facf6435ced4e416d74111d9c3ff00ee19ab7d)
  [#2859](https://github.com/npm/cli/issues/2859)
  expanded "Did you mean?" suggestions for missing cmds and scripts
  ([@wraithgar](https://github.com/wraithgar))

### BUG FIXES

* [`8cce4282f`](https://github.com/npm/cli/commit/8cce4282f7bef11aeeb73cffd532b477b241985e)
  [#2865](https://github.com/npm/cli/issues/2865)
  `npm publish`: handle case where multiple config list is present
  ([@kenrick95](https://github.com/kenrick95))
* [`6598bfe86`](https://github.com/npm/cli/commit/6598bfe8697439e827d84981f8504febca64a55a)
  mark deprecated configs
  ([@isaacs](https://github.com/isaacs))
* [`8a38afe77`](https://github.com/npm/cli/commit/8a38afe779ce71a10178ed62b13709d06adf7a66)
  [#2881](https://github.com/npm/cli/issues/2881)
  docs(package-json): document default main behavior
  ([@klausbayrhammer](https://github.com/klausbayrhammer))
* [`93a061d73`](https://github.com/npm/cli/commit/93a061d737dc769663652368e8586e4202267b9e)
  [#2917](https://github.com/npm/cli/issues/2917)
  add action items to `npm run` error output
  ([@wraithgar](https://github.com/wraithgar))

### DOCUMENTATION

* [`ad65bd910`](https://github.com/npm/cli/commit/ad65bd9101aa8e8b94bc1e48df3ef93deca6d30c)
  [#2860](https://github.com/npm/cli/issues/2860)
  fix link in configuring-npm
  ([@varmakarthik12](https://github.com/varmakarthik12))
* [`b419bfb02`](https://github.com/npm/cli/commit/b419bfb0259596fb338d45b2eaeab25a7a0d1f1e)
  [#2876](https://github.com/npm/cli/issues/2876)
  fix test-coverage command in contributing guide
  ([@chowkapow](https://github.com/chowkapow))

### DEPENDENCIES

* [`7b5606b93`](https://github.com/npm/cli/commit/7b5606b931083e8a70f5ea094c2b46f0b7a38a18)
  `@npmcli/arborist@2.2.9`
    * [#254](https://github.com/npm/arborist/pull/254) Honor explicit
      prefix when saving dependencies
      ([@jameschensmith](https://github.com/jameschensmith))
    * [#255](https://github.com/npm/arborist/pull/255) Never save to
      `bundleDependencies` when saving a `peer` or `peerOptional`
      dependency. ([@isaacs](https://github.com/isaacs))
* [`f76e7c21f`](https://github.com/npm/cli/commit/f76e7c21ffd87b08593d8c396a78ab9c5fa790bd)
  `pacote@11.3.1`
    * increases tarball compression level
* [`4928512bc`](https://github.com/npm/cli/commit/4928512bcefd8448ff5852978cfc7f903e3ae996)
  `semver@7.3.5`
    * fix handling prereleases/ANY ranges in subset
* [`1924eb457`](https://github.com/npm/cli/commit/1924eb457aea7c93dfaf4a911355a63d84d66eee)
  `libnpmversion@1.0.12`
    * fix removing undescored-prefixed package.json properties in `npm version`
* [`916623056`](https://github.com/npm/cli/commit/91662305643509eebd2f79ed7e3ff01562aa4968)
  `@npmcli/run-script@1.8.4`
    * fix expanding windows-style environment variables
* [`a8d0751e4`](https://github.com/npm/cli/commit/a8d0751e4b7c7d8b808c8a49f288fc7272f729b0)
  `npm-pick-manifest@6.1.1`
    * fix running packages with a single executable binary with `npm exec`
* [`af7eaac50`](https://github.com/npm/cli/commit/af7eaac5018ed821d72d43d08f1d7e49e7491453)
  `hosted-git-info@4.0.1`
* [`f52c51db1`](https://github.com/npm/cli/commit/f52c51db13c39cfbaed18dbd13ba7302a4b6a0d9)
  `@npmcli/config@2.0.0`

## v7.6.3 (2021-03-11)

### DOCUMENTATION

* [`8c44e999b`](https://github.com/npm/cli/commit/8c44e999bdf7639893535c55beebf7996da2c47f)
  [#2855](https://github.com/npm/cli/issues/2855)
  Correct "npm COMMAND help" to "npm help COMMAND"
  ([@dwardu](https://github.com/dwardu))

### DEPENDENCIES

* [`57ed390d6`](https://github.com/npm/cli/commit/57ed390d64a44ae0a1b2c4afd79d690170b194ec)
  `@npmcli/arborist@2.2.8`
    * Respect link deps when calculating peerDep sets

## v7.6.2 (2021-03-09)

### BUG FIXES

* [`e0a3a5218`](https://github.com/npm/cli/commit/e0a3a5218cac7ca5850930aaaad8a939ddf75d4d)
  [#2831](https://github.com/npm/cli/issues/2831)
  Fix cb() never called in search with --json option
  ([@fraqe](https://github.com/fraqe))
* [`85a8694dd`](https://github.com/npm/cli/commit/85a8694dd9b4a924a474ba75261914511a216868)
  [#2795](https://github.com/npm/cli/issues/2795)
  fix(npm.output): make output go through npm.output
  ([@wraithgar](https://github.com/wraithgar))
* [`9fe0df5b5`](https://github.com/npm/cli/commit/9fe0df5b5d7606e5841288d9931be6c04767c9ca)
  [#2821](https://github.com/npm/cli/issues/2821)
  fix(usage): clean up usage declarations
  ([@wraithgar](https://github.com/wraithgar))

### DEPENDENCIES

* [`7f470b5c2`](https://github.com/npm/cli/commit/7f470b5c25d544e36d97b28e28ae20dfa1d4ab31)
  `@npmcli/arborist@2.2.7`
    * fix(install): Do not revert a file: dep to version on bare name re-install
* [`e9b7fc275`](https://github.com/npm/cli/commit/e9b7fc275a0bdf8f00dbcf5dd2283675776fc459)
  `libnpmdiff@2.0.4`
    * fix(diff): Gracefully handle packages with prepare script
* [`c7314aa62`](https://github.com/npm/cli/commit/c7314aa62195b7f0d8886776692e8a2c892413ed)
  `byte-size@7.0.1`
* [`864f48d43`](https://github.com/npm/cli/commit/864f48d4327269f521161cf89888ea2b6db5fdab)
  `pacote@11.3.0`

## v7.6.1 (2021-03-04)

### BUG FIXES

* [`3c9a589b0`](https://github.com/npm/cli/commit/3c9a589b004fa828a304abaf52d1d781710e1143)
  [#2807](https://github.com/npm/cli/issues/2807)
  `npm explain` show when an edge is a bundled edge
  ([@kumavis](https://github.com/kumavis))
* [`b33c760ce`](https://github.com/npm/cli/commit/b33c760cea7fe2696d35b5530abc1b455980fef1)
  [#2766](https://github.com/npm/cli/issues/2766)
  unused arguments cleanup
  ([@sandersn](https://github.com/sandersn))
* [`4a5dd3a5a`](https://github.com/npm/cli/commit/4a5dd3a5a200b3f4f7b47168497d8e03dca3a2ca)
  [#2772](https://github.com/npm/cli/issues/2772)
  fix(npm) pass npm context everywhere
  ([@wraithgar](https://github.com/wraithgar))
* [`e69be2ac5`](https://github.com/npm/cli/commit/e69be2ac5c35e985732e2baa00b70d39332e4b9f)
  [#2789](https://github.com/npm/cli/issues/2789)
  fix npm prefix on all Windows unix shells
  ([@isaacs](https://github.com/isaacs))
* [`2d682e4ca`](https://github.com/npm/cli/commit/2d682e4cab0cf109a16332f3222f1e9a4027db69)
  [#2803](https://github.com/npm/cli/issues/2803)
  fix(search): don't pass unused args
  ([@wraithgar](https://github.com/wraithgar))
* [`b3e7dd19b`](https://github.com/npm/cli/commit/b3e7dd19bb4888dad2bfb6702aed6560a7f91bf8)
  [#2822](https://github.com/npm/cli/issues/2822)
  fix(diff): set option "where" for pacote
  ([@ruyadorno](https://github.com/ruyadorno))
* [`96006640b`](https://github.com/npm/cli/commit/96006640b902d31415260df5ce3ad8d066a64623)
  [#2824](https://github.com/npm/cli/issues/2824)
  fix(repo, auth.sso): don't promisify open-url
  ([@wraithgar](https://github.com/wraithgar))

### DOCUMENTATION

* [`c8b73db82`](https://github.com/npm/cli/commit/c8b73db82f0f2445c20a0a64110586253accd66b)
  [#2690](https://github.com/npm/cli/issues/2690)
  fix(docs): update scripts docs
  ([@wraithgar](https://github.com/wraithgar))
* [`5d922394b`](https://github.com/npm/cli/commit/5d922394b7874b2b38d34f03f2decbe0eb3e8583)
  [#2809](https://github.com/npm/cli/issues/2809)
  update republish timeout after unpublish
  ([@BAJ-](https://github.com/BAJ-))

### DEPENDENCIES

* [`2d4ae598f`](https://github.com/npm/cli/commit/2d4ae598f30049680797685f76154b16a7e15a66)
  `@npmcli/arborist@2.2.6`

## v7.6.0 (2021-02-25)

### FEATURES

* [`983d218f7`](https://github.com/npm/cli/commit/983d218f7e68e3c7866f2efa23ea2aff7ff3881e)
  [#2750](https://github.com/npm/cli/issues/2750)
  feat(explain): mark when dependency is bundled
  ([@kumavis](https://github.com/kumavis))

### DEPENDENCIES

* [`b9fa7e32a`](https://github.com/npm/cli/commit/b9fa7e32a63a3dc3a4865865c4ca737c862b9cf2)
  chore(package-lock): resetdeps and `eslint@7.20.0`
  ([@wraithgar](https://github.com/wraithgar))
* [`28d036ae9`](https://github.com/npm/cli/commit/28d036ae9179f742bd0518e558a54f014a7a895e)
  `arborist@2.2.5`
    * fix: hidden lockfiles were not respected on Node v10.0-10.12

### DOCUMENTATION

* [`ba1adef42`](https://github.com/npm/cli/commit/ba1adef4292123e87e26b59e0c6fd6f5ff1fe775)
  [#2760](https://github.com/npm/cli/issues/2760)
  chore(docs): capitalize all Instaces of "package"
  ([@MrBrain295](https://github.com/MrBrain295))
* [`8bfa05fa1`](https://github.com/npm/cli/commit/8bfa05fa1dfd4a64381c7ec750df6d174724e8c1)
  [#2775](https://github.com/npm/cli/issues/2775)
  chore(docs): add navigation configuration
  ([@ethomson](https://github.com/ethomson))
* [`238e474a4`](https://github.com/npm/cli/commit/238e474a48ddecc33c76eb3d2c4d0642cfe8829a)
  [#2778](https://github.com/npm/cli/issues/2778)
  chore(docs):update unpublish cooldown
  ([@christoflemke](https://github.com/christoflemke))

## v7.5.6 (2021-02-22)

### BUG FIXES

* [`4e58274ed`](https://github.com/npm/cli/commit/4e58274ed0fd2dd29d3c8d6c7c47f37a37dc0f0f)
  [#2742](https://github.com/npm/cli/issues/2742)
  Do not print error banner for shell proxy commands
  ([@isaacs](https://github.com/isaacs))

### DOCS

* [`3c72ab441`](https://github.com/npm/cli/commit/3c72ab4412111c708736e3a7b8342150372a4af4)
  [#2749](https://github.com/npm/cli/issues/2749)
  Capitalize Package in a Heading
  ([@MrBrain295](https://github.com/MrBrain295))

### DEPENDENCIES

* [`f3ae6ed0d`](https://github.com/npm/cli/commit/f3ae6ed0d25ce80868f59353ef71c09ac77b1cf5)
  `read-package-json@3.0.1`, `read-package-json-fast@2.0.2`
* [`9b311fe52`](https://github.com/npm/cli/commit/9b311fe522077c7f8a242b94b0e1dbe746992bef)
  [#2736](https://github.com/npm/cli/issue/2736) `@npmcli/arborist@2.2.4`:
  * Do not rely on underscore fields in `package.json` files
  * Do not remove global packages when updating by name
  * Keep `yarn.lock` and `package-lock.json` more in sync

## v7.5.5 (2021-02-22)

### BUG FIXES
* [`49c95375a`](https://github.com/npm/cli/commit/49c95375af49308e2db6ba28e91c65193754e091)
  [#2688](https://github.com/npm/cli/issues/2688)
  fix shrinkwrap in node v10.0
  ([@ljharb](https://github.com/ljharb))
* [`00afa3161`](https://github.com/npm/cli/commit/00afa316195f2db903146110a07ffdaec9bb6aa2)
  [#2718](https://github.com/npm/cli/issues/2718)
  restore the prefix on output from `npm version <inc>`
  ([@nlf](https://github.com/nlf))
* [`69e0c4e8c`](https://github.com/npm/cli/commit/69e0c4e8cd684c475a4450c40dfb32c995061aea)
  [#2716](https://github.com/npm/cli/issues/2716)
  throw an error when trying to dedupe in global mode
  ([@nlf](https://github.com/nlf))
* [`b018eb842`](https://github.com/npm/cli/commit/b018eb84266dc5a02274849135ca148cb59cc349)
  [#2719](https://github.com/npm/cli/issues/2719)
  obey silent loglevel in run-script
  ([@wraithgar](https://github.com/wraithgar))

### DEPENDENCIES
* [`8c36697df`](https://github.com/npm/cli/commit/8c36697dfffe8b5e853fe889c9ead5578100c413)
  `@npmcli/arborist@2.2.3`
    * [#1875](https://github.com/npm/cli/issues/1875)
      [arborist#230](https://github.com/npm/arborist/pull/230)
      Set default advisory `severity`/`vulnerable_range` when missing from audit endpoint data
      ([@isaacs](https://github.com/isaacs))
    * [npm/arborist#231](https://github.com/npm/arborist/pull/231)
      skip optional deps with mismatched platform or engine
      ([@nlf](https://github.com/nlf))
    * [#2251](https://github.com/npm/cli/issues/2251)
      Unpack shrinkwrapped deps not already unpacked
      ([@isaacs](https://github.com/isaacs),
      [@nlf](https://github.com/nlf))
    * [#2714](https://github.com/npm/cli/issues/2714)
      Do not write package.json if nothing changed
      ([@isaacs](https://github.com/isaacs))
    * [npm/rfcs#324](https://github.com/npm/rfcs/issues/324)
      Prefer peer over prod dep, if both specified
      ([@isaacs](https://github.com/isaacs))
    * [npm/arborist#236](https://github.com/npm/arborist/issues/236)
      Fix additional peerOptional conflict cases
      ([@isaacs](https://github.com/isaacs))
* [`d865b101f`](https://github.com/npm/cli/commit/d865b101f72142619531311645479f0596a68a1a)
  `libnpmpack@2.0.1`
    * respect silent loglevel
* [`e606953e5`](https://github.com/npm/cli/commit/e606953e5795803a7c4eddb4ea993735ef65ec95)
  `libnpmversion@1.0.11`
    * respect silent loglevel
* [`9c51005a1`](https://github.com/npm/cli/commit/9c51005a19fd4c3e7cd4c987d2e39d1b763036bf)
  `npm-package-arg@8.1.1`
    * do a better job of detecting git specifiers like `git@github.com:npm/cli`
* [`8b6bf0db4`](https://github.com/npm/cli/commit/8b6bf0db49a3378bd85a0d1ffdd19fbdd68a944a)
  `pacote@11.2.7`
    * respect silent loglevel
    * fix INVALID_URL errors for certain git dependencies

### TESTS
* [`80c2ac995`](https://github.com/npm/cli/commit/80c2ac995170a05b26856a2b72fe9c8163b2c999)
  [#2717](https://github.com/npm/cli/issues/2717)
  refactor publish tests
  ([@wraithgar](https://github.com/wraithgar))
* [`9d81e0ceb`](https://github.com/npm/cli/commit/9d81e0ceba7d69e0651662508415ee3705bddfd9)
  [#2729](https://github.com/npm/cli/issues/2729)
  fix typo in shrinkwrap tests
  ([@eltociear](https://github.com/eltociear))

### DOCUMENTATION
* [`e3de7befb`](https://github.com/npm/cli/commit/e3de7befb3a9e2fcb7aac5b740d09b3b7d99d724)
  [#2685](https://github.com/npm/cli/issues/2685)
  docs(readme): add note back about branding/origin
  ([@darcyclarke](https://github.com/darcyclarke))
* [`38d87e7c2`](https://github.com/npm/cli/commit/38d87e7c24aea13b0f1c1157aad58d9d15bf8e63)
  [#2698](https://github.com/npm/cli/issues/2698)
  mention nodenv in README.md
  ([@RA80533](https://github.com/RA80533))
* [`af4422cdb`](https://github.com/npm/cli/commit/af4422cdbc110f93203667efc08b16f7aa74ac2f)
  [#2711](https://github.com/npm/cli/issues/2711)
  validate that the docs can be parsed by mdx
  ([@ethomson](https://github.com/ethomson))


## v7.5.4 (2021-02-12)

### BUG FIXES

* [`ef687f545`](https://github.com/npm/cli/commit/ef687f545b177d0496ce74faacf1bf738978355a)
  [#2655](https://github.com/npm/cli/issues/2655)
  fix(env): Do not clobber defined 'env' script
  ([@isaacs](https://github.com/isaacs))
* [`868954a72`](https://github.com/npm/cli/commit/868954a72c06ff2210b35e1e75571f4ec3357c43)
  [#2654](https://github.com/npm/cli/issues/2654)
  [fix] node v10.0 lacks `fs.promises`
  ([@ljharb](https://github.com/ljharb))


### DEPENDENCIES

* [`14dd93853`](https://github.com/npm/cli/commit/14dd9385358b3815c2285526f7c2e53ed3c5e8da)
  fix(package.json): resetdeps
  ([@wraithgar](https://github.com/wraithgar))
* [`39e4a6401`](https://github.com/npm/cli/commit/39e4a640130b85d62199a33cc2026b04390520ee)
  `graceful-fs@4.2.6`
* [`96dffab98`](https://github.com/npm/cli/commit/96dffab988048164516d8cf73c1fbf66781f86df)
  `eslint-plugin-promise@4.3.1`
* [`9a6e9d38a`](https://github.com/npm/cli/commit/9a6e9d38abccec793b6ac14871c2b639d62a6c41)
  `@npmcli/run-script@1.8.3`
    * fix fs.promises reference to run in node v10.0
* [`584b746a2`](https://github.com/npm/cli/commit/584b746a2c8cdc697629298be27dd23d19de1231)
  `@npmcli/git@2.0.5`
* [`6305ebde4`](https://github.com/npm/cli/commit/6305ebde43796737014aedbe019db8cd81dcbbec)
  `make-fetch-happen@8.0.14`
* [`e99881117`](https://github.com/npm/cli/commit/e998811170ce5df00a725b2d683b4bff124c6792)
  `libnpmversion@1.0.10`
* [`554d91cdf`](https://github.com/npm/cli/commit/554d91cdf82e9c92c2ac3752ed91e7081c2271e5)
  chore(package-lock): rebuild package-lock
  ([@wraithgar](https://github.com/wraithgar))
* [`37e8cc507`](https://github.com/npm/cli/commit/37e8cc507b2ce0b89f92e7e77b1d909d1bf5513f)
  `@npmcli/arborist@2.2.2`
    * [#2505](https://github.com/npm/cli/issues/2505) properly install
      dependenciess of linked dependencies
      ([@ruyadorno](https://github.com/ruyadorno))
    * [#2504](https://github.com/npm/cli/issues/2504) Allow `--force` to
      override conflicted optional peerDependencies
      ([@isaacs](https://github.com/isaacs))
    * Ensure correct flags on shrinkwrapped module deps
      ([@isaacs](https://github.com/isaacs))
    * Correct relative paths for global packages installed from tarball files
      ([nlf](https://github.com/nlf))
* [`7788ce47b`](https://github.com/npm/cli/commit/7788ce47bc264d9d951055da85f2b695eb8b3f15)
  `@npmcli/map-workspaces@1.0.3`

### TESTS

* [`3a159d27e`](https://github.com/npm/cli/commit/3a159d27e976933098ec18fa9c3e474c85b5b332)
  [#2681](https://github.com/npm/cli/issues/2681)
  fix(tests): rewrite doctor tests
  ([@ljharb](https://github.com/ljharb))
* [`abcc96a20`](https://github.com/npm/cli/commit/abcc96a204ed581fc7cd603f47cdca0afe299530)
  [#2682](https://github.com/npm/cli/issues/2682)
  [tests] separate tests from linting and license validation
  ([@ljharb](https://github.com/ljharb))

### DOCUMENTATION

* [`7e1e84181`](https://github.com/npm/cli/commit/7e1e84181ccaca8a8b499a21b1aa7d731a14d5b7)
  [#2662](https://github.com/npm/cli/issues/2662)
  fix(docs): fix angle brackets in npm diff docs
  ([@ethomson](https://github.com/ethomson))

## v7.5.3 (2021-02-08)

### BUG FIXES

* [`df596bf4c`](https://github.com/npm/cli/commit/df596bf4c10d6917672579cc38800f5e846002bc)
  fix(publish): follow all configs for registry auth check
  [#2602](https://github.com/npm/cli/issues/2602)
  ([@wraithgar](https://github.com/wraithgar))
* [`6d7afb03c`](https://github.com/npm/cli/commit/6d7afb03cd7602b60e709516711a2f94cd61ff25)
  [#2613](https://github.com/npm/cli/issues/2613)
  install script: pass -q to curl calls to disable user .curlrc files
  ([@nlf](https://github.com/nlf))

### DEPENDENCIES

* [`3294fed6f`](https://github.com/npm/cli/commit/3294fed6f18626516978c21fac5f28ecfdb58124)
  `pacote@11.2.5`
    * prevent infinite recursion in git dep preparation
* [`0f7a3a87c`](https://github.com/npm/cli/commit/0f7a3a87c12e30bdd2cdab596ca6511de787c969)
  `read-package-json-fast@2.0.1`
    * avoid duplicating optionalDependencies as dependencies in package.json
* [`6f46b0f7f`](https://github.com/npm/cli/commit/6f46b0f7fef9891e6de4af3547c70a67cb3a7a13)
  `init-package-json@2.0.2`
* [`df4f65acc`](https://github.com/npm/cli/commit/df4f65acc4ceaf15db4c227670e80f94584c055c)
  `@npmcli/arborist@2.2.0`
* [`7038c2ff4`](https://github.com/npm/cli/commit/7038c2ff49022f8babd495d1b831b5c82d6aed05)
  `@npmcli/run-script@1.8.2`
* [`54cd4c87a`](https://github.com/npm/cli/commit/54cd4c87a71c9381519d8ac52e306096066dc92e)
  `libnpmversion@1.0.8`
* [`9ab36aae4`](https://github.com/npm/cli/commit/9ab36aae429784df754211d5f086a515012b9bdd)
  `graceful-fs@4.2.5`
* [`e1822cf27`](https://github.com/npm/cli/commit/e1822cf277336728f1d5696ffe0db3ea6e700d9e)
  `@npmcli/installed-package-contents@1.0.7`

## v7.5.2 (2021-02-02)

### BUG FIXES

* [`37613e4e6`](https://github.com/npm/cli/commit/37613e4e686e4891210acaabc9c23f41456eda3f)
[#2395](https://github.com/npm/cli/issues/2395)
[#2329](https://github.com/npm/cli/issues/2329)
fix(exec): use latest version when possible
([@wraithgar](https://github.com/wraithgar))
* [`567c9bd03`](https://github.com/npm/cli/commit/567c9bd03a7669111fbba6eb6d1f12ed7cad5a1b)
fix(lib/npm): do not clobber config.execPath
([@wraithgar](https://github.com/wraithgar))

### DEPENDENCIES

* [`643709706`](https://github.com/npm/cli/commit/64370970653af5c8d7a2be2c2144e355aa6431b0)
`@npmcli/config@1.2.9` ([@isaacs](https://github.com/isaacs))
  * [`4c6be4a`](https://github.com/npm/config/commit/4c6be4a66a3e89ae607e08172b8543b588a95fb5) Restore npm v6 behavior with `INIT_CWD`
  * [`bbebc66`](https://github.com/npm/config/commit/bbebc668888f71dba57959682364b6ff26ff4fac) Do not set the `PREFIX` environment variable

## v7.5.1 (2021-02-01

### BUG FIXES

* [`0ea134e41`](https://github.com/npm/cli/commit/0ea134e4190f322138299c51672eab5387ec41bb)
  [#2587](https://github.com/npm/cli/issues/2587)
  pass all settings through to pacote.packument, fixes #2060
  ([@nlf](https://github.com/nlf))
* [`8c5ca2f51`](https://github.com/npm/cli/commit/8c5ca2f516f5ac87f3bbd7f1fd95c0b283a21f14)
  Add test for npm-usage.js, and fix 'npm --long' output
  ([@isaacs](https://github.com/isaacs))

### DEPENDENCIES

* [`7e4e88e93`](https://github.com/npm/cli/commit/7e4e88e938323e34a2a41176472d8e43e84bd4dd)
  `@npmcli/arborist@2.1.1`, `pacote@11.2.4`
  * Properly raise ERESOLVE errors on root dev dependencies
  * Ignore ERESOLVE errors when performing git dep 'prepare' scripts
  * Always reinstall packages that are explicitly requested
  * fix global update all so it actually updates things
  * Install bins properly when global root is a link
  ([@isaacs](https://github.com/isaacs))

### DOCUMENTATION

* [`23dac2fef`](https://github.com/npm/cli/commit/23dac2feff1d02193791c7e39d9e93bc9bf8e624)
  [#2557](https://github.com/npm/cli/issues/2557)
  npm team revamp
  ([@ruyadorno](https://github.com/ruyadorno))
* [`dd05ba0c0`](https://github.com/npm/cli/commit/dd05ba0c0b2f4c70eb8558c0ecc54889efbe98f5)
  [#2572](https://github.com/npm/cli/issues/2572)
  add note about `--force` overriding peer dependencies
  ([@isaacs](https://github.com/isaacs))
* [`e27639780`](https://github.com/npm/cli/commit/e276397809aceb01cc468e02a83bc6f2265376d9)
  [#2584](https://github.com/npm/cli/issues/2584)
  Fixed the spelling of contributor as it was written as conributor
  ([@pavanbellamkonda](https://github.com/pavanbellamkonda))
* [`13a5e3178`](https://github.com/npm/cli/commit/13a5e31781cdaa37d3f007e1c8583c7cb591c62a)
  [#2502](https://github.com/npm/cli/issues/2502)
  elaborate that npm help uses browser
  ([@ariccio](https://github.com/ariccio))

## v7.5.0 (2021-01-28)

### FEATURES

* [`d011266b7`](https://github.com/npm/cli/commit/d011266b733367aad283ccbfb9d2b19442c3405f)
  [#1319](https://github.com/npm/cli/issues/1319)
  add npm diff command
  ([@ruyadorno](https://github.com/ruyadorno))

### BUG FIXES

* [`d2f8af2da`](https://github.com/npm/cli/commit/d2f8af2da64d510d3d363aec10531bebf840d84e)
  [#2445](https://github.com/npm/cli/issues/2445)
  publish: don't complain about missing auth until after registry is chosen
  ([@dr-js](https://github.com/dr-js))

### DOCUMENTATION

* [`8d3fd63aa`](https://github.com/npm/cli/commit/8d3fd63aaa6a5c9b3d2281dd0bd9e1c270b35941)
  [#2559](https://github.com/npm/cli/issues/2559)
  updates to readme, removal, contributing and several other docs
  ([@darcyclarke](https://github.com/darcyclarke))
* [`7772d9f9f`](https://github.com/npm/cli/commit/7772d9f9f9f853573a7ff8e7fb60c5e46566f596)
  [#2542](https://github.com/npm/cli/issues/2542)
  fix grammar on caching docs for search, exec and init
  ([@wraithgar](https://github.com/wraithgar))
* [`52e8a1aef`](https://github.com/npm/cli/commit/52e8a1aef4aab3f378c20276a9109bb3f00eccd5)
  [#2558](https://github.com/npm/cli/issues/2558)
  refreshed npm updated docs
  ([@ruyadorno](https://github.com/ruyadorno))
* [`abae00ca0`](https://github.com/npm/cli/commit/abae00ca05925e521696dd12480853509aab6c0a)
  [#2565](https://github.com/npm/cli/issues/2565)
  update npm command docs
  ([@wraithgar](https://github.com/wraithgar))
* [`9351cbf9a`](https://github.com/npm/cli/commit/9351cbf9afd2310c56b9953c005505ea5126a5d4)
  [#2566](https://github.com/npm/cli/issues/2566)
  refresh npm run-script docs
  ([@ruyadorno](https://github.com/ruyadorno))

### DEPENDENCIES

* [`56c08863e`](https://github.com/npm/cli/commit/56c08863e15cb9cf8662b99ddc627cfcdff0348d)
  `hosted-git-info@3.0.8`
* [`18a93f06b`](https://github.com/npm/cli/commit/18a93f06b632be051b9455e32a85e4e75066f52c)
  `ssri@8.0.1`
* [`cb768f671`](https://github.com/npm/cli/commit/cb768f671c4d8d5a09d9a6c5a74227d300e81104)
  `@npmcli/move-file@1.1.1`
* [`32cc0a4be`](https://github.com/npm/cli/commit/32cc0a4be76465093e3d0f314215a0ec46dc03c6)
  `minipass-fetch@1.3.3`
    * fixes ssl settings passthrough
* [`530997968`](https://github.com/npm/cli/commit/530997968fbbd9e8bf016689b1d192daa812b4de)
  `@npmcli/arborist@2.1.0`
    * added signal handler to rollback when possible
    * prevent ERESOLVEs caused by loose root dep specs
    * detect conflicts among nested peerOptional deps
    * properly buildIdealTree when root is a symlink

## v7.4.3 (2021-01-21)

### DOCUMENTATION

* [`ec1f06d06`](https://github.com/npm/cli/commit/ec1f06d06447a29c74bee063cff103ede7a2111b)
  [#2498](https://github.com/npm/cli/issues/2498)
  docs(npm): update `npm` docs
  ([@darcyclarke](https://github.com/darcyclarke))

### DEPENDENCIES
* [`bc23284cd`](https://github.com/npm/cli/commit/bc23284cd5c4cc4532875aff14df94213727a509)
  [#2511](https://github.com/npm/cli/issues/2511)
  remove coverage files
  ([@ruyadorno](https://github.com/ruyadorno))
* [`fcbc676b8`](https://github.com/npm/cli/commit/fcbc676b88e1b7c8d01a3799683cd388a82c44d6)
  `pacote@11.2.3`
* [`ebd3a24ff`](https://github.com/npm/cli/commit/ebd3a24ff8381f2def306136b745d1615fd6139f)
  `@npmcli/arborist@2.0.6`
    * Preserve git+https auth when provided

## v7.4.2 (2021-01-15)

### DEPENDENCIES

* [`e5ce6bbba`](https://github.com/npm/cli/commit/e5ce6bbbad82b85c8e74a4558503513e4f337476)
  * `@npmcli/arborist@2.0.5`
    * fix creating missing dirs when using --prefix and --global
    * fix omit types of deps in global installs
    * fix prioritizing npm-shrinkwrap.json over package-lock.json
    * better cache system for packuments
    * improves audit performance

## v7.4.1 (2021-01-14)

### BUG FIXES

* [`23df96d33`](https://github.com/npm/cli/commit/23df96d3394ba0b69a37f416d7f0c26bb9354975)
  [#2486](https://github.com/npm/cli/issues/2486)
  npm link no longer deletes entire project when global prefix is a symlink
  ([@nlf](https://github.com/nlf))

### DOCUMENTATION

* [`7dd0dfc59`](https://github.com/npm/cli/commit/7dd0dfc59c861e7d3e30a86a8e6db10872fc6b44)
  [#2459](https://github.com/npm/cli/issues/2459)
  fix(docs): clean up `npm start` docs
  ([@wraithgar](https://github.com/wraithgar))
* [`307b3bd9f`](https://github.com/npm/cli/commit/307b3bd9f90e96fcc8805a1d5ddec80787a3d3a7)
  [#2460](https://github.com/npm/cli/issues/2460)
  fix(docs): clean up `npm stop` docs
  ([@wraithgar](https://github.com/wraithgar))
* [`23f01b739`](https://github.com/npm/cli/commit/23f01b739d7a01a7dc3672322e14eb76ff33d712)
  [#2462](https://github.com/npm/cli/issues/2462)
  fix(docs): clean up `npm test` docs
  ([@wraithgar](https://github.com/wraithgar))
* [`4b43656fc`](https://github.com/npm/cli/commit/4b43656fc608783a29ccf8495dc305459abc5cc7)
  [#2463](https://github.com/npm/cli/issues/2463)
  fix(docs): clean up `npm prefix` docs
  ([@wraithgar](https://github.com/wraithgar))
* [`1135539ba`](https://github.com/npm/cli/commit/1135539bac9f98bb1a5d5ed05227a8ecd19493d3)
  [`a07bb8e69`](https://github.com/npm/cli/commit/a07bb8e692a85b55d51850534c09fa58224c2285)
  [`9b55b798e`](https://github.com/npm/cli/commit/9b55b798ed8f2b9be7b3199a1bfc23b1cd89c4cd)
  [`cd5eeaaa0`](https://github.com/npm/cli/commit/cd5eeaaa08eabb505b65747a428c3c59159663dc)
  [`6df69ce10`](https://github.com/npm/cli/commit/6df69ce107912f8429665eb851825d2acebc8575)
  [`dc6b2a8b0`](https://github.com/npm/cli/commit/dc6b2a8b032d118be3566ce0fa7c67c171c8d2cb)
  [`a3c127446`](https://github.com/npm/cli/commit/a3c1274460e16d1edbdca6a0cee86ef313fdd961)
  [#2464](https://github.com/npm/cli/issues/2464)
  fix(docs): clean up `npm uninstall` docs
  ([@wraithgar](https://github.com/wraithgar))
* [`cfdcf32fd`](https://github.com/npm/cli/commit/cfdcf32fd7628501712b8cad4a541c6b8e7b66bc)
  [#2474](https://github.com/npm/cli/issues/2474)
  fix(docs): clean up `npm unpublish` docs
  ([@wraithgar](https://github.com/wraithgar))
* [`acd5b062a`](https://github.com/npm/cli/commit/acd5b062a811fcd98849df908ce26855823ca671)
  [#2475](https://github.com/npm/cli/issues/2475)
  fix(docs): update `package-lock.json` docs
  ([@isaacs](https://github.com/isaacs))
* [`b0b0edf6d`](https://github.com/npm/cli/commit/b0b0edf6de1678de7f4a000700c88daa5f7194ef)
  [#2482](https://github.com/npm/cli/issues/2482)
  fix(docs): clean up `npm token` docs
  ([@wraithgar](https://github.com/wraithgar))
* [`35559201a`](https://github.com/npm/cli/commit/35559201a4a0a5b111ce58d6824e5b4030eb4496)
  [#2487](https://github.com/npm/cli/issues/2487)
  fix(docs): clean up `npm search` docs
  ([@wraithgar](https://github.com/wraithgar))

### DEPENDENCIES

* [`ea8c02169`](https://github.com/npm/cli/commit/ea8c02169cfbf0484d67db7c0e7a6ec8aecb7210)
  `@npmcli/arborist@2.0.5`
* [`fb6f2c313`](https://github.com/npm/cli/commit/fb6f2c313d1d9770cc7d02a3900c7945df3cb661)
  `pacote@11.2.1`
* [`c549b7657`](https://github.com/npm/cli/commit/c549b76573b1835a63e1e5898e9c16860079d84e)
  `make-fetch-happen@8.0.13`

## v7.4.0 (2021-01-07)

### FEATURES

* [`47ed2dfd8`](https://github.com/npm/cli/commit/47ed2dfd865566643bc1d39e8a4f98d2e1add99a)
  [#2456](https://github.com/npm/cli/issues/2456) add
  `--foreground-scripts` option ([@isaacs](https://github.com/isaacs))

### BUG FIXES

* [`d01746a5a`](https://github.com/npm/cli/commit/d01746a5a6dde115ee6a600cdf54c9b35afcab3f)
  [#2444](https://github.com/npm/cli/issues/2444)
  [#1103](https://github.com/npm/cli/issues/1103) Remove deprecated
  `process.umask()` ([@isaacs](https://github.com/isaacs))
* [`b2e2edf8a`](https://github.com/npm/cli/commit/b2e2edf8aee57347c96a61209c7a10139a0cc85a)
  [#2422](https://github.com/npm/cli/issues/2422) npm publish --dry-run
  should not check login status ([@buyan302](https://github.com/buyan302))
* [`99156df80`](https://github.com/npm/cli/commit/99156df8099f55bc69dfa99d7ddcf8d1d569016e)
  [#2448](https://github.com/npm/cli/issues/2448)
  [#2425](https://github.com/npm/cli/issues/2425) pass extra arguments
  directly to run-script as an array ([@nlf](https://github.com/nlf))
* [`907b34b2e`](https://github.com/npm/cli/commit/907b34b2ecc34ac376d989f824f7492064e43ef4)
  [#2455](https://github.com/npm/cli/issues/2455) fix(ci): pay attention to
  --ignore-scripts ([@wraithgar](https://github.com/wraithgar))

### DEPENDENCIES

* [`7a49fd4af`](https://github.com/npm/cli/commit/7a49fd4afc8cd24db40aee008031ea648583d0bc)
  `tar@6.1.0`, `pacote@11.1.14`
* [`54a7bd16c`](https://github.com/npm/cli/commit/54a7bd16c130525ade71ec9894af71c2825d8584)
  `@npmcli/arborist@2.0.3`

### DOCUMENTATION

* [`a390d7456`](https://github.com/npm/cli/commit/a390d74561b72f0b13cba65844ce60c379198087)
  [#2440](https://github.com/npm/cli/issues/2440) Updated the url for RFC
  19 so that it isn't a 404.
  ([@therealjeffg](https://github.com/therealjeffg))
* [`e02b46ad7`](https://github.com/npm/cli/commit/e02b46ad7acdeb9fbb63f782e546c2f8db94ae6e)
  [#2436](https://github.com/npm/cli/issues/2436) Grammatical Fix in npm-ls
  Documentation 'Therefore' is spelled 'Therefor'
  ([@marsonya](https://github.com/marsonya))
* [`0fed44dea`](https://github.com/npm/cli/commit/0fed44dea12f125b639b5e3575adcea74a86d3a0)
  [#2417](https://github.com/npm/cli/issues/2417) Fix npm bug reporting url
  ([@AkiaCode](https://github.com/AkiaCode))

## 7.3.0 (2020-12-18)

### FEATURES

* [`a9b8bf263`](https://github.com/npm/cli/commit/a9b8bf2634c627fbb16ca3a6bb2c2f1058c3e586)
  [#2362](https://github.com/npm/cli/issues/2362)
  Support multiple set/get/deletes in npm config
  ([@isaacs](https://github.com/isaacs))

### BUG FIXES

* [`9eef63849`](https://github.com/npm/cli/commit/9eef638499c88689acb00d812c10f0407cb95c08)
  Pass full set of options to login helper functions.
  This fixes `npm login --no-strict-ssl`, as well as a host of other
  options that one might want to set while logging in.
  Reported by: [@toddself](https://github.com/toddself)
  ([@isaacs](https://github.com/isaacs))
* [`628a554bc`](https://github.com/npm/cli/commit/628a554bc113e4e115d34778bfe8a77cfad1d933)
  [#2358](https://github.com/npm/cli/issues/2358)
  fix doctor test to work correctly for node pre-release versions
  ([@nlf](https://github.com/nlf))
* [`be4a0900b`](https://github.com/npm/cli/commit/be4a0900b14b2c6315bf62bed8f5affb648215ae)
  [#2360](https://github.com/npm/cli/issues/2360)
  raise an error early if publishing without login, registry
  ([@isaacs](https://github.com/isaacs))
* [`44d433105`](https://github.com/npm/cli/commit/44d4331058c53909ada62470b23b2185102b2128)
  [#2366](https://github.com/npm/cli/issues/2366)
  Include prerelease versions when deprecating
  ([@tiegz](https://github.com/tiegz))
* [`cba3341da`](https://github.com/npm/cli/commit/cba3341dae4c92541049dc976e82e2ba19566e95)
  [#2373](https://github.com/npm/cli/issues/2373)
  npm profile refactor
  ([@ruyadorno](https://github.com/ruyadorno))
* [`7539504e3`](https://github.com/npm/cli/commit/7539504e3abdec28039a7798e5ccb745b536cb6e)
  [#2382](https://github.com/npm/cli/issues/2382)
  remove the metrics sender
  ([@nlf](https://github.com/nlf))

### DOCS

* [`b98569a8c`](https://github.com/npm/cli/commit/b98569a8ca28dbd611fe84492aee996e2e567b55)
  add note about `INIT_CWD` to run-script doc
* [`292929279`](https://github.com/npm/cli/commit/292929279854a06ca60ff737b574cbd6503ec5db)
  [#2368](https://github.com/npm/cli/issues/2368)
  Revert bug-reporting links to GH.
  Re: <https://blog.npmjs.org/post/188841555980/updates-to-community-docs-more>
  ([@tiegz](https://github.com/tiegz))
* [`f4560626f`](https://github.com/npm/cli/commit/f4560626f09dba4889d752f7f739aa5a5f3da741)
  update `ISSUE_TEMPLATE` with modern links
  ([@isaacs](https://github.com/isaacs))
* [`bc1c567ed`](https://github.com/npm/cli/commit/bc1c567ed3d853ed4f01d33a800eb453956de6ef)
  update npm command doc feature request links
  ([@isaacs](https://github.com/isaacs))
* [`0ad958fe1`](https://github.com/npm/cli/commit/0ad958fe1cb811699caca235f361c8328baac8c4)
  [#2381](https://github.com/npm/cli/issues/2381)
  (docs,test): assorted typo fixes
  ([@XhmikosR](https://github.com/XhmikosR))

### TESTING

* [`a92d310b7`](https://github.com/npm/cli/commit/a92d310b7e9e4c48b08f52785c2e3a6d52a82ad7)
  [#2361](https://github.com/npm/cli/issues/2361)
  Add max-len to lint rules
  ([@Edu93Jer](https://github.com/Edu93Jer))

### DEPENDENCIES

* [`4fc2f3e05`](https://github.com/npm/cli/commit/4fc2f3e05b600aa64fe5eb6b8b77bc070e5a9403)
  [#2300](https://github.com/npm/cli/issues/2300)
  `@npmcli/config@1.2.8`:
    * Support setting email without username/password

## 7.2.0 (2020-12-15)

### FEATURES

* [`a9c4b158c`](https://github.com/npm/cli/commit/a9c4b158c46dd0d0c8d8744a97750ffd0c30cc09)
  [#2342](https://github.com/npm/cli/issues/2342)
  allow npm rebuild to accept a path to a module
  ([@nlf](https://github.com/nlf))

### DEPENDENCIES

* [`beb371800`](https://github.com/npm/cli/commit/beb371800292140bf3882253c447168a378bc154)
  [#2334](https://github.com/npm/cli/issues/2334)
  remove unused top level dep tough-cookie
  ([@darcyclarke](https://github.com/darcyclarke))
* [`d45e181d1`](https://github.com/npm/cli/commit/d45e181d17dd88d82b3a97f8d9cd5fa5b6230e48)
  [#2335](https://github.com/npm/cli/issues/2335)
  `ini@2.0.0`, `@npmcli/config@1.2.7`
  ([@isaacs](https://github.com/isaacs))
* [`ef4b18b5a`](https://github.com/npm/cli/commit/ef4b18b5a70381b264d234817cff32eeb6848a73)
  [#2309](https://github.com/npm/cli/issues/2309)
  `@npmcli/arborist@2.0.2`
    * properly remove deps when no lockfile and package.json is present
* [`c6c013e6e`](https://github.com/npm/cli/commit/c6c013e6ebc4fe036695db1fd491eb68f3b57c68)
  `readdir-scoped-modules@1.1.0`
* [`a1a2134aa`](https://github.com/npm/cli/commit/a1a2134aa9a1092493db6d6c9a729ff5203f0dd4)
  remove unused sorted-object dep
  ([@nlf](https://github.com/nlf))
* [`85c2a2d31`](https://github.com/npm/cli/commit/85c2a2d318ae066fb2c161174f5aea97e18bc9c5)
  [#2344](https://github.com/npm/cli/issues/2344)
  remove editor dependency
  ([@nlf](https://github.com/nlf))

### TESTING

* [`3a6dd511c`](https://github.com/npm/cli/commit/3a6dd511c944c5f2699825a99bba1dde333a45ef)
  npm edit
  ([@nlf](https://github.com/nlf))
* [`3ba5de4e7`](https://github.com/npm/cli/commit/3ba5de4e7f6c5c0f995a29844926d6ed2833addd)
  [#2347](https://github.com/npm/cli/issues/2347)
  npm help-search
  ([@nlf](https://github.com/nlf))
* [`6caf19f49`](https://github.com/npm/cli/commit/6caf19f491e144be3e2a1a50f492dad48b01f361)
  [#2348](https://github.com/npm/cli/issues/2348)
  npm help
  ([@nlf](https://github.com/nlf))
* [`cb5847e32`](https://github.com/npm/cli/commit/cb5847e3203c52062485b5de68e4f6d29b33c361)
  [#2349](https://github.com/npm/cli/issues/2349)
  npm hook
  ([@nlf](https://github.com/nlf))
* [`996a2f6b1`](https://github.com/npm/cli/commit/996a2f6b130d6678998a2f6a5ec97d75534d5f66)
  [#2353](https://github.com/npm/cli/issues/2353)
  npm org
  ([@nlf](https://github.com/nlf))
* [`8c67c38a4`](https://github.com/npm/cli/commit/8c67c38a4f476ff5be938db6b6b3ee9ac6b44db5)
  [#2354](https://github.com/npm/cli/issues/2354)
  npm set
  ([@nlf](https://github.com/nlf))

## 7.1.2 (2020-12-11)

### DEPENDENCIES

* [`c3ba1daf7`](https://github.com/npm/cli/commit/c3ba1daf7cd335d72aeba80ae0e9f9d215ca9ea5)
  [#2033](https://github.com/npm/cli/issues/2033) `@npmcli/config@1.2.6`:
    * Set `INIT_CWD` to initial current working directory
    * Set `NODE` to initial process.execPath
* [`8029608b9`](https://github.com/npm/cli/commit/8029608b914fe5ba35a7cd37ae95ab93b0532e2e)
  `json-parse-even-better-errors@2.3.1`
* [`0233818e6`](https://github.com/npm/cli/commit/0233818e606888b80881b17a2c6aca9f10a619b2)
  [#2332](https://github.com/npm/cli/issues/2332) `treeverse@1.0.4`
* [`e401d6bb3`](https://github.com/npm/cli/commit/e401d6bb37ffc767b4fefe89878dd3c3ef490b2c)
  `ini@1.3.8`
* [`011bb1220`](https://github.com/npm/cli/commit/011bb122035dcd43769ec35982662cca41635068)
  [#2320](https://github.com/npm/cli/issues/2320) `@npmcli/arborist@2.0.1`:
    * Do not save with `^` and no version

### BUGFIXES

* [`244c2069f`](https://github.com/npm/cli/commit/244c2069fd093f053d3061c85575ac13e72e2454)
  [#2325](https://github.com/npm/cli/issues/2325) npm search
  include/exclude ([@ruyadorno](https://github.com/ruyadorno))
* [`d825e901e`](https://github.com/npm/cli/commit/d825e901eceea4cf8d860e35238dc30008eb4da4)
  [#1905](https://github.com/npm/cli/issues/1905)
  [#2316](https://github.com/npm/cli/issues/2316) run install scripts for
  root project
* [`315449142`](https://github.com/npm/cli/commit/31544914294948085a84097af7f0f5de2a2e8f7e)
  [#2331](https://github.com/npm/cli/issues/2331)
  [#2021](https://github.com/npm/cli/issues/2021) Set `NODE_ENV=production`
  if 'dev' is on the omit list ([@isaacs](https://github.com/isaacs))

### TESTING

* [`c243e3b9d`](https://github.com/npm/cli/commit/c243e3b9d9bda0580a0fc1b3e230b4d47412176e)
  [#2313](https://github.com/npm/cli/issues/2313) tests: completion
  ([@nlf](https://github.com/nlf))
* [`7ff6efbb8`](https://github.com/npm/cli/commit/7ff6efbb866591b2330b967215cef8146dff3ebf)
  [#2314](https://github.com/npm/cli/issues/2314) npm team
  ([@ruyadorno](https://github.com/ruyadorno))
* [`7a4f0c96c`](https://github.com/npm/cli/commit/7a4f0c96c2ab9f264f7bda2caf7e72c881571270)
  [#2323](https://github.com/npm/cli/issues/2323) npm doctor
  ([@nlf](https://github.com/nlf))

### DOCUMENTATION

* [`e340cf64b`](https://github.com/npm/cli/commit/e340cf64ba31ef329a9049b60c32ffd0342cfb7d)
  [#2330](https://github.com/npm/cli/issues/2330) explain through
  run-script ([@isaacs](https://github.com/isaacs))

## 7.1.1 (2020-12-08)

### DEPENDENCIES

* [`bf09e719c`](https://github.com/npm/cli/commit/bf09e719c7f563a255b1e9af6b1237ebc5598db6)
  `@npmcli/arborist@2.0.0`
    * Much stricter tree integrity guarantees
    * Fix issues where the root project is a symlink, or linked as a
      workspace
* [`7ceb5b728`](https://github.com/npm/cli/commit/7ceb5b728b9f326c567f5ffe5831c9eccf013aa0)
  `ini@1.3.6`
* [`77c6ced2a`](https://github.com/npm/cli/commit/77c6ced2a6daaadbff715c8f05b2e61ba76e9bab)
  `make-fetch-happen@8.0.11`
    * Avoid caching headers that are hazardous or unnecessary to leave
      lying around (authorization, npm-session, etc.)
    * [#38](https://github.com/npm/make-fetch-happen/pull/38) Include query
      string in cache key ([@jpb](https://github.com/jpb))
* [`0ef25b6cd`](https://github.com/npm/cli/commit/0ef25b6cd2921794d36f066e2b11c406342cf167)
  `libnpmsearch@3.1.0`:
    * Update to accept query params as options, so we can paginate.
      ([@nlf](https://github.com/nlf))
* [`518a66450`](https://github.com/npm/cli/commit/518a664500bcde30475788e8c1c3e651f23e881b)
  `@npmcli/config@1.2.4`:
    * Do not allow path options to be set to a boolean `false` value
* [`3d7aff9d8`](https://github.com/npm/cli/commit/3d7aff9d8dd1cf29956aa306464cd44fbc2af426)
  update all dependencies using latest npm to install them

### TESTS

* [`2848f5940`](https://github.com/npm/cli/commit/2848f594034b87939bfc5546e3e603f123d98a01)
  [npm/statusboard#173](https://github.com/npm/statusboard/issues/173)
  [#2293](https://github.com/npm/cli/issues/2293) npm shrinkwrap
  ([@ruyadorno](https://github.com/ruyadorno))
* [`f6824459a`](https://github.com/npm/cli/commit/f6824459ae0c86e2fa9c84b3dcec85f572ae8e1b)
  [#2302](https://github.com/npm/cli/issues/2302) npm deprecate
  ([@nlf](https://github.com/nlf))
* [`b7d74b627`](https://github.com/npm/cli/commit/b7d74b627859f08fca23209d6e0d3ec6657a4489)
  [npm/statusboard#180](https://github.com/npm/statusboard/issues/180)
  [#2304](https://github.com/npm/cli/issues/2304) npm unpublish
  ([@ruyadorno](https://github.com/ruyadorno))

### FEATURES

* [`3db90d944`](https://github.com/npm/cli/commit/3db90d94474f673591811fdab5eb6a5bfdeba261)
  [#2303](https://github.com/npm/cli/issues/2303) allow for passing object
  keys to searchopts to allow pagination ([@nlf](https://github.com/nlf))

## 7.1.0 (2020-12-04)

### FEATURES

* [`6b1575110`](https://github.com/npm/cli/commit/6b15751106beb99234aa4bf39ae05cf40076d42a)
  [#2237](https://github.com/npm/cli/pull/2237)
  add `npm set-script` command
  ([@Yash-Singh1](https://github.com/Yash-Singh1))
* [`15d7333f8`](https://github.com/npm/cli/commit/15d7333f832e3d68ae16895569f27a27ef86573e)
  add interactive `npm exec`
  ([@isaacs](https://github.com/isaacs))

### BUG FIXES

* [`2a1192e4b`](https://github.com/npm/cli/commit/2a1192e4b03acdf6e6e24e58de68f736ab9bb35f)
  [#2202](https://github.com/npm/cli/pull/2202)
  Do not run interactive `npm exec` in CI when a TTY
  ([@isaacs](https://github.com/isaacs))

### DOCUMENTATION

* [`0599cc37d`](https://github.com/npm/cli/commit/0599cc37df453bf79d47490eb4fca3cd63f67f80)
  [#2271](https://github.com/npm/cli/pull/2271)
  don't wrap code block
  ([@ethomson](https://github.com/ethomson))

### DEPENDENCIES

* [`def85c726`](https://github.com/npm/cli/commit/def85c72640ffe2d27977c56b7aa06c6f6346ca9)
  `@npmcli/arborist@1.0.14`
    * fixes running `npm exec` from file system root folder
* [`4c94673ab`](https://github.com/npm/cli/commit/4c94673ab5399d27e5a48e52f7a65b038a456265)
  `semver@7.3.4`

## 7.0.15 (2020-11-27)

### DEPENDENCIES

* [`00e6028ef`](https://github.com/npm/cli/commit/00e6028ef83bf76eaae10241fd7ba59e39768603)
  `@npmcli/arborist@1.0.13`
    * do not override user-defined shorthand values when saving `package.json`

### BUG FIXES

* [`9c3413fbc`](https://github.com/npm/cli/commit/9c3413fbcb37e79fc0b3d980e0b5810d7961277c)
  [#2034](https://github.com/npm/cli/issues/2034)
  [#2245](https://github.com/npm/cli/issues/2245)
  `npm link <pkg>` should not save `package.json`
  ([@ruyadorno](https://github.com/ruyadorno))

### DOCUMENTATION

* [`1875347f9`](https://github.com/npm/cli/commit/1875347f9f4f2b50c28fe8857c5533eeebf42da2)
  [#2196](https://github.com/npm/cli/issues/2196)
  remove doc on obsolete `unsafe-perm` flag
  ([@kaizhu256](https://github.com/kaizhu256))
* [`f51e50603`](https://github.com/npm/cli/commit/f51e5060340c783a8a00dadd98e5786960caf43f)
  [#2200](https://github.com/npm/cli/issues/2200)
  `config.md` cleanup
  ([@alexwoollam](https://github.com/alexwoollam))
* [`997cbdb40`](https://github.com/npm/cli/commit/997cbdb400bcd22e457e8a06b69a7be697cfd66d)
  [#2238](https://github.com/npm/cli/issues/2238)
  Fix broken link to `package.json` documentation
  ([@d-fischer](https://github.com/d-fischer))
* [`9da972dc4`](https://github.com/npm/cli/commit/9da972dc44c21cf0e337f1c3fca44eb9df3e40d5)
  [#2241](https://github.com/npm/cli/issues/2241)
  `npm star` docs cleanup
  ([@ruyadorno](https://github.com/ruyadorno))

## 7.0.14 (2020-11-23)

### DEPENDENCIES
* [`09d21ab90`](https://github.com/npm/cli/commit/09d21ab903dcfebdfd446b8b29ad46c425b6510e)
  `@npmcli/run-script@1.8.1`
    - fix a regression in how scripts are escaped

## 7.0.13 (2020-11-20)

### BUG FIXES
* [`5fc56b6db`](https://github.com/npm/cli/commit/5fc56b6dbcc7d7d1463a761abb67d2fc16ad3657)
  [npm/statusboard#174](https://github.com/npm/statusboard/issues/174)
  [#2204](https://github.com/npm/cli/issues/2204)
  fix npm unstar command
  ([@ruyadorno](https://github.com/ruyadorno))
* [`7842b4d4d`](https://github.com/npm/cli/commit/7842b4d4dca1e076b0d26d554f9dce67484cd7be)
  [npm/statusboard#182](https://github.com/npm/statusboard/issues/182)
  [#2205](https://github.com/npm/cli/issues/2205)
  fix npm version usage output
  ([@ruyadorno](https://github.com/ruyadorno))
* [`a0adbf9f8`](https://github.com/npm/cli/commit/a0adbf9f8f77531fcf81ae31bbc7102698765ee3)
  [#2206](https://github.com/npm/cli/issues/2206)
  [#2213](https://github.com/npm/cli/issues/2213)
  fix: fix flatOptions usage in npm init
  ([@ruyadorno](https://github.com/ruyadorno))

### DEPENDENCIES

* [`3daaf000a`](https://github.com/npm/cli/commit/3daaf000aee0ba81af855977d7011850e79099e6)
  `@npmcli/arborist@1.0.12`
    - fixes some windows specific bugs in how paths are handled and compared

### DOCUMENTATION

* [`084a7b6ad`](https://github.com/npm/cli/commit/084a7b6ad6eaf9f2d92eb05da93e745f5357cce2)
  [#2210](https://github.com/npm/cli/issues/2210)
  docs: Fix typo
  ([@HollowMan6](https://github.com/HollowMan6))

## 7.0.12 (2020-11-17)

### BUG FIXES

* [`7b89576bd`](https://github.com/npm/cli/commit/7b89576bd1fa557a312a841afa66b895558d1b12)
  [#2174](https://github.com/npm/cli/issues/2174)
  fix running empty scripts with `npm run-script`
  ([@nlf](https://github.com/nlf))
* [`bc9afb195`](https://github.com/npm/cli/commit/bc9afb195f5aad7c06bc96049c0f00dc8e752dee)
  [#2002](https://github.com/npm/cli/issues/2002)
  [#2184](https://github.com/npm/cli/issues/2184)
  Preserve builtin conf when installing npm globally
  ([@isaacs](https://github.com/isaacs))

### DEPENDENCIES

* [`b74c05d88`](https://github.com/npm/cli/commit/b74c05d88dc48fabef031ea66ffaa4e548845655)
  `@npmcli/run-script@1.8.0`
    * fix windows command-line argument escaping

### DOCUMENTATION

* [`4e522fdc9`](https://github.com/npm/cli/commit/4e522fdc917bc85af2ca8ff7669a0178e2f35123)
  [#2179](https://github.com/npm/cli/issues/2179)
  remove mention to --parseable option from `npm audit` docs
  ([@Primajin](https://github.com/Primajin))

## 7.0.11 (2020-11-13)

### DEPENDENCIES

* [`629a667a9`](https://github.com/npm/cli/commit/629a667a9b30b0b870075da965606979622a5e2e)
  `eslint@7.13.0`
* [`de9891bd2`](https://github.com/npm/cli/commit/de9891bd2a16fe890ff5cfb140c7b1209aeac0de)
  `eslint-plugin-standard@4.1.0`
* [`c3e7aa31c`](https://github.com/npm/cli/commit/c3e7aa31c565dfe21cd1f55a8433bfbcf58aa289)
  [#2123](https://github.com/npm/cli/issues/2123)
  [#1957](https://github.com/npm/cli/issues/1957)
  `@npmcli/arborist@1.0.11`

### BUG FIXES

* [`a8aa38513`](https://github.com/npm/cli/commit/a8aa38513ad5c4ad44e6bb3e1499bfc40c31e213)
  [#2134](https://github.com/npm/cli/issues/2134)
  [#2156](https://github.com/npm/cli/issues/2156)
  Fix `cannot read property length of undefined` in `ERESOLVE` explanation code
  ([@isaacs](https://github.com/isaacs))
* [`1dbf0f9bb`](https://github.com/npm/cli/commit/1dbf0f9bb26ba70f4c6d0a807701d7652c31d7d4)
  [#2150](https://github.com/npm/cli/issues/2150)
  [#2155](https://github.com/npm/cli/issues/2155)
  send json errors to stderr, not stdout
  ([@isaacs](https://github.com/isaacs))
* [`fd1d7a21b`](https://github.com/npm/cli/commit/fd1d7a21b247bb35d112c51ff8d8a06fd83c8b44)
  [#1927](https://github.com/npm/cli/issues/1927)
  [#2154](https://github.com/npm/cli/issues/2154)
  Set process.title a bit more usefully
  ([@isaacs](https://github.com/isaacs))
* [`2a80c67ef`](https://github.com/npm/cli/commit/2a80c67ef8c12c3d9d254f5be6293a6461067d99)
  [#2008](https://github.com/npm/cli/issues/2008)
  [#2153](https://github.com/npm/cli/issues/2153)
  Support legacy auth tokens for registries that use them
  ([@ruyadorno](https://github.com/ruyadorno))
* [`786e36404`](https://github.com/npm/cli/commit/786e36404068fd51657ddac766e066a98754edbf)
  [#2017](https://github.com/npm/cli/issues/2017)
  [#2159](https://github.com/npm/cli/issues/2159)
  pass all options to Arborist for `npm ci`
  ([@darcyclarke](https://github.com/darcyclarke))
* [`b47ada7d1`](https://github.com/npm/cli/commit/b47ada7d1623e9ee586ee0cf781ee3ac5ea3c223)
  [#2161](https://github.com/npm/cli/issues/2161)
  fixed typo
  ([@scarabedore](https://github.com/scarabedore))

## 7.0.10 (2020-11-10)

### DOCUMENTATION

* [`e48badb03`](https://github.com/npm/cli/commit/e48badb03058286a557584d7319db4143049cc6b)
  [#2148](https://github.com/npm/cli/issues/2148)
  Fix link in documentation
  ([@gurdiga](https://github.com/gurdiga))

### BUG FIXES

* [`8edbbdc70`](https://github.com/npm/cli/commit/8edbbdc706694fa32f52d0991c76ae9f207b7bbc)
  [#1972](https://github.com/npm/cli/issues/1972)
  Support exec auto pick bin when all bin is alias
  ([@dr-js](https://github.com/dr-js))

### DEPENDENCIES

* [`04a3e8c10`](https://github.com/npm/cli/commit/04a3e8c10c3f38e1c7a35976d77c2929bdc39868)
  [#1962](https://github.com/npm/cli/issues/1962)
  `@npmcli/arborist@1.0.10`:
    * prevent self-assignment of parent/fsParent
    * Support update options in global package space

## 7.0.9 (2020-11-06)

### BUG FIXES

* [`96a0d2802`](https://github.com/npm/cli/commit/96a0d2802d3e619c6ea47290f5c460edfe94070a)
  default the 'start' script when server.js present
  ([@isaacs](https://github.com/isaacs))
* [`7716e423e`](https://github.com/npm/cli/commit/7716e423ee92a81730c0dfe5b9ecb4bb41a3f947)
  [#2075](https://github.com/npm/cli/issues/2075)
  [#2071](https://github.com/npm/cli/issues/2071) print the registry when
  using 'npm login' ([@Wicked7000](https://github.com/Wicked7000))
* [`7046fe10c`](https://github.com/npm/cli/commit/7046fe10c5035ac57246a31ca8a6b09e3f5562bf)
  [#2122](https://github.com/npm/cli/issues/2122) tests for `npm cache`
  command ([@nlf](https://github.com/nlf))

### DEPENDENCIES

* [`74325f53b`](https://github.com/npm/cli/commit/74325f53b9d813b0e42203c037189418fad2f64a)
  [#2124](https://github.com/npm/cli/issues/2124)
  `@npmcli/run-script@1.7.5`:
    * Export the `isServerPackage` method
    * Proxy signals to and from foreground child processes
* [`0e58e6f6b`](https://github.com/npm/cli/commit/0e58e6f6b8f0cd62294642a502c17561aaf46553)
  [#1984](https://github.com/npm/cli/issues/1984)
  [#2079](https://github.com/npm/cli/issues/2079)
  [#1923](https://github.com/npm/cli/issues/1923)
  [#606](https://github.com/npm/cli/issues/606)
  [#2031](https://github.com/npm/cli/issues/2031) `@npmcli/arborist@1.0.9`:
    * Process deps for all link nodes
    * Use junctions instead of symlinks
    * Use @npmcli/move-file instead of fs.rename
* [`1dad328a1`](https://github.com/npm/cli/commit/1dad328a17d93def7799545596b4eba9833b35aa)
  [#1865](https://github.com/npm/cli/issues/1865)
  [#2106](https://github.com/npm/cli/issues/2106)
  [#2084](https://github.com/npm/cli/issues/2084) `pacote@11.1.13`:
    * Properly set the installation command for `prepare` scripts when
      installing git/dir deps
* [`e090d706c`](https://github.com/npm/cli/commit/e090d706ca637d4df96d28bff1660590aa3f3b62)
  [#2097](https://github.com/npm/cli/issues/2097) `libnpmversion@1.0.7`:
    * Do not crash when the package.json file lacks a 'version' field
* [`8fa541a10`](https://github.com/npm/cli/commit/8fa541a10dbdc09376175db7a378cc9b33e8b17b)
  `cmark-gfm@0.8.4`

## 7.0.8 (2020-11-03)

### DOCUMENTATION

* [`052e977b9`](https://github.com/npm/cli/commit/052e977b9d071e1b3654976881d10cd3ddcba788)
  [#1822](https://github.com/npm/cli/issues/1822)
  [#1247](https://github.com/npm/cli/issues/1247)
  add section on peerDependenciesMeta field in package.json
  ([@foxxyz](https://github.com/foxxyz))
* [`52d32d175`](https://github.com/npm/cli/commit/52d32d1758c5ebc58944a1e8d98d57e30048e527)
  [#1970](https://github.com/npm/cli/issues/1970)
  match npm-exec.md -p usage with lib/exec.js
  ([@dr-js](https://github.com/dr-js))
* [`48ee8d01e`](https://github.com/npm/cli/commit/48ee8d01edd11ed6186c483e1169ff4d2070b963)
  [#2096](https://github.com/npm/cli/issues/2096)
  Fix RFC links in changelog
  ([@jtojnar](https://github.com/jtojnar))


### BUG FIXES

* [`6cd3cd08a`](https://github.com/npm/cli/commit/6cd3cd08af56445e13757cac3af87f3e7d54ed27)
  Support *all* conf keys in publishConfig
* [`a1f9be8a7`](https://github.com/npm/cli/commit/a1f9be8a7f9b7a3a813fc3e5e705bc982470b0e2)
  [#2074](https://github.com/npm/cli/issues/2074)
  Support publishing any kind of spec, not just directories

### DEPENDENCIES

* [`545382df6`](https://github.com/npm/cli/commit/545382df62e3014f3e51d7034e52498fb2b01a37)
  `libnpmpublish@4.0.0`:
    * Support publishing things other than folders
* [`7d88f1719`](https://github.com/npm/cli/commit/7d88f17197e3c8cca9b277378d6f9b054b1b7886)
  `npm-registry-fetch@9.0.0`
* [`823b40a4e`](https://github.com/npm/cli/commit/823b40a4e9c6ef76388af6fe01a3624f6f7675be)
  `pacote@11.1.12`
* [`90bf57826`](https://github.com/npm/cli/commit/90bf57826edf2f78ddf8deb0793115ead8a8b556)
  `npm-profile@5.0.2`
* [`e5a413577`](https://github.com/npm/cli/commit/e5a4135770d13cf114fac439167637181f87d824)
  `libnpmteam@2.0.2`
* [`fc5aa7b4a`](https://github.com/npm/cli/commit/fc5aa7b4ad45cb65893f734e1229a6720f7966e5)
  `libnpmsearch@3.0.1`
* [`9fc1dee13`](https://github.com/npm/cli/commit/9fc1dee138ca33ecdbd57e63142b27c60cf88f9b)
  `libnpmorg@2.0.1`
* [`0ea870ec5`](https://github.com/npm/cli/commit/0ea870ec5d2be1d44f050ad8bc24ed936cc45fde)
  `libnpmhook@6.0.1`
* [`32fd744ea`](https://github.com/npm/cli/commit/32fd744ea745f297f0be79a80955f077a57c4ac7)
  `libnpmaccess@4.0.1`
* [`fc76f3d9f`](https://github.com/npm/cli/commit/fc76f3d9fcf19e65a9373ab3d9068c4326d2f782)
  `@npmcli/arborist@1.0.8`
    * Fix `cannot read property 'description' of undefined` in `npm ls`
      when `package-lock.json` is corrupted
    * Do not allow peerDependencies to be nested under dependents in any
      circumstances
    * Always resolve peerDependencies in `--prefer-dedupe` mode

## 7.0.7 (2020-10-30)

### BUG FIXES

* [`3990b422d`](https://github.com/npm/cli/commit/3990b422d3ff63c54d96b61596bdb8f26a45ca7b)
  [#2067](https://github.com/npm/cli/pull/2067)
  use sh as default unix shell, not bash
  ([@isaacs](https://github.com/isaacs))
* [`81d6ceef6`](https://github.com/npm/cli/commit/81d6ceef6947e46355eb3ddb05a73da50870dfc1)
  [#1975](https://github.com/npm/cli/issues/1975)
  fix npm exec on folders missing package.json
  ([@ruyadorno](https://github.com/ruyadorno))
* [`2a680e91a`](https://github.com/npm/cli/commit/2a680e91a2be1f3f03a6fbd946f74628ee1cb370)
  [#2083](https://github.com/npm/cli/pull/2083)
  delete the contents of `node_modules` only in `npm ci`
  ([@nlf](https://github.com/nlf))
* [`2636fe1f4`](https://github.com/npm/cli/commit/2636fe1f45383cb1b6fc164564dc49318815db37)
  [#2086](https://github.com/npm/cli/pull/2086)
  disable banner output if loglevel is silent in `npm run-script`
  ([@macno](https://github.com/macno))

### DEPENDENCIES

* [`4156f053e`](https://github.com/npm/cli/commit/4156f053ee8712a4b53a210e62fba1e6562ba43a)
  `@npmcli/run-script@1.7.4`
    * restore the default `npm start` script
* [`1900ae9ad`](https://github.com/npm/cli/commit/1900ae9adecd227dd6f8b49de61a99c978ba89cf)
  `@npmcli/promise-spawn@1.3.2`
    * fix errors when processing scripts as root
* [`8cb0c166c`](https://github.com/npm/cli/commit/8cb0c166ccc019146a7a94d13c12723f001d2551)
  `@npmcli/arborist@1.0.6`
    * make sure missing bin links get set on reify

## 7.0.6 (2020-10-27)

### BUG FIXES

* [`46c7f792a`](https://github.com/npm/cli/commit/46c7f792ab16dd0b091e1ad6d37de860c8885883)
  [#2047](https://github.com/npm/cli/pull/2047)
  [#1935](https://github.com/npm/cli/issues/1935)
  skip the prompt when in a known ci environment
  ([@nlf](https://github.com/nlf))
* [`f8f6e1fad`](https://github.com/npm/cli/commit/f8f6e1fad8057edc02e4ce4382b1bc086d01211c)
  [#2049](https://github.com/npm/cli/pull/2049)
  properly remove pycache in release script
  ([@MylesBorins](https://github.com/MylesBorins))
* [`5db95b393`](https://github.com/npm/cli/commit/5db95b393e9c461ad34c1774f3515c322bf375bf)
  [#2050](https://github.com/npm/cli/pull/2050)
  pack: do not show individual files of bundled deps
  ([@isaacs](https://github.com/isaacs))
* [`3ee8f3b34`](https://github.com/npm/cli/commit/3ee8f3b34055da2ef1e735e1a06f64593512f1e3)
  [#2051](https://github.com/npm/cli/pull/2051)
  view: Better errors when package.json is not JSON
  ([@isaacs](https://github.com/isaacs))

### DEPENDENCIES

* [`99ae633f6`](https://github.com/npm/cli/commit/99ae633f6ccc8aa93dc3dcda863071658b0653db)
  `libnpmversion@1.0.6`
    - respect gitTagVersion = false
* [`d4173f58d`](https://github.com/npm/cli/commit/d4173f58ddefdd5456145f34f3c9f4ba5fca407e)
  `@npmcli/promise-spawn@1.3.1`
    - do not return empty buffer when stdio is inherited
    - attach child process to returned promise
* [`c09380fa5`](https://github.com/npm/cli/commit/c09380fa51b720141a9971602f4bb7aabd4d6242)
  `@npmcli/run-script@1.7.3`
    - forward SIGINT and SIGTERM to children that inherit stdio
* [`b154861ad`](https://github.com/npm/cli/commit/b154861ad244b6a14020c43738d0cce1948bfdd3)
  `@npmcli/arborist@1.0.5`
* [`ffea6596b`](https://github.com/npm/cli/commit/ffea6596b8653da32a2b4c9a4903970e7146eee4)
  `agent-base@6.0.2`
    - support http proxy for https registries

## 7.0.5 (2020-10-23)

* [`77ad86b5e`](https://github.com/npm/cli/commit/77ad86b5eedf139dda3329a6686d5f104dc233bb)
  Merge docs deps with main project

## 7.0.4 (2020-10-23)

### DOCUMENTATION

* [`cc026daf8`](https://github.com/npm/cli/commit/cc026daf8c8330256de01375350a1407064562f9)
  docs: `npm-dedupe` through `npm-install`
* [`aec77acf8`](https://github.com/npm/cli/commit/aec77acf886d73f85e747cafdf7a2b360befba16)
  [#1915](https://github.com/npm/cli/pull/1915)
  use "dockhand" for faster static documentation generation
  ([@ethomson](https://github.com/ethomson))
* [`aeb10d210`](https://github.com/npm/cli/commit/aeb10d210816cf6829e0ac557c79d9efd8c4bdd1)
  [#2024](https://github.com/npm/cli/pull/2024)
  Fix post-install script name
  ([@irajtaghlidi](https://github.com/irajtaghlidi))

### BUG FIXES

* [`59e8dd6c6`](https://github.com/npm/cli/commit/59e8dd6c621f9a5c6e0b65533d8256be87a8e0d3)
  [#2015](https://github.com/npm/cli/issues/2015)
  [#2016](https://github.com/npm/cli/pull/2016)
  Properly set `npm_command` environment variable.

### TESTS

* [`39ad1ad9e`](https://github.com/npm/cli/commit/39ad1ad9e1e1a9530db5b90a588b5081b71abc8d)
  [#2001](https://github.com/npm/cli/pull/2001)
  `npm config` tests
  ([@ruyadorno](https://github.com/ruyadorno))
* [`b9c1caa8e`](https://github.com/npm/cli/commit/b9c1caa8e4cc7c900d09657425ea361db5974319)
  [#2026](https://github.com/npm/cli/pull/2026)
  `npm owner` test and refactor
  ([@ruyadorno](https://github.com/ruyadorno))

### DEPENDENCIES

* [`ed6e6a9d3`](https://github.com/npm/cli/commit/ed6e6a9d3c36ffc5fb77fc25b6d66dbcb26beeb9)
  `eslint-plugin-standard@4.0.2`
* [`b737ee999`](https://github.com/npm/cli/commit/b737ee99961364827bacf210a3e5ca5d2b7edad2)
  [#2009](https://github.com/npm/cli/issues/2009)
  [#2007](https://github.com/npm/cli/issues/2007)
  `npm-packlist@2.1.4`:

    * Maintain order in package.json files array globs
    * Strip slashes from package files list results

* [`783965508`](https://github.com/npm/cli/commit/783965508d49f8ab0d8ceff38bee700cd0a06a54)
  [#1997](https://github.com/npm/cli/issues/1997)
  [#2000](https://github.com/npm/cli/issues/2000)
  [#2005](https://github.com/npm/cli/issues/2005)
  `@npmcli/arborist@1.0.4`

    * Ensure that root is added when root.meta is set
    * Include all edges in explain() output when a root edge exists
    * Do not conflict on meta-peers that will not be replaced
    * Install peerOptionals if explicitly requested, or dev

## 7.0.3 (2020-10-20)

### BUG FIXES

* [`ce4724a38`](https://github.com/npm/cli/commit/ce4724a3835ded9a4a29d8d67323f925461155e5)
  [#1986](https://github.com/npm/cli/pull/1986)
  check `result` when determining exit code of `ls <filter>`
  ([@G-Rath](https://github.com/G-Rath))
* [`00d926f8d`](https://github.com/npm/cli/commit/00d926f8d884872d08d9a0cd73aa9cace2acb91b)
  [#1987](https://github.com/npm/cli/pull/1987)
  don't suppress run output when `--silent` is passed
  ([@G-Rath](https://github.com/G-Rath))
* [`043da2347`](https://github.com/npm/cli/commit/043da234745f36d55742e827314837dead5807ab)
  improve cache clear error message
  ([@isaacs](https://github.com/isaacs))

### DOCUMENTATION

* [`a57f5c466`](https://github.com/npm/cli/commit/a57f5c466ceae59575ef05bb7941cce8752d8c58)
  update docs for: access, adduser, audit, bin, bugs, build, cache, ci,
  completion, config and dedupe
  ([@isaacs](https://github.com/isaacs))
* [`5b88b72b9`](https://github.com/npm/cli/commit/5b88b72b9821f7114cc4e475bbf52726a1674e52)
  remove the long-gone bundle command
  ([@isaacs](https://github.com/isaacs))
* [`ae09aa5c1`](https://github.com/npm/cli/commit/ae09aa5c1cd150727b05ccfaeaba8d45e5697e50)
  [#1993](https://github.com/npm/cli/pull/1993)
  document --save-peer as a common option to npm install
  ([@JakeChampion](https://github.com/JakeChampion))
* [`c9993e6b1`](https://github.com/npm/cli/commit/c9993e6b1c2918699c2d125bf9b966f44f5d3ebe)
  [#1982](https://github.com/npm/cli/pull/1982)
  fix url links for init-package-json/node-semver
  ([@takenspc](https://github.com/takenspc))

### DEPENDENCIES

* [`5d9df8395`](https://github.com/npm/cli/commit/5d9df83958d3d5e6d8acad2ebabfbe5f3fd23c13)
  `node-gyp@7.1.2`

## 7.0.2 (2020-10-16)

### DOCUMENTATION

* [`9476734b7`](https://github.com/npm/cli/commit/9476734b7d5fa6df80ad17ad277a6bee9a16235c)
  [#1967](https://github.com/npm/cli/pull/1967)
  add mention to workspaces prepare lifecycle
  ([@ruyadorno](https://github.com/ruyadorno))

### BUG FIXES

* [`5cf71c689`](https://github.com/npm/cli/commit/5cf71c689bcfcd423405e59d05b7cc5704cb4c02)
  [#1971](https://github.com/npm/cli/pull/1971)
  owner rm at local pkg not work
  ([@ShangguanQuail](https://github.com/ShangguanQuail))

### DEPENDENCIES

* [`722b7ae63`](https://github.com/npm/cli/commit/722b7ae63da8b386fe188066dc2dae0121d9353b)
  [#1974](https://github.com/npm/cli/pull/1974)
  patch node-gyp
  ([@targos](https://github.com/MylesBorins))
* [`4ae825c01`](https://github.com/npm/cli/commit/4ae825c01c7ca3031361f9df72594a190c6ed1e4)
  [#1976](https://github.com/npm/cli/pull/1976)
  patch node-gyp
  ([@MylesBorins](https://github.com/MylesBorins))
* [`181eabf13`](https://github.com/npm/cli/commit/181eabf132c823af086380368de73d2f42e5aac1)
  `@npmcli/arborist@1.0.3`
    * fix workspaces `prepare` lifecycle scripts
    * fix peer deps overchecks resulting in ERESOLVE
* [`6cc115409`](https://github.com/npm/cli/commit/6cc115409b7eb2df8e11db6232ee3d00e4316a7d)
  `init-package-json@2.0.1`
* [`dbf9d6d1f`](https://github.com/npm/cli/commit/dbf9d6d1f060ea43b700409306574396a798127d)
  `libnpmpublish@3.0.2`

## 7.0.1 (2020-10-15)

### DOCUMENTATION

* [`03fca6a3b`](https://github.com/npm/cli/commit/03fca6a3b227f71562863bec7a1de1732bd719f1)
  Adds docs on workspaces, explaining its basic concept and how to use it.
  ([@ruyadorno](https://github.com/ruyadorno))

### BUG FIXES

* [`2ccb63659`](https://github.com/npm/cli/commit/2ccb63659f9a757201658d5d019099b492d04a5b)
  [#1951](https://github.com/npm/cli/issues/1951)
  [#1956](https://github.com/npm/cli/pull/1956)
  Handle errors from audit endpoint appropriately
  ([@isaacs](https://github.com/isaacs))

### DEPENDENCIES

* [`120e62736`](https://github.com/npm/cli/commit/120e6273604f15a2ce55668dfb2c23d06bf1e06c)
  `node-gyp@7.1.1`
* [`6560b8d95`](https://github.com/npm/cli/commit/6560b8d952a613cefbd900186aa38df53bc201d1)
  `@npmcli/arborist@1.0.2`
    * do not drop scope information when fetching scoped package tarballs
    * fix cycles/ordering resolution when peer deps require nesting
* [`282a1e008`](https://github.com/npm/cli/commit/282a1e00820b9abfb3465d044b30b2cade107909)
  `npm-user-validate@1.0.1`
* [`b259edcb4`](https://github.com/npm/cli/commit/b259edcb4bac37e6f26d56af5f6666afbda8c126)
  `hosted-git-info@3.0.7`

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
    * fix unknown envs to be passed through
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
  [RFC-0029](https://github.com/npm/rfcs/blob/latest/implemented/0029-add-ability-to-skip-hooks.md)
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
  20](https://github.com/npm/rfcs/blob/latest/implemented/0020-npm-option-handling.md)
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
  21](https://github.com/npm/rfcs/blob/latest/implemented/0021-reduce-lifecycle-script-environment.md)
  Environment no longer includes `npm_package_*` fields, or `npm_config_*`
  fields for default configs.  `npm_package_json`, `npm_package_integrity`,
  `npm_package_resolved`, and `npm_command` environment variables added.

    (NB: this [will change a bit prior to a `v7.0.0` GA
    release](https://github.com/npm/rfcs/pull/183))

- [RFC
  22](https://github.com/npm/rfcs/blob/latest/implemented/0022-quieter-install-scripts.md)
  Scripts run during the normal course of installation are silenced unless
  they exit in error (ie, with a signal or non-zero exit status code), and
  are for a non-optional dependency.

- [RFC
  24](https://github.com/npm/rfcs/blob/latest/implemented/0024-npm-run-traverse-directory-tree.md)
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
  13](https://github.com/npm/rfcs/blob/latest/implemented/0013-no-package-json-_fields.md)
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
  25](https://github.com/npm/rfcs/blob/latest/implemented/0025-install-peer-deps.md)
  `peerDependencies` are installed by default.  This behavior can be
  disabled by setting the `legacy-peer-deps` configuration flag.

    **BREAKING CHANGE**: this can cause some packages to not be
    installable, if they have unresolveable peer dependency conflicts.
    While the correct solution is to fix the conflict, this was not forced
    upon users for several years, and some have come to rely on this lack
    of correctness.  Use the `--legacy-peer-deps` config flag if impacted.

- [RFC
  23](https://github.com/npm/rfcs/blob/latest/implemented/0023-acceptDependencies.md)
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
  26](https://github.com/npm/rfcs/blob/latest/implemented/0026-workspaces.md)
  First phase of `workspaces` support is added.  This changes npm's
  behavior when a root project's `package.json` file contains a
  `workspaces` field.

### `npm update`

- [RFC
  19](https://github.com/npm/rfcs/blob/latest/implemented/0019-remove-update-depth-option.md)
  Update all dependencies when `npm update` is run without any arguments.
  As it is no longer relevant, `--depth` config flag removed from `npm
  update`.

### `npm outdated`

- [RFC
  27](https://github.com/npm/rfcs/blob/latest/implemented/0027-remove-depth-outdated.md)
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
