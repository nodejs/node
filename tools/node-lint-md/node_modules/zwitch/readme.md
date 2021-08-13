# zwitch

[![Build][build-badge]][build]
[![Coverage][coverage-badge]][coverage]
[![Downloads][downloads-badge]][downloads]
[![Size][size-badge]][size]

Handle values based on a property.

## Install

This package is ESM only: Node 12+ is needed to use it and it must be `import`ed
instead of `require`d.

[npm][]:

```sh
npm install zwitch
```

## Use

```js
import {zwitch} from 'zwitch'

var handle = zwitch('type', {invalid, unknown, handlers: {alpha: handleAlpha}})

handle({type: 'alpha'})

function handleAlpha() { /* … */ }
```

Or, with a `switch` statement:

```js
var field = 'type'

function handle(value) {
  var fn

  if (!value || typeof value !== 'object' || !(field in value)) {
    fn = invalid
  } else {
    switch (value[field]) {
      case 'alpha':
        fn = handleAlpha
        break
      default:
        fn = unknown
        break
    }
  }

  return fn.apply(this, arguments)
}

handle({type: 'alpha'})

function handleAlpha() { /* … */ }
```

## API

This package exports the following identifiers: `zwitch`.
There is no default export.

### `zwitch(key[, options])`

Create a functional switch, based on a `key` (`string`).

###### `options`

Options can be omitted and added later to `one`.

*   `handlers` (`Object.<Function>`, optional)
    — Object mapping values to handle, stored on `one.handlers`
*   `invalid` (`Function`, optional)
    — Handle values without `key`, stored on `one.invalid`
*   `unknown` (`Function`, optional)
    — Handle values with an unhandled `key`, stored on `one.unknown`

###### Returns

`Function` — See [`one`][one].

#### `one(value[, rest…])`

Handle one value.
Based on the bound `key`, a respective handler will be called.
If `value` is not an object, or doesn’t have a `key` property, the special
“invalid” handler will be called.
If `value` has an unknown `key`, the special “unknown” handler will be called.

All arguments, and the context object, are passed through to the [handler][],
and it’s result is returned.

#### `one.handlers`

Map of [handler][]s (`Object.<string, Function>`).

#### `one.invalid`

Special [`handler`][handler] called if a value doesn’t have a `key` property.
If not set, `undefined` is returned for invalid values.

#### `one.unknown`

Special [`handler`][handler] called if a value does not have a matching
handler.
If not set, `undefined` is returned for unknown values.

### `function handler(value[, rest…])`

Handle one value.

## Related

*   [`mapz`](https://github.com/wooorm/mapz)
    — Functional map

## License

[MIT][license] © [Titus Wormer][author]

<!-- Definitions -->

[build-badge]: https://github.com/wooorm/zwitch/workflows/main/badge.svg

[build]: https://github.com/wooorm/zwitch/actions

[coverage-badge]: https://img.shields.io/codecov/c/github/wooorm/zwitch.svg

[coverage]: https://codecov.io/github/wooorm/zwitch

[downloads-badge]: https://img.shields.io/npm/dm/zwitch.svg

[downloads]: https://www.npmjs.com/package/zwitch

[size-badge]: https://img.shields.io/bundlephobia/minzip/zwitch.svg

[size]: https://bundlephobia.com/result?p=zwitch

[npm]: https://docs.npmjs.com/cli/install

[license]: license

[author]: https://wooorm.com

[one]: #onevalue-rest

[handler]: #function-handlervalue-rest
