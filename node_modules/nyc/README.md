# nyc

[![Build Status](https://travis-ci.org/istanbuljs/nyc.svg?branch=master)](https://travis-ci.org/istanbuljs/nyc)
[![Coverage Status](https://coveralls.io/repos/bcoe/nyc/badge.svg?branch=)](https://coveralls.io/r/bcoe/nyc?branch=master)
[![NPM version](https://img.shields.io/npm/v/nyc.svg)](https://www.npmjs.com/package/nyc)
[![Windows Tests](https://img.shields.io/appveyor/ci/bcoe/nyc-ilw23/master.svg?label=Windows%20Tests)](https://ci.appveyor.com/project/bcoe/nyc-ilw23)
[![Conventional Commits](https://img.shields.io/badge/Conventional%20Commits-1.0.0-yellow.svg)](https://conventionalcommits.org)
[![community slack](http://devtoolscommunity.herokuapp.com/badge.svg)](http://devtoolscommunity.herokuapp.com)

_Having problems? want to contribute? join our [community slack](http://devtoolscommunity.herokuapp.com)_.

Istanbul's state of the art command line interface, with support for:

* applications that spawn subprocesses.
* ES2015 transforms, via [babel-plugin-istanbul](https://github.com/istanbuljs/babel-plugin-istanbul), or source-maps.

## Instrumenting your code

You can install nyc as a development dependency and add it to the test stanza
in your package.json.

```shell
npm i nyc --save-dev
```

```json
{
  "script": {
    "test": "nyc mocha"
  }
}
```

Alternatively, you can install nyc globally and use it to execute `npm test`:

```shell
npm i nyc -g
```

```shell
nyc npm test
```

nyc accepts a wide variety of configuration arguments, run `nyc --help` for
thorough documentation.

Configuration arguments should be provided prior to the program that nyc
is executing. As an example, the following command executes `npm test`,
and indicates to nyc that it should output both an `lcov`
and a `text-lcov` coverage report.

```shell
nyc --reporter=lcov --reporter=text-lcov npm test
```

### Accurate stack traces using source maps

When `produce-source-map` is set to true, then the instrumented source files will
include inline source maps for the instrumenter transform. When combined with
[source-map-support](https://github.com/evanw/node-source-map-support),
stack traces for instrumented code will reflect their original lines.

### Support for custom require hooks (babel, webpack, etc.)

nyc supports custom require hooks like
[`babel-register`](http://babeljs.io/docs/usage/require/). nyc can
load the hooks for you, [using the `--require`
flag](#require-additional-modules).

Source maps are used to map coverage information back to the appropriate lines
of the pre-transpiled code. You'll have to configure your custom require hook
to inline the source map in the transpiled code. For Babel that means setting
the `sourceMaps` option to `inline`.

## Use with `babel-plugin-istanbul` for Babel Support

We recommend using [`babel-plugin-istanbul`](https://github.com/istanbuljs/babel-plugin-istanbul) if your
project uses the babel tool chain:

1. enable the `babel-plugin-istanbul` plugin:

  ```json
    {
      "babel": {
        "presets": ["es2015"],
        "env": {
          "test": {
            "plugins": ["istanbul"]
          }
        }
      }
    }
  ```

  Note: With this configuration, the Istanbul instrumentation will only be active when `NODE_ENV` or `BABEL_ENV` is `test`.

  We recommend using the [`cross-env`](https://npmjs.com/package/cross-env) package to set these environment variables
  in your `package.json` scripts in a way that works cross-platform.

2. disable nyc's instrumentation and source-maps, e.g. in `package.json`:

  ```json
  {
    "nyc": {
      "require": [
        "babel-register"
      ],
      "sourceMap": false,
      "instrument": false
    },
    "scripts": {
      "test": "cross-env NODE_ENV=test nyc mocha"
    }
  }
  ```

That's all there is to it, better ES2015+ syntax highlighting awaits:

<img width="500" src="screen2.png">

## Support for alternate file extensions (.jsx, .es6)

Supporting file extensions can be configured through either the configuration arguments or with the `nyc` config section in `package.json`.

```shell
nyc --extension .jsx --extension .es6 npm test
```

```json
{
  "nyc": {
    "extension": [
      ".jsx",
      ".es6"
    ]
  }
}
```

## Checking coverage

nyc can fail tests if coverage falls below a threshold.
After running your tests with nyc, simply run:

```shell
nyc check-coverage --lines 95 --functions 95 --branches 95
```

nyc also accepts a `--check-coverage` shorthand, which can be used to
both run tests and check that coverage falls within the threshold provided:

```shell
nyc --check-coverage --lines 100 npm test
```

The above check fails if coverage falls below 100%.

To check thresholds on a per-file basis run:

```shell
nyc check-coverage --lines 95 --per-file
```

## Running reports

Once you've run your tests with nyc, simply run:

```bash
nyc report
```

To view your coverage report:

<img width="500" src="screen.png">

you can use [any reporters that are supported by `istanbul`](https://github.com/istanbuljs/istanbuljs/tree/master/packages/istanbul-reports/lib):

```bash
nyc report --reporter=lcov
```

## Excluding files

You can tell nyc to exclude specific files and directories by adding
an `nyc.exclude` array to your `package.json`. Each element of
the array is a glob pattern indicating which paths should be omitted.

Globs are matched using [micromatch](https://www.npmjs.com/package/micromatch).

For example, the following config will exclude any files with the extension `.spec.js`,
and anything in the `build` directory:

```json
{
  "nyc": {
    "exclude": [
      "**/*.spec.js",
      "build"
    ]
  }
}
```
> Note: Since version 9.0 files under `node_modules/` are excluded by default.
  add the exclude rule `!**/node_modules/` to stop this.

> Note: exclude defaults to `['test', 'test{,-*}.js', '**/*.test.js', '**/__tests__/**', '**/node_modules/**']`,
which would exclude `test`/`__tests__` directories as well as `test.js`, `*.test.js`,
and `test-*.js` files. Specifying your own exclude property overrides these defaults.

## Including files

As an alternative to providing a list of files to `exclude`, you can provide
an `include` key with a list of globs to specify specific files that should be covered:

```json
{
  "nyc": {
    "include": ["**/build/umd/moment.js"]
  }
}
```

> Note: include defaults to `['**']`

> ### Use the `--all` flag to include files that have not been required in your tests.

## Require additional modules

The `--require` flag can be provided to `nyc` to indicate that additional
modules should be required in the subprocess collecting coverage:

`nyc --require babel-register --require babel-polyfill mocha`

## Caching

You can run `nyc` with the optional `--cache` flag, to prevent it from
instrumenting the same files multiple times. This can significantly
improve runtime performance.

## Configuring `nyc`

Any configuration options that can be set via the command line can also be specified in the `nyc` stanza of your package.json, or within a `.nycrc` file:

**package.json:**

```json
{
  "description": "These are just examples for demonstration, nothing prescriptive",
  "nyc": {
    "check-coverage": true,
    "per-file": true,
    "lines": 99,
    "statements": 99,
    "functions": 99,
    "branches": 99,
    "include": [
      "src/**/*.js"
    ],
    "exclude": [
      "src/**/*.spec.js"
    ],
    "reporter": [
      "lcov",
      "text-summary"
    ],
    "require": [
      "./test/helpers/some-helper.js"
    ],
    "extension": [
      ".jsx"
    ],
    "cache": true,
    "all": true,
    "report-dir": "./alternative"
  }
}
```

### Publish, and reuse, your nyc configuration

nyc allows you to inherit other configurations using the key `extends`. As an example,
an alternative way to configure nyc for `babel-plugin-istanbul` would be to use the
[@istanbuljs/nyc-config-babel preset](https://www.npmjs.com/package/@istanbuljs/nyc-config-babel):

```json
{
  "nyc": {
    "extends": "@istanbuljs/nyc-config-babel"
  }
}
```

To publish and resuse your own `nyc` configuration, simply create an npm module that
exports an `index.json` with your `nyc` config.

## High and low watermarks

Several of the coverage reporters supported by nyc display special information
for high and low watermarks:

* high-watermarks represent healthy test coverage (in many reports
  this is represented with green highlighting).
* low-watermarks represent sub-optimal coverage levels (in many reports
  this is represented with red highlighting).

You can specify custom high and low watermarks in nyc's configuration:

```json
{
  "nyc": {
    "watermarks": {
      "lines": [80, 95],
      "functions": [80, 95],
      "branches": [80, 95],
      "statements": [80, 95]
    }
  }
}
```

## Other advanced features

Take a look at http://istanbul.js.org/docs/advanced/ and please feel free to [contribute documentation](https://github.com/istanbuljs/istanbuljs.github.io/tree/development/content).

## Integrating with coveralls

[coveralls.io](https://coveralls.io) is a great tool for adding
coverage reports to your GitHub project. Here's how to get nyc
integrated with coveralls and travis-ci.org:

1. add the coveralls and nyc dependencies to your module:

  ```shell
  npm install coveralls nyc --save-dev
  ```

2. update the scripts in your package.json to include these bins:

  ```json
  {
     "script": {
       "test": "nyc mocha",
       "coverage": "nyc report --reporter=text-lcov | coveralls"
     }
  }
  ```

3. For private repos, add the environment variable `COVERALLS_REPO_TOKEN` to travis.

4. add the following to your `.travis.yml`:

  ```yaml
  after_success: npm run coverage
  ```

That's all there is to it!

> Note: by default coveralls.io adds comments to pull-requests on GitHub, this can feel intrusive. To disable this, click on your repo on coveralls.io and uncheck `LEAVE COMMENTS?`.

## Integrating with codecov

`nyc npm test && nyc report --reporter=text-lcov > coverage.lcov && codecov`

[codecov](https://codecov.io/) is a great tool for adding
coverage reports to your GitHub project, even viewing them inline on GitHub with a browser extension:

Here's how to get `nyc` integrated with codecov and travis-ci.org:

1. add the codecov and nyc dependencies to your module:

  ```shell
  npm install codecov nyc --save-dev
  ```

2. update the scripts in your package.json to include these bins:

  ```json
  {
     "script": {
       "test": "nyc tap ./test/*.js",
       "coverage": "nyc report --reporter=text-lcov > coverage.lcov && codecov"
     }
  }
  ```

3. For private repos, add the environment variable `CODECOV_TOKEN` to travis.

4. add the following to your `.travis.yml`:

  ```yaml
  after_success: npm run coverage
  ```

That's all there is to it!

## Integrating with TAP formatters
Many testing frameworks (Mocha, Tape, Tap, etc.) can produce [TAP](https://en.wikipedia.org/wiki/Test_Anything_Protocol) output. [tap-nyc](https://github.com/MegaArman/tap-nyc) is a TAP formatter designed to look nice with nyc.

## More tutorials

You can find more tutorials at http://istanbul.js.org/docs/tutorials
