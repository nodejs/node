# is-alphabetical [![Build Status][travis-badge]][travis] [![Coverage Status][codecov-badge]][codecov]

<!--lint disable heading-increment list-item-spacing-->

Check if a character is alphabetical.

## Installation

[npm][npm-install]:

```bash
npm install is-alphabetical
```

## Usage

Dependencies:

```javascript
var alphabetical = require('is-alphabetical');

alphabetical('a'); // true
alphabetical('B'); // true
alphabetical('0'); // false
alphabetical('💩'); // false
```

## API

### `alphabetical(character)`

Check whether the given character code (`number`), or the character
code at the first position (`string`), is alphabetical.

## Related

*   [`is-decimal`](https://github.com/wooorm/is-decimal)
*   [`is-hexadecimal`](https://github.com/wooorm/is-hexadecimal)

## License

[MIT][license] © [Titus Wormer][author]

<!-- Definitions -->

[travis-badge]: https://img.shields.io/travis/wooorm/is-alphabetical.svg

[travis]: https://travis-ci.org/wooorm/is-alphabetical

[codecov-badge]: https://img.shields.io/codecov/c/github/wooorm/is-alphabetical.svg

[codecov]: https://codecov.io/github/wooorm/is-alphabetical

[npm-install]: https://docs.npmjs.com/cli/install

[license]: LICENSE

[author]: http://wooorm.com
