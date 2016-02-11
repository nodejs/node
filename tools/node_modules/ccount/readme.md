# ccount [![Build Status](https://img.shields.io/travis/wooorm/ccount.svg?style=flat)](https://travis-ci.org/wooorm/ccount) [![Coverage Status](https://img.shields.io/coveralls/wooorm/ccount.svg?style=flat)](https://coveralls.io/r/wooorm/ccount?branch=master)

Count characters.

## Installation

[npm](https://docs.npmjs.com/cli/install):

```bash
npm install ccount
```

**ccount** is also available for [bower](http://bower.io/#install-packages),
[component](https://github.com/componentjs/component), [duo](http://duojs.org/#getting-started),
and for AMD, CommonJS, and globals ([uncompressed](ccount.js) and
[compressed](ccount.min.js)).

## Usage

```javascript
var ccount = require('ccount');

ccount('foo(bar(baz)', '(') // 2
ccount('foo(bar(baz)', ')') // 1
```

## API

### ccount(value, character)

Get the total count of `character` in `value`.

Parameters:

*   `value` (`string`) — Content, coerced to string.
*   `character` (`string`) — Single character to look for.

Returns: `number` — Number of times `character` occurred in `value`.

Throws:

*   `Error` — when `character` is not a single character string.

## License

[MIT](LICENSE) @ [Titus Wormer](http://wooorm.com)
