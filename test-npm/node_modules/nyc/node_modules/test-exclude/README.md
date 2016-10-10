# test-exclude

The file include/exclude logic used by [nyc](https://github.com/bcoe/nyc).

[![Build Status](https://travis-ci.org/bcoe/test-exclude.svg)](https://travis-ci.org/bcoe/test-exclude)
[![Coverage Status](https://coveralls.io/repos/github/bcoe/test-exclude/badge.svg?branch=master)](https://coveralls.io/github/bcoe/test-exclude?branch=master)
[![Standard Version](https://img.shields.io/badge/release-standard%20version-brightgreen.svg)](https://github.com/conventional-changelog/standard-version)

## Usage

```js
const exclude = require('test-exclude')
if (exclude().shouldInstrument('./foo.js')) {
  // let's instrument this file for test coverage!
}
```

_you can load configuration from a key in package.json:_

_package.json_

```json
{
  "name": "awesome-module",
  "test": {
    "include": ["**/index.js"]
  }
}
```

_app.js_

```js
const exclude = require('test-exclude')
if (exclude({configKey: 'test'}).shouldInstrument('./index.js')) {
  // let's instrument this file for test coverage!
}
```

## License

ISC
