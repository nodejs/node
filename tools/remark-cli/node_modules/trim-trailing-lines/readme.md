# trim-trailing-lines [![Build Status](https://img.shields.io/travis/wooorm/trim-trailing-lines.svg?style=flat)](https://travis-ci.org/wooorm/trim-trailing-lines) [![Coverage Status](https://img.shields.io/coveralls/wooorm/trim-trailing-lines.svg?style=flat)](https://coveralls.io/r/wooorm/trim-trailing-lines?branch=master)

Remove final newline characters from a string.

## Installation

[npm](https://docs.npmjs.com/cli/install):

```bash
npm install trim-trailing-lines
```

**trim-trailing-lines** is also available for [bower](http://bower.io/#install-packages),
[component](https://github.com/componentjs/component), [duo](http://duojs.org/#getting-started),
and for AMD, CommonJS, and globals ([uncompressed](trim-trailing-lines.js) and
[compressed](trim-trailing-lines.min.js)).

## Usage

Dependencies.

```javascript
var trimTrailingLines = require('trim-trailing-lines');
```

Trim trailing newline characters:

```javascript
trimTrailingLines('foo\nbar'); // 'foo\nbar'
trimTrailingLines('foo\nbar\n'); // 'foo\nbar'
trimTrailingLines('foo\nbar\n\n'); // 'foo\nbar'
```

## API

### trimTrailingLines(value)

Remove final newline characters from `value`.

Parameters:

*   `value` (`string`) — Value with trailing newlines, coerced to string.

Returns: `string` — Value without trailing newlines.

## License

[MIT](LICENSE) @ [Titus Wormer](http://wooorm.com)
