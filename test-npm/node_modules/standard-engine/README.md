# Standard Engine
[![travis][travis-image]][travis-url]
[![npm][npm-image]][npm-url]
[![downloads][downloads-image]][downloads-url]

[travis-image]: https://img.shields.io/travis/Flet/standard-engine.svg?style=flat
[travis-url]: https://travis-ci.org/Flet/standard-engine
[npm-image]: https://img.shields.io/npm/v/standard-engine.svg?style=flat
[npm-url]: https://npmjs.org/package/standard-engine
[downloads-image]: https://img.shields.io/npm/dm/standard-engine.svg?style=flat
[downloads-url]: https://npmjs.org/package/standard-engine

## Overview
Wrap your own eslint rules in a easy-to-use command line tool and/or a JS module.

## Install
```
npm install standard-engine
```

## Who is using `standard-engine`?
Here is a list of packages using `standard-engine`. Dive into them for ideas!

- [standard](https://github.com/feross/standard) - JavaScript Standard Style.
-  [semistandard](https://github.com/Flet/semistandard) - Its `standard` with semicolons sprinkled on top.
- [happiness](https://github.com/JedWatson/happiness) - JavaScript Happiness Style (semicolons and tabs)
- [doublestandard](https://github.com/flet/doublestandard) - Require TWO semicolons at the end of every line!
- [strict-standard](https://github.com/denis-sokolov/strict-standard) - Standard Style with strict error checking.

Did you make your own? Create a pull request and we will add it to the README!

## Usage
Create the files below and fill in your own values for `options.js`.

**index.js**
```js
// programmatic usage
var Linter = require('standard-engine').linter
var opts = require('./options.js')
module.exports = new Linter(opts)
```

**cli.js**
```js
#!/usr/bin/env node

var opts = require('../options.js')
require('standard-engine').cli(opts)
```
**options.js**
```js
var eslint = require('eslint')
var path = require('path')
var pkg = require('./package.json')

module.exports = {
  // homepage, version and bugs pulled from package.json
  version: pkg.version,
  homepage: pkg.homepage,
  bugs: pkg.bugs.url,
  eslint: eslint, // pass any version of eslint >= 1.0.0
  cmd: 'pocketlint', // should match the "bin" key in your package.json
  tagline: 'Live by your own standards!', // displayed in output --help
  eslintConfig: {
    configFile: path.join(__dirname, 'eslintrc.json')
  },
  cwd: '', // current working directory, passed to eslint

  // These are optional. If included, the --format option will be made available
  formatter: require('pocketlint-format'), // note you'll have to create this :)
  formatterName: 'pocketlint-format'
}
```

**eslintrc.json**
 Put all your .eslintrc rules in this file. A good practice is to create an  [ESLint Shareable Config](http://eslint.org/docs/developer-guide/shareable-configs) and extend it, but its not required:
```js
{
  // pretend that the package eslint-config-pocketlint exists!
  "extends": ["pocketlint"]
}
```
Take a look at [eslint-config-standard](https://github.com/feross/eslint-config-standard) as an example, or if you want to extend/mutate `standard`, see [eslint-config-semistandard](https://github.com/flet/eslint-config-semistandard).

## Engine Features

### Ignoring Files

The paths `node_modules/**`, `*.min.js`, `bundle.js`, `coverage/**`, hidden files/folders
(beginning with `.`), and all patterns in a project's root `.gitignore` file are
automatically ignored.

Sometimes you need to ignore additional folders or specific minfied files. To do that, add
a `ignore` property to `package.json`:

```js
"pocketlint": { // this key should equal the value of cmd in options.js
  "ignore": [
    "**/out/",
    "/lib/select2/",
    "/lib/ckeditor/",
    "tmp.js"
  ]
}
```

### Hiding Warnings

Since `standard-engine` uses [`eslint`](http://eslint.org/) under-the-hood, you can
hide warnings as you normally would if you used `eslint` directly.

To get verbose output (so you can find the particular rule name to ignore), run:

```bash
$ pocketlint --verbose
Error: Live by your own standards!
  routes/error.js:20:36: 'file' was used before it was defined. (no-use-before-define)
```

Disable **all rules** on a specific line:

```js
file = 'I know what I am doing' // eslint-disable-line
```

Or, disable **only** the `"no-use-before-define"` rule:

```js
file = 'I know what I am doing' // eslint-disable-line no-use-before-define
```

Or, disable the `"no-use-before-define"` rule for **multiple lines**:

```js
/*eslint-disable no-use-before-define */
// offending code here...
// offending code here...
// offending code here...
/*eslint-enable no-use-before-define */
```

### Defining Globals in a project's  `package.json`

`standard-engine` will also look in a project's `package.json` and respect any global variables defined like so:

```js
{
  "pocketlint": { // this key should equal the value of cmd in options.js
    "global": [ "myVar1", "myVar2" ]
  }
}
```

### Custom JS parsers for bleeding-edge ES6 or ES7 support?

`standard-engine` supports custom JS parsers. To use a custom parser, install it from npm
(example: `npm install babel-eslint`) and add this to your `package.json`:

```js
{
  "pocketlint": { // this key should equal the value of cmd in your options.js
    "parser": "babel-eslint"
  }
}
```


If you're using your custom linter globally (you installed it with `-g`), then you also need to
install `babel-eslint` globally with `npm install babel-eslint -g`.

## API Usage

### `standardEngine.lintText(text, [opts], callback)`

Lint the provided source `text` to enforce your defined style. An `opts` object may
be provided:

```js
{
  globals: [],  // global variables to declare
  parser: ''    // custom js parser (e.g. babel-eslint)
}
```

The `callback` will be called with an `Error` and `results` object:

```js
{
  results: [
    {
      filePath: '',
      messages: [
        { ruleId: '', message: '', line: 0, column: 0 }
      ],
      errorCount: 0,
      warningCount: 0
    }
  ],
  errorCount: 0,
  warningCount: 0
}
```

### `standardEngine.lintFiles(files, [opts], callback)`

Lint the provided `files` globs. An `opts` object may be provided:

```js
{
  globals: [],  // global variables to declare
  parser: '',   // custom js parser (e.g. babel-eslint)
  ignore: [],   // file globs to ignore (has sane defaults)
  cwd: ''       // current working directory (default: process.cwd())
}
```

The `callback` will be called with an `Error` and `results` object (same as above).
