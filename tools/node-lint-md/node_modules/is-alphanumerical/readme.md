# is-alphanumerical

[![Build][build-badge]][build]
[![Coverage][coverage-badge]][coverage]
[![Downloads][downloads-badge]][downloads]
[![Size][size-badge]][size]

Check if a character is alphanumerical (`[a-zA-Z0-9]`).

## Install

This package is ESM only: Node 12+ is needed to use it and it must be `import`ed
instead of `require`d.

[npm][]:

```sh
npm install is-alphanumerical
```

## Use

```js
import {isAlphanumerical} from 'is-alphanumerical'

isAlphanumerical('a') // => true
isAlphanumerical('Z') // => true
isAlphanumerical('0') // => true
isAlphanumerical(' ') // => false
isAlphanumerical('💩') // => false
```

## API

This package exports the following identifiers: `isAlphanumerical`.
There is no default export.

### `isAlphanumerical(character)`

Check whether the given character code (`number`), or the character code at the
first position (`string`), is alphanumerical.

## Related

*   [`is-alphabetical`](https://github.com/wooorm/is-alphabetical)
*   [`is-decimal`](https://github.com/wooorm/is-decimal)
*   [`is-hexadecimal`](https://github.com/wooorm/is-hexadecimal)
*   [`is-whitespace-character`](https://github.com/wooorm/is-whitespace-character)
*   [`is-word-character`](https://github.com/wooorm/is-word-character)

## License

[MIT][license] © [Titus Wormer][author]

<!-- Definitions -->

[build-badge]: https://github.com/wooorm/is-alphanumerical/workflows/main/badge.svg

[build]: https://github.com/wooorm/is-alphanumerical/actions

[coverage-badge]: https://img.shields.io/codecov/c/github/wooorm/is-alphanumerical.svg

[coverage]: https://codecov.io/github/wooorm/is-alphanumerical

[downloads-badge]: https://img.shields.io/npm/dm/is-alphanumerical.svg

[downloads]: https://www.npmjs.com/package/is-alphanumerical

[size-badge]: https://img.shields.io/bundlephobia/minzip/is-alphanumerical.svg

[size]: https://bundlephobia.com/result?p=is-alphanumerical

[npm]: https://docs.npmjs.com/cli/install

[license]: license

[author]: https://wooorm.com
