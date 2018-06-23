# trim-trailing-lines [![Build Status][travtrim-trailing-lines]][travis] [![Coverage Status][codecov-badge]][codecov]

Remove final newline characters from a string.

## Installation

[npm][]:

```bash
npm install trim-trailing-lines
```

## Usage

```js
var trimTrailingLines = require('trim-trailing-lines')

trimTrailingLines('foo\nbar') // => 'foo\nbar'
trimTrailingLines('foo\nbar\n') // => 'foo\nbar'
trimTrailingLines('foo\nbar\n\n') // => 'foo\nbar'
```

## API

### `trimTrailingLines(value)`

Remove final newline characters from `value`.

###### Parameters

*   `value` (`string`) — Value with trailing newlines, coerced to string.

###### Returns

`string` — Value without trailing newlines.

## License

[MIT][license] © [Titus Wormer][author]

<!-- Definitions -->

[travtrim-trailing-lines]: https://img.shields.io/travis/wooorm/trim-trailing-lines.svg

[travis]: https://travis-ci.org/wooorm/trim-trailing-lines

[codecov-badge]: https://img.shields.io/codecov/c/github/wooorm/trim-trailing-lines.svg

[codecov]: https://codecov.io/github/wooorm/trim-trailing-lines

[npm]: https://docs.npmjs.com/cli/install

[license]: LICENSE

[author]: http://wooorm.com
