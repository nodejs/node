# Change Log
All notable changes to this project will be documented in this file.
This project adheres to [Semantic Versioning](http://semver.org/).
This change log adheres to standards from [Keep a CHANGELOG](http://keepachangelog.com).

## [4.3.0] - 2016-04-07
### Added
* Add `require-render-return` rule ([#482][] @shmuga)
* Add auto fix for `jsx-equals-spacing` ([#506][] @peet)
* Add auto fix for `jsx-closing-bracket-location` ([#511][] @KevinGrandon)

### Fixed
* Fix `prefer-stateless-function` for conditional JSX ([#516][])
* Fix `jsx-pascal-case` to support single letter component names ([#505][] @dthielman)

### Changed
* Update dependencies
* Documentation improvements ([#509][] @coryhouse, [#526][] @ahoym)

[4.3.0]: https://github.com/yannickcr/eslint-plugin-react/compare/v4.2.3...v4.3.0
[#482]: https://github.com/yannickcr/eslint-plugin-react/issues/482
[#506]: https://github.com/yannickcr/eslint-plugin-react/pull/506
[#511]: https://github.com/yannickcr/eslint-plugin-react/pull/511
[#516]: https://github.com/yannickcr/eslint-plugin-react/issues/516
[#505]: https://github.com/yannickcr/eslint-plugin-react/issues/505
[#509]: https://github.com/yannickcr/eslint-plugin-react/pull/509
[#526]: https://github.com/yannickcr/eslint-plugin-react/pull/526

## [4.2.3] - 2016-03-15
### Fixed
* Fix class properties retrieval in `prefer-stateless-function` ([#499][])

[4.2.3]: https://github.com/yannickcr/eslint-plugin-react/compare/v4.2.2...v4.2.3
[#499]: https://github.com/yannickcr/eslint-plugin-react/issues/499

## [4.2.2] - 2016-03-14
### Fixed
* Rewrite `prefer-stateless-function` rule ([#491][])
* Fix `self-closing-comp` to treat non-breaking spaces as content ([#496][])
* Fix detection for direct props in `prop-types` ([#497][])
* Fix annotated function detection in `prop-types` ([#498][])
* Fix `this` usage in `jsx-no-undef` ([#489][])

### Changed
* Update dependencies
* Add shared setting for React version

[4.2.2]: https://github.com/yannickcr/eslint-plugin-react/compare/v4.2.1...v4.2.2
[#491]: https://github.com/yannickcr/eslint-plugin-react/issues/491
[#496]: https://github.com/yannickcr/eslint-plugin-react/issues/496
[#497]: https://github.com/yannickcr/eslint-plugin-react/issues/497
[#498]: https://github.com/yannickcr/eslint-plugin-react/issues/498
[#489]: https://github.com/yannickcr/eslint-plugin-react/issues/489

## [4.2.1] - 2016-03-08
### Fixed
* Fix `sort-prop-types` crash with spread operator ([#478][])
* Fix stateless components detection when conditionally returning JSX ([#486][])
* Fix case where props were not assigned to the right component ([#485][])
* Fix missing `getChildContext` lifecycle method in `prefer-stateless-function` ([#492][])

[4.2.1]: https://github.com/yannickcr/eslint-plugin-react/compare/v4.2.0...v4.2.1
[#478]: https://github.com/yannickcr/eslint-plugin-react/issues/478
[#486]: https://github.com/yannickcr/eslint-plugin-react/issues/486
[#485]: https://github.com/yannickcr/eslint-plugin-react/issues/485
[#492]: https://github.com/yannickcr/eslint-plugin-react/issues/492

## [4.2.0] - 2016-03-05
### Added
* Add support for Flow annotations on stateless components ([#467][])
* Add `prefer-stateless-function` rule ([#214][])
* Add auto fix for `jsx-indent-props` ([#483][] @shioju)

### Fixed
* Fix `jsx-no-undef` crash on objects ([#469][])
* Fix propTypes detection when declared before the component ([#472][])

### Changed
* Update dependencies
* Documentation improvements ([#464][] @alex-tan, [#466][] @awong-dev, [#470][] @Gpx; [#462][] @thaggie)

[4.2.0]: https://github.com/yannickcr/eslint-plugin-react/compare/v4.1.0...v4.2.0
[#467]: https://github.com/yannickcr/eslint-plugin-react/issues/467
[#214]: https://github.com/yannickcr/eslint-plugin-react/issues/214
[#483]: https://github.com/yannickcr/eslint-plugin-react/pull/483
[#469]: https://github.com/yannickcr/eslint-plugin-react/issues/469
[#472]: https://github.com/yannickcr/eslint-plugin-react/issues/472
[#464]: https://github.com/yannickcr/eslint-plugin-react/pull/464
[#466]: https://github.com/yannickcr/eslint-plugin-react/pull/466
[#470]: https://github.com/yannickcr/eslint-plugin-react/pull/470
[#462]: https://github.com/yannickcr/eslint-plugin-react/pull/462

## [4.1.0] - 2016-02-23
### Added
* Add component detection for class expressions
* Add displayName detection for class expressions in `display-name` ([#419][])

### Fixed
* Fix used props detection in components for which we are not confident in `prop-types` ([#420][])
* Fix false positive in `jsx-key` ([#456][] @jkimbo)

### Changed
* Documentation improvements ([#457][] @wyze)

[4.1.0]: https://github.com/yannickcr/eslint-plugin-react/compare/v4.0.0...v4.1.0
[#419]: https://github.com/yannickcr/eslint-plugin-react/issues/419
[#420]: https://github.com/yannickcr/eslint-plugin-react/issues/420
[#456]: https://github.com/yannickcr/eslint-plugin-react/pull/456
[#457]: https://github.com/yannickcr/eslint-plugin-react/pull/457

## [4.0.0] - 2016-02-19
### Added
* Add `jsx-space-before-closing` rule ([#244][] @ryym)
* Add support for destructing in function signatures to `prop-types` ([#354][] @lencioni)

### Breaking
* Add support for static methods to `sort-comp`. Static methods must now be declared first, see [rule documentation](docs/rules/sort-comp.md) ([#128][] @lencioni)
* Add shareable config in place of default configuration. `jsx-uses-vars` is not enabled by default anymore, see [documentation](README.md#recommended-configuration) ([#192][])
* Rename `jsx-sort-prop-types` to `sort-prop-types`. `jsx-sort-prop-types` still works but will trigger a warning ([#87][] @lencioni)
* Remove deprecated `jsx-quotes` rule ([#433][] @lencioni)
* `display-name` now accept the transpiler name by default. You can use the `ignoreTranspilerName` option to get the old behavior, see [rule documentation](docs/rules/display-name.md#ignoretranspilername) ([#440][] @lencioni)

### Fixed
* Only ignore lowercase JSXIdentifier in `jsx-no-undef` ([#435][])
* Fix `jsx-handler-names` regex ([#425][])
* Fix destructured props detection in `prop-types` ([#443][])

### Changed
* Update dependencies ([#426][] @quentin-)
* Documentation improvements ([#414][] @vkrol, [#370][] @tmcw, [#441][] [#429][] @lencioni, [#432][] @note89, [#438][] @jmann6)

[4.0.0]: https://github.com/yannickcr/eslint-plugin-react/compare/v3.16.1...v4.0.0
[#244]: https://github.com/yannickcr/eslint-plugin-react/issues/244
[#354]: https://github.com/yannickcr/eslint-plugin-react/issues/354
[#128]: https://github.com/yannickcr/eslint-plugin-react/issues/128
[#192]: https://github.com/yannickcr/eslint-plugin-react/issues/192
[#87]: https://github.com/yannickcr/eslint-plugin-react/issues/87
[#440]: https://github.com/yannickcr/eslint-plugin-react/pull/440
[#435]: https://github.com/yannickcr/eslint-plugin-react/issues/435
[#425]: https://github.com/yannickcr/eslint-plugin-react/issues/425
[#443]: https://github.com/yannickcr/eslint-plugin-react/issues/443
[#426]: https://github.com/yannickcr/eslint-plugin-react/pull/426
[#414]: https://github.com/yannickcr/eslint-plugin-react/pull/414
[#370]: https://github.com/yannickcr/eslint-plugin-react/pull/370
[#441]: https://github.com/yannickcr/eslint-plugin-react/pull/441
[#429]: https://github.com/yannickcr/eslint-plugin-react/pull/429
[#432]: https://github.com/yannickcr/eslint-plugin-react/pull/432
[#438]: https://github.com/yannickcr/eslint-plugin-react/pull/438
[#433]: https://github.com/yannickcr/eslint-plugin-react/pull/433

## [3.16.1] - 2016-01-24
### Fixed
* Fix `jsx-sort-prop-types` issue with custom propTypes ([#408][] @alitaheri)

[3.16.1]: https://github.com/yannickcr/eslint-plugin-react/compare/v3.16.0...v3.16.1
[#408]: https://github.com/yannickcr/eslint-plugin-react/issues/408

## [3.16.0] - 2016-01-24
### Added
* Add `jsx-equals-spacing` rule ([#394][] @ryym)
* Add auto fix for `wrap-multiline`
* Add auto fix for `jsx-boolean-value`
* Add auto fix for `no-unknown-property`
* Add auto fix for `jsx-curly-spacing` ([#407][] @ewendel)
* Add `requiredFirst` option to `jsx-sort-prop-types` ([#392][] @chrislaskey)
* Add `ignoreRefs` option to `jsx-no-bind` ([#330][] @silvenon)

### Fixed
* Ignore `ref` in `jsx-handler-names` (again) ([#396][])

### Changed
* Update dependencies

[3.16.0]: https://github.com/yannickcr/eslint-plugin-react/compare/v3.15.0...v3.16.0
[#394]: https://github.com/yannickcr/eslint-plugin-react/issues/394
[#407]: https://github.com/yannickcr/eslint-plugin-react/pull/407
[#392]: https://github.com/yannickcr/eslint-plugin-react/pull/392
[#330]: https://github.com/yannickcr/eslint-plugin-react/issues/330
[#396]: https://github.com/yannickcr/eslint-plugin-react/issues/396

## [3.15.0] - 2016-01-12
### Added
* Add support for flow annotations to `prop-types` ([#382][] @phpnode)

### Fixed
* Fix `prop-types` crash when initializing class variable with an empty object ([#383][])
* Fix `prop-types` crash when propTypes are using the spread operator ([#389][])

### Changed
* Improve `sort-comp` error messages ([#372][] @SystemParadox)
* Update dependencies

[3.15.0]: https://github.com/yannickcr/eslint-plugin-react/compare/v3.14.0...v3.15.0
[#382]: https://github.com/yannickcr/eslint-plugin-react/pull/382
[#383]: https://github.com/yannickcr/eslint-plugin-react/issues/383
[#389]: https://github.com/yannickcr/eslint-plugin-react/issues/389
[#372]: https://github.com/yannickcr/eslint-plugin-react/pull/372

## [3.14.0] - 2016-01-05
### Added
* Add `jsx-indent` rule ([#342][])
* Add shared setting for pragma configuration ([#228][] @NickStefan)

### Fixed
* Fix crash in `jsx-key` ([#380][] @nfcampos)
* Fix crash in `forbid-prop-types` ([#377][] @nfcampos)
* Ignore `ref` in `jsx-handler-names` ([#375][])

### Changed
* Add AppVeyor CI to run tests on a Windows platform
* Add `sort-comp` codemod to `sort-comp` documentation ([#381][] @turadg)

[3.14.0]: https://github.com/yannickcr/eslint-plugin-react/compare/v3.13.1...v3.14.0
[#342]: https://github.com/yannickcr/eslint-plugin-react/issues/342
[#228]: https://github.com/yannickcr/eslint-plugin-react/issues/228
[#380]: https://github.com/yannickcr/eslint-plugin-react/pull/380
[#377]: https://github.com/yannickcr/eslint-plugin-react/pull/377
[#375]: https://github.com/yannickcr/eslint-plugin-react/issues/375
[#381]: https://github.com/yannickcr/eslint-plugin-react/pull/381

## [3.13.1] - 2015-12-26
### Fixed
* Fix crash in `jsx-key` ([#373][] @lukekarrys)

[3.13.1]: https://github.com/yannickcr/eslint-plugin-react/compare/v3.13.0...v3.13.1
[#373]: https://github.com/yannickcr/eslint-plugin-react/issues/373

## [3.13.0] - 2015-12-24
### Added
* Add `no-string-refs` rule ([#341][] @Intellicode)
* Add support for propTypes assigned via a variable in `prop-types` ([#355][])

### Fixed
* Fix `never` option in `prefer-es6-class`
* Fix `jsx-key` false-positives ([#320][] @silvenon)

### Changed
* Documentation improvements ([#368][] @lencioni, [#370][] @tmcw, [#371][])
* Update dependencies

[3.13.0]: https://github.com/yannickcr/eslint-plugin-react/compare/v3.12.0...v3.13.0
[#341]: https://github.com/yannickcr/eslint-plugin-react/issues/341
[#355]: https://github.com/yannickcr/eslint-plugin-react/issues/355
[#320]: https://github.com/yannickcr/eslint-plugin-react/issues/320

[#368]: https://github.com/yannickcr/eslint-plugin-react/pull/368
[#370]: https://github.com/yannickcr/eslint-plugin-react/pull/370
[#371]: https://github.com/yannickcr/eslint-plugin-react/issues/371

## [3.12.0] - 2015-12-20
### Added
* Add `no-deprecated` rule ([#356][] @graue)
* Add `no-is-mounted` rule ([#37][] @lencioni)
* Add `never` option to `prefer-es6-class` rule ([#359][] @pwmckenna)

### Fixed
* Fix `jsx-pascal-case` to stop checking lower cased components ([#329][])
* Fix crash in component detection class ([#364][])

### Changed
* Add link to [eslint-plugin-react-native](https://github.com/Intellicode/eslint-plugin-react-native) in Readme
* Update dependencies

[3.12.0]: https://github.com/yannickcr/eslint-plugin-react/compare/v3.11.3...v3.12.0
[#356]: https://github.com/yannickcr/eslint-plugin-react/pull/356
[#37]: https://github.com/yannickcr/eslint-plugin-react/issues/37
[#359]: https://github.com/yannickcr/eslint-plugin-react/pull/359
[#329]: https://github.com/yannickcr/eslint-plugin-react/issues/329
[#364]: https://github.com/yannickcr/eslint-plugin-react/issues/364

## [3.11.3] - 2015-12-05
### Fixed
* Fix crash in `prop-types` when reassigning props ([#345][])
* Fix `jsx-handler-names` for stateless components ([#346][])

### Changed
* Update `jsx-handler-names` error messages to be less specific ([#348][] @jakemmarsh)

[3.11.3]: https://github.com/yannickcr/eslint-plugin-react/compare/v3.11.2...v3.11.3
[#345]: https://github.com/yannickcr/eslint-plugin-react/issues/345
[#346]: https://github.com/yannickcr/eslint-plugin-react/issues/346
[#348]: https://github.com/yannickcr/eslint-plugin-react/pull/348

## [3.11.2] - 2015-12-01
### Fixed
* Allow numbers in `jsx-pascal-case` ([#339][])
* Fix `jsx-handler-names` crash with arrays ([#340][])

### Changed
* Add allow-in-func option to `no-did-update-set-state` documentation

[3.11.2]: https://github.com/yannickcr/eslint-plugin-react/compare/v3.11.1...v3.11.2
[#339]: https://github.com/yannickcr/eslint-plugin-react/issues/339
[#340]: https://github.com/yannickcr/eslint-plugin-react/issues/340

## [3.11.1] - 2015-11-29
### Fixed
* Fix SVG attributes support for `no-unknown-property` ([#338][])

[3.11.1]: https://github.com/yannickcr/eslint-plugin-react/compare/v3.11.0...v3.11.1
[#338]: https://github.com/yannickcr/eslint-plugin-react/issues/338

## [3.11.0] - 2015-11-29
### Added
* Add `jsx-handler-names` rule ([#315][] @jakemmarsh)
* Add SVG attributes support to `no-unknown-property` ([#318][])
* Add shorthandFirst option to `jsx-sort-props` ([#336][] @lucasmotta)

### Fixed
* Fix destructured props detection in stateless components ([#326][])
* Fix props validation for nested stateless components ([#331][])
* Fix `require-extension` to ignore extension if it's part of the package name ([#319][])

### Changed
* Allow consecutive uppercase letters in `jsx-pascal-case` ([#328][] @lencioni)
* Update dependencies

[3.11.0]: https://github.com/yannickcr/eslint-plugin-react/compare/v3.10.0...v3.11.0
[#315]: https://github.com/yannickcr/eslint-plugin-react/pull/315
[#318]: https://github.com/yannickcr/eslint-plugin-react/issues/318
[#336]: https://github.com/yannickcr/eslint-plugin-react/pull/336
[#326]: https://github.com/yannickcr/eslint-plugin-react/issues/326
[#331]: https://github.com/yannickcr/eslint-plugin-react/issues/331
[#319]: https://github.com/yannickcr/eslint-plugin-react/issues/319
[#328]: https://github.com/yannickcr/eslint-plugin-react/issues/328

## [3.10.0] - 2015-11-21
### Added
* Add `jsx-pascal-case` rule ([#306][] @jakemmarsh)

### Fixed
* Fix crash on incomplete class property declaration ([#317][] @dapetcu21)
* Fix crash with ESLint 1.10.0 ([#323][] @lukekarrys)

[3.10.0]: https://github.com/yannickcr/eslint-plugin-react/compare/v3.9.0...v3.10.0
[#306]: https://github.com/yannickcr/eslint-plugin-react/pull/306
[#317]: https://github.com/yannickcr/eslint-plugin-react/issues/317
[#323]: https://github.com/yannickcr/eslint-plugin-react/issues/323

## [3.9.0] - 2015-11-17
### Added
* Add `jsx-key` rule ([#293][] @benmosher)
* Add `allow-in-func` option to `no-did-update-set-state` ([#300][])
* Add option to only enforce `jsx-closing-bracket-location` rule to one type of tag (nonEmpty or selfClosing) ([#307][])

### Fixed
* Fix crash when destructuring with only the rest spread ([#269][])
* Fix variables detection when searching for related components ([#303][])
* Fix `no-unknown-property` to not check custom elements ([#308][] @zertosh)

### Changed
* Improve `jsx-closing-bracket-location` error message ([#301][] @alopatin)
* Update dependencies

[3.9.0]: https://github.com/yannickcr/eslint-plugin-react/compare/v3.8.0...v3.9.0
[#293]: https://github.com/yannickcr/eslint-plugin-react/pull/293
[#300]: https://github.com/yannickcr/eslint-plugin-react/issues/300
[#307]: https://github.com/yannickcr/eslint-plugin-react/issues/307
[#269]: https://github.com/yannickcr/eslint-plugin-react/issues/269
[#303]: https://github.com/yannickcr/eslint-plugin-react/issues/303
[#308]: https://github.com/yannickcr/eslint-plugin-react/pull/308
[#301]: https://github.com/yannickcr/eslint-plugin-react/pull/301

## [3.8.0] - 2015-11-07
### Added
* Add ignoreStateless option to `no-multi-comp` ([#290][])

### Fixed
* Fix classes with properties to always be marked as components ([#291][])
* Fix ES5 class detection when using `createClass` by itself ([#297][])
* Fix direct props detection ([#298][])
* Ignore functions containing the keyword `this` during component detection

[3.8.0]: https://github.com/yannickcr/eslint-plugin-react/compare/v3.7.1...v3.8.0
[#290]: https://github.com/yannickcr/eslint-plugin-react/issues/290
[#291]: https://github.com/yannickcr/eslint-plugin-react/issues/291
[#297]: https://github.com/yannickcr/eslint-plugin-react/issues/297
[#298]: https://github.com/yannickcr/eslint-plugin-react/issues/298

## [3.7.1] - 2015-11-05
### Fixed
* Fix `sort-comp` crash on stateless components ([#285][])
* Fix crash in ES5 components detection ([#286][])
* Fix ES5 components detection from nested functions ([#287][])

[3.7.1]: https://github.com/yannickcr/eslint-plugin-react/compare/v3.7.0...v3.7.1
[#285]: https://github.com/yannickcr/eslint-plugin-react/issues/285
[#286]: https://github.com/yannickcr/eslint-plugin-react/issues/286
[#287]: https://github.com/yannickcr/eslint-plugin-react/issues/287

## [3.7.0] - 2015-11-05
### Added
* Add `jsx-no-bind` rule ([#184][] @Daniel15)
* Add line-aligned option to `jsx-closing-bracket-location` ([#243][] @alopatin)

### Fixed
* Fix a lot of issues about components detection, mostly related to stateless components ([#264][], [#267][], [#268][], [#276][], [#277][], [#280][])

### Changed
* Update dependencies

[3.7.0]: https://github.com/yannickcr/eslint-plugin-react/compare/v3.6.3...v3.7.0
[#184]: https://github.com/yannickcr/eslint-plugin-react/issues/184
[#243]: https://github.com/yannickcr/eslint-plugin-react/issues/243
[#264]: https://github.com/yannickcr/eslint-plugin-react/issues/264
[#267]: https://github.com/yannickcr/eslint-plugin-react/issues/267
[#268]: https://github.com/yannickcr/eslint-plugin-react/issues/268
[#276]: https://github.com/yannickcr/eslint-plugin-react/issues/276
[#277]: https://github.com/yannickcr/eslint-plugin-react/issues/277
[#280]: https://github.com/yannickcr/eslint-plugin-react/issues/280

## [3.6.3] - 2015-10-20
### Fixed
* Fix `display-name` for stateless components ([#256][])
* Fix `prop-types` props validation in constructor ([#259][])
* Fix typo in README ([#261][] @chiedojohn)

[3.6.3]: https://github.com/yannickcr/eslint-plugin-react/compare/v3.6.2...v3.6.3
[#256]: https://github.com/yannickcr/eslint-plugin-react/issues/256
[#259]: https://github.com/yannickcr/eslint-plugin-react/issues/259
[#261]: https://github.com/yannickcr/eslint-plugin-react/pull/261

## [3.6.2] - 2015-10-18
### Fixed
* Fix wrong prop-types detection ([#255][])

[3.6.2]: https://github.com/yannickcr/eslint-plugin-react/compare/v3.6.1...v3.6.2
[#255]: https://github.com/yannickcr/eslint-plugin-react/issues/255

## [3.6.1] - 2015-10-18
### Fixed
* Fix props validation in constructor ([#254][])

[3.6.1]: https://github.com/yannickcr/eslint-plugin-react/compare/v3.6.0...v3.6.1
[#254]: https://github.com/yannickcr/eslint-plugin-react/issues/254

## [3.6.0] - 2015-10-18
### Added
* Add support for stateless function components to `display-name` and `prop-types` ([#237][])
* Add callbacksLast option to `jsx-sort-props` and `jsx-sort-prop-types` ([#242][] @Daniel15)
* Add `prefer-es6-class` rule ([#247][] @hamiltondanielb)

### Fixed
* Fix `forbid-prop-types` crash with destructured PropTypes ([#230][] @epmatsw)
* Fix `forbid-prop-types` to do not modify AST directly ([#249][] @rhysd)
* Fix `prop-types` crash with empty destructuring ([#251][])
* Fix `prop-types` to not validate computed keys in destructuring ([#236][])

### Changed
* Update dependencies
* Improve components detection ([#233][])
* Documentation improvements ([#248][] @dguo)

[3.6.0]: https://github.com/yannickcr/eslint-plugin-react/compare/v3.5.1...v3.6.0
[#237]: https://github.com/yannickcr/eslint-plugin-react/issues/237
[#242]: https://github.com/yannickcr/eslint-plugin-react/pull/242
[#247]: https://github.com/yannickcr/eslint-plugin-react/issues/247
[#230]: https://github.com/yannickcr/eslint-plugin-react/issues/230
[#249]: https://github.com/yannickcr/eslint-plugin-react/issues/249
[#251]: https://github.com/yannickcr/eslint-plugin-react/issues/251
[#236]: https://github.com/yannickcr/eslint-plugin-react/issues/236
[#233]: https://github.com/yannickcr/eslint-plugin-react/issues/233
[#248]: https://github.com/yannickcr/eslint-plugin-react/pull/248

## [3.5.1] - 2015-10-01
### Fixed
* Fix `no-direct-mutation-state` to report only in React components ([#229][])
* Fix `forbid-prop-types` for arrayOf and instanceOf ([#230][])

### Changed
* Documentation improvements ([#232][] @edge)

[3.5.1]: https://github.com/yannickcr/eslint-plugin-react/compare/v3.5.0...v3.5.1
[#229]: https://github.com/yannickcr/eslint-plugin-react/issues/229
[#230]: https://github.com/yannickcr/eslint-plugin-react/issues/230
[#232]: https://github.com/yannickcr/eslint-plugin-react/pull/232

## [3.5.0] - 2015-09-28
### Added
* Add `no-direct-mutation-state` rule ([#133][], [#201][] @petersendidit)
* Add `forbid-prop-types` rule ([#215][] @pwmckenna)

### Fixed
* Fix no-did-mount/update-set-state rules, these rules were not working on ES6 classes

### Changed
* Update dependencies
* Documentation improvements ([#222][] @Andersos)

[3.5.0]: https://github.com/yannickcr/eslint-plugin-react/compare/v3.4.2...v3.5.0
[#133]: https://github.com/yannickcr/eslint-plugin-react/issues/133
[#201]: https://github.com/yannickcr/eslint-plugin-react/issues/201
[#215]: https://github.com/yannickcr/eslint-plugin-react/issues/215
[#222]: https://github.com/yannickcr/eslint-plugin-react/pull/222

## [3.4.2] - 2015-09-18
### Fixed
* Only display the `jsx-quotes` deprecation warning with the default formatter ([#221][])

[3.4.2]: https://github.com/yannickcr/eslint-plugin-react/compare/v3.4.1...v3.4.2
[#221]: https://github.com/yannickcr/eslint-plugin-react/issues/221

## [3.4.1] - 2015-09-17
### Fixed
* Fix `jsx-quotes` rule deprecation message ([#220][])

[3.4.1]: https://github.com/yannickcr/eslint-plugin-react/compare/v3.4.0...v3.4.1
[#220]: https://github.com/yannickcr/eslint-plugin-react/issues/220

## [3.4.0] - 2015-09-16
### Added
* Add namespaced JSX support to `jsx-no-undef` ([#219][] @zertosh)
* Add option to `jsx-closing-bracket-location` to configure different styles for self-closing and non-empty tags ([#208][] @evocateur)

### Deprecated
* Deprecate `jsx-quotes` rule, will now trigger a warning if used ([#217][])

[3.4.0]: https://github.com/yannickcr/eslint-plugin-react/compare/v3.3.2...v3.4.0
[#219]: https://github.com/yannickcr/eslint-plugin-react/pull/219
[#208]: https://github.com/yannickcr/eslint-plugin-react/pull/208
[#217]: https://github.com/yannickcr/eslint-plugin-react/issues/217

## [3.3.2] - 2015-09-10
### Changed
* Add `state` in lifecycle methods for `sort-comp` rule ([#197][] @mathieudutour)
* Treat component with render which returns `createElement` as valid ([#206][] @epmatsw)

### Fixed
* Fix allowed methods on arrayOf in `prop-types` ([#146][])
* Fix default configuration for `jsx-boolean-value` ([#210][])

[3.3.2]: https://github.com/yannickcr/eslint-plugin-react/compare/v3.3.1...v3.3.2
[#146]: https://github.com/yannickcr/eslint-plugin-react/issues/146
[#197]: https://github.com/yannickcr/eslint-plugin-react/pull/197
[#206]: https://github.com/yannickcr/eslint-plugin-react/pull/206
[#210]: https://github.com/yannickcr/eslint-plugin-react/issues/210

## [3.3.1] - 2015-09-01
### Changed
* Update dependencies
* Update changelog to follow the Keep a CHANGELOG standards
* Documentation improvements ([#198][] @lencioni)

### Fixed
* Fix `jsx-closing-bracket-location` for multiline props ([#199][])

[3.3.1]: https://github.com/yannickcr/eslint-plugin-react/compare/v3.3.0...v3.3.1
[#198]: https://github.com/yannickcr/eslint-plugin-react/pull/198
[#199]: https://github.com/yannickcr/eslint-plugin-react/issues/199

## [3.3.0] - 2015-08-26
### Added
* Add `jsx-indent-props` rule ([#15][], [#181][])
* Add `no-set-state rule` ([#197][] @markdalgleish)
* Add `jsx-closing-bracket-location` rule ([#14][], [#64][])

### Changed
* Update dependencies

### Fixed
* Fix crash on propTypes declarations with an empty body ([#193][] @mattyod)

[3.3.0]: https://github.com/yannickcr/eslint-plugin-react/compare/v3.2.3...v3.3.0
[#15]: https://github.com/yannickcr/eslint-plugin-react/issues/15
[#181]: https://github.com/yannickcr/eslint-plugin-react/issues/181
[#197]: https://github.com/yannickcr/eslint-plugin-react/pull/197
[#14]: https://github.com/yannickcr/eslint-plugin-react/issues/14
[#64]: https://github.com/yannickcr/eslint-plugin-react/issues/64
[#193]: https://github.com/yannickcr/eslint-plugin-react/pull/193

## [3.2.3] - 2015-08-16
### Changed
* Update dependencies

### Fixed
* Fix object rest/spread handling ([#187][] @xjamundx, [#189][] @Morantron)

[3.2.3]: https://github.com/yannickcr/eslint-plugin-react/compare/v3.2.2...v3.2.3
[#187]: https://github.com/yannickcr/eslint-plugin-react/pull/187
[#189]: https://github.com/yannickcr/eslint-plugin-react/pull/189

## [3.2.2] - 2015-08-11
### Changed
* Remove peerDependencies ([#178][])

[3.2.2]: https://github.com/yannickcr/eslint-plugin-react/compare/v3.2.1...v3.2.2
[#178]: https://github.com/yannickcr/eslint-plugin-react/issues/178

## [3.2.1] - 2015-08-08
### Fixed
* Fix crash when propTypes don't have any parent ([#182][])
* Fix jsx-no-literals reporting errors outside JSX ([#183][] @CalebMorris)

[3.2.1]: https://github.com/yannickcr/eslint-plugin-react/compare/v3.2.0...v3.2.1
[#182]: https://github.com/yannickcr/eslint-plugin-react/issues/182
[#183]: https://github.com/yannickcr/eslint-plugin-react/pull/183

## [3.2.0] - 2015-08-04
### Added
* Add `jsx-max-props-per-line` rule ([#13][])
* Add `jsx-no-literals` rule ([#176][] @CalebMorris)

### Changed
* Update dependencies

### Fixed
* Fix object access in `jsx-no-undef` ([#172][])

[3.2.0]: https://github.com/yannickcr/eslint-plugin-react/compare/v3.1.0...v3.2.0
[#13]: https://github.com/yannickcr/eslint-plugin-react/issues/13
[#176]: https://github.com/yannickcr/eslint-plugin-react/pull/176
[#172]: https://github.com/yannickcr/eslint-plugin-react/issues/172

## [3.1.0] - 2015-07-28
### Added
* Add event handlers to `no-unknown-property` ([#164][] @mkenyon)
* Add customValidators option to `prop-types` ([#145][] @CalebMorris)

### Changed
* Update dependencies
* Documentation improvements ([#167][] @ngbrown)

### Fixed
* Fix comment handling in `jsx-curly-spacing` ([#165][])

[3.1.0]: https://github.com/yannickcr/eslint-plugin-react/compare/v3.0.0...v3.1.0
[#164]: https://github.com/yannickcr/eslint-plugin-react/pull/164
[#145]: https://github.com/yannickcr/eslint-plugin-react/issues/145
[#165]: https://github.com/yannickcr/eslint-plugin-react/issues/165
[#167]: https://github.com/yannickcr/eslint-plugin-react/pull/167

## [3.0.0] - 2015-07-21
### Added
* Add jsx-no-duplicate-props rule ([#161][] @hummlas)
* Add allowMultiline option to the `jsx-curly-spacing` rule ([#156][] @mathieumg)

### Breaking
* In `jsx-curly-spacing` braces spanning multiple lines are now allowed with `never` option ([#156][] @mathieumg)

### Fixed
* Fix multiple var and destructuring handling in `props-types` ([#159][])
* Fix crash when retrieving propType name ([#163][])

[3.0.0]: https://github.com/yannickcr/eslint-plugin-react/compare/v2.7.1...v3.0.0
[#161]: https://github.com/yannickcr/eslint-plugin-react/pull/161
[#156]: https://github.com/yannickcr/eslint-plugin-react/pull/156
[#159]: https://github.com/yannickcr/eslint-plugin-react/issues/159
[#163]: https://github.com/yannickcr/eslint-plugin-react/issues/163

## [2.7.1] - 2015-07-16
### Changed
* Update peerDependencies requirements ([#154][])
* Update codebase for ESLint v1.0.0
* Change oneOfType to actually keep the child types ([#148][] @CalebMorris)
* Documentation improvements ([#147][] @lencioni)

[2.7.1]: https://github.com/yannickcr/eslint-plugin-react/compare/v2.7.0...v2.7.1
[#154]: https://github.com/yannickcr/eslint-plugin-react/issues/154
[#148]: https://github.com/yannickcr/eslint-plugin-react/issues/148
[#147]: https://github.com/yannickcr/eslint-plugin-react/pull/147

## [2.7.0] - 2015-07-11
### Added
* Add `no-danger` rule ([#138][] @scothis)
* Add `jsx-curly-spacing` rule ([#142][])

### Fixed
* Fix properties limitations on propTypes ([#139][])
* Fix component detection ([#144][])

[2.7.0]: https://github.com/yannickcr/eslint-plugin-react/compare/v2.6.4...v2.7.0
[#138]: https://github.com/yannickcr/eslint-plugin-react/pull/138
[#142]: https://github.com/yannickcr/eslint-plugin-react/issues/142
[#139]: https://github.com/yannickcr/eslint-plugin-react/issues/139
[#144]: https://github.com/yannickcr/eslint-plugin-react/issues/144

## [2.6.4] - 2015-07-02
### Fixed
* Fix simple destructuring handling ([#137][])

[2.6.4]: https://github.com/yannickcr/eslint-plugin-react/compare/v2.6.3...v2.6.4
[#137]: https://github.com/yannickcr/eslint-plugin-react/issues/137

## [2.6.3] - 2015-06-30
### Fixed
* Fix ignore option for `prop-types` rule ([#135][])
* Fix nested props destructuring ([#136][])

[2.6.3]: https://github.com/yannickcr/eslint-plugin-react/compare/v2.6.2...v2.6.3
[#135]: https://github.com/yannickcr/eslint-plugin-react/issues/135
[#136]: https://github.com/yannickcr/eslint-plugin-react/issues/136

## [2.6.2] - 2015-06-28
### Fixed
* Fix props validation when using a prop as an object key ([#132][])

[2.6.2]: https://github.com/yannickcr/eslint-plugin-react/compare/v2.6.1...v2.6.2
[#132]: https://github.com/yannickcr/eslint-plugin-react/issues/132

## [2.6.1] - 2015-06-28
### Fixed
* Fix crash in `prop-types` when encountering an empty variable declaration ([#130][])

[2.6.1]: https://github.com/yannickcr/eslint-plugin-react/compare/v2.6.0...v2.6.1
[#130]: https://github.com/yannickcr/eslint-plugin-react/issues/130

## [2.6.0] - 2015-06-28
### Added
* Add support for nested propTypes ([#62][] [#105][] @Cellule)
* Add `require-extension` rule ([#117][] @scothis)
* Add support for computed string format in `prop-types` ([#127][] @Cellule)
* Add ES6 methods to `sort-comp` default configuration ([#97][] [#122][])
* Add support for props destructuring directly on the this keyword
* Add `acceptTranspilerName` option to `display-name` rule ([#75][])
* Add schema to validate rules options

### Changed
* Update dependencies

### Fixed
* Fix test command for Windows ([#114][] @Cellule)
* Fix detection of missing displayName and propTypes when `ecmaFeatures.jsx` is false ([#119][] @rpl)
* Fix propTypes destructuring with properties as string ([#118][] @Cellule)
* Fix `jsx-sort-prop-types` support for keys as string ([#123][] @Cellule)
* Fix crash if a ClassProperty has only one token ([#125][])
* Fix invalid class property handling in `jsx-sort-prop-types` ([#129][])

[2.6.0]: https://github.com/yannickcr/eslint-plugin-react/compare/v2.5.2...v2.6.0
[#62]: https://github.com/yannickcr/eslint-plugin-react/issues/62
[#105]: https://github.com/yannickcr/eslint-plugin-react/issues/105
[#114]: https://github.com/yannickcr/eslint-plugin-react/pull/114
[#117]: https://github.com/yannickcr/eslint-plugin-react/pull/117
[#119]: https://github.com/yannickcr/eslint-plugin-react/pull/119
[#118]: https://github.com/yannickcr/eslint-plugin-react/issues/118
[#123]: https://github.com/yannickcr/eslint-plugin-react/pull/123
[#125]: https://github.com/yannickcr/eslint-plugin-react/issues/125
[#127]: https://github.com/yannickcr/eslint-plugin-react/pull/127
[#97]: https://github.com/yannickcr/eslint-plugin-react/issues/97
[#122]: https://github.com/yannickcr/eslint-plugin-react/issues/122
[#129]: https://github.com/yannickcr/eslint-plugin-react/issues/129
[#75]: https://github.com/yannickcr/eslint-plugin-react/issues/75

## [2.5.2] - 2015-06-14
### Fixed
* Fix regression in `jsx-uses-vars` with `babel-eslint` ([#110][])

[2.5.2]: https://github.com/yannickcr/eslint-plugin-react/compare/v2.5.1...v2.5.2
[#110]: https://github.com/yannickcr/eslint-plugin-react/issues/110

## [2.5.1] - 2015-06-14
### Changed
* Update dependencies
* Documentation improvements ([#99][] @morenoh149)

### Fixed
* Fix `prop-types` crash when propTypes definition is invalid ([#95][])
* Fix `jsx-uses-vars` for ES6 classes ([#96][])
* Fix hasOwnProperty that is taken for a prop ([#102][])

[2.5.1]: https://github.com/yannickcr/eslint-plugin-react/compare/v2.5.0...v2.5.1
[#95]: https://github.com/yannickcr/eslint-plugin-react/issues/95
[#96]: https://github.com/yannickcr/eslint-plugin-react/issues/96
[#102]: https://github.com/yannickcr/eslint-plugin-react/issues/102
[#99]: https://github.com/yannickcr/eslint-plugin-react/pull/99

## [2.5.0] - 2015-06-04
### Added
* Add option to make `wrap-multilines` more granular ([#94][] @PiPeep)

### Changed
* Update dependencies
* Documentation improvements ([#92][] [#93][] @lencioni)

[2.5.0]: https://github.com/yannickcr/eslint-plugin-react/compare/v2.4.0...v2.5.0
[#94]: https://github.com/yannickcr/eslint-plugin-react/pull/94
[#92]: https://github.com/yannickcr/eslint-plugin-react/pull/92
[#93]: https://github.com/yannickcr/eslint-plugin-react/pull/93

## [2.4.0] - 2015-05-30
### Added
* Add pragma option to `jsx-uses-react` ([#82][] @dominicbarnes)
* Add context props to `sort-comp` ([#89][] @zertosh)

### Changed
* Update dependencies
* Documentation improvement ([#91][] @matthewwithanm)

### Fixed
* Fix itemID in `no-unknown-property` rule ([#85][] @cody)
* Fix license field in package.json ([#90][] @zertosh)
* Fix usage of contructor in `sort-comp` options ([#88][])

[2.4.0]: https://github.com/yannickcr/eslint-plugin-react/compare/v2.3.0...v2.4.0
[#82]: https://github.com/yannickcr/eslint-plugin-react/pull/82
[#89]: https://github.com/yannickcr/eslint-plugin-react/pull/89
[#85]: https://github.com/yannickcr/eslint-plugin-react/pull/85
[#90]: https://github.com/yannickcr/eslint-plugin-react/pull/90
[#88]: https://github.com/yannickcr/eslint-plugin-react/issues/88
[#91]: https://github.com/yannickcr/eslint-plugin-react/pull/91

## [2.3.0] - 2015-05-14
### Added
* Add `sort-comp` rule ([#39][])
* Add `allow-in-func` option to `no-did-mount-set-state` ([#56][])

### Changed
* Update dependencies
* Improve errors locations for `prop-types`

### Fixed
* Fix quoted propTypes in ES6 ([#77][])

[2.3.0]: https://github.com/yannickcr/eslint-plugin-react/compare/v2.2.0...v2.3.0
[#39]: https://github.com/yannickcr/eslint-plugin-react/issues/39
[#77]: https://github.com/yannickcr/eslint-plugin-react/issues/77
[#56]: https://github.com/yannickcr/eslint-plugin-react/issues/56

## [2.2.0] - 2015-04-22
### Added
* Add `jsx-sort-prop-types` rule ([#38][] @AlexKVal)

### Changed
* Documentation improvements ([#71][] @AlexKVal)

### Fixed
* Fix variables marked as used when a prop has the same name ([#69][] @burnnat)

[2.2.0]: https://github.com/yannickcr/eslint-plugin-react/compare/v2.1.1...v2.2.0
[#38]: https://github.com/yannickcr/eslint-plugin-react/issues/38
[#69]: https://github.com/yannickcr/eslint-plugin-react/pull/69
[#71]: https://github.com/yannickcr/eslint-plugin-react/pull/71

## [2.1.1] - 2015-04-17
### Added
* Add support for classes static properties ([#43][])
* Add tests for the `babel-eslint` parser
* Add ESLint as peerDependency ([#63][] @AlexKVal)

### Changed
* Documentation improvements ([#55][] @AlexKVal, [#60][] @chriscalo)

[2.1.1]: https://github.com/yannickcr/eslint-plugin-react/compare/v2.1.0...v2.1.1
[#43]: https://github.com/yannickcr/eslint-plugin-react/issues/43
[#63]: https://github.com/yannickcr/eslint-plugin-react/pull/63
[#55]: https://github.com/yannickcr/eslint-plugin-react/pull/55
[#60]: https://github.com/yannickcr/eslint-plugin-react/pull/60

## [2.1.0] - 2015-04-06
### Added
* Add `jsx-boolean-value` rule ([#11][])
* Add support for static methods in `display-name` and `prop-types` ([#48][])

### Changed
* Update `jsx-sort-props` to reset the alphabetical verification on spread ([#47][] @zertosh)
* Update `jsx-uses-vars` to be enabled by default ([#49][] @banderson)

### Fixed
* Fix describing comment for hasSpreadOperator() method ([#53][] @AlexKVal)

[2.1.0]: https://github.com/yannickcr/eslint-plugin-react/compare/v2.0.2...v2.1.0
[#47]: https://github.com/yannickcr/eslint-plugin-react/pull/47
[#49]: https://github.com/yannickcr/eslint-plugin-react/pull/49
[#11]: https://github.com/yannickcr/eslint-plugin-react/issues/11
[#48]: https://github.com/yannickcr/eslint-plugin-react/issues/48
[#53]: https://github.com/yannickcr/eslint-plugin-react/pull/53

## [2.0.2] - 2015-03-31
### Fixed
* Fix ignore rest spread when destructuring props ([#46][])
* Fix component detection in `prop-types` and `display-name` ([#45][])
* Fix spread handling in `jsx-sort-props` ([#42][] @zertosh)

[2.0.2]: https://github.com/yannickcr/eslint-plugin-react/compare/v2.0.1...v2.0.2
[#46]: https://github.com/yannickcr/eslint-plugin-react/issues/46
[#45]: https://github.com/yannickcr/eslint-plugin-react/issues/45
[#42]: https://github.com/yannickcr/eslint-plugin-react/pull/42

## [2.0.1] - 2015-03-30
### Fixed
* Fix props detection when used in an object ([#41][])

[2.0.1]: https://github.com/yannickcr/eslint-plugin-react/compare/v2.0.0...v2.0.1
[#41]: https://github.com/yannickcr/eslint-plugin-react/issues/41

## [2.0.0] - 2015-03-29
### Added
* Add `jsx-sort-props` rule ([#16][])
* Add `no-unknown-property` rule ([#28][])
* Add ignore option to `prop-types` rule

### Changed
* Update dependencies

### Breaking
* In `prop-types` the children prop is no longer ignored

### Fixed
* Fix components are now detected when using ES6 classes ([#24][])
* Fix `prop-types` now return the right line/column ([#33][])
* Fix props are now detected when destructuring ([#27][])
* Fix only check for computed property names in `prop-types` ([#36][] @burnnat)

[2.0.0]: https://github.com/yannickcr/eslint-plugin-react/compare/v1.6.1...v2.0.0
[#16]: https://github.com/yannickcr/eslint-plugin-react/issues/16
[#28]: https://github.com/yannickcr/eslint-plugin-react/issues/28
[#24]: https://github.com/yannickcr/eslint-plugin-react/issues/24
[#33]: https://github.com/yannickcr/eslint-plugin-react/issues/33
[#27]: https://github.com/yannickcr/eslint-plugin-react/issues/27
[#36]: https://github.com/yannickcr/eslint-plugin-react/pull/36

## [1.6.1] - 2015-03-25
### Changed
* Update `jsx-quotes` documentation

### Fixed
* Fix `jsx-no-undef` with `babel-eslint` ([#30][])
* Fix `jsx-quotes` on Literal childs ([#29][])

[1.6.1]: https://github.com/yannickcr/eslint-plugin-react/compare/v1.6.0...v1.6.1
[#30]: https://github.com/yannickcr/eslint-plugin-react/issues/30
[#29]: https://github.com/yannickcr/eslint-plugin-react/issues/29

## [1.6.0] - 2015-03-22
### Added
* Add `jsx-no-undef` rule
* Add `jsx-quotes` rule ([#12][])
* Add `@jsx` pragma support ([#23][])

### Changed
* Allow `this.getState` references (not calls) in lifecycle methods ([#22][] @benmosher)
* Update dependencies

### Fixed
* Fix `react-in-jsx-scope` in Node.js env
* Fix usage of propTypes with an external object ([#9][])

[1.6.0]: https://github.com/yannickcr/eslint-plugin-react/compare/v1.5.0...v1.6.0
[#12]: https://github.com/yannickcr/eslint-plugin-react/issues/12
[#23]: https://github.com/yannickcr/eslint-plugin-react/issues/23
[#9]: https://github.com/yannickcr/eslint-plugin-react/issues/9
[#22]: https://github.com/yannickcr/eslint-plugin-react/pull/22

## [1.5.0] - 2015-03-14
### Added
* Add `jsx-uses-vars` rule

### Fixed
* Fix `jsx-uses-react` for ESLint 0.17.0

[1.5.0]: https://github.com/yannickcr/eslint-plugin-react/compare/v1.4.1...v1.5.0

## [1.4.1] - 2015-03-03
### Fixed
* Fix `this.props.children` marked as missing in props validation ([#7][])
* Fix usage of `this.props` without property ([#8][])

[1.4.1]: https://github.com/yannickcr/eslint-plugin-react/compare/v1.4.0...v1.4.1
[#7]: https://github.com/yannickcr/eslint-plugin-react/issues/7
[#8]: https://github.com/yannickcr/eslint-plugin-react/issues/8

## [1.4.0] - 2015-02-24
### Added
* Add `react-in-jsx-scope` rule ([#5][] @glenjamin)
* Add `jsx-uses-react` rule ([#6][] @glenjamin)

### Changed
* Update `prop-types` to check props usage insead of propTypes presence ([#4][])

[1.4.0]: https://github.com/yannickcr/eslint-plugin-react/compare/v1.3.0...v1.4.0
[#4]: https://github.com/yannickcr/eslint-plugin-react/issues/4
[#5]: https://github.com/yannickcr/eslint-plugin-react/pull/5
[#6]: https://github.com/yannickcr/eslint-plugin-react/pull/6

## [1.3.0] - 2015-02-24
### Added
* Add `no-did-mount-set-state` rule
* Add `no-did-update-set-state` rule

### Changed
* Update dependencies

[1.3.0]: https://github.com/yannickcr/eslint-plugin-react/compare/v1.2.2...v1.3.0

## [1.2.2] - 2015-02-09
### Changed
* Update dependencies

### Fixed
* Fix childs detection in `self-closing-comp` ([#3][])

[1.2.2]: https://github.com/yannickcr/eslint-plugin-react/compare/v1.2.1...v1.2.2
[#3]: https://github.com/yannickcr/eslint-plugin-react/issues/3

## [1.2.1] - 2015-01-29
### Changed
* Update Readme
* Update dependencies
* Update `wrap-multilines` and `self-closing-comp` rules for ESLint 0.13.0

[1.2.1]: https://github.com/yannickcr/eslint-plugin-react/compare/v1.2.0...v1.2.1

## [1.2.0] - 2014-12-29
### Added
* Add `self-closing-comp` rule

### Fixed
* Fix `display-name` and `prop-types` rules

[1.2.0]: https://github.com/yannickcr/eslint-plugin-react/compare/v1.1.0...v1.2.0

## [1.1.0] - 2014-12-28
### Added
 * Add `display-name` rule
 * Add `wrap-multilines` rule
 * Add rules documentation
 * Add rules tests

[1.1.0]: https://github.com/yannickcr/eslint-plugin-react/compare/v1.0.0...v1.1.0

## 1.0.0 - 2014-12-16
### Added
 * First revision
