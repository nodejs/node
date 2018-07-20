# is-decimal [![Build Status][travis-badge]][travis] [![Coverage Status][codecov-badge]][codecov]

Check if a character is decimal.

## Installation

[npm][]:

```bash
npm install is-decimal
```

## Usage

```javascript
var decimal = require('is-decimal')

decimal('0') // => true
decimal('9') // => true
decimal('a') // => false
decimal('💩') // => false
```

## API

### `decimal(character|code)`

Check whether the given character code (`number`), or the character
code at the first position (`string`), is decimal.

## Related

*   [`is-alphabetical`](https://github.com/wooorm/is-alphabetical)
*   [`is-hexadecimal`](https://github.com/wooorm/is-hexadecimal)
*   [`is-whitespace-character`](https://github.com/wooorm/is-whitespace-character)
*   [`is-word-character`](https://github.com/wooorm/is-word-character)

## License

[MIT][license] © [Titus Wormer][author]

<!-- Definitions -->

[travis-badge]: https://img.shields.io/travis/wooorm/is-decimal.svg

[travis]: https://travis-ci.org/wooorm/is-decimal

[codecov-badge]: https://img.shields.io/codecov/c/github/wooorm/is-decimal.svg

[codecov]: https://codecov.io/github/wooorm/is-decimal

[npm]: https://docs.npmjs.com/cli/install

[license]: LICENSE

[author]: http://wooorm.com
