# mapz [![Build Status][travis-badge]][travis] [![Coverage Status][codecov-badge]][codecov]

Functional map with sugar.

## Installation

[npm][]:

```bash
npm install mapz
```

## Usage

```javascript
var mapz = require('mapz')

var map = mapz(fn, {key: 'children', gapless: true})

map({children: [1, 2, 3]}) // => ['Hi, 2', 'Hi, 3']

function fn(value) {
  return value > 1 ? 'Hi, ' + value + '.' : null
}
```

## API

### `mapz(fn[, options])`

Functional map with sugar (functional, as values are provided as a
parameter, instead of context object).

Wraps the supplied [`fn`][fn], which handles one value, so that it
accepts multiple values, invoking `fn` for each and returning all
results.

If `options` is a string, it’s treated as `{key: options}`.

###### `options.gapless`

Whether to filter out `null` and `undefined` results `boolean`,
default: `false`.

###### `options.indices`

Whether to invoke `fn` with the index of the value in its context
(`boolean`, default: `true`);

###### `options.key`

If a key (`string`, optional) is given, and an object supplied to the
wrapped `fn`, values at that object’s `key` property are mapped and
the object, instead of the values, is given to `fn` as a last
parameter.  If a key is given and an array is passed to the wrapped
`fn`, no value is given to `fn` as a last parameter.

###### Returns

`Function` — See [`map(values)`][map]

#### `map(values)`

Invoke the bound [`fn`][fn] for all values.  If a `key` is bound,
`values` can be an object.  See [`options.key`][key] for more info.

###### Returns

`Array.<*>` — Values returned by `fn`.  If `gapless` is `true`, `null`
or `undefined` results are not returned by `map`.

#### `fn(value[, index], parent?)`

Handle one value.  If `indices` is `false`, no index parameter is
passed.  If `key` is set and an array is given, no `parent` is passed.

###### Returns

`*` — Any value.

## Related

*   [`zwitch`](https://github.com/wooorm/zwitch)
    — Handle values based on a property

## License

[MIT][license] © [Titus Wormer][author]

<!-- Definitions -->

[travis-badge]: https://img.shields.io/travis/wooorm/mapz.svg

[travis]: https://travis-ci.org/wooorm/mapz

[codecov-badge]: https://img.shields.io/codecov/c/github/wooorm/mapz.svg

[codecov]: https://codecov.io/github/wooorm/mapz

[npm]: https://docs.npmjs.com/cli/install

[license]: LICENSE

[author]: http://wooorm.com

[map]: #mapvalues

[key]: #optionskey

[fn]: #fnvalue-index-parent
