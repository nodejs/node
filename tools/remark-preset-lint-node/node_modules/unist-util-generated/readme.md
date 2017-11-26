# unist-util-generated [![Build Status][build-badge]][build-page] [![Coverage Status][coverage-badge]][coverage-page]

Check if a [**Unist**][unist] [node][] is [**generated**][spec].

## Installation

[npm][]:

```bash
npm install unist-util-generated
```

## Usage

```javascript
var generated = require('unist-util-generated');

generated({}); //=> true

generated({
  position: {start: {}, end: {}}
}); //=> true

generated({
  position: {
    start: {line: 1, column: 1},
    end: {line: 1, column: 2}
  }
}); //=> false
```

## API

### `generated(node)`

Detect if [`node`][node] is [**generated**][spec].

###### Parameters

*   `node` ([`Node`][node]) — Node to check.

###### Returns

`boolean` — Whether `node` is generated.

## Related

*   [`unist-util-position`](https://github.com/syntax-tree/unist-util-position)
    — Get the position of nodes
*   [`unist-util-remove-position`](https://github.com/syntax-tree/unist-util-remove-position)
    — Remove `position`s from a tree
*   [`unist-util-stringify-position`](https://github.com/syntax-tree/unist-util-stringify-position)
    — Stringify a node, location, or position

## License

[MIT][license] © [Titus Wormer][author]

<!-- Definition -->

[build-badge]: https://img.shields.io/travis/syntax-tree/unist-util-generated.svg

[build-page]: https://travis-ci.org/syntax-tree/unist-util-generated

[coverage-badge]: https://img.shields.io/codecov/c/github/syntax-tree/unist-util-generated.svg

[coverage-page]: https://codecov.io/github/syntax-tree/unist-util-generated?branch=master

[npm]: https://docs.npmjs.com/cli/install

[license]: LICENSE

[author]: http://wooorm.com

[unist]: https://github.com/syntax-tree/unist

[node]: https://github.com/syntax-tree/unist#node

[spec]: https://github.com/syntax-tree/unist#location
