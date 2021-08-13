# bail

[![Build][build-badge]][build]
[![Coverage][coverage-badge]][coverage]
[![Downloads][downloads-badge]][downloads]
[![Size][size-badge]][size]

:warning: Throw a given error.

## Install

This package is ESM only: Node 12+ is needed to use it and it must be `import`ed
instead of `require`d.

[npm][]:

```sh
npm install bail
```

## Use

```js
import {bail} from 'bail'

bail()

bail(new Error('failure'))
// Error: failure
//     at repl:1:6
//     at REPLServer.defaultEval (repl.js:154:27)
//     …
```

## API

This package exports the following identifiers: `bail`.
There is no default export.

### `bail([err])`

Throw a given error.

###### Parameters

*   `err` (`Error?`) — Optional error.

###### Throws

*   `Error` — Given error, if any.

## Related

*   [`noop`][noop]
*   [`noop2`][noop2]
*   [`noop3`][noop3]

## License

[MIT][license] © [Titus Wormer][author]

<!-- Definitions -->

[build-badge]: https://github.com/wooorm/bail/workflows/main/badge.svg

[build]: https://github.com/wooorm/bail/actions

[coverage-badge]: https://img.shields.io/codecov/c/github/wooorm/bail.svg

[coverage]: https://codecov.io/github/wooorm/bail

[downloads-badge]: https://img.shields.io/npm/dm/bail.svg

[downloads]: https://www.npmjs.com/package/bail

[size-badge]: https://img.shields.io/bundlephobia/minzip/bail.svg

[size]: https://bundlephobia.com/result?p=bail

[npm]: https://docs.npmjs.com/cli/install

[license]: license

[author]: https://wooorm.com

[noop]: https://www.npmjs.com/package/noop

[noop2]: https://www.npmjs.com/package/noop2

[noop3]: https://www.npmjs.com/package/noop3
