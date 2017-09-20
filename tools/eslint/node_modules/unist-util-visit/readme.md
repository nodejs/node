# unist-util-visit [![Build Status][build-badge]][build-page] [![Coverage Status][coverage-badge]][coverage-page]

[Unist][] node visitor.  Useful when working with [**remark**][remark],
[**retext**][retext], or [**rehype**][rehype].

## Installation

[npm][]:

```bash
npm install unist-util-visit
```

## Usage

```javascript
var remark = require('remark');
var visit = require('unist-util-visit');

var tree = remark.parse('Some _emphasis_, **importance**, and `code`.');

visit(tree, 'text', visitor);

function visitor(node) {
  console.log(node);
}
```

Yields:

```js
{ type: 'text', value: 'Some ' }
{ type: 'text', value: 'emphasis' }
{ type: 'text', value: ', ' }
{ type: 'text', value: 'importance' }
{ type: 'text', value: ', and ' }
{ type: 'text', value: '.' }
```

## API

### `visit(node[, type], visitor[, reverse])`

Visit nodes.  Optionally by node type.  Optionally in reverse.

###### Parameters

*   `node` ([`Node`][node])
    — Node to search
*   `type` (`string`, optional)
    — Node type
*   `visitor` ([Function][visitor])
    — Visitor invoked when a node is found
*   `reverse` (`boolean`, default: `false`)
    — When falsey, checking starts at the first child and continues
    through to later children.  When truthy, this is reversed.
    This **does not** mean checking starts at the deepest node and
    continues on to the highest node

#### `stop? = visitor(node, index, parent)`

Invoked when a node (when `type` is given, matching `type`) is found.

###### Parameters

*   `node` (`Node`) — Found node
*   `index` (`number?`) — Position of `node` in `parent`
*   `parent` (`Node?`) — Parent of `node`

###### Returns

`boolean?` - When `false`, visiting is immediately stopped.

## Related

*   [`unist-util-visit-parents`](https://github.com/syntax-tree/unist-util-visit-parents)
    — Like `visit`, but with a stack of parents
*   [`unist-util-filter`](https://github.com/eush77/unist-util-filter)
    — Create a new tree with all nodes that pass a test
*   [`unist-util-map`](https://github.com/syntax-tree/unist-util-map)
    — Create a new tree with all nodes mapped by a given function
*   [`unist-util-remove`](https://github.com/eush77/unist-util-remove)
    — Remove nodes from a tree that pass a test
*   [`unist-util-select`](https://github.com/eush77/unist-util-select)
    — Select nodes with CSS-like selectors

## License

[MIT][license] © [Titus Wormer][author]

<!-- Definition -->

[build-badge]: https://img.shields.io/travis/syntax-tree/unist-util-visit.svg

[build-page]: https://travis-ci.org/syntax-tree/unist-util-visit

[coverage-badge]: https://img.shields.io/codecov/c/github/syntax-tree/unist-util-visit.svg

[coverage-page]: https://codecov.io/github/syntax-tree/unist-util-visit?branch=master

[npm]: https://docs.npmjs.com/cli/install

[license]: LICENSE

[author]: http://wooorm.com

[unist]: https://github.com/syntax-tree/unist

[retext]: https://github.com/wooorm/retext

[remark]: https://github.com/wooorm/remark

[rehype]: https://github.com/wooorm/rehype

[node]: https://github.com/syntax-tree/unist#node

[visitor]: #stop--visitornode-index-parent
