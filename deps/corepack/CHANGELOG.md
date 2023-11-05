# Changelog

## [0.23.0](https://github.com/nodejs/corepack/compare/v0.22.0...v0.23.0) (2023-11-05)


### Features

* update package manager versions ([#325](https://github.com/nodejs/corepack/issues/325)) ([450cd33](https://github.com/nodejs/corepack/commit/450cd332d00d3428f49ed09a4235bd12139931c9))

## [0.22.0](https://github.com/nodejs/corepack/compare/v0.21.0...v0.22.0) (2023-10-21)


### Features

* allow fallback to application/json for custom registries ([#314](https://github.com/nodejs/corepack/issues/314)) ([92f8e71](https://github.com/nodejs/corepack/commit/92f8e71f8c97c44f404ce9b7df8787a4292e6830))
* update package manager versions ([#318](https://github.com/nodejs/corepack/issues/318)) ([0bd2577](https://github.com/nodejs/corepack/commit/0bd2577bb4c6c3a5a33ecdb3b6ca2ff244c54f28))

## [0.21.0](https://github.com/nodejs/corepack/compare/v0.20.0...v0.21.0) (2023-10-08)


### ⚠ BREAKING CHANGES

* remove support for Node.js 16.x

### Features

* update package manager versions ([#297](https://github.com/nodejs/corepack/issues/297)) ([503e135](https://github.com/nodejs/corepack/commit/503e135878935cc881ebd94b48d5eca94ec4c27b))


### Miscellaneous Chores

* update supported Node.js versions ([#309](https://github.com/nodejs/corepack/issues/309)) ([787e24d](https://github.com/nodejs/corepack/commit/787e24df609513702eafcd8c6a5f03544d7d45cc))

## [0.20.0](https://github.com/nodejs/corepack/compare/v0.19.0...v0.20.0) (2023-08-29)


### Features

* refactor the CLI interface ([#291](https://github.com/nodejs/corepack/issues/291)) ([fe3e5cd](https://github.com/nodejs/corepack/commit/fe3e5cd86c45db0d87c7fdea87d57d59b0bdcb78))
* update package manager versions ([#292](https://github.com/nodejs/corepack/issues/292)) ([be9c286](https://github.com/nodejs/corepack/commit/be9c286846443ff03081e736fdf4a0ff031fbd38))

## [0.19.0](https://github.com/nodejs/corepack/compare/v0.18.1...v0.19.0) (2023-06-24)


### Features

* support ESM ([#270](https://github.com/nodejs/corepack/issues/270)) ([be2489c](https://github.com/nodejs/corepack/commit/be2489cd0aaabf26a019e1c089a3c8bcc329e94a))
* update package manager versions ([#280](https://github.com/nodejs/corepack/issues/280)) ([4188f2b](https://github.com/nodejs/corepack/commit/4188f2b4671228339fe16f9f566e7bac0c2c4f6d))

## [0.18.1](https://github.com/nodejs/corepack/compare/v0.18.0...v0.18.1) (2023-06-13)


### Features

* update package manager versions ([#272](https://github.com/nodejs/corepack/issues/272)) ([5345774](https://github.com/nodejs/corepack/commit/53457747a26a5de3debbd0d9282b338186bbd7c3))


### Bug Fixes

* disable `v8-compile-cache` when using `npm@&gt;=9.7.0` ([#276](https://github.com/nodejs/corepack/issues/276)) ([2f3678c](https://github.com/nodejs/corepack/commit/2f3678cd7915978f4e2ce7a32cbe5db58e9d0b8d))
* don't override `process.exitCode` ([#268](https://github.com/nodejs/corepack/issues/268)) ([17d1f3d](https://github.com/nodejs/corepack/commit/17d1f3dd41ef6127228d427fd5cca373d6c97f0f))

## [0.18.0](https://github.com/nodejs/corepack/compare/v0.17.2...v0.18.0) (2023-05-19)


### ⚠ BREAKING CHANGES

* remove support for Node.js 14.x

### Features

* update package manager versions ([#256](https://github.com/nodejs/corepack/issues/256)) ([7b61ff6](https://github.com/nodejs/corepack/commit/7b61ff6bc797ec4ed50c2ba1e1f1689264cbf4fc))


### Bug Fixes

* **doc:** add a note about troubleshooting network errors ([#259](https://github.com/nodejs/corepack/issues/259)) ([aa3cbdb](https://github.com/nodejs/corepack/commit/aa3cbdb54fb21b8e0adde96dc781cdf750932843))


### Miscellaneous Chores

* update supported Node.js versions ([#258](https://github.com/nodejs/corepack/issues/258)) ([74f679d](https://github.com/nodejs/corepack/commit/74f679d8a72cc10a3720fc679b95e9bd086d95be))

## [0.17.2](https://github.com/nodejs/corepack/compare/v0.17.1...v0.17.2) (2023-04-07)


### Features

* update package manager versions ([#249](https://github.com/nodejs/corepack/issues/249)) ([2507e9b](https://github.com/nodejs/corepack/commit/2507e9b317eacdeb939aee086de5711218ebd794))

## [0.17.1](https://github.com/nodejs/corepack/compare/v0.17.0...v0.17.1) (2023-03-17)


### Features

* update package manager versions ([#245](https://github.com/nodejs/corepack/issues/245)) ([673f3b7](https://github.com/nodejs/corepack/commit/673f3b7f18421a49da1e2c55656666a74ce94474))

## [0.17.0](https://github.com/nodejs/corepack/compare/v0.16.0...v0.17.0) (2023-02-24)


### ⚠ BREAKING CHANGES

* add `"exports"` to the `package.json` ([#239](https://github.com/nodejs/corepack/issues/239))

### Features

* update package manager versions ([#242](https://github.com/nodejs/corepack/issues/242)) ([5141639](https://github.com/nodejs/corepack/commit/5141639af8198a343105be1e98a74f7c9e152472))


### Bug Fixes

* add `"exports"` to the `package.json` ([#239](https://github.com/nodejs/corepack/issues/239)) ([8e12d08](https://github.com/nodejs/corepack/commit/8e12d088dec171c03e90f623895a1fbf867130e6))

## [0.16.0](https://github.com/nodejs/corepack/compare/v0.15.3...v0.16.0) (2023-02-17)


### Features

* update package manager versions ([#228](https://github.com/nodejs/corepack/issues/228)) ([bb000f9](https://github.com/nodejs/corepack/commit/bb000f9c10a1fbd85f2c15a90218d90b42473130))
* build: migrate to ESBuild ([#229](https://github.com/nodejs/corepack/pull/229)) ([15ceb83](https://github.com/nodejs/corepack/commit/15ceb832a34a223efbe3d3f9cb792d9101a7022a))


### Bug Fixes

* npm registry override ([#219](https://github.com/nodejs/corepack/issues/219)) ([1b35362](https://github.com/nodejs/corepack/commit/1b353624e644874d9ef6c9acaf6d1254bff3015a))

## [0.15.3](https://github.com/nodejs/corepack/compare/v0.15.2...v0.15.3) (2022-12-30)


### Features

* update package manager versions ([#215](https://github.com/nodejs/corepack/issues/215)) ([f84cfcb](https://github.com/nodejs/corepack/commit/f84cfcb00ffb985d44b6aa0b563b2b4056a8f0d0))

## [0.15.2](https://github.com/nodejs/corepack/compare/v0.15.1...v0.15.2) (2022-11-25)


### Features

* update package manager versions ([#211](https://github.com/nodejs/corepack/issues/211)) ([c536c0c](https://github.com/nodejs/corepack/commit/c536c0c27c137c87a14487a2c2a63a1fe6bf88ec))

## [0.15.1](https://github.com/nodejs/corepack/compare/v0.15.0...v0.15.1) (2022-11-04)


### Features

* update package manager versions ([#205](https://github.com/nodejs/corepack/issues/205)) ([5bfac11](https://github.com/nodejs/corepack/commit/5bfac11715474a4318c67fc806fd1ff4252c683a))

## [0.15.0](https://github.com/nodejs/corepack/compare/v0.14.2...v0.15.0) (2022-10-28)


### Features

* add support for configurable registries and applicable auth options ([#186](https://github.com/nodejs/corepack/issues/186)) ([662ae90](https://github.com/nodejs/corepack/commit/662ae9057c7360cb05e9476914e611a9bf0074db))
* update package manager versions ([#193](https://github.com/nodejs/corepack/issues/193)) ([0ec3a73](https://github.com/nodejs/corepack/commit/0ec3a7384729c5cf4ac566d91f1a4bb74e08a64f))
* when strict checking is off, treat like transparent ([#197](https://github.com/nodejs/corepack/issues/197)) ([5eadc50](https://github.com/nodejs/corepack/commit/5eadc50192e205c60bfb1cad91854e9014a747b8))


### Bug Fixes

* **doc:** add package configuration instruction to readme ([#188](https://github.com/nodejs/corepack/issues/188)) ([0b7abb9](https://github.com/nodejs/corepack/commit/0b7abb9833d332bad97902260d31652482c274a0))
* recreate cache folder if necessary ([#200](https://github.com/nodejs/corepack/issues/200)) ([7b5f2f9](https://github.com/nodejs/corepack/commit/7b5f2f9fcb24fe3fe517a96deaac7f32854f3124))

## [0.14.2](https://github.com/nodejs/corepack/compare/v0.14.1...v0.14.2) (2022-09-24)

### Features

* update package manager versions ([#184](https://github.com/nodejs/corepack/issues/184)) ([84ae313](https://github.com/nodejs/corepack/commit/84ae3139e4b9a86d97465e36b50beb9201fda732))

## [0.14.1](https://github.com/nodejs/corepack/compare/v0.14.0...v0.14.1) (2022-09-16)


### Features

* update package manager versions ([#179](https://github.com/nodejs/corepack/issues/179)) ([0b88dcb](https://github.com/nodejs/corepack/commit/0b88dcbaaf190117c6f407b6632a4b3b10da8ad9))

## [0.14.0](https://github.com/nodejs/corepack/compare/v0.13.0...v0.14.0) (2022-09-02)


### Features

* add `COREPACK_ENABLE_STRICT` env variable ([#167](https://github.com/nodejs/corepack/issues/167)) ([92b52f6](https://github.com/nodejs/corepack/commit/92b52f6b4918aff968c0942b89fc722ebf57bce2))
* update package manager versions ([#170](https://github.com/nodejs/corepack/issues/170)) ([6f70bfc](https://github.com/nodejs/corepack/commit/6f70bfc4b6a8a57cccb1ff9cbf2f49240648f1ed))


### Bug Fixes

* handle tags including numbers in `prepare` command ([#165](https://github.com/nodejs/corepack/issues/165)) ([5a0727b](https://github.com/nodejs/corepack/commit/5a0727b43976e0dc299151876c0dde2c4a85174d))

## [0.13.0](https://github.com/nodejs/corepack/compare/v0.12.3...v0.13.0) (2022-08-19)


### Features

* do not use `~/.node` as default value for `COREPACK_HOME` ([#152](https://github.com/nodejs/corepack/issues/152)) ([09e24cf](https://github.com/nodejs/corepack/commit/09e24cf497de27fe92668cf0a8e555f2c7e2530d))
* download the latest version instead of a pinned one ([#134](https://github.com/nodejs/corepack/issues/134)) ([055b928](https://github.com/nodejs/corepack/commit/055b92807f711b5c8c8be6e62b8d3ce83e1ff002))
* update package manager versions ([#163](https://github.com/nodejs/corepack/issues/163)) ([af38d5a](https://github.com/nodejs/corepack/commit/af38d5afbbc10d61265b2f4687c5cc498b059b41))

## [0.12.3](https://github.com/nodejs/corepack/compare/v0.12.2...v0.12.3) (2022-08-12)


### Features

* update package manager versions ([#160](https://github.com/nodejs/corepack/issues/160)) ([ad092a7](https://github.com/nodejs/corepack/commit/ad092a7fb4296143fa5224c04dbd628451b3c158))

## [0.12.2](https://github.com/nodejs/corepack/compare/v0.12.1...v0.12.2) (2022-08-05)

### Features

* update package manager versions ([#154](https://github.com/nodejs/corepack/issues/154)) ([4b95fd3](https://github.com/nodejs/corepack/commit/4b95fd3b926659855970a887c893c10db0b98e5d))

## [0.12.1](https://github.com/nodejs/corepack/compare/v0.12.0...v0.12.1) (2022-07-21)


### Bug Fixes

* **doc:** update DESIGN.md s/engines.pm/packageManager/ ([#141](https://github.com/nodejs/corepack/issues/141)) ([d6039c5](https://github.com/nodejs/corepack/commit/d6039c5b16cdddb33069b9aa864854ed16d17e4e))
* update package manager versions ([#146](https://github.com/nodejs/corepack/issues/146)) ([fdb187a](https://github.com/nodejs/corepack/commit/fdb187a640de77df9b3688623ba510bdafda8702))

## [0.12.0](https://github.com/nodejs/corepack/compare/v0.11.2...v0.12.0) (2022-07-09)


### Features

* add support for hash checking ([#133](https://github.com/nodejs/corepack/issues/133)) ([6a480a7](https://github.com/nodejs/corepack/commit/6a480a72c2e9fc6725f2ab6dfaf4c52e4d3d2ade))
* add support for tags and ranges in `prepare` command ([#136](https://github.com/nodejs/corepack/issues/136)) ([29da06c](https://github.com/nodejs/corepack/commit/29da06c515e917829e5ffbedb34284a6597e9d56))
* update package manager versions ([#129](https://github.com/nodejs/corepack/issues/129)) ([2470f58](https://github.com/nodejs/corepack/commit/2470f58b74491a1301221df643c55be5adf1d349))
* update package manager versions ([#139](https://github.com/nodejs/corepack/issues/139)) ([cd0dcad](https://github.com/nodejs/corepack/commit/cd0dcade85621199048d7ca30dfc3efce11e1f37))


### Bug Fixes

* streamline the cache exploration ([#135](https://github.com/nodejs/corepack/issues/135)) ([185da44](https://github.com/nodejs/corepack/commit/185da44078fd1ca34aec2e4e6f8a52ecffcf3c11))

## [0.11.2](https://github.com/nodejs/corepack/compare/v0.11.1...v0.11.2) (2022-06-13)

### Bug Fixes

* only set bins on pack ([#127](https://github.com/nodejs/corepack/issues/127)) ([7ae489a](https://github.com/nodejs/corepack/commit/7ae489a86c3fe584b9915f4ec57deb7c316c1a25))

## [0.11.1](https://github.com/nodejs/corepack/compare/v0.11.0...v0.11.1) (2022-06-12)


### Bug Fixes

* **ci:** YAML formatting in publish workflow ([#124](https://github.com/nodejs/corepack/issues/124)) ([01c7d63](https://github.com/nodejs/corepack/commit/01c7d638b04a1340b3939a7985e24b586e344995))

## 0.11.0 (2022-06-12)


### Features

* auto setup proxy for http requests ([#69](https://github.com/nodejs/corepack/issues/69)) ([876ce02](https://github.com/nodejs/corepack/commit/876ce02fe7385ea5bc896b2dc93d1fb320361c64))


### Bug Fixes

* avoid symlinks to work on Windows ([#13](https://github.com/nodejs/corepack/issues/13)) ([b56df30](https://github.com/nodejs/corepack/commit/b56df30796da9c7cb0ba5e1bb7152c81582abba6))
* avoid using eval to get the corepack version ([#45](https://github.com/nodejs/corepack/issues/45)) ([78d94eb](https://github.com/nodejs/corepack/commit/78d94eb297444d7558e8b4395f0108c97117f8ab))
* bin file name for pnpm >=6.0 ([#35](https://github.com/nodejs/corepack/issues/35)) ([8ff2499](https://github.com/nodejs/corepack/commit/8ff2499e831c8cf2dea604ea985d830afc8a479e))
* generate cmd shim files ([a900b4d](https://github.com/nodejs/corepack/commit/a900b4db12fcd4d99c0a4d011b426cdc6485d323))
* handle package managers with a bin array correctly ([#20](https://github.com/nodejs/corepack/issues/20)) ([1836d17](https://github.com/nodejs/corepack/commit/1836d17b4fc4c0164df2fe1ccaca4d2f16f6f2d1))
* handle parallel installs ([#84](https://github.com/nodejs/corepack/issues/84)) ([5cfc6c9](https://github.com/nodejs/corepack/commit/5cfc6c9df0dbec8e4de4324be37aa0a54a300552))
* handle prereleases ([#32](https://github.com/nodejs/corepack/issues/32)) ([2a46b6d](https://github.com/nodejs/corepack/commit/2a46b6d13adae139141012254ef670d6ddcb5d11))


### Performance Improvements

* load binaries in the same process ([#97](https://github.com/nodejs/corepack/issues/97)) ([5ff6e82](https://github.com/nodejs/corepack/commit/5ff6e82028e58448ba5ba986854b61ecdc69885b))
