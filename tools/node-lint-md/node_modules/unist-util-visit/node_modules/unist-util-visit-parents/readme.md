# unist-util-visit-parents

[![Build][build-badge]][build]
[![Coverage][coverage-badge]][coverage]
[![Downloads][downloads-badge]][downloads]
[![Size][size-badge]][size]
[![Sponsors][sponsors-badge]][collective]
[![Backers][backers-badge]][collective]
[![Chat][chat-badge]][chat]

[**unist**][unist] utility to visit nodes, with ancestral information.

## Install

This package is [ESM only](https://gist.github.com/sindresorhus/a39789f98801d908bbc7ff3ecc99d99c):
Node 12+ is needed to use it and it must be `import`ed instead of `require`d.

[npm][]:

```sh
npm install unist-util-visit-parents
```

## Use

```js
import {visitParents} from 'unist-util-visit-parents'
import {fromMarkdown} from 'mdast-util-from-markdown'

const tree = fromMarkdown('Some _emphasis_, **importance**, and `code`.')

visitParents(tree, 'strong', (node, ancestors) => {
  console.log(node.type, ancestors)
})
```

Yields:

```js
strong
[
  {
    type: 'root',
    children: [[Object]],
    position: {start: [Object], end: [Object]}
  },
  {
    type: 'paragraph',
    children: [
      [Object],
      [Object],
      [Object],
      [Object],
      [Object],
      [Object],
      [Object]
    ],
    position: {start: [Object], end: [Object]}
  }
]
```

## API

This package exports the following identifiers: `visitParents`, `SKIP`,
`CONTINUE`, and `EXIT`.
There is no default export.

### `visitParents(tree[, test], visitor[, reverse])`

Visit nodes ([*inclusive descendants*][descendant] of [`tree`][tree]), with
ancestral information.
Optionally filtering nodes.
Optionally in reverse.

This algorithm performs [*depth-first*][depth-first]
[*tree traversal*][tree-traversal] in [*preorder*][preorder] (**NLR**), or
if `reverse` is given, in *reverse preorder* (**NRL**).

Walking the tree is an intensive task.
Make use of the return values of the visitor when possible.
Instead of walking a tree multiple times with different `test`s, walk it once
without a test, and use [`unist-util-is`][is] to check if a node matches a test,
and then perform different operations.

###### Parameters

*   `tree` ([`Node`][node]) — [Tree][] to traverse
*   `test` ([`Test`][is], optional) — [`is`][is]-compatible test (such as a
    [type][])
*   `visitor` ([Function][visitor]) — Function invoked when a node is found
    that passes `test`
*   `reverse` (`boolean`, default: `false`) — The tree is traversed in
    [preorder][] (NLR), visiting the node itself, then its [head][], etc.
    When `reverse` is passed, the tree is traversed in reverse preorder (NRL):
    the node itself is visited, then its [tail][], etc.

#### `next? = visitor(node, ancestors)`

Invoked when a node (matching `test`, if given) is found.

Visitors are free to transform `node`.
They can also transform the [parent][] of node (the last of `ancestors`).
Replacing `node` itself, if `SKIP` is not returned, still causes its
[descendant][]s to be visited.
If adding or removing previous [sibling][]s (or next siblings, in case of
`reverse`) of `node`, `visitor` should return a new [`index`][index] (`number`)
to specify the sibling to traverse after `node` is traversed.
Adding or removing next siblings of `node` (or previous siblings, in case of
reverse) is handled as expected without needing to return a new `index`.
Removing the `children` property of an ancestor still results in them being
traversed.

###### Parameters

*   `node` ([`Node`][node]) — Found node
*   `ancestors` (`Array.<Node>`) — [Ancestor][]s of `node`

##### Returns

The return value can have the following forms:

*   [`index`][index] (`number`) — Treated as a tuple of `[CONTINUE, index]`
*   `action` (`*`) — Treated as a tuple of `[action]`
*   `tuple` (`Array.<*>`) — List with one or two values, the first an `action`,
    the second and `index`.
    Note that passing a tuple only makes sense if the `action` is `SKIP`.
    If the `action` is `EXIT`, that action can be returned.
    If the `action` is `CONTINUE`, `index` can be returned.

###### `action`

An action can have the following values:

*   `EXIT` (`false`) — Stop traversing immediately
*   `CONTINUE` (`true`) — Continue traversing as normal (same behaviour
    as not returning anything)
*   `SKIP` (`'skip'`) — Do not traverse this node’s children; continue
    with the specified index

###### `index`

[`index`][index] (`number`) — Move to the sibling at `index` next (after `node`
itself is completely traversed).
Useful if mutating the tree, such as removing the node the visitor is currently
on, or any of its previous siblings (or next siblings, in case of `reverse`)
Results less than `0` or greater than or equal to `children.length` stop
traversing the parent

## Related

*   [`unist-util-visit`](https://github.com/syntax-tree/unist-util-visit)
    — Like `visit-parents`, but with one parent
*   [`unist-util-filter`](https://github.com/syntax-tree/unist-util-filter)
    — Create a new tree with all nodes that pass a test
*   [`unist-util-map`](https://github.com/syntax-tree/unist-util-map)
    — Create a new tree with all nodes mapped by a given function
*   [`unist-util-flatmap`](https://gitlab.com/staltz/unist-util-flatmap)
    — Create a new tree by mapping (to an array) with the given function
*   [`unist-util-remove`](https://github.com/syntax-tree/unist-util-remove)
    — Remove nodes from a tree that pass a test
*   [`unist-util-select`](https://github.com/syntax-tree/unist-util-select)
    — Select nodes with CSS-like selectors

## Contribute

See [`contributing.md` in `syntax-tree/.github`][contributing] for ways to get
started.
See [`support.md`][support] for ways to get help.

This project has a [code of conduct][coc].
By interacting with this repository, organization, or community you agree to
abide by its terms.

## License

[MIT][license] © [Titus Wormer][author]

<!-- Definition -->

[build-badge]: https://github.com/syntax-tree/unist-util-visit-parents/workflows/main/badge.svg

[build]: https://github.com/syntax-tree/unist-util-visit-parents/actions

[coverage-badge]: https://img.shields.io/codecov/c/github/syntax-tree/unist-util-visit-parents.svg

[coverage]: https://codecov.io/github/syntax-tree/unist-util-visit-parents

[downloads-badge]: https://img.shields.io/npm/dm/unist-util-visit-parents.svg

[downloads]: https://www.npmjs.com/package/unist-util-visit-parents

[size-badge]: https://img.shields.io/bundlephobia/minzip/unist-util-visit-parents.svg

[size]: https://bundlephobia.com/result?p=unist-util-visit-parents

[sponsors-badge]: https://opencollective.com/unified/sponsors/badge.svg

[backers-badge]: https://opencollective.com/unified/backers/badge.svg

[collective]: https://opencollective.com/unified

[chat-badge]: https://img.shields.io/badge/chat-discussions-success.svg

[chat]: https://github.com/syntax-tree/unist/discussions

[npm]: https://docs.npmjs.com/cli/install

[license]: license

[author]: https://wooorm.com

[unist]: https://github.com/syntax-tree/unist

[node]: https://github.com/syntax-tree/unist#node

[visitor]: #next--visitornode-ancestors

[contributing]: https://github.com/syntax-tree/.github/blob/HEAD/contributing.md

[support]: https://github.com/syntax-tree/.github/blob/HEAD/support.md

[coc]: https://github.com/syntax-tree/.github/blob/HEAD/code-of-conduct.md

[is]: https://github.com/syntax-tree/unist-util-is

[depth-first]: https://github.com/syntax-tree/unist#depth-first-traversal

[tree-traversal]: https://github.com/syntax-tree/unist#tree-traversal

[preorder]: https://github.com/syntax-tree/unist#preorder

[descendant]: https://github.com/syntax-tree/unist#descendant

[head]: https://github.com/syntax-tree/unist#head

[tail]: https://github.com/syntax-tree/unist#tail

[parent]: https://github.com/syntax-tree/unist#parent-1

[sibling]: https://github.com/syntax-tree/unist#sibling

[index]: https://github.com/syntax-tree/unist#index

[ancestor]: https://github.com/syntax-tree/unist#ancestor

[tree]: https://github.com/syntax-tree/unist#tree

[type]: https://github.com/syntax-tree/unist#type
