# unist-util-stringify-position [![Build Status][build-badge]][build-page] [![Coverage Status][coverage-badge]][coverage-page]

Stringify a [**Unist**][unist] [position][] or [location][].

## Installation

[npm][]:

```bash
npm install unist-util-stringify-position
```

**unist-util-stringify-position** is also available as an AMD, CommonJS, and
globals module, [uncompressed and compressed][releases].

## Usage

Dependencies:

```javascript
var stringifyPosition = require('unist-util-stringify-position');
```

Given a position:

```javascript
var result = stringifyPosition({ 'line': 2, 'column': 3 });
```

Yields:

```txt
2:3
```

Given a (partial) location:

```javascript
result = stringifyPosition({
    'start': { 'line': 2 },
    'end': { 'line': 3 }
});
```

Yields:

```txt
2:1-3:1
```

Given a node:

```javascript
result = stringifyPosition({
    'type': 'text',
    'value': '!',
    'position': {
        'start': { 'line': 5, 'column': 11 },
        'end': { 'line': 5, 'column': 12 }
    }
});
```

Yields:

```txt
5:11-5:12
```

## API

### `stringifyPosition(node|location|position)`

Stringify one position, a location (start and end positions), or
a node’s location.

**Parameters**:

*   `node` ([`Node`][node])
    — Node whose `'position'` property to stringify;

*   `location` ([`Location`][location])
    — Location whose `'start'` and `'end'` positions to stringify;

*   `position` ([`Position`][position])
    — Location whose `'line'` and `'column'` to stringify.

**Returns**: `string?` — A range `ls:cs-le:ce` (when given `node` or
`location`) or a point `l:c` (when given `position`), where `l` stands
for line, `c` for column, `s` for `start`, and `e` for
end.  `null` is returned if the given value is neither `node`,
`location`, nor `position`.

## License

[MIT][license] © [Titus Wormer][author]

<!-- Definition -->

[build-badge]: https://img.shields.io/travis/wooorm/unist-util-stringify-position.svg

[build-page]: https://travis-ci.org/wooorm/unist-util-stringify-position

[coverage-badge]: https://img.shields.io/codecov/c/github/wooorm/unist-util-stringify-position.svg

[coverage-page]: https://codecov.io/github/wooorm/unist-util-stringify-position?branch=master

[npm]: https://docs.npmjs.com/cli/install

[releases]: https://github.com/wooorm/unist-util-stringify-position/releases

[license]: LICENSE

[author]: http://wooorm.com

[unist]: https://github.com/wooorm/unist

[node]: https://github.com/wooorm/unist#node

[location]: https://github.com/wooorm/unist#location

[position]: https://github.com/wooorm/unist#position
