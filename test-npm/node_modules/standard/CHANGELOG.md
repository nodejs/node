# Change Log

All notable changes to this project will be documented in this file.
This project adheres to [Semantic Versioning](http://semver.org/).

## 6.0.7 - 2016-02-18

- Revert: Use install location of standard as eslint `cwd` (fixes [#429](https://github.com/feross/standard/issues/429))

## 6.0.6 - 2016-02-18

- Use eslint 2.1.0
- Fix: Use install location of standard as eslint `cwd` (fixes [snazzy/#8](https://github.com/feross/snazzy/issues/8))

## 6.0.5 - 2016-02-12

- Use eslint 2.0.0 stable

## 6.0.4 - 2016-02-07

- Relax rule: Validate closing bracket location in JSX ([jsx-closing-bracket-location](https://github.com/yannickcr/eslint-plugin-react/blob/master/docs/rules/jsx-closing-bracket-location.md))

## 6.0.3 - 2016-02-06

- Fix "Error: Cannot find module 'eslint-config-standard-jsx'" with npm 2 (node 0.10, 0.12, 4)

## 6.0.2 - 2016-02-06

- Internal change: Remove .eslintrc file, and use inline config

## 6.0.1 - 2016-02-05

- Internal change: Move .eslintrc file to root folder

## 6.0.0 - 2016-02-05

The goal of this release is to make `standard` faster to install, and simpler to use.

### Remove `standard-format` ([#340](https://github.com/feross/standard/issues/340)) ([#397](https://github.com/feross/standard/issues/397))

- Eliminates 250 packages, and cuts install time in half!
- For npm 2, install time goes from 20 secs —> 10 secs.
- For npm 3, install time goes from 24 secs —> 12 secs.
- To continue using `standard-format`, just install it separately: `npm install -g standard-format`

### React-specific linting rules are removed ([#351](https://github.com/feross/standard/issues/351)) ([#367](https://github.com/feross/standard/issues/367)) ([eslint-config-standard-react/#13](https://github.com/feross/eslint-config-standard-react/pull/13))

- JSX is still supported, and it continues to be checked for style.
- There were only a few React-specific rules, but they made it extremely difficult for users of alternatives like `virtual-dom` or `deku`, and unecessarily tied `standard` to a single library.
- JSX rules come from `eslint-config-standard-jsx`. The `eslint-config-standard-react` dependency was removed.

### New Rules

*The percentage (%) of users that rule changes will effect, based on real-world testing of the top ~400 npm packages is denoted in brackets.*

- Disallow `__dirname`/`__filename` string concatenation ([#403](https://github.com/feross/standard/issues/403)) ([no-path-concat](http://eslint.org/docs/2.0.0/rules/no-path-concat)) [5%]
- Require parens in arrow function arguments  ([#309](https://github.com/feross/standard/issues/309)) ([arrow-parens](http://eslint.org/docs/2.0.0/rules/arrow-parens.html)) [5%]
- Ensure that `new Promise()` is instantiated with the parameter names
`resolve`, `reject` ([#282](https://github.com/feross/standard/issues/282)) ([promise/param-names](https://github.com/xjamundx/eslint-plugin-promise#param-names)) [1%]
- Enforce Usage of Spacing in Template Strings ([template-curly-spacing](http://eslint.org/docs/2.0.0/rules/template-curly-spacing)) [1%]
- Template strings are only allowed when necessary, i.e. template string features are being used (eslint got stricter: https://github.com/eslint/eslint/issues/5147) [1%]
- Better dead code detection after conditional statements (eslint got stricter) [1%]
- Enforce spaces around `*` in `yield * something` ([#335](https://github.com/feross/standard/issues/335)) ([yield-star-spacing](http://eslint.org/docs/2.0.0/rules/yield-star-spacing)) [0%]
- Disallow labels on loops/switch statements too (made rule stricter) ([no-labels](http://eslint.org/docs/2.0.0/rules/no-labels.html)) [0%]
- Disallow unnecessary constructor ([no-useless-constructor](http://eslint.org/docs/2.0.0/rules/no-useless-constructor)) [0%]
- Disallow empty destructuring patterns ([no-empty-pattern](http://eslint.org/docs/2.0.0/rules/no-empty-pattern)) [0%]
- Disallow Symbol Constructor ([no-new-symbol](http://eslint.org/docs/2.0.0/rules/no-new-symbol)) [0%]
- Disallow Self Assignment ([no-self-assign](http://eslint.org/docs/2.0.0/rules/no-self-assign)) [0%]

### Removed Rules

- `parseInt()` radix rule because ES5 fixes this issue ([#384](https://github.com/feross/standard/issues/384))  ([radix](http://eslint.org/docs/2.0.0/rules/radix.html)) [0%]

### Expose eslint configuration via command line options and `package.json`

For power users, it might be easier to use one of these new hooks instead of forking
`standard`, though that's still encouraged, too!

- Set eslint "plugins" ([#386](https://github.com/feross/standard/issues/386))
- Set eslint "rules" ([#367](https://github.com/feross/standard/issues/367))
- Set eslint "env" ([#371](https://github.com/feross/standard/issues/371))

To set custom ESLint plugins, rules, or envs, use the command line `--plugin`, `--rules`, and `--env` flags.

In `package.json`, use the "standard" property:

```json
{
  "standard": {
    "plugins": [ "my-plugin" ]
  }
}
```

### Upgrade to ESLint v2
- There may be slight behavior changes to existing rules. When possible, we've noted these in the "New Rules" and "Removed Rules" section.

### Improve test suite

- Rule changes can be tested against every package on npm. For sanity, this is limited to packages with at least 4 dependents. Around ~400 packages.

### Known Issues

- Using prerelease eslint version (2.0.0-rc.0). There may be breaking changes before the stable release.
- `no-return-assign` behavior changed with arrow functions (https://github.com/eslint/eslint/issues/5150)

### Relevant diffs

- standard ([v5.4.1...v6.0.0](https://github.com/feross/standard/compare/v5.4.1...v6.0.0))
- eslint-config-standard ([v4.4.0...v5.0.0](https://github.com/feross/eslint-config-standard/compare/v4.4.0...v5.0.0))
- eslint-config-standard-jsx ([v1.0.0](https://github.com/feross/eslint-config-standard-jsx/commit/47d5e248e2e078eb87619493999e3e74d4b7e70e))
- standard-engine ([v2.2.4...v3.2.1](https://github.com/Flet/standard-engine/compare/v2.2.4...v3.2.1))

## 5.4.1 - 2015-11-16
[view diff](https://github.com/feross/standard/compare/v5.4.0...v5.4.1)

### Fixed

* Fix for `standard-engine` change. Fix error tagline.

## 5.4.0 - 2015-11-16
[view diff](https://github.com/feross/standard/compare/v5.3.1...v5.4.0)

### Added

* eslint-config-standard-react@1.2.0 ([history](eslint-config-standard-react))
  * Disallow duplicate JSX properties

## 5.3.1 - 2015-09-18
[view diff](https://github.com/feross/standard/compare/v5.3.0...v5.3.1)

### Changed
* eslint-plugin-react@3.4.2 ([history](eslint-plugin-react))

## 5.3.0 - 2015-09-16
[view diff](https://github.com/feross/standard/compare/v5.2.2...v5.3.0)

### Changed
* eslint-config-standard@4.4.0 ([history][eslint-config-standard])
  * **New rule:** must have space after semicolon in for-loop ([commit](https://github.com/feross/eslint-config-standard/commit/6e5025eef8900f686e19b4a31836743d98323119))
  * **New rule:** No default assignment with ternary operator ([commit](https://github.com/feross/eslint-config-standard/commit/0903c19ca6a8bc0c8625c41ca844ee69968bf948))
  * **New rule:** Require spaces before keywords ([commit](https://github.com/feross/eslint-config-standard/commit/656ba93cda9cd4ab38e032649aafb795993d5176))
* eslint-config-standard-react@1.1.0 ([history][eslint-config-standard-react])
* eslint-plugin-react@3.4.0 ([history][eslint-plugin-react])
* eslint-plugin-standard@1.3.1 ([history][eslint-plugin-standard])

## 5.2.2
[view diff](https://github.com/feross/standard/compare/v5.2.1...v5.2.2)

### Fixed
* We have a changelog now, and you're reading it!
* Minor README update
* Removed direct dependency on `eslint` (its now moved to [standard-engine](https://github.com/flet/standard-engine))

## 5.2.1 - 2015-09-03
[view diff](https://github.com/feross/standard/compare/v5.2.0...v5.2.1)

### Changed
* eslint-config-standard@4.3.1 ([history][eslint-config-standard])
  * **Revert rule**: Disallow unncessary concatenation of strings

### Fixed
* eslint-config-standard@4.3.1 ([history][eslint-config-standard])
  * fix regression with ternary operator handling

## 5.2.0 - 2015-09-03
[view diff](https://github.com/feross/standard/compare/v5.1.1...v5.2.0)

### Added
* eslint-config-standard@4.3.0 ([history][eslint-config-standard])
  * **New rule:** Disallow unncessary concatenation of strings
  * **New rule:** Disallow duplicate name in class members
  * **New rule:** enforce spaces inside of single line blocks
  * **Re-add rule:** padded-blocks ([Closes #170](https://github.com/feross/standard/issues/170))

### Changed
* Bump `eslint` from 1.1.0 to 1.3.1 ([CHANGELOG][eslint])
* eslint-plugin-standard@1.3.0 ([history][eslint-plugin-standard])
  * A small change to make the plugin compatible with browserify which does not affect behavior.

### Fixed
* eslint-plugin-react@3.3.1 ([CHANGELOG][eslint-plugin-react])
  * Fix object rest/spread handling.
* Added white background to badge.svg to make it work with dark backgrounds ([Closes #234](https://github.com/feross/standard/issues/234))
* Minor updates to README.md

## 5.1.1 - 2015-08-28
[view diff](https://github.com/feross/standard/compare/v5.1.0...v5.1.1)

### Fixed
* Update to RULES.md to remove a missing hyperlink
* Add atom linter information to README.md
* Fixed duplicated word in the tagline message on the CLI
* Removed failing repository from tests (yoshuawuyts/initialize)

## 5.1.0 - 2015-08-14
[view diff](https://github.com/feross/standard/compare/v5.0.2...v5.1.0)

## Fixed
* eslint-config-standard@4.1.0 ([history][eslint-config-standard])
  * Added rest/spread feature to `eslintrc.json` to fix [#226](https://github.com/feross/standard/issues/226) and [eslint-plugin-standard#3](https://github.com/xjamundx/eslint-plugin-standard/issues/3)
* eslint-plugin-react@3.2.2 ([CHANGELOG][eslint-plugin-react])
  * Fix crash when propTypes don't have any parent
  * Fix jsx-no-literals reporting errors outside JSX

### Changed
* Bump eslint from 1.0.0 to 1.2.0 ([CHANGELOG][eslint])
* Added more test repositories and disabled some that were failing
* Update bikeshedding link on README.md

## 5.0.2 - 2015-08-06
[view diff](https://github.com/feross/standard/compare/v5.0.1...v5.0.2)

### Changed
* eslint-config-standard-react@1.0.4 ([history][eslint-config-standard-react])
  - **Disable Rule:** react/wrap-multilines
* Minor README updates

## 5.0.1 - 2015-08-05
[view diff](https://github.com/feross/standard/compare/v5.0.0...v5.0.1)

## 5.0.0 - 2015-08-03
[view diff](https://github.com/feross/standard/compare/v4.5.4...v5.0.0)

## 4.5.4 - 2015-07-13
[view diff](https://github.com/feross/standard/compare/v4.5.3...v4.5.4)

## 4.5.3 - 2015-07-10
[view diff](https://github.com/feross/standard/compare/v4.5.2...v4.5.3)

## 4.5.2 - 2015-07-02
[view diff](https://github.com/feross/standard/compare/v4.5.1...v4.5.2)

## 4.5.1 - 2015-06-30
[view diff](https://github.com/feross/standard/compare/v4.5.0...v4.5.1)

## 4.5.0 - 2015-06-30
[view diff](https://github.com/feross/standard/compare/v4.4.1...v4.5.0)

## 4.4.1 - 2015-06-29
[view diff](https://github.com/feross/standard/compare/v4.4.0...v4.4.1)

## 4.4.0 - 2015-06-27
[view diff](https://github.com/feross/standard/compare/v4.3.3...v4.4.0)

## 4.3.3 - 2015-06-26
[view diff](https://github.com/feross/standard/compare/v4.3.2...v4.3.3)

## 4.3.2 - 2015-06-23
[view diff](https://github.com/feross/standard/compare/v4.3.1...v4.3.2)

## 4.3.1 - 2015-06-18
[view diff](https://github.com/feross/standard/compare/v4.3.0...v4.3.1)

## 4.3.0 - 2015-06-16
[view diff](https://github.com/feross/standard/compare/v4.2.1...v4.3.0)

## 4.2.1 - 2015-06-12
[view diff](https://github.com/feross/standard/compare/v4.2.0...v4.2.1)

## 4.2.0 - 2015-06-11
[view diff](https://github.com/feross/standard/compare/v4.1.1...v4.2.0)

## 4.1.1 - 2015-06-11
[view diff](https://github.com/feross/standard/compare/v4.1.0...v4.1.1)

## 4.1.0 - 2015-06-10
[view diff](https://github.com/feross/standard/compare/v4.0.1...v4.1.0)

## 4.0.1 - 2015-06-01
[view diff](https://github.com/feross/standard/compare/v4.0.0...v4.0.1)

## 4.0.0 - 2015-05-30
[view diff](https://github.com/feross/standard/compare/v3.9.0...v4.0.0)

[eslint-config-standard-react]: https://github.com/feross/eslint-config-standard-react/commits/master
[eslint-config-standard]: https://github.com/feross/eslint-config-standard/commits/master
[eslint-plugin-react]: https://github.com/yannickcr/eslint-plugin-react/blob/master/CHANGELOG.md
[eslint-plugin-standard]: https://github.com/xjamundx/eslint-plugin-standard/commits/master
[eslint]: https://github.com/eslint/eslint/blob/master/CHANGELOG.md
