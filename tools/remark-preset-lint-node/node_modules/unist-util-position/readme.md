# unist-util-position [![Build Status][build-badge]][build-status] [![Coverage Status][coverage-badge]][coverage-status]

[**unist**][unist] utility to get the position of nodes.

## Installation

[npm][]:

```bash
npm install unist-util-position
```

## Usage

```js
var remark = require('remark');
var position = require('unist-util-position');

var tree = remark().parse([
  '# foo',
  '',
  '* bar',
  ''
].join('\n'));

position.start(tree); //=> {line: 1, column: 1}
position.end(tree); //=> {line: 4, column: 1}

position.start(); //=> {line: null, column: null}
position.end(); //=> {line: null, column: null}
```

## API

### `position.start([node])`

### `position.end([node])`

Get the start or end position, respectively, of `node`.

###### Parameters

*   `node` ([`Node?`][node]) — Node to check.

###### Returns

[`Position`][position] — Filled with `line` (nullable `uint32 >= 1`),
`column` (nullable `uint32 >= 1`), `offset` (nullable `uint32 >= 0`).

## License

[MIT][license] © [Titus Wormer][author]

<!-- Definitions -->

[build-badge]: https://img.shields.io/travis/wooorm/unist-util-position.svg

[build-status]: https://travis-ci.org/wooorm/unist-util-position

[coverage-badge]: https://img.shields.io/codecov/c/github/wooorm/unist-util-position.svg

[coverage-status]: https://codecov.io/github/wooorm/unist-util-position

[license]: LICENSE

[author]: http://wooorm.com

[npm]: https://docs.npmjs.com/cli/install

[unist]: https://github.com/wooorm/unist

[node]: https://github.com/wooorm/unist#node

[position]: https://github.com/wooorm/unist#position
