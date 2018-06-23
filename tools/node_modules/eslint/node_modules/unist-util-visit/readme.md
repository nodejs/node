# unist-util-visit [![Build Status][build-badge]][build-page] [![Coverage Status][coverage-badge]][coverage-page]

[unist][] node visitor.  Useful when working with [**remark**][remark],
[**retext**][retext], or [**rehype**][rehype].

## Installation

[npm][]:

```bash
npm install unist-util-visit
```

## Usage

```javascript
var remark = require('remark')
var visit = require('unist-util-visit')

var tree = remark().parse('Some _emphasis_, **importance**, and `code`.')

visit(tree, 'text', visitor)

function visitor(node) {
  console.log(node)
}
```

Yields:

```js
{type: 'text', value: 'Some '}
{type: 'text', value: 'emphasis'}
{type: 'text', value: ', '}
{type: 'text', value: 'importance'}
{type: 'text', value: ', and '}
{type: 'text', value: '.'}
```

## API

### `visit(tree[, test], visitor[, reverse])`

Visit nodes ([**inclusive descendants**][descendant] of `tree`).  Optionally
filtering nodes.  Optionally in reverse.

###### Parameters

*   `tree` ([`Node`][node])
    — Tree to traverse
*   `test` ([`Test`][is], optional)
    — [`is`][is]-compatible test (such as a node type)
*   `visitor` ([Function][visitor])
    — Function invoked when a node is found that passes `test`
*   `reverse` (`boolean`, default: `false`)
    — When falsey, checking starts at the first child and continues
    through to later children.  When truthy, this is reversed.
    This **does not** mean checking starts at the deepest node and
    continues on to the highest node

#### `next? = visitor(node, index, parent)`

Invoked when a node (matching `test` if given) is found.

You can transform visited nodes.  You can transform `parent`, but if adding or
removing [**children**][child] before `index`, you should return a new `index`
(`number`) from `visitor` to specify the next sibling to visit.  Replacing
`node` itself still causes its descendants to be visited.  Adding or removing
nodes after `index` is handled as expected without needing to return a new
`index`.  Removing the `children` property on `parent` still results in them
being traversed.

###### Parameters

*   `node` (`Node`) — Found node
*   `index` (`number?`) — Position of `node` in `parent`
*   `parent` (`Node?`) — Parent of `node`

###### Returns

*   `visit.EXIT` (`false`)
    — Stop traversing immediately
*   `visit.CONTINUE` (`true`)
    — Continue traversing as normal (same behaviour as not returning anything)
*   `visit.SKIP` (`'skip'`)
    — Do not enter this node (traversing into its children), but do continue
    with the next sibling
*   `index` (`number`)
    — Move to the sibling at position `index` next (after `node` itself is
    traversed).  Useful if you’re mutating the tree (such as removing the node
    you’re currently on, or any of its preceding siblings).  Results less than
    `0` or greater than or equal to `children.length` stop iteration of the
    parent

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

## Contribute

See [`contributing.md` in `syntax-tree/unist`][contributing] for ways to get
started.

This organisation has a [Code of Conduct][coc].  By interacting with this
repository, organisation, or community you agree to abide by its terms.

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

[retext]: https://github.com/retextjs/retext

[remark]: https://github.com/remarkjs/remark

[rehype]: https://github.com/rehypejs/rehype

[node]: https://github.com/syntax-tree/unist#node

[descendant]: https://github.com/syntax-tree/unist#descendant

[child]: https://github.com/syntax-tree/unist#child

[is]: https://github.com/syntax-tree/unist-util-is#istest-node-index-parent-context

[visitor]: #next--visitornode-index-parent

[contributing]: https://github.com/syntax-tree/unist/blob/master/contributing.md

[coc]: https://github.com/syntax-tree/unist/blob/master/code-of-conduct.md
