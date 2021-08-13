# is-decimal

[![Build][build-badge]][build]
[![Coverage][coverage-badge]][coverage]
[![Downloads][downloads-badge]][downloads]
[![Size][size-badge]][size]

Check if a character is decimal.

## Install

This package is ESM only: Node 12+ is needed to use it and it must be `import`ed
instead of `require`d.

[npm][]:

```sh
npm install is-decimal
```

## Use

```js
import {isDecimal} from 'is-decimal'

isDecimal('0') // => true
isDecimal('9') // => true
isDecimal('a') // => false
isDecimal('💩') // => false
```

## API

This package exports the following identifiers: `isDecimal`.
There is no default export.

### `isDecimal(character|code)`

Check whether the given character code (`number`), or the character code at the
first position (`string`), is decimal.

## Related

*   [`is-alphabetical`](https://github.com/wooorm/is-alphabetical)
*   [`is-hexadecimal`](https://github.com/wooorm/is-hexadecimal)
*   [`is-whitespace-character`](https://github.com/wooorm/is-whitespace-character)
*   [`is-word-character`](https://github.com/wooorm/is-word-character)

## License

[MIT][license] © [Titus Wormer][author]

<!-- Definitions -->

[build-badge]: https://github.com/wooorm/is-decimal/workflows/main/badge.svg

[build]: https://github.com/wooorm/is-decimal/actions

[coverage-badge]: https://img.shields.io/codecov/c/github/wooorm/is-decimal.svg

[coverage]: https://codecov.io/github/wooorm/is-decimal

[downloads-badge]: https://img.shields.io/npm/dm/is-decimal.svg

[downloads]: https://www.npmjs.com/package/is-decimal

[size-badge]: https://img.shields.io/bundlephobia/minzip/is-decimal.svg

[size]: https://bundlephobia.com/result?p=is-decimal

[npm]: https://docs.npmjs.com/cli/install

[license]: license

[author]: https://wooorm.com
