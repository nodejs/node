# unist-util-modify-children [![Build Status][travis-badge]][travis] [![Coverage Status][codecov-badge]][codecov]

Modify direct children of a parent.

## Installation

[npm][]:

```bash
npm install unist-util-modify-children
```

## Usage

```javascript
var remark = require('remark');
var modifyChildren = require('unist-util-modify-children');

var doc = remark().use(plugin).process('This _and_ that');

console.log(String(doc));

function plugin() {
  return transformer;
  function transformer(tree) {
    modifyChildren(modifier)(tree.children[0]);
  }
}

function modifier(node, index, parent) {
  if (node.type === 'emphasis') {
    parent.children.splice(index, 1, {type: 'strong', children: node.children});
    return index + 1;
  }
}
```

Yields:

```js
This **and** that
```

## API

### `modify = modifyChildren(modifier)`

Wrap [`modifier`][modifier] to be invoked for each child in the node given to
[`modify`][modify].

#### `next? = modifier(child, index, parent)`

Invoked if [`modify`][modify] is called on a parent node for each `child`
in `parent`.

###### Returns

`number` (optional) — Next position to iterate.

#### `function modify(parent)`

Invoke the bound [`modifier`][modifier] for each child in `parent`
([`Node`][node]).

## License

[MIT][license] © [Titus Wormer][author]

<!-- Definitions -->

[travis-badge]: https://img.shields.io/travis/syntax-tree/unist-util-modify-children.svg

[travis]: https://travis-ci.org/syntax-tree/unist-util-modify-children

[codecov-badge]: https://img.shields.io/codecov/c/github/syntax-tree/unist-util-modify-children.svg

[codecov]: https://codecov.io/github/syntax-tree/unist-util-modify-children

[npm]: https://docs.npmjs.com/cli/install

[license]: LICENSE

[author]: http://wooorm.com

[node]: https://github.com/syntax-tree/unist#node

[modifier]: #next--modifierchild-index-parent

[modify]: #function-modifyparent
