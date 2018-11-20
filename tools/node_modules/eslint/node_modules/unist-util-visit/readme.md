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

This function works exactly the same as [`unist-util-visit-parents`][vp],
but `visitor` has a different signature.

#### `next? = visitor(node, index, parent)`

Instead of being passed an array of ancestors, `visitor` is invoked with the
node’s [`index`][index] and its [`parent`][parent].

Otherwise the same as [`unist-util-visit-parents`][vp].

## Related

*   [`unist-util-visit-parents`][vp]
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

[contributing]: https://github.com/syntax-tree/unist/blob/master/contributing.md

[coc]: https://github.com/syntax-tree/unist/blob/master/code-of-conduct.md

[vp]: https://github.com/syntax-tree/unist-util-visit-parents

[index]: https://github.com/syntax-tree/unist#index

[parent]: https://github.com/syntax-tree/unist#parent-1
