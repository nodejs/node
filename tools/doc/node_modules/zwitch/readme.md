# zwitch [![Build Status][travis-badge]][travis] [![Coverage Status][codecov-badge]][codecov]

Handle values based on a property.

## Installation

[npm][]:

```bash
npm install zwitch
```

## Usage

```javascript
var zwitch = require('zwitch')

var handle = zwitch('type')

handle.invalid = invalid
handle.unknown = unknown
handle.handlers.alpha = handle

handle({type: 'alpha'})
```

Or, with a `switch` statement:

```javascript
function handle(value) {
  var fn

  if (!value || typeof value !== 'object' || !('type' in value)) {
    fn = invalid
  } else {
    switch (value.type) {
      case 'alpha':
        fn = handle
        break
      default:
        fn = unknown
        break
    }
  }

  return fn.apply(this, arguments)
}

handle({type: 'alpha'})
```

## API

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

#### `one(value[, rest...])`

Handle one value.  Based on the bound `key`, a respective handler will
be invoked.  If `value` is not an object, or doesn’t have a `key`
property, the special “invalid” handler will be invoked.  If `value`
has an unknown `key`, the special “unknown” handler will be invoked.

All arguments, and the context object, are passed through to the
[handler][], and it’s result is returned.

#### `one.handlers`

Map of [handler][]s (`Object.<string, Function>`).

#### `one.invalid`

Special [`handler`][handler] invoked if a value doesn’t have a `key`
property.  If not set, `undefined` is returned for invalid values.

#### `one.unknown`

Special [`handler`][handler] invoked if a value does not have a matching
handler.  If not set, `undefined` is returned for unknown values.

### `function handler(value[, rest...])`

Handle one value.

## Related

*   [`mapz`](https://github.com/wooorm/mapz)
    — Functional map

## License

[MIT][license] © [Titus Wormer][author]

<!-- Definitions -->

[travis-badge]: https://img.shields.io/travis/wooorm/zwitch.svg

[travis]: https://travis-ci.org/wooorm/zwitch

[codecov-badge]: https://img.shields.io/codecov/c/github/wooorm/zwitch.svg

[codecov]: https://codecov.io/github/wooorm/zwitch

[npm]: https://docs.npmjs.com/cli/install

[license]: LICENSE

[author]: http://wooorm.com

[one]: #onevalue-rest

[handler]: #function-handlervalue-rest
