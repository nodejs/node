# collapse-white-space [![Build Status](https://img.shields.io/travis/wooorm/collapse-white-space.svg?style=flat)](https://travis-ci.org/wooorm/collapse-white-space) [![Coverage Status](https://img.shields.io/coveralls/wooorm/collapse-white-space.svg?style=flat)](https://coveralls.io/r/wooorm/collapse-white-space?branch=master)

Replace multiple white-space characters with a single space.

## Installation

[npm](https://docs.npmjs.com/cli/install):

```bash
npm install collapse-white-space
```

**collapse-white-space** is also available for [bower](http://bower.io/#install-packages),
[component](https://github.com/componentjs/component), [duo](http://duojs.org/#getting-started),
and for AMD, CommonJS, and globals ([uncompressed](collapse-white-space.js) and
[compressed](collapse-white-space.min.js)).

## Usage

Dependencies.

```javascript
var collapse = require('collapse-white-space');
```

Collapse white space:

```javascript
var result = collapse('\tfoo \n\tbar  \t\r\nbaz');
```

Yields:

```text
 foo bar baz
```

## API

### collapse(value)

Replace multiple white-space characters with a single space.

Parameters:

*   `value` (`string`) — Value with uncollapsed white-space, coerced to string.

Returns: `string` — Value with collapsed white-space.

## License

[MIT](LICENSE) @ [Titus Wormer](http://wooorm.com)
