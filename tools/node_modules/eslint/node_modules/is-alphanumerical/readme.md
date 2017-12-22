# is-alphanumerical [![Build Status][travis-badge]][travis] [![Coverage Status][codecov-badge]][codecov]

Check if a character is alphanumerical (`[a-zA-Z0-9]`).

## Installation

[npm][]:

```bash
npm install is-alphanumerical
```

## Usage

```javascript
var alphanumerical = require('is-alphanumerical');

alphanumerical('a'); //=> true
alphanumerical('Z'); //=> true
alphanumerical('0'); //=> true
alphanumerical(' '); //=> false
alphanumerical('💩'); //=> false
```

## API

### `alphanumerical(character)`

Check whether the given character code (`number`), or the character
code at the first position (`string`), is alphanumerical.

## Related

*   [`is-alphabetical`](https://github.com/wooorm/is-alphabetical)
*   [`is-decimal`](https://github.com/wooorm/is-decimal)
*   [`is-hexadecimal`](https://github.com/wooorm/is-hexadecimal)
*   [`is-whitespace-character`](https://github.com/wooorm/is-whitespace-character)
*   [`is-word-character`](https://github.com/wooorm/is-word-character)

## License

[MIT][license] © [Titus Wormer][author]

<!-- Definitions -->

[travis-badge]: https://img.shields.io/travis/wooorm/is-alphanumerical.svg

[travis]: https://travis-ci.org/wooorm/is-alphanumerical

[codecov-badge]: https://img.shields.io/codecov/c/github/wooorm/is-alphanumerical.svg

[codecov]: https://codecov.io/github/wooorm/is-alphanumerical

[npm]: https://docs.npmjs.com/cli/install

[license]: LICENSE

[author]: http://wooorm.com
