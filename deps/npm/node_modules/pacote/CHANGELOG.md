# Change Log

All notable changes to this project will be documented in this file. See [standard-version](https://github.com/conventional-changelog/standard-version) for commit guidelines.

<a name="7.6.1"></a>
## [7.6.1](https://github.com/zkat/pacote/compare/v7.6.0...v7.6.1) (2018-03-08)


### Bug Fixes

* **standard:** update to new standard rules ([bb52d02](https://github.com/zkat/pacote/commit/bb52d02))



<a name="7.6.0"></a>
# [7.6.0](https://github.com/zkat/pacote/compare/v7.5.3...v7.6.0) (2018-03-08)


### Features

* **git:** added retry logic for all git operations. ([#136](https://github.com/zkat/pacote/issues/136)) ([425c58d](https://github.com/zkat/pacote/commit/425c58d))



<a name="7.5.3"></a>
## [7.5.3](https://github.com/zkat/pacote/compare/v7.5.2...v7.5.3) (2018-03-02)


### Bug Fixes

* **tarball:** stop dropping stream errors on the floor ([3db03c2](https://github.com/zkat/pacote/commit/3db03c2))



<a name="7.5.2"></a>
## [7.5.2](https://github.com/zkat/pacote/compare/v7.5.1...v7.5.2) (2018-03-02)


### Bug Fixes

* **console:** remove spurious debugging console.log :< ([5b8b509](https://github.com/zkat/pacote/commit/5b8b509))



<a name="7.5.1"></a>
## [7.5.1](https://github.com/zkat/pacote/compare/v7.5.0...v7.5.1) (2018-03-01)


### Bug Fixes

* **tarball:** catch errors thrown from stream handler ([bdd6628](https://github.com/zkat/pacote/commit/bdd6628))



<a name="7.5.0"></a>
# [7.5.0](https://github.com/zkat/pacote/compare/v7.4.2...v7.5.0) (2018-03-01)


### Features

* **logging:** let users know when file: resolved packages fail integrity check ([0fb8037](https://github.com/zkat/pacote/commit/0fb8037))



<a name="7.4.2"></a>
## [7.4.2](https://github.com/zkat/pacote/compare/v7.4.1...v7.4.2) (2018-02-23)


### Bug Fixes

* **deps:** move mkdirp and rimraf to dependencies ([#140](https://github.com/zkat/pacote/issues/140)) ([bba20c8](https://github.com/zkat/pacote/commit/bba20c8)), closes [#128](https://github.com/zkat/pacote/issues/128)



<a name="7.4.1"></a>
## [7.4.1](https://github.com/zkat/pacote/compare/v7.4.0...v7.4.1) (2018-02-23)


### Bug Fixes

* **tarball:** fix spurious errors from tarball.stream() ([0286ba5](https://github.com/zkat/pacote/commit/0286ba5))



<a name="7.4.0"></a>
# [7.4.0](https://github.com/zkat/pacote/compare/v7.3.3...v7.4.0) (2018-02-17)


### Features

* **tarball:** support file: opts.resolved shortcut ([a6cf279](https://github.com/zkat/pacote/commit/a6cf279))



<a name="7.3.3"></a>
## [7.3.3](https://github.com/zkat/pacote/compare/v7.3.2...v7.3.3) (2018-02-15)


### Bug Fixes

* **tarball:** another attempt at fixing opts.resolved ([aff3b6a](https://github.com/zkat/pacote/commit/aff3b6a))



<a name="7.3.2"></a>
## [7.3.2](https://github.com/zkat/pacote/compare/v7.3.1...v7.3.2) (2018-02-15)


### Bug Fixes

* **tarball:** opts.resolved impl was triggering extra registry lookups ([0a4729d](https://github.com/zkat/pacote/commit/0a4729d))



<a name="7.3.1"></a>
## [7.3.1](https://github.com/zkat/pacote/compare/v7.3.0...v7.3.1) (2018-02-14)


### Bug Fixes

* **tarball:** stop using mississippi.pipe() in tarball.js and extract.js ([f5c1da9](https://github.com/zkat/pacote/commit/f5c1da9))



<a name="7.3.0"></a>
# [7.3.0](https://github.com/zkat/pacote/compare/v7.2.0...v7.3.0) (2018-02-07)


### Bug Fixes

* **git:** fix resolution of prerelease versions ([#130](https://github.com/zkat/pacote/issues/130)) ([83be46b](https://github.com/zkat/pacote/commit/83be46b)), closes [#129](https://github.com/zkat/pacote/issues/129)


### Features

* **extract:** append _resolved and _integrity automatically ([#134](https://github.com/zkat/pacote/issues/134)) ([6886b65](https://github.com/zkat/pacote/commit/6886b65))



<a name="7.2.0"></a>
# [7.2.0](https://github.com/zkat/pacote/compare/v7.1.1...v7.2.0) (2018-01-19)


### Features

* **resolved:** tarball shortcut when opts.resolved is provided ([46a2f58](https://github.com/zkat/pacote/commit/46a2f58))



<a name="7.1.1"></a>
## [7.1.1](https://github.com/zkat/pacote/compare/v7.1.0...v7.1.1) (2018-01-08)


### Bug Fixes

* **publish:** a spurious file was included in the previous release ([296741a](https://github.com/zkat/pacote/commit/296741a))



<a name="7.1.0"></a>
# [7.1.0](https://github.com/zkat/pacote/compare/v7.0.2...v7.1.0) (2018-01-07)


### Bug Fixes

* **security:** deep-update debug due to vulnerabilities ([ff16da7](https://github.com/zkat/pacote/commit/ff16da7))


### Features

* **resolved:** add opts.resolved for cache stuff ([#131](https://github.com/zkat/pacote/issues/131)) ([149a4b5](https://github.com/zkat/pacote/commit/149a4b5))



<a name="7.0.2"></a>
## [7.0.2](https://github.com/zkat/pacote/compare/v7.0.1...v7.0.2) (2017-11-28)


### Bug Fixes

* **git:** only resolvedRefs can be shallow-cloned ([899720f](https://github.com/zkat/pacote/commit/899720f))



<a name="7.0.1"></a>
## [7.0.1](https://github.com/zkat/pacote/compare/v7.0.0...v7.0.1) (2017-11-15)


### Bug Fixes

* **git:** use resolved ref if available when doing a full clone (#125) ([46ca45a](https://github.com/zkat/pacote/commit/46ca45a)), closes [#125](https://github.com/zkat/pacote/issues/125)
* **move:** bump cacache for some cross-platform move fixes ([eebdcda](https://github.com/zkat/pacote/commit/eebdcda))
* **test:** missed a spot converting tests to promises ([c43caed](https://github.com/zkat/pacote/commit/c43caed))



<a name="7.0.0"></a>
# [7.0.0](https://github.com/zkat/pacote/compare/v6.1.0...v7.0.0) (2017-11-15)


### Bug Fixes

* **docs:** You totally should use pacote now (#126) ([d49a9b5](https://github.com/zkat/pacote/commit/d49a9b5))
* **git:** stop generating integrity for git ([d45363b](https://github.com/zkat/pacote/commit/d45363b))
* **integrity:** stop defaulting to sha1 hashes ([62f8cdf](https://github.com/zkat/pacote/commit/62f8cdf))
* **license:** relicense to MIT for OSI-compat ([ba6b3e0](https://github.com/zkat/pacote/commit/ba6b3e0))


### Features

* **tarball:** add externall pacote.tarball() api ([e30bd49](https://github.com/zkat/pacote/commit/e30bd49))


### prefetch

* deprecate pacote.prefetch ([e47e521](https://github.com/zkat/pacote/commit/e47e521))


### BREAKING CHANGES

* **license:** The license has changed from CC0-1.0 to MIT, which is less permissive and also OSI-approved.
* pacote.prefetch is deprecated in favor of pacote.tarball



<a name="6.1.0"></a>
# [6.1.0](https://github.com/zkat/pacote/compare/v6.0.4...v6.1.0) (2017-10-19)


### Bug Fixes

* **git:** use actual default git branch instead of assuming master (#122) ([79ce949](https://github.com/zkat/pacote/commit/79ce949))
* **npa:** ensure spec is a valid npa instance ([1757b2b](https://github.com/zkat/pacote/commit/1757b2b))


### Features

* **selection:** add opts.includeDeprecated (#123) ([2001549](https://github.com/zkat/pacote/commit/2001549))



<a name="6.0.4"></a>
## [6.0.4](https://github.com/zkat/pacote/compare/v6.0.3...v6.0.4) (2017-10-05)


### Bug Fixes

* **file:** include integrity hash for streamed tarballs too ([030cee7](https://github.com/zkat/pacote/commit/030cee7))



<a name="6.0.3"></a>
## [6.0.3](https://github.com/zkat/pacote/compare/v6.0.2...v6.0.3) (2017-10-05)


### Bug Fixes

* **extract:** clean up mode/fmode/dmode tests ([f915045](https://github.com/zkat/pacote/commit/f915045))
* **file:** make sure file tarballs are written to cache and have integrity data ([dae391a](https://github.com/zkat/pacote/commit/dae391a))
* **git:** version resolution regression from #115 (#119) ([9a68205](https://github.com/zkat/pacote/commit/9a68205))



<a name="6.0.2"></a>
## [6.0.2](https://github.com/zkat/pacote/compare/v6.0.1...v6.0.2) (2017-09-06)


### Bug Fixes

* **extract:** preserve executable perms on extracted files ([19b3dfd](https://github.com/zkat/pacote/commit/19b3dfd))


### Performance Improvements

* replace some calls to .match() with .starts/endsWith() (#115) ([192a02f](https://github.com/zkat/pacote/commit/192a02f))



<a name="6.0.1"></a>
## [6.0.1](https://github.com/zkat/pacote/compare/v6.0.0...v6.0.1) (2017-08-22)


### Bug Fixes

* **finalize:** insist on getting a package.json ([f72ee91](https://github.com/zkat/pacote/commit/f72ee91))



<a name="6.0.0"></a>
# [6.0.0](https://github.com/zkat/pacote/compare/v5.0.1...v6.0.0) (2017-08-19)


### Bug Fixes

* **tar:** bring back the .gitignore -> .npmignore logic (#113) ([0dd518e](https://github.com/zkat/pacote/commit/0dd518e))


### BREAKING CHANGES

* **tar:** this reverts a previous change to disable this feature.



<a name="5.0.1"></a>
## [5.0.1](https://github.com/zkat/pacote/compare/v5.0.0...v5.0.1) (2017-08-17)


### Bug Fixes

* **tar:** chown directories on extract as well ([2fa4598](https://github.com/zkat/pacote/commit/2fa4598))



<a name="5.0.0"></a>
# [5.0.0](https://github.com/zkat/pacote/compare/v4.0.0...v5.0.0) (2017-08-16)


### Bug Fixes

* **registry:** Pass maxSockets options down (#110) ([3f05b79](https://github.com/zkat/pacote/commit/3f05b79))


### Features

* **deps:** replace tar-fs/tar-stream with tar[@3](https://github.com/3) ([28c80a9](https://github.com/zkat/pacote/commit/28c80a9))
* **tar:** switch to tarv3 ([53899c7](https://github.com/zkat/pacote/commit/53899c7))


### BREAKING CHANGES

* **tar:** this changes the underlying tar library, and thus may introduce some subtle low-level incompatibility. Also:

* The tarball packer built into pacote works much closer to how the one npm injects does.
* Special characters on Windows will now be escaped the way tar(1) usually does: by replacing them with the `0xf000` masked character on the way out.
* Directories won't be chowned.



<a name="4.0.0"></a>
# [4.0.0](https://github.com/zkat/pacote/compare/v3.0.0...v4.0.0) (2017-06-29)


### Bug Fixes

* **extract:** revert uid/gid change ([41852e0](https://github.com/zkat/pacote/commit/41852e0))


### BREAKING CHANGES

* **extract:** behavior for setting uid/gid on extracted contents was restored to what it was in pacote@2



<a name="3.0.0"></a>
# [3.0.0](https://github.com/zkat/pacote/compare/v2.7.38...v3.0.0) (2017-06-29)


### Bug Fixes

* **extract:** always extract as current user gid/uid ([6fc01a5](https://github.com/zkat/pacote/commit/6fc01a5))


### BREAKING CHANGES

* **extract:** pacote will no longer set ownership of extracted
contents -- uid/gid will *only* be used for the cache and other internal
details.



<a name="2.7.38"></a>
## [2.7.38](https://github.com/zkat/pacote/compare/v2.7.37...v2.7.38) (2017-06-29)


### Bug Fixes

* **manifest:** bump npm-pick-manifest for loose semver fix ([b3d45ef](https://github.com/zkat/pacote/commit/b3d45ef))



<a name="2.7.37"></a>
## [2.7.37](https://github.com/zkat/pacote/compare/v2.7.36...v2.7.37) (2017-06-29)


### Bug Fixes

* **deps:** bump deps for fixes ([f156655](https://github.com/zkat/pacote/commit/f156655))



<a name="2.7.36"></a>
## [2.7.36](https://github.com/zkat/pacote/compare/v2.7.35...v2.7.36) (2017-06-10)


### Bug Fixes

* **deps:** update tar-fs with the special characters patch (#102) ([ed43aa3](https://github.com/zkat/pacote/commit/ed43aa3))



<a name="2.7.35"></a>
## [2.7.35](https://github.com/zkat/pacote/compare/v2.7.34...v2.7.35) (2017-06-09)


### Bug Fixes

* **registry:** only print one 199 warning (#100) ([b395138](https://github.com/zkat/pacote/commit/b395138))



<a name="2.7.34"></a>
## [2.7.34](https://github.com/zkat/pacote/compare/v2.7.33...v2.7.34) (2017-06-09)


### Bug Fixes

* **git:** whitelist specific shallow-cloneable hosts ([b210cc8](https://github.com/zkat/pacote/commit/b210cc8))



<a name="2.7.33"></a>
## [2.7.33](https://github.com/zkat/pacote/compare/v2.7.32...v2.7.33) (2017-06-08)


### Bug Fixes

* **git:** better error reporting when ls-remote fails ([10aae8f](https://github.com/zkat/pacote/commit/10aae8f))



<a name="2.7.32"></a>
## [2.7.32](https://github.com/zkat/pacote/compare/v2.7.31...v2.7.32) (2017-06-07)


### Bug Fixes

* **registry:** print both 111 and 199 warnings ([2f8c201](https://github.com/zkat/pacote/commit/2f8c201))



<a name="2.7.31"></a>
## [2.7.31](https://github.com/zkat/pacote/compare/v2.7.30...v2.7.31) (2017-06-06)


### Bug Fixes

* **extract:** always return a bluebird promise ([06ca91d](https://github.com/zkat/pacote/commit/06ca91d))
* **registry:** bump make-fetch-happen for local cache header issue fix ([868615c](https://github.com/zkat/pacote/commit/868615c))



<a name="2.7.30"></a>
## [2.7.30](https://github.com/zkat/pacote/compare/v2.7.29...v2.7.30) (2017-06-05)


### Bug Fixes

* **ssri:** bump ssri for bugfix ([70a859c](https://github.com/zkat/pacote/commit/70a859c))



<a name="2.7.29"></a>
## [2.7.29](https://github.com/zkat/pacote/compare/v2.7.28...v2.7.29) (2017-06-05)


### Bug Fixes

* **registry:** use cert instead of certfile opt ([a45880d](https://github.com/zkat/pacote/commit/a45880d))



<a name="2.7.28"></a>
## [2.7.28](https://github.com/zkat/pacote/compare/v2.7.27...v2.7.28) (2017-06-05)


### Bug Fixes

* **git:** limit ls-remote output to heads/tags (#97) ([c1e3dcd](https://github.com/zkat/pacote/commit/c1e3dcd))
* **proxy:** send certificate authority, key and other options (#95) ([c4b6128](https://github.com/zkat/pacote/commit/c4b6128))
* **registry:** add support for global auth and _auth token (#96) ([7919fb7](https://github.com/zkat/pacote/commit/7919fb7))
* **registry:** emit npm-session header (#98) ([9816b18](https://github.com/zkat/pacote/commit/9816b18))



<a name="2.7.27"></a>
## [2.7.27](https://github.com/zkat/pacote/compare/v2.7.26...v2.7.27) (2017-06-01)


### Bug Fixes

* **git:** fix semver range detection. oops ([76d9233](https://github.com/zkat/pacote/commit/76d9233))



<a name="2.7.26"></a>
## [2.7.26](https://github.com/zkat/pacote/compare/v2.7.25...v2.7.26) (2017-06-01)


### Bug Fixes

* **git:** hash was not being replaced/appended correctly ([6fcbed5](https://github.com/zkat/pacote/commit/6fcbed5))



<a name="2.7.25"></a>
## [2.7.25](https://github.com/zkat/pacote/compare/v2.7.24...v2.7.25) (2017-05-31)


### Bug Fixes

* **git:** git deps were getting _resolved without shasums ([96f0675](https://github.com/zkat/pacote/commit/96f0675))



<a name="2.7.24"></a>
## [2.7.24](https://github.com/zkat/pacote/compare/v2.7.23...v2.7.24) (2017-05-31)


### Bug Fixes

* **deps:** update dep versions with new patches ([dc2e4ff](https://github.com/zkat/pacote/commit/dc2e4ff))



<a name="2.7.23"></a>
## [2.7.23](https://github.com/zkat/pacote/compare/v2.7.22...v2.7.23) (2017-05-31)


### Bug Fixes

* **git:** fix ls-remote command and throw away ^{} junk ([62ba84d](https://github.com/zkat/pacote/commit/62ba84d))
* **git:** use the parsed git committish from npa ([77a676a](https://github.com/zkat/pacote/commit/77a676a))



<a name="2.7.22"></a>
## [2.7.22](https://github.com/zkat/pacote/compare/v2.7.21...v2.7.22) (2017-05-31)


### Bug Fixes

* **git:** accept shortened git hashes (#91) ([4466388](https://github.com/zkat/pacote/commit/4466388))



<a name="2.7.21"></a>
## [2.7.21](https://github.com/zkat/pacote/compare/v2.7.20...v2.7.21) (2017-05-25)


### Bug Fixes

* **registry:** stop URIEncoding username/password ([011c9a2](https://github.com/zkat/pacote/commit/011c9a2))



<a name="2.7.20"></a>
## [2.7.20](https://github.com/zkat/pacote/compare/v2.7.19...v2.7.20) (2017-05-25)


### Bug Fixes

* **registry:** encode username and password for auth ([c48b651](https://github.com/zkat/pacote/commit/c48b651))



<a name="2.7.19"></a>
## [2.7.19](https://github.com/zkat/pacote/compare/v2.7.18...v2.7.19) (2017-05-25)


### Bug Fixes

* **registry:** respect alwaysAuth ([150788a](https://github.com/zkat/pacote/commit/150788a))



<a name="2.7.18"></a>
## [2.7.18](https://github.com/zkat/pacote/compare/v2.7.17...v2.7.18) (2017-05-25)


### Bug Fixes

* **cache:** pass uid/gid settings through to mfh ([d8845df](https://github.com/zkat/pacote/commit/d8845df))
* **deps:** update m-f-h for cache opts fix ([faab6cd](https://github.com/zkat/pacote/commit/faab6cd))



<a name="2.7.17"></a>
## [2.7.17](https://github.com/zkat/pacote/compare/v2.7.16...v2.7.17) (2017-05-25)


### Bug Fixes

* **deps:** bump cacache ([34bd656](https://github.com/zkat/pacote/commit/34bd656))



<a name="2.7.16"></a>
## [2.7.16](https://github.com/zkat/pacote/compare/v2.7.15...v2.7.16) (2017-05-24)


### Bug Fixes

* **deps:** pull in various fixes from deps ([4354703](https://github.com/zkat/pacote/commit/4354703))



<a name="2.7.15"></a>
## [2.7.15](https://github.com/zkat/pacote/compare/v2.7.14...v2.7.15) (2017-05-24)


### Bug Fixes

* **proxy:** bump m-f-h with more patches ([26d4170](https://github.com/zkat/pacote/commit/26d4170))



<a name="2.7.14"></a>
## [2.7.14](https://github.com/zkat/pacote/compare/v2.7.13...v2.7.14) (2017-05-24)


### Bug Fixes

* **proxy:** pull in new m-f-h with fixed http proxies ([d6a14e0](https://github.com/zkat/pacote/commit/d6a14e0))



<a name="2.7.13"></a>
## [2.7.13](https://github.com/zkat/pacote/compare/v2.7.12...v2.7.13) (2017-05-23)


### Bug Fixes

* **deps:** bump dep versions to fix http redirect issues ([b23a9fa](https://github.com/zkat/pacote/commit/b23a9fa))



<a name="2.7.12"></a>
## [2.7.12](https://github.com/zkat/pacote/compare/v2.7.11...v2.7.12) (2017-05-16)


### Bug Fixes

* **fetch:** fix default userAgent ([4b9d344](https://github.com/zkat/pacote/commit/4b9d344))
* **registry:** log failed requests too ([0f23f06](https://github.com/zkat/pacote/commit/0f23f06))
* **remote:** send a useful pkg id header for remote tarballs ([ac13356](https://github.com/zkat/pacote/commit/ac13356))



<a name="2.7.11"></a>
## [2.7.11](https://github.com/zkat/pacote/compare/v2.7.10...v2.7.11) (2017-05-12)


### Bug Fixes

* **fetch:** make it play nicer with bundlers ([67cd713](https://github.com/zkat/pacote/commit/67cd713))



<a name="2.7.10"></a>
## [2.7.10](https://github.com/zkat/pacote/compare/v2.7.9...v2.7.10) (2017-05-12)


### Bug Fixes

* **logging:** shhhhhhh ([e7ea56e](https://github.com/zkat/pacote/commit/e7ea56e))
* **manifest:** _resolved is the only main field we do not overwrite ([4c12421](https://github.com/zkat/pacote/commit/4c12421))



<a name="2.7.9"></a>
## [2.7.9](https://github.com/zkat/pacote/compare/v2.7.8...v2.7.9) (2017-05-09)


### Bug Fixes

* **git:** Resolve to ref git specs w/o committishes (#88) ([cb885f5](https://github.com/zkat/pacote/commit/cb885f5)), closes [#88](https://github.com/zkat/pacote/issues/88)



<a name="2.7.8"></a>
## [2.7.8](https://github.com/zkat/pacote/compare/v2.7.7...v2.7.8) (2017-05-07)


### Bug Fixes

* **git:** integrity hash was not always emitted ([97ed9e1](https://github.com/zkat/pacote/commit/97ed9e1))



<a name="2.7.7"></a>
## [2.7.7](https://github.com/zkat/pacote/compare/v2.7.6...v2.7.7) (2017-05-06)


### Bug Fixes

* **auth:** redirects no longer send auth to different host ([82e78c5](https://github.com/zkat/pacote/commit/82e78c5))



<a name="2.7.6"></a>
## [2.7.6](https://github.com/zkat/pacote/compare/v2.7.5...v2.7.6) (2017-05-05)


### Bug Fixes

* **git:** only use longpaths on win32 because old gits ([32846fc](https://github.com/zkat/pacote/commit/32846fc))



<a name="2.7.5"></a>
## [2.7.5](https://github.com/zkat/pacote/compare/v2.7.4...v2.7.5) (2017-05-04)


### Bug Fixes

* **registry-key:** Use pathname instead of path in registryKey (#85) ([5339831](https://github.com/zkat/pacote/commit/5339831))



<a name="2.7.4"></a>
## [2.7.4](https://github.com/zkat/pacote/compare/v2.7.3...v2.7.4) (2017-05-04)


### Bug Fixes

* **pick-manifest:** fix =1.2.3 semver range requests ([dd6911c](https://github.com/zkat/pacote/commit/dd6911c))



<a name="2.7.3"></a>
## [2.7.3](https://github.com/zkat/pacote/compare/v2.7.2...v2.7.3) (2017-05-04)


### Bug Fixes

* **pick-manifest:** spaces in requested version are now trimmed out ([6422b28](https://github.com/zkat/pacote/commit/6422b28))



<a name="2.7.2"></a>
## [2.7.2](https://github.com/zkat/pacote/compare/v2.7.1...v2.7.2) (2017-05-04)


### Bug Fixes

* **extract:** missing or corrupted content properly re-fetched again ([46f60c2](https://github.com/zkat/pacote/commit/46f60c2))



<a name="2.7.1"></a>
## [2.7.1](https://github.com/zkat/pacote/compare/v2.7.0...v2.7.1) (2017-05-01)


### Bug Fixes

* **logging:** log specs correctly on extract ([4b5bab0](https://github.com/zkat/pacote/commit/4b5bab0))
* **manifest:** obey opts.preferOnline when fetching from memoized ([26928a7](https://github.com/zkat/pacote/commit/26928a7))



<a name="2.7.0"></a>
# [2.7.0](https://github.com/zkat/pacote/compare/v2.6.0...v2.7.0) (2017-04-29)


### Bug Fixes

* **registry:** stop using integrity hashes for metadata. again. ([4595ab2](https://github.com/zkat/pacote/commit/4595ab2))


### Features

* **manifest:** include _shasum for legacy compat ([b3a7eed](https://github.com/zkat/pacote/commit/b3a7eed))



<a name="2.6.0"></a>
# [2.6.0](https://github.com/zkat/pacote/compare/v2.5.0...v2.6.0) (2017-04-29)


### Features

* **manifest:** annotate manifests with _from ([e45e968](https://github.com/zkat/pacote/commit/e45e968))



<a name="2.5.0"></a>
# [2.5.0](https://github.com/zkat/pacote/compare/v2.4.0...v2.5.0) (2017-04-28)


### Bug Fixes

* **registry:** JSON text is not a valid header value ([78951ea](https://github.com/zkat/pacote/commit/78951ea))


### Features

* **memoization:** allow injection and control of memoizers ([d8a2be7](https://github.com/zkat/pacote/commit/d8a2be7))



<a name="2.4.0"></a>
# [2.4.0](https://github.com/zkat/pacote/compare/v2.3.2...v2.4.0) (2017-04-27)


### Bug Fixes

* **tests:** nicer error message on registry 404 ([e8e71c8](https://github.com/zkat/pacote/commit/e8e71c8))


### Features

* **auth:** added basic auth and always-auth support ([548aeb5](https://github.com/zkat/pacote/commit/548aeb5))
* **proxy:** proxy support for registry and remote deps ([3766bbb](https://github.com/zkat/pacote/commit/3766bbb))



<a name="2.3.2"></a>
## [2.3.2](https://github.com/zkat/pacote/compare/v2.3.1...v2.3.2) (2017-04-26)


### Bug Fixes

* **deps:** reduce deps size with m-f-h upgrade ([ba75461](https://github.com/zkat/pacote/commit/ba75461))



<a name="2.3.1"></a>
## [2.3.1](https://github.com/zkat/pacote/compare/v2.3.0...v2.3.1) (2017-04-26)


### Bug Fixes

* **git:** another attempt at fixing EPERM b.s. ([e445bef](https://github.com/zkat/pacote/commit/e445bef))



<a name="2.3.0"></a>
# [2.3.0](https://github.com/zkat/pacote/compare/v2.2.2...v2.3.0) (2017-04-26)


### Bug Fixes

* **git:** had ENOTSUP error on windows ([ee17c35](https://github.com/zkat/pacote/commit/ee17c35))
* **memoization:** actually memoize package metadata ([e2078c0](https://github.com/zkat/pacote/commit/e2078c0))


### Features

* **memoization:** better packument memoization + pacote.clearMemoized() ([eb1bd4f](https://github.com/zkat/pacote/commit/eb1bd4f))



<a name="2.2.2"></a>
## [2.2.2](https://github.com/zkat/pacote/compare/v2.2.1...v2.2.2) (2017-04-24)


### Bug Fixes

* **prefetch:** pull in new cacache + fix prefetch hasContent call ([9f476b8](https://github.com/zkat/pacote/commit/9f476b8))



<a name="2.2.1"></a>
## [2.2.1](https://github.com/zkat/pacote/compare/v2.2.0...v2.2.1) (2017-04-23)


### Bug Fixes

* **finalize:** pass on engines/cpu/os ([0a73c78](https://github.com/zkat/pacote/commit/0a73c78))



<a name="2.2.0"></a>
# [2.2.0](https://github.com/zkat/pacote/compare/v2.1.2...v2.2.0) (2017-04-22)


### Bug Fixes

* **git:** fix shortcut fallback order again ([5759d40](https://github.com/zkat/pacote/commit/5759d40))
* **registry:** fix infinite manifetch loop ([6c6a62b](https://github.com/zkat/pacote/commit/6c6a62b))


### Features

* **manifest:** opts.fullMetadata to get unfiltered manifests ([ff2945b](https://github.com/zkat/pacote/commit/ff2945b))



<a name="2.1.2"></a>
## [2.1.2](https://github.com/zkat/pacote/compare/v2.1.1...v2.1.2) (2017-04-20)



<a name="2.1.1"></a>
## [2.1.1](https://github.com/zkat/pacote/compare/v2.1.0...v2.1.1) (2017-04-19)


### Bug Fixes

* **git:** use sshurl instead of ssh for ssh clones ([ff20803](https://github.com/zkat/pacote/commit/ff20803))
* **notice:** only log npm-notice if the packument came from network ([eeeb411](https://github.com/zkat/pacote/commit/eeeb411))
* **registry:** improve 404 error messages ([6a5cbdb](https://github.com/zkat/pacote/commit/6a5cbdb))



<a name="2.1.0"></a>
# [2.1.0](https://github.com/zkat/pacote/compare/v2.0.5...v2.1.0) (2017-04-18)


### Bug Fixes

* **cache:** bump deps for cache fixes ([9596434](https://github.com/zkat/pacote/commit/9596434))


### Features

* **warn:** http warning headers now logged ([f22ce1d](https://github.com/zkat/pacote/commit/f22ce1d))



<a name="2.0.5"></a>
## [2.0.5](https://github.com/zkat/pacote/compare/v2.0.4...v2.0.5) (2017-04-18)


### Bug Fixes

* **file:** oops, the type for these is file ([e7a3d35](https://github.com/zkat/pacote/commit/e7a3d35))



<a name="2.0.4"></a>
## [2.0.4](https://github.com/zkat/pacote/compare/v2.0.3...v2.0.4) (2017-04-18)


### Bug Fixes

* **deps:** remove normalize-git-url ([12d464a](https://github.com/zkat/pacote/commit/12d464a))
* **git:** Correctly read in the HEAD ref after cloning ([dbe1b15](https://github.com/zkat/pacote/commit/dbe1b15))
* **git:** The full clone path doesn't have _resolved set ([ddce561](https://github.com/zkat/pacote/commit/ddce561))
* **manifest:** no _from ever ([15087c4](https://github.com/zkat/pacote/commit/15087c4))



<a name="2.0.3"></a>
## [2.0.3](https://github.com/zkat/pacote/compare/v2.0.2...v2.0.3) (2017-04-15)


### Bug Fixes

* **manifest:** meh just shove _from in there ([4396f09](https://github.com/zkat/pacote/commit/4396f09))
* **registry:** include CI header ([86ad911](https://github.com/zkat/pacote/commit/86ad911))
* **registry:** include npm-scope header ([574cd93](https://github.com/zkat/pacote/commit/574cd93))
* **registry:** make sure to send referer header ([2d3aaac](https://github.com/zkat/pacote/commit/2d3aaac))



<a name="2.0.2"></a>
## [2.0.2](https://github.com/zkat/pacote/compare/v2.0.1...v2.0.2) (2017-04-15)


### Bug Fixes

* **directory:** fix default pack-dir and write a test for it ([9d9266f](https://github.com/zkat/pacote/commit/9d9266f))
* **extract:** brainfart with extractByManifest fixed. lol. ([a1367fb](https://github.com/zkat/pacote/commit/a1367fb))



<a name="2.0.1"></a>
## [2.0.1](https://github.com/zkat/pacote/compare/v2.0.0...v2.0.1) (2017-04-15)


### Bug Fixes

* **tarball:** missed the local->tarball rename ([ac42dc4](https://github.com/zkat/pacote/commit/ac42dc4))



<a name="2.0.0"></a>
# [2.0.0](https://github.com/zkat/pacote/compare/v1.0.0...v2.0.0) (2017-04-15)


### Bug Fixes

* **api:** use npa[@5](https://github.com/5) for spec parsing (#78) ([3f56298](https://github.com/zkat/pacote/commit/3f56298))
* **deprecated:** remove underscore from manifest._deprecated ([9f4af93](https://github.com/zkat/pacote/commit/9f4af93))
* **directory:** add _resolved to directory manifests ([1d305db](https://github.com/zkat/pacote/commit/1d305db))
* **directory:** return null instead of throwing ([d35630d](https://github.com/zkat/pacote/commit/d35630d))
* **finalize:** don't try to cache manifests we can't get a good key for ([8ab1758](https://github.com/zkat/pacote/commit/8ab1758))
* **finalize:** refactored finalize-manifest code + add _integrity=false sentinel ([657b7fa](https://github.com/zkat/pacote/commit/657b7fa))
* **git:** cleaner handling of git tarball streams when caching ([11acd0a](https://github.com/zkat/pacote/commit/11acd0a))
* **git:** emit manifests from git tarball handler ([b139d4b](https://github.com/zkat/pacote/commit/b139d4b))
* **git:** fix .git exclusion, set mtime = 0 to make tarballs idempotent ([9a9fa1b](https://github.com/zkat/pacote/commit/9a9fa1b))
* **git:** fix fallback order and only fall back on hosted shortcuts ([551cb33](https://github.com/zkat/pacote/commit/551cb33))
* **git:** fix filling-out of git manifests ([95e807c](https://github.com/zkat/pacote/commit/95e807c))
* **git:** got dir packer option working with git ([7669b3e](https://github.com/zkat/pacote/commit/7669b3e))
* **headers:** nudge around some headers to make things behave ([db1e0a1](https://github.com/zkat/pacote/commit/db1e0a1))
* **manifest:** get rid of resolved-with-non-error warning ([d4d4917](https://github.com/zkat/pacote/commit/d4d4917))
* **manifest:** stop using digest for manifests ([4ddd2f5](https://github.com/zkat/pacote/commit/4ddd2f5))
* **opts:** bring opt-check up to date ([564419e](https://github.com/zkat/pacote/commit/564419e))
* **opts:** rename refreshCache to preferOnline cause much clearer ([94171d6](https://github.com/zkat/pacote/commit/94171d6))
* **prefetch:** fall back to the _integrity in the manifest if none calculated ([083ac79](https://github.com/zkat/pacote/commit/083ac79))
* **prefetch:** if there's no stream, just skip (for directory) ([714de91](https://github.com/zkat/pacote/commit/714de91))
* **registry:** fix error handling for registry tarballs ([e69539f](https://github.com/zkat/pacote/commit/e69539f))
* **registry:** nudging logging stuff around a bit ([61d62cc](https://github.com/zkat/pacote/commit/61d62cc))
* **registry:** only send auth info if tarball is hosted on the same registry ([1de5a2b](https://github.com/zkat/pacote/commit/1de5a2b))
* **registry:** redirect tarball urls to provided registry port+protocol if same host ([f50167e](https://github.com/zkat/pacote/commit/f50167e))
* **registry:** support memoizing packuments ([e7fff31](https://github.com/zkat/pacote/commit/e7fff31))
* **registry:** treat registry cache as "private" -- bumps m-f-h ([6fa1503](https://github.com/zkat/pacote/commit/6fa1503))


### Features

* **directory:** implement local dir packing ([017d989](https://github.com/zkat/pacote/commit/017d989))
* **fetch:** bump make-fetch-happen for new restarts ([cf90716](https://github.com/zkat/pacote/commit/cf90716))
* **git:** support pulling in git submodules ([5825d33](https://github.com/zkat/pacote/commit/5825d33))
* **integrity:** replace http client (#72) ([189cdd2](https://github.com/zkat/pacote/commit/189cdd2))
* **prefetch:** return cache-related info on prefetch ([623b7f3](https://github.com/zkat/pacote/commit/623b7f3))
* **registry:** allow injection of request agents ([805e5ae](https://github.com/zkat/pacote/commit/805e5ae))
* **registry:** fast request pooling ([321f84b](https://github.com/zkat/pacote/commit/321f84b))
* **registry:** registry requests now follow cache spec more closely, respect Age, etc ([9e47098](https://github.com/zkat/pacote/commit/9e47098))


### BREAKING CHANGES

* **api:** spec objects can no longer be realize-package-specifier objects. Pass a string or generate npa@>=5 spec objects to pass in.
* **integrity:** This PR replaces a pretty fundamental chunk of pacote.

* Caching now follows standard-ish cache rules for http-related requests.

* manifest() no longer includes the `_shasum` field. It's been replaced by `_integrity`, which is a Subresource Integrity hash string containing equivalent data. These strings can be parsed and managed using https://npm.im/ssri.

* Any functions that accepted `opts.digest` and/or `opts.hashAlgorithm` now expect `opts.integrity` instead.

* Packuments and finalized manifests are now cached using sha512. Tarballs can start using that hash (or any other more secure hash) once registries start supporting them: `packument.dist.integrity` will be prioritized over `packument.shasum`.

* If opts.offline is used, a `ENOCACHE` error will be returned.



<a name="1.0.0"></a>
# [1.0.0](https://github.com/zkat/pacote/compare/v0.1.1...v1.0.0) (2017-03-17)


### Bug Fixes

* **extract-stream:** adapt to tar-fs api ([aa21308](https://github.com/zkat/pacote/commit/aa21308))
* add 'use strict' to all .js files (#26) ([021bd59](https://github.com/zkat/pacote/commit/021bd59))
* **cache:** this is really a user error, so just throw ([5c9c0fa](https://github.com/zkat/pacote/commit/5c9c0fa))
* **deps:** cacache[@5](https://github.com/5).0.3 ([37cddc5](https://github.com/zkat/pacote/commit/37cddc5))
* **deps:** tar-fs[@1](https://github.com/1).15.1 ([e0d853a](https://github.com/zkat/pacote/commit/e0d853a))
* **docs:** correct fixtures table (#57) ([23d2eb4](https://github.com/zkat/pacote/commit/23d2eb4))
* **extract:** correctly detect digest cache misses ([ec6672b](https://github.com/zkat/pacote/commit/ec6672b))
* **extract:** fixed race condition ([14fd2a8](https://github.com/zkat/pacote/commit/14fd2a8))
* **finalize-manifest:** use digest to uniquify cached manifests ([931a9cb](https://github.com/zkat/pacote/commit/931a9cb))
* **http:** Fixed cache-related race condition ([b70a4b1](https://github.com/zkat/pacote/commit/b70a4b1))
* **manifest:** dir manifests should throw ENOPACKAGEJSON ([b06882d](https://github.com/zkat/pacote/commit/b06882d))
* **manifest:** ETARGET when no packages match ([ea2127d](https://github.com/zkat/pacote/commit/ea2127d))
* **manifest:** local manifest fn should return a promise ([c700622](https://github.com/zkat/pacote/commit/c700622))
* **manifest:** retry registry manifests once on ETARGET (#66) ([3b99adc](https://github.com/zkat/pacote/commit/3b99adc))
* **prefetch:** hashAlgorithm is required for hasContent ([f03d51c](https://github.com/zkat/pacote/commit/f03d51c))
* **request:** report cache write errors on end ([c102b86](https://github.com/zkat/pacote/commit/c102b86))


### Features

* **api:** support pre-realized specifiers as specs (#62) ([1d5bf39](https://github.com/zkat/pacote/commit/1d5bf39))
* **cache:** grabbing info and hasContent ([a559711](https://github.com/zkat/pacote/commit/a559711))
* **deps:** minimatch[@3](https://github.com/3).0.3 ([2bb8cd5](https://github.com/zkat/pacote/commit/2bb8cd5))
* **deps:** normalize-package-data[@2](https://github.com/2).3.5 ([4250e0d](https://github.com/zkat/pacote/commit/4250e0d))
* **directory:** directory dep support (#68) ([6d5307a](https://github.com/zkat/pacote/commit/6d5307a))
* **git:** baseline git support (#69) ([6d7eaf5](https://github.com/zkat/pacote/commit/6d7eaf5))
* **handlers:** added remote tarball support (#64) ([add1808](https://github.com/zkat/pacote/commit/add1808))
* **local:** local tarball support (#67) ([e50d625](https://github.com/zkat/pacote/commit/e50d625))
* **manifest:** handle deprecation notice (#60) ([db82dae](https://github.com/zkat/pacote/commit/db82dae))
* **manifest:** standardize manifest format ([3dd9a72](https://github.com/zkat/pacote/commit/3dd9a72))
* **manifest:** switch to cacache for caching ([8ba7249](https://github.com/zkat/pacote/commit/8ba7249))
* **prefetch:** added tarball prefetch support ([26c34ce](https://github.com/zkat/pacote/commit/26c34ce))
* **request:** accept maxSockets opt ([3987807](https://github.com/zkat/pacote/commit/3987807))
* **scopes:** new scopeTargets option (#59) ([b5db7ae](https://github.com/zkat/pacote/commit/b5db7ae))


### Performance Improvements

* **finalize-manifest:** cache finalized manifests ([fa3c430](https://github.com/zkat/pacote/commit/fa3c430))


### BREAKING CHANGES

* **manifest:** Toplevel APIs now return Promises instead of using callbacks.
