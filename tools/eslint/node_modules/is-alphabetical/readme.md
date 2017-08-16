# is-alphabetical [![Build Status][travis-badge]][travis] [![Coverage Status][codecov-badge]][codecov]

Check if a character is alphabetical.

## Installation

[npm][]:

```bash
npm install is-alphabetical
```

## Usage

```javascript
var alphabetical = require('is-alphabetical');

alphabetical('a'); // true
alphabetical('B'); // true
alphabetical('0'); // false
alphabetical('💩'); // false
```

## API

### `alphabetical(character|code)`

Check whether the given character code (`number`), or the character
code at the first position (`string`), is alphabetical.

## Related

*   [`is-decimal`](https://github.com/wooorm/is-decimal)
*   [`is-hexadecimal`](https://github.com/wooorm/is-hexadecimal)
*   [`is-alphanumerical`](https://github.com/wooorm/is-alphanumerical)
*   [`is-whitespace-character`](https://github.com/wooorm/is-whitespace-character)
*   [`is-word-character`](https://github.com/wooorm/is-word-character)

## License

[MIT][license] © [Titus Wormer][author]

<!-- Definitions -->

[travis-badge]: https://img.shields.io/travis/wooorm/is-alphabetical.svg

[travis]: https://travis-ci.org/wooorm/is-alphabetical

[codecov-badge]: https://img.shields.io/codecov/c/github/wooorm/is-alphabetical.svg

[codecov]: https://codecov.io/github/wooorm/is-alphabetical

[npm]: https://docs.npmjs.com/cli/install

[license]: LICENSE

[author]: http://wooorm.com
