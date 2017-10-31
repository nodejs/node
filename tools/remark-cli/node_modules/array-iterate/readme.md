# array-iterate [![Build Status][travis-badge]][travis] [![Coverage Status][codecov-badge]][codecov]

[`Array#forEach()`][foreach] with the possibility to change the next
position.

## Installation

[npm][]:

```bash
npm install array-iterate
```

## Usage

```js
var iterate = require('array-iterate');
var isFirst = true;
var context = 'iterate';

iterate([1, 2, 3, 4], function (value, index, values) {
  console.log(this, value, index, values)

  if (isFirst && index + 1 === values.length) {
    isFirst = false;
    return 0;
  }
}, context);
```

Yields:

```js
[String: 'iterate'], 1, 0, [ 1, 2, 3, 4 ]
[String: 'iterate'], 2, 1, [ 1, 2, 3, 4 ]
[String: 'iterate'], 3, 2, [ 1, 2, 3, 4 ]
[String: 'iterate'], 4, 3, [ 1, 2, 3, 4 ]
[String: 'iterate'], 1, 0, [ 1, 2, 3, 4 ]
[String: 'iterate'], 2, 1, [ 1, 2, 3, 4 ]
[String: 'iterate'], 3, 2, [ 1, 2, 3, 4 ]
[String: 'iterate'], 4, 3, [ 1, 2, 3, 4 ]
```

## API

### `iterate(values, callback[, context])`

Functions just like [`Array#forEach()`][foreach], but when `callback`
returns a `number`, iterates over the item at `number` next.

###### Parameters

*   `values` (`Array`-like thing) — Values to iterate over
*   `callback` ([`Function`][callback]) — Callback given to `iterate`
*   `context` (`*`, optional) — Context object to use when invoking `callback`

#### `function callback(value, index, values)`

Callback given to `iterate`.

###### Parameters

*   `value` (`*`) — Current iteration
*   `index` (`number`) — Position of `value` in `values`
*   `values` (`Array`-like thing) — Currently iterated over

###### Context

`context`, when given to `iterate`.

###### Returns

`number` (optional) — Position to go to next.

## License

[MIT][license] © [Titus Wormer][author]

<!-- Definitions -->

[travis-badge]: https://img.shields.io/travis/wooorm/array-iterate.svg

[travis]: https://travis-ci.org/wooorm/array-iterate

[codecov-badge]: https://img.shields.io/codecov/c/github/wooorm/array-iterate.svg

[codecov]: https://codecov.io/github/wooorm/array-iterate

[npm]: https://docs.npmjs.com/cli/install

[license]: LICENSE

[author]: http://wooorm.com

[foreach]: https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Array/forEach

[callback]: #function-callbackvalue-index-values
