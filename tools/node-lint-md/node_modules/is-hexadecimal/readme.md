# is-hexadecimal

[![Build][build-badge]][build]
[![Coverage][coverage-badge]][coverage]
[![Downloads][downloads-badge]][downloads]
[![Size][size-badge]][size]

Check if a character is hexadecimal.

## Install

This package is ESM only: Node 12+ is needed to use it and it must be `import`ed
instead of `require`d.

[npm][]:

```sh
npm install is-hexadecimal
```

## Use

```js
import {isHexadecimal} from 'is-hexadecimal'

isHexadecimal('a') // => true
isHexadecimal('0') // => true
isHexadecimal('G') // => false
isHexadecimal('💩') // => false
```

## API

This package exports the following identifiers: `isHexadecimal`.
There is no default export.

### `isHexadecimal(character|code)`

Check whether the given character code (`number`), or the character code at the
first position (`string`), is isHexadecimal.

## Related

*   [`is-alphabetical`](https://github.com/wooorm/is-alphabetical)
*   [`is-alphanumerical`](https://github.com/wooorm/is-alphabetical)
*   [`is-decimal`](https://github.com/wooorm/is-decimal)
*   [`is-whitespace-character`](https://github.com/wooorm/is-whitespace-character)
*   [`is-word-character`](https://github.com/wooorm/is-word-character)

## License

[MIT][license] © [Titus Wormer][author]

<!-- Definitions -->

[build-badge]: https://github.com/wooorm/is-hexadecimal/workflows/main/badge.svg

[build]: https://github.com/wooorm/is-hexadecimal/actions

[coverage-badge]: https://img.shields.io/codecov/c/github/wooorm/is-hexadecimal.svg

[coverage]: https://codecov.io/github/wooorm/is-hexadecimal

[downloads-badge]: https://img.shields.io/npm/dm/is-hexadecimal.svg

[downloads]: https://www.npmjs.com/package/is-hexadecimal

[size-badge]: https://img.shields.io/bundlephobia/minzip/is-hexadecimal.svg

[size]: https://bundlephobia.com/result?p=is-hexadecimal

[npm]: https://docs.npmjs.com/cli/install

[license]: license

[author]: https://wooorm.com
