# is-alphabetical

[![Build][build-badge]][build]
[![Coverage][coverage-badge]][coverage]
[![Downloads][downloads-badge]][downloads]
[![Size][size-badge]][size]

Check if a character is (ASCII) alphabetical.

## Install

This package is ESM only: Node 12+ is needed to use it and it must be `import`ed
instead of `require`d.

[npm][]:

```sh
npm install is-alphabetical
```

## Use

```js
import {isAlphabetical} from 'is-alphabetical'

isAlphabetical('a') // => true
isAlphabetical('B') // => true
isAlphabetical('0') // => false
isAlphabetical('💩') // => false
```

## API

This package exports the following identifiers: `isAlphabetical`.
There is no default export.

### `isAlphabetical(character|code)`

Check whether the given character code (`number`), or the character code at the
first position (`string`), is alphabetical.

## Related

*   [`is-decimal`](https://github.com/wooorm/is-decimal)
*   [`is-hexadecimal`](https://github.com/wooorm/is-hexadecimal)
*   [`is-alphanumerical`](https://github.com/wooorm/is-alphanumerical)
*   [`is-whitespace-character`](https://github.com/wooorm/is-whitespace-character)
*   [`is-word-character`](https://github.com/wooorm/is-word-character)

## License

[MIT][license] © [Titus Wormer][author]

<!-- Definitions -->

[build-badge]: https://github.com/wooorm/is-alphabetical/workflows/main/badge.svg

[build]: https://github.com/wooorm/is-alphabetical/actions

[coverage-badge]: https://img.shields.io/codecov/c/github/wooorm/is-alphabetical.svg

[coverage]: https://codecov.io/github/wooorm/is-alphabetical

[downloads-badge]: https://img.shields.io/npm/dm/is-alphabetical.svg

[downloads]: https://www.npmjs.com/package/is-alphabetical

[size-badge]: https://img.shields.io/bundlephobia/minzip/is-alphabetical.svg

[size]: https://bundlephobia.com/result?p=is-alphabetical

[npm]: https://docs.npmjs.com/cli/install

[license]: license

[author]: https://wooorm.com
