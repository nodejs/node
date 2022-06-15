# Changelog

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
