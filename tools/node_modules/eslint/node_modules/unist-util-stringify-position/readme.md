# unist-util-stringify-position [![Build Status][build-badge]][build-page] [![Coverage Status][coverage-badge]][coverage-page]

Stringify a [**Unist**][unist] [position][] or [location][].

## Installation

[npm][]:

```bash
npm install unist-util-stringify-position
```

## Usage

```javascript
var stringify = require('unist-util-stringify-position');

stringify({line: 2, column: 3 }); //=> '2:3'

stringify({
  start: {line: 2},
  end: {line: 3}
}); //=> '2:1-3:1'

stringify({
  type: 'text',
  value: '!',
  position: {
    start: {line: 5, column: 11},
    end: {line: 5, column: 12}
  }
}); //=> '5:11-5:12'
```

## API

### `stringifyPosition(node|location|position)`

Stringify one position, a location (start and end positions), or
a node’s location.

###### Parameters

*   `node` ([`Node`][node])
    — Node whose `'position'` property to stringify
*   `location` ([`Location`][location])
    — Location whose `'start'` and `'end'` positions to stringify
*   `position` ([`Position`][position])
    — Location whose `'line'` and `'column'` to stringify

###### Returns

`string?` — A range `ls:cs-le:ce` (when given `node` or
`location`) or a point `l:c` (when given `position`), where `l` stands
for line, `c` for column, `s` for `start`, and `e` for
end.  `null` is returned if the given value is neither `node`,
`location`, nor `position`.

## License

[MIT][license] © [Titus Wormer][author]

<!-- Definition -->

[build-badge]: https://img.shields.io/travis/syntax-tree/unist-util-stringify-position.svg

[build-page]: https://travis-ci.org/syntax-tree/unist-util-stringify-position

[coverage-badge]: https://img.shields.io/codecov/c/github/syntax-tree/unist-util-stringify-position.svg

[coverage-page]: https://codecov.io/github/syntax-tree/unist-util-stringify-position?branch=master

[npm]: https://docs.npmjs.com/cli/install

[license]: LICENSE

[author]: http://wooorm.com

[unist]: https://github.com/syntax-tree/unist

[node]: https://github.com/syntax-tree/unist#node

[location]: https://github.com/syntax-tree/unist#location

[position]: https://github.com/syntax-tree/unist#position
