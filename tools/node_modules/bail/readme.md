# bail [![Build Status](https://img.shields.io/travis/wooorm/bail.svg)](https://travis-ci.org/wooorm/bail) [![Coverage Status](https://img.shields.io/codecov/c/github/wooorm/bail.svg)](https://codecov.io/github/wooorm/bail?branch=master)

:warning: Throw a given error.

## Installation

[npm](https://docs.npmjs.com/cli/install):

```bash
npm install bail
```

**bail** is also available for [bower](http://bower.io/#install-packages),
[component](https://github.com/componentjs/component), [duo](http://duojs.org/#getting-started),
and for AMD, CommonJS, and globals ([uncompressed](bail.js) and
[compressed](bail.min.js)).

## Usage

```js
var bail = require('bail');

bail();

bail(new Error('failure'));
// Error: failure
//     at repl:1:6
//     at REPLServer.defaultEval (repl.js:154:27)
//     ...
```

## API

### bail(err?)

Throw a given error.

**Parameters**

*   `err` (`Error`, optional) — Optional error.

**Throws**

*   `Error` - `err`, When given.

## Why

No one modularized this yet. And the name was available.

## License

[MIT](LICENSE) @ [Titus Wormer](http://wooorm.com)
