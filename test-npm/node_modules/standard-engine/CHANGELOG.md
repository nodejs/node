# standard-engine Change Log
All notable changes to this project will be documented in this file.
This project adheres to [Semantic Versioning](http://semver.org/).

## 3.1.1 - 2016-02-03
  * Replace deprecated `win-spawn` dev dependency with `cross-spawn-async`.

## 3.1.0 - 2016-02-03
  * Support `opts.format` string option to print message about how to install formatter.

## 3.0.1 - 2016-02-03
  * Remove `dezalgo` dependency

## 3.0.0 - 2016-01-29
  * Breaking: eslint is now a mandatory option in your options.js

## 2.2.4 - 2015-12-02
  * Fixed: Log warning messages when no errors are present

## 2.2.3 - 2015-11-17
  * remove prepended "Use" from tagline output

## 2.2.2 - 2015-11-06
  * update eslint to 1.9.0

## 2.2.1 - 2015-11-01
  * lock down eslint to 1.7.3 to fix space-in-parens eslint bug
  * avoid mutating the eslint config object that's passed in (fixes #9)

## 2.2.0 - 2015-09-18
  * update eslint to 1.5.x

## 2.1.1 - 2015-09-18
  * fix fetching of eslint with older npm versions

## 2.1.0 - 2015-06-16
  * set custom parser in non-hacky way (fixes #12)

## 2.0.7 - 2105-08-28
  * ignore vendor/ folder by default

## 2.0.6 - 2105-08-07
  * readme changes

## 2.0.5 - 2015-08-05
  ### `standard` is now using `standard-engine` Cool!
  * All `standard` maintainers have been added to `standard-engine`.
  * Instead of synchronizing, we will now just update `standard-engine` directly to add features/fixes!

  ### Other things
  * **BREAKING CHANGE** `standard-engine` is now using eslint 1.0!
    * Be sure your eslint configuration works with eslint 1.0 before upgrading.

  * Other dependencies were bumped in order to facilitate the update
  * The code was also meticulously synchronized with `standard`
    * CLI output is now identical
    * A few code style updates were made for consistency

## 1.11.1 - 2015-07-30
  * fix standard issues in cmd.js

## 1.11.0 - 2015-07-30
  * Merged standard 5.0.0-8
  * New feature: ability to specify globals via --global flag and `global` option

## 1.10.2 - 2015-07-17
  * Merged standard 4.5.4 changes: switch to using `deglob`

## 1.10.1 - 2015-07-06
  * Removed a stray console.log

## 1.10.0 - 2015-07-06
  * Fix bug in custom parser option to make it work.
  * Merged from standard: New "globals" option can be set in package.json to define an array of global variables.

## 1.9.0 - 2015-06-29
 * merge from latest standard 4.4.1 including:
  * Fixes to the gitignore feature
  * added `parser` parameter
  * Lots of repos added to clone.js test and made a lot faster! @feross is the best :)

## 1.8.1 - 2015-06-17
 * Fix NPE error when opts._config is undefined. Thanks @wombleton

## 1.8.0 - 2015-06-16

 * Fix gitignore support for Windows.
 * Refactor to use pkg-config.
 * Update to newer version of eslint to allow extending multiple eslint-config files.

## 1.7.0 - 2015-05-30
### Merged from `standard`
  * NEW FEATURE: Add proper .gitignore support

## 1.6.0 - 2015-05-29
### Merged from `standard`
  * NEW FEATURE: Custom Parsers can now be specified in `package.json`

  To use a custom parser, install it from npm (example: `npm install
  babel-eslint`) and add this to your package.json:
  ```
  {
    "yourcmdname": {
      "parser": "babel-eslint"
    }
  }
  ```
  (Thanks @feross)


## 1.5.0 - 2015-05-25
### Merged from `standard`
  * NEW FEATURE: pass in a formatter to get `--format` as an option for the command line! Thanks @ricardofbarros!

## 1.4.3 - 2015-05-25
### Merged from `standard`
  * Speed increased significantly by reverting a default ignore pattern change.

## 1.4.2 - 2015-05-22
 * Fix bug where absolute paths to files was not working.

## 1.4.1 - 2015-04-21
### Merged from `standard`
 * Fix bug in `parseOpts` to ensure original options are not modified
 * Upgrade to eslint 0.21.0

## 1.4.0 - 2015-04-21
### Merged from `standard`
 * Disable `.gitignore` support for now.

## 1.3.1 - 2015-04-14

### Merged from `standard`
* Fix crash on absolute filesystem path

## 1.3.0 - 2015-04-14
* moved some files back to their original locations to make merging from `standard` easier.

### Merged from `standard`
* Ignore linting for all files in `.gitignore`.
* Removed `/git/**` exclusion as its redundant.
* Output errors to stdout instead of stderr.
* Updated `eslint-plugin-react` and `tape` dependencies.
* Additional tests.
