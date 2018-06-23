# unist-util-position [![Build Status][build-badge]][build-status] [![Coverage Status][coverage-badge]][coverage-status]

[**unist**][unist] utility to get the positional info of nodes.

## Installation

[npm][]:

```bash
npm install unist-util-position
```

## Usage

```js
var remark = require('remark')
var position = require('unist-util-position')

var tree = remark().parse(['# foo', '', '* bar', ''].join('\n'))

position.start(tree) // => {line: 1, column: 1}
position.end(tree) // => {line: 4, column: 1}

position.start() // => {line: null, column: null}
position.end() // => {line: null, column: null}
```

## API

### `position.start([node])`

### `position.end([node])`

Get the start or end points in the positional info of `node`.

###### Parameters

*   `node` ([`Node?`][node]) — Node to check.

###### Returns

[`Point`][point] — Filled with `line` (nullable `uint32 >= 1`),
`column` (nullable `uint32 >= 1`), `offset` (nullable `uint32 >= 0`).

## Contribute

See [`contributing.md` in `syntax-tree/unist`][contributing] for ways to get
started.

This organisation has a [Code of Conduct][coc].  By interacting with this
repository, organisation, or community you agree to abide by its terms.

## License

[MIT][license] © [Titus Wormer][author]

<!-- Definitions -->

[build-badge]: https://img.shields.io/travis/syntax-tree/unist-util-position.svg

[build-status]: https://travis-ci.org/syntax-tree/unist-util-position

[coverage-badge]: https://img.shields.io/codecov/c/github/syntax-tree/unist-util-position.svg

[coverage-status]: https://codecov.io/github/syntax-tree/unist-util-position

[license]: LICENSE

[author]: http://wooorm.com

[npm]: https://docs.npmjs.com/cli/install

[unist]: https://github.com/syntax-tree/unist

[node]: https://github.com/syntax-tree/unist#node

[point]: https://github.com/syntax-tree/unist#point

[contributing]: https://github.com/syntax-tree/unist/blob/master/contributing.md

[coc]: https://github.com/syntax-tree/unist/blob/master/code-of-conduct.md
