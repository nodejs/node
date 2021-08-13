# ccount

[![Build][build-badge]][build]
[![Coverage][coverage-badge]][coverage]
[![Downloads][downloads-badge]][downloads]
[![Size][size-badge]][size]

Count characters.

## Install

This package is ESM only: Node 12+ is needed to use it and it must be `import`ed
instead of `require`d.

[npm][]:

```sh
npm install ccount
```

## Use

```js
import {ccount} from 'ccount'

ccount('foo(bar(baz)', '(') // => 2
ccount('foo(bar(baz)', ')') // => 1
```

## API

This package exports the following identifiers: `ccount`.
There is no default export.

### `ccount(value, character)`

Get the total count of `character` in `value`.

###### Parameters

*   `value` (`string`) — Content, coerced to string
*   `character` (`string`) — Single character to look for

###### Returns

`number` — Number of times `character` occurred in `value`.

## License

[MIT][license] © [Titus Wormer][author]

<!-- Definitions -->

[build-badge]: https://github.com/wooorm/ccount/workflows/main/badge.svg

[build]: https://github.com/wooorm/ccount/actions

[coverage-badge]: https://img.shields.io/codecov/c/github/wooorm/ccount.svg

[coverage]: https://codecov.io/github/wooorm/ccount

[downloads-badge]: https://img.shields.io/npm/dm/ccount.svg

[downloads]: https://www.npmjs.com/package/ccount

[size-badge]: https://img.shields.io/bundlephobia/minzip/ccount.svg

[size]: https://bundlephobia.com/result?p=ccount

[npm]: https://docs.npmjs.com/cli/install

[license]: license

[author]: https://wooorm.com
