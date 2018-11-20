# is-hexadecimal [![Build Status][travis-badge]][travis] [![Coverage Status][codecov-badge]][codecov]

Check if a character is hexadecimal.

## Installation

[npm][]:

```bash
npm install is-hexadecimal
```

## Usage

```javascript
var hexadecimal = require('is-hexadecimal')

hexadecimal('a') // => true
hexadecimal('0') // => true
hexadecimal('G') // => false
hexadecimal('💩') // => false
```

## API

### `hexadecimal(character|code)`

Check whether the given character code (`number`), or the character
code at the first position (`string`), is hexadecimal.

## Related

*   [`is-alphabetical`](https://github.com/wooorm/is-alphabetical)
*   [`is-alphanumerical`](https://github.com/wooorm/is-alphabetical)
*   [`is-decimal`](https://github.com/wooorm/is-decimal)
*   [`is-whitespace-character`](https://github.com/wooorm/is-whitespace-character)
*   [`is-word-character`](https://github.com/wooorm/is-word-character)

## License

[MIT][license] © [Titus Wormer][author]

<!-- Definitions -->

[travis-badge]: https://img.shields.io/travis/wooorm/is-hexadecimal.svg

[travis]: https://travis-ci.org/wooorm/is-hexadecimal

[codecov-badge]: https://img.shields.io/codecov/c/github/wooorm/is-hexadecimal.svg

[codecov]: https://codecov.io/github/wooorm/is-hexadecimal

[npm]: https://docs.npmjs.com/cli/install

[license]: LICENSE

[author]: http://wooorm.com
