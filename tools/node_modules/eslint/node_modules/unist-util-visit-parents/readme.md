# unist-util-visit-parents [![Build Status][build-badge]][build-page] [![Coverage Status][coverage-badge]][coverage-page]

[Unist][] node visitor, with ancestral information.

## Installation

[npm][]:

```bash
npm install unist-util-visit-parents
```

## Usage

```javascript
var remark = require('remark')
var visit = require('unist-util-visit-parents')

var tree = remark.parse('Some _emphasis_, **importance**, and `code`.')

visit(tree, 'strong', visitor)

function visitor(node, ancestors) {
  console.log(ancestors)
}
```

Yields:

```js
[ { type: 'root', children: [ [Object] ] },
  { type: 'paragraph',
    children:
     [ [Object],
       [Object],
       [Object],
       [Object],
       [Object],
       [Object],
       [Object] ] } ]
```

## API

### `visit(tree[, test], visitor[, reverse])`

Visit nodes ([**inclusive descendants**][descendant] of [`tree`][tree]), with
ancestral information.  Optionally filtering nodes.  Optionally in reverse.

###### Parameters

*   `tree` ([`Node`][node]) — Tree to traverse
*   `test` ([`Test`][is], optional) — [`is`][is]-compatible test (such as a
    node type)
*   `visitor` ([Function][visitor]) — Function invoked when a node is found
    that passes `test`
*   `reverse` (`boolean`, default: `false`) — The tree is walked in [preorder][]
    (NLR), visiting the node itself, then its [head][], etc.
    When `reverse` is passed, the tree is stilled walked in preorder, but now
    in NRL (the node itself, then its [tail][], etc.)

#### `next? = visitor(node, ancestors)`

Invoked when a node (matching `test`, if given) is found.

Visitors are free to transform `node`.
They can also transform the [parent][] of node (the last of `ancestors`).
Replacing `node` itself, if `visit.SKIP` is not returned, still causes its
[descendant][]s to be visited.
If adding or removing previous [sibling][]s (or next siblings, in case of
`reverse`) of `node`, `visitor` should return a new [`index`][index] (`number`)
to specify the sibling to traverse after `node` is traversed.
Adding or removing next siblings of `node` (or previous siblings, in case of
reverse) is handled as expected without needing to return a new `index`.
Removing the `children` property of parent still results in them being
traversed.

###### Parameters

*   `node` ([`Node`][node]) — Found node
*   `ancestors` (`Array.<Node>`) — [Ancestor][]s of `node`

###### Returns

*   `visit.EXIT` (`false`) — Stop traversing immediately
*   `visit.CONTINUE` (`true`) — Continue traversing as normal (same behaviour
    as not returning anything)
*   `visit.SKIP` (`'skip'`) — Do not traverse this node’s children; continue
    with the next sibling
*   [`index`][index] (`number`) — Move to the sibling at `index` next (after
    `node` itself is completely traversed).
    Useful if mutating the tree, such as removing the node the visitor is
    currently on, or any of its previous siblings (or next siblings, in case of
    `reverse`)
    Results less than `0` or greater than or equal to `children.length` stop
    traversing the parent

## Related

*   [`unist-util-visit`](https://github.com/syntax-tree/unist-util-visit)
    — Like `visit-parents`, but with one parent
*   [`unist-util-filter`](https://github.com/eush77/unist-util-filter)
    — Create a new tree with all nodes that pass a test
*   [`unist-util-map`](https://github.com/syntax-tree/unist-util-map)
    — Create a new tree with all nodes mapped by a given function
*   [`unist-util-flatmap`](https://gitlab.com/staltz/unist-util-flatmap)
    — Create a new tree by mapping (to an array) with the provided function and
    then flattening
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

[build-badge]: https://img.shields.io/travis/syntax-tree/unist-util-visit-parents.svg

[build-page]: https://travis-ci.org/syntax-tree/unist-util-visit-parents

[coverage-badge]: https://img.shields.io/codecov/c/github/syntax-tree/unist-util-visit-parents.svg

[coverage-page]: https://codecov.io/github/syntax-tree/unist-util-visit-parents?branch=master

[npm]: https://docs.npmjs.com/cli/install

[license]: LICENSE

[author]: http://wooorm.com

[unist]: https://github.com/syntax-tree/unist

[node]: https://github.com/syntax-tree/unist#node

[visitor]: #next--visitornode-ancestors

[contributing]: https://github.com/syntax-tree/unist/blob/master/contributing.md

[coc]: https://github.com/syntax-tree/unist/blob/master/code-of-conduct.md

[is]: https://github.com/syntax-tree/unist-util-is

[preorder]: https://www.geeksforgeeks.org/tree-traversals-inorder-preorder-and-postorder/

[descendant]: https://github.com/syntax-tree/unist#descendant

[head]: https://github.com/syntax-tree/unist#head

[tail]: https://github.com/syntax-tree/unist#tail

[parent]: https://github.com/syntax-tree/unist#parent-1

[sibling]: https://github.com/syntax-tree/unist#sibling

[index]: https://github.com/syntax-tree/unist#index

[ancestor]: https://github.com/syntax-tree/unist#ancestor

[tree]: https://github.com/syntax-tree/unist#tree
