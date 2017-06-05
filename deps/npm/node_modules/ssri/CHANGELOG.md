# Change Log

All notable changes to this project will be documented in this file. See [standard-version](https://github.com/conventional-changelog/standard-version) for commit guidelines.

<a name="4.1.5"></a>
## [4.1.5](https://github.com/zkat/ssri/compare/v4.1.4...v4.1.5) (2017-06-05)


### Bug Fixes

* **integrityStream:** stop crashing if opts.algorithms and opts.integrity have an algo mismatch ([fb1293e](https://github.com/zkat/ssri/commit/fb1293e))



<a name="4.1.4"></a>
## [4.1.4](https://github.com/zkat/ssri/compare/v4.1.3...v4.1.4) (2017-05-31)


### Bug Fixes

* **node:** older versions of node[@4](https://github.com/4) do not support base64buffer string parsing ([513df4e](https://github.com/zkat/ssri/commit/513df4e))



<a name="4.1.3"></a>
## [4.1.3](https://github.com/zkat/ssri/compare/v4.1.2...v4.1.3) (2017-05-24)


### Bug Fixes

* **check:** handle various bad hash corner cases better ([c2c262b](https://github.com/zkat/ssri/commit/c2c262b))



<a name="4.1.2"></a>
## [4.1.2](https://github.com/zkat/ssri/compare/v4.1.1...v4.1.2) (2017-04-18)


### Bug Fixes

* **stream:** _flush can be called multiple times. use on("end") ([b1c4805](https://github.com/zkat/ssri/commit/b1c4805))



<a name="4.1.1"></a>
## [4.1.1](https://github.com/zkat/ssri/compare/v4.1.0...v4.1.1) (2017-04-12)


### Bug Fixes

* **pickAlgorithm:** error if pickAlgorithm() is used in an empty Integrity ([fab470e](https://github.com/zkat/ssri/commit/fab470e))



<a name="4.1.0"></a>
# [4.1.0](https://github.com/zkat/ssri/compare/v4.0.0...v4.1.0) (2017-04-07)


### Features

* adding ssri.create for a crypto style interface (#2) ([96f52ad](https://github.com/zkat/ssri/commit/96f52ad))



<a name="4.0.0"></a>
# [4.0.0](https://github.com/zkat/ssri/compare/v3.0.2...v4.0.0) (2017-04-03)


### Bug Fixes

* **integrity:** should have changed the error code before. oops ([8381afa](https://github.com/zkat/ssri/commit/8381afa))


### BREAKING CHANGES

* **integrity:** EBADCHECKSUM -> EINTEGRITY for verification errors



<a name="3.0.2"></a>
## [3.0.2](https://github.com/zkat/ssri/compare/v3.0.1...v3.0.2) (2017-04-03)



<a name="3.0.1"></a>
## [3.0.1](https://github.com/zkat/ssri/compare/v3.0.0...v3.0.1) (2017-04-03)


### Bug Fixes

* **package.json:** really should have these in the keywords because search ([a6ac6d0](https://github.com/zkat/ssri/commit/a6ac6d0))



<a name="3.0.0"></a>
# [3.0.0](https://github.com/zkat/ssri/compare/v2.0.0...v3.0.0) (2017-04-03)


### Bug Fixes

* **hashes:** IntegrityMetadata -> Hash ([d04aa1f](https://github.com/zkat/ssri/commit/d04aa1f))


### Features

* **check:** return IntegrityMetadata on check success ([2301e74](https://github.com/zkat/ssri/commit/2301e74))
* **fromHex:** ssri.fromHex to make it easier to generate them from hex valus ([049b89e](https://github.com/zkat/ssri/commit/049b89e))
* **hex:** utility function for getting hex version of digest ([a9f021c](https://github.com/zkat/ssri/commit/a9f021c))
* **hexDigest:** added hexDigest method to Integrity objects too ([85208ba](https://github.com/zkat/ssri/commit/85208ba))
* **integrity:** add .isIntegrity and .isIntegrityMetadata ([1b29e6f](https://github.com/zkat/ssri/commit/1b29e6f))
* **integrityStream:** new stream that can both generate and check streamed data ([fd23e1b](https://github.com/zkat/ssri/commit/fd23e1b))
* **parse:** allow parsing straight into a single IntegrityMetadata object ([c8ddf48](https://github.com/zkat/ssri/commit/c8ddf48))
* **pickAlgorithm:** Intergrity#pickAlgorithm() added ([b97a796](https://github.com/zkat/ssri/commit/b97a796))
* **size:** calculate and update stream sizes ([02ed1ad](https://github.com/zkat/ssri/commit/02ed1ad))


### BREAKING CHANGES

* **hashes:** `.isIntegrityMetadata` is now `.isHash`. Also, any references to `IntegrityMetadata` now refer to `Hash`.
* **integrityStream:** createCheckerStream has been removed and replaced with a general-purpose integrityStream.

To convert existing createCheckerStream code, move the `sri` argument into `opts.integrity` in integrityStream. All other options should be the same.
* **check:** `checkData`, `checkStream`, and `createCheckerStream` now yield a whole IntegrityMetadata instance representing the first successful hash match.



<a name="2.0.0"></a>
# [2.0.0](https://github.com/zkat/ssri/compare/v1.0.0...v2.0.0) (2017-03-24)


### Bug Fixes

* **strict-mode:** make regexes more rigid ([122a32c](https://github.com/zkat/ssri/commit/122a32c))


### Features

* **api:** added serialize alias for unparse ([999b421](https://github.com/zkat/ssri/commit/999b421))
* **concat:** add Integrity#concat() ([cae12c7](https://github.com/zkat/ssri/commit/cae12c7))
* **pickAlgo:** pick the strongest algorithm provided, by default ([58c18f7](https://github.com/zkat/ssri/commit/58c18f7))
* **strict-mode:** strict SRI support ([3f0b64c](https://github.com/zkat/ssri/commit/3f0b64c))
* **stringify:** replaced unparse/serialize with stringify ([4acad30](https://github.com/zkat/ssri/commit/4acad30))
* **verification:** add opts.pickAlgorithm ([f72e658](https://github.com/zkat/ssri/commit/f72e658))


### BREAKING CHANGES

* **pickAlgo:** ssri will prioritize specific hashes now
* **stringify:** serialize and unparse have been removed. Use ssri.stringify instead.
* **strict-mode:** functions that accepted an optional `sep` argument now expect `opts.sep`.



<a name="1.0.0"></a>
# 1.0.0 (2017-03-23)


### Features

* **api:** implemented initial api ([4fbb16b](https://github.com/zkat/ssri/commit/4fbb16b))


### BREAKING CHANGES

* **api:** Initial API established.
