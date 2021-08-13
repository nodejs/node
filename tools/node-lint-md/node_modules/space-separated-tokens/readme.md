# space-separated-tokens

[![Build][build-badge]][build]
[![Coverage][coverage-badge]][coverage]
[![Downloads][downloads-badge]][downloads]
[![Size][size-badge]][size]

Parse and stringify space-separated tokens according to the [spec][].

## Install

This package is ESM only: Node 12+ is needed to use it and it must be `import`ed
instead of `require`d.

[npm][]:

```sh
npm install space-separated-tokens
```

## Usage

```js
import {parse, stringify} from 'space-separated-tokens'

parse(' foo\tbar\nbaz  ')
//=> ['foo', 'bar', 'baz']

stringify(['foo', 'bar', 'baz'])
//=> 'foo bar baz'
```

## API

This package exports the following identifiers: `parse`, `stringify`.
There is no default export.

### `parse(value)`

Parse space separated tokens (`string`) to an array of strings (`string[]`),
according to the [spec][].

### `stringify(values)`

Serialize an array of strings (`string[]`) to space separated tokens (`string`).
Note that it’s not possible to specify empty or whitespace only values.

## Related

*   [`collapse-white-space`](https://github.com/wooorm/collapse-white-space)
    — Replace multiple white-space characters with a single space
*   [`property-information`](https://github.com/wooorm/property-information)
    — Information on HTML properties
*   [`comma-separated-tokens`](https://github.com/wooorm/comma-separated-tokens)
    — Parse/stringify comma-separated tokens

## License

[MIT][license] © [Titus Wormer][author]

<!-- Definition -->

[build-badge]: https://github.com/wooorm/space-separated-tokens/workflows/main/badge.svg

[build]: https://github.com/wooorm/space-separated-tokens/actions

[coverage-badge]: https://img.shields.io/codecov/c/github/wooorm/space-separated-tokens.svg

[coverage]: https://codecov.io/github/wooorm/space-separated-tokens

[downloads-badge]: https://img.shields.io/npm/dm/space-separated-tokens.svg

[downloads]: https://www.npmjs.com/package/space-separated-tokens

[size-badge]: https://img.shields.io/bundlephobia/minzip/space-separated-tokens.svg

[size]: https://bundlephobia.com/result?p=space-separated-tokens

[npm]: https://docs.npmjs.com/cli/install

[license]: license

[author]: https://wooorm.com

[spec]: https://html.spec.whatwg.org/#space-separated-tokens
