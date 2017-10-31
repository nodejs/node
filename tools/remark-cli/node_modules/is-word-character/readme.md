# is-word-character [![Build Status][travis-badge]][travis] [![Coverage Status][codecov-badge]][codecov]

Check if a character is a word character (`\w`, which equals
`[a-zA-Z0-9_]`).

## Installation

[npm][]:

```bash
npm install is-word-character
```

## Usage

```javascript
var wordCharacter = require('is-word-character');

wordCharacter('a'); //=> true
wordCharacter('Z'); //=> true
wordCharacter('0'); //=> true
wordCharacter('_'); //=> true
wordCharacter(' '); //=> false
wordCharacter('💩'); //=> false
```

## API

### `wordCharacter(character|code)`

Check whether the given character code (`number`), or the character
code at the first position (`string`), is a word character.

## Related

*   [`is-alphabetical`](https://github.com/wooorm/is-alphabetical)
*   [`is-alphanumerical`](https://github.com/wooorm/is-alphanumerical)
*   [`is-decimal`](https://github.com/wooorm/is-decimal)
*   [`is-hexadecimal`](https://github.com/wooorm/is-hexadecimal)
*   [`is-whitespace-character`](https://github.com/wooorm/is-whitespace-character)

## License

[MIT][license] © [Titus Wormer][author]

<!-- Definitions -->

[travis-badge]: https://img.shields.io/travis/wooorm/is-word-character.svg

[travis]: https://travis-ci.org/wooorm/is-word-character

[codecov-badge]: https://img.shields.io/codecov/c/github/wooorm/is-word-character.svg

[codecov]: https://codecov.io/github/wooorm/is-word-character

[npm]: https://docs.npmjs.com/cli/install

[license]: LICENSE

[author]: http://wooorm.com
