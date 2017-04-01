# ccount [![Build Status][travis-badge]][travis] [![Coverage Status][codecov-badge]][codecov]

<!--lint disable heading-increment list-item-spacing-->

Count characters.

## Installation

[npm][npm-install]:

```bash
npm install ccount
```

## Usage

```javascript
var ccount = require('ccount');

ccount('foo(bar(baz)', '(') // 2
ccount('foo(bar(baz)', ')') // 1
```

## API

### `ccount(value, character)`

Get the total count of `character` in `value`.

###### Parameters

*   `value` (`string`) — Content, coerced to string.
*   `character` (`string`) — Single character to look for.

###### Returns

`number` — Number of times `character` occurred in `value`.

## License

[MIT][license] © [Titus Wormer][author]

<!-- Definitions -->

[travis-badge]: https://img.shields.io/travis/wooorm/ccount.svg

[travis]: https://travis-ci.org/wooorm/ccount

[codecov-badge]: https://img.shields.io/codecov/c/github/wooorm/ccount.svg

[codecov]: https://codecov.io/github/wooorm/ccount

[npm-install]: https://docs.npmjs.com/cli/install

[license]: LICENSE

[author]: http://wooorm.com
