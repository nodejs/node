# unist-util-is [![Build Status][travis-badge]][travis] [![Coverage Status][codecov-badge]][codecov]

[**Unist**][unist] utility to check if a node passes a test.

## Installation

[npm][]:

```bash
npm install unist-util-is
```

## Usage

```js
var is = require('unist-util-is');

var node = {type: 'strong'};
var parent = {type: 'paragraph', children: [node]};

function test(node, n) { return n === 5 }

is(); // false
is(null, {children: []}); // false
is(null, node); // true
is('strong', node); // true
is('emphasis', node); // false

is(node, node) // true
is({type: 'paragraph'}, parent) // true
is({type: 'strong'}, parent) // false

is(test, node); // false
is(test, node, 4, parent); // false
is(test, node, 5, parent); // true
```

## API

### `is(test, node[, index, parent[, context]])`

###### Parameters

*   `test` ([`Function`][test], `string`, `Object`, or `Array.<Test>`, optional)
    —  When not given, checks if `node` is a [`Node`][node].
    When `string`, works like passing `function (node) {return
    node.type === test}`.
    When `array`, checks any one of the subtests pass.
    When `object`, checks that all keys in `test` are in `node`,
    and that they have (strictly) equal values
*   `node` ([`Node`][node]) — Node to check.  `false` is returned
*   `index` (`number`, optional) — Position of `node` in `parent`
*   `parent` (`Node`, optional) — Parent of `node`
*   `context` (`*`, optional) — Context object to invoke `test` with

###### Returns

`boolean` — Whether `test` passed _and_ `node` is a [`Node`][node] (object
with `type` set to non-empty `string`).

#### `function test(node[, index, parent])`

###### Parameters

*   `node` (`Node`) — Node to test
*   `index` (`number?`) — Position of `node` in `parent`
*   `parent` (`Node?`) — Parent of `node`

###### Context

`*` — The to `is` given `context`.

###### Returns

`boolean?` — Whether `node` matches.

## Related

*   [`unist-util-find-after`](https://github.com/syntax-tree/unist-util-find-after)
    — Find a node after another node
*   [`unist-util-find-before`](https://github.com/syntax-tree/unist-util-find-before)
    — Find a node before another node
*   [`unist-util-find-all-after`](https://github.com/syntax-tree/unist-util-find-all-after)
    — Find all nodes after another node
*   [`unist-util-find-all-before`](https://github.com/syntax-tree/unist-util-find-all-before)
    — Find all nodes before another node
*   [`unist-util-find-all-between`](https://github.com/mrzmmr/unist-util-find-all-between)
    — Find all nodes between two nodes
*   [`unist-util-find`](https://github.com/blahah/unist-util-find)
    — Find nodes matching a predicate
*   [`unist-util-filter`](https://github.com/eush77/unist-util-filter)
    — Create a new tree with nodes that pass a check
*   [`unist-util-remove`](https://github.com/eush77/unist-util-remove)
    — Remove nodes from tree

## License

[MIT][license] © [Titus Wormer][author]

<!-- Definitions -->

[travis-badge]: https://img.shields.io/travis/syntax-tree/unist-util-is.svg

[travis]: https://travis-ci.org/syntax-tree/unist-util-is

[codecov-badge]: https://img.shields.io/codecov/c/github/syntax-tree/unist-util-is.svg

[codecov]: https://codecov.io/github/syntax-tree/unist-util-is

[npm]: https://docs.npmjs.com/cli/install

[license]: LICENSE

[author]: http://wooorm.com

[unist]: https://github.com/syntax-tree/unist

[node]: https://github.com/syntax-tree/unist#node

[test]: #function-testnode-index-parent
