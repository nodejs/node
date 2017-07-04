# is-whitespace-character [![Build Status][travis-badge]][travis] [![Coverage Status][codecov-badge]][codecov]

<!--lint disable heading-increment list-item-spacing-->

Check if a character is a whitespace character: `\s`, which equals
all Unicode Space Separators (including `[ \t\v\f]`), the BOM
(`\uFEFF`), and line terminator (`[\n\r\u2028\u2029]`).

## Installation

[npm][npm-install]:

```bash
npm install is-whitespace-character
```

## Usage

```javascript
var whitespace = require('is-whitespace-character');

whitespace(' '); // true
whitespace('\n'); // true
whitespace('\ufeff'); // true
whitespace('_'); // false
whitespace('a'); // true
whitespace('💩'); // false
```

## API

### `whitespaceCharacter(character)`

Check whether the given character code (`number`), or the character
code at the first position (`string`), is a whitespace character.

## Related

*   [`is-alphabetical`](https://github.com/wooorm/is-alphabetical)
*   [`is-alphanumerical`](https://github.com/wooorm/is-alphanumerical)
*   [`is-decimal`](https://github.com/wooorm/is-decimal)
*   [`is-hexadecimal`](https://github.com/wooorm/is-hexadecimal)
*   [`is-word-character`](https://github.com/wooorm/is-word-character)

## License

[MIT][license] © [Titus Wormer][author]

<!-- Definitions -->

[travis-badge]: https://img.shields.io/travis/wooorm/is-whitespace-character.svg

[travis]: https://travis-ci.org/wooorm/is-whitespace-character

[codecov-badge]: https://img.shields.io/codecov/c/github/wooorm/is-whitespace-character.svg

[codecov]: https://codecov.io/github/wooorm/is-whitespace-character

[npm-install]: https://docs.npmjs.com/cli/install

[license]: LICENSE

[author]: http://wooorm.com
