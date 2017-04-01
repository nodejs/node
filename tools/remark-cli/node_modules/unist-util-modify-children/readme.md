# unist-util-modify-children [![Build Status](https://img.shields.io/travis/wooorm/unist-util-modify-children.svg)](https://travis-ci.org/wooorm/unist-util-modify-children) [![Coverage Status](https://img.shields.io/codecov/c/github/wooorm/unist-util-modify-children.svg)](https://codecov.io/github/wooorm/unist-util-modify-children)

[Unist](https://github.com/wooorm/unist) ([mdast](https://github.com/wooorm/mdast/blob/master/doc/mdastnode.7.md),
[retext](https://github.com/wooorm/retext)) utility to modify direct children of
a parent. As in, wrap `fn` so it accepts a parent and invoke `fn` for each of
its children. When `fn` returns a number, goes to that child next.

## Installation

[npm](https://docs.npmjs.com/cli/install):

```bash
npm install unist-util-modify-children
```

**unist-util-modify-children** is also available for [bower](http://bower.io/#install-packages),
[component](https://github.com/componentjs/component), and
[duo](http://duojs.org/#getting-started), and as an AMD, CommonJS, and globals
module, [uncompressed](unist-util-modify-children.js) and [compressed](unist-util-modify-children.min.js).

## Usage

```js
var modifyChildren = require('unist-util-modify-children');

var modifier = modifyChildren(function (child, index, parent) {
    console.log(child, index);

    if (child.value === 'bravo') {
        parent.children.splice(index + 1, 0, { 'type': 'bar', 'value': 'delta' });
        return index + 1;
    }
});

var parent = {
    'type': 'foo',
    'children': [
        { 'type': 'bar', 'value': 'alpha' },
        { 'type': 'bar', 'value': 'bravo' },
        { 'type': 'bar', 'value': 'charlie' }
    ]
};

modifier(parent);
/*
 * { type: 'bar', value: 'alpha' } 0
 * { type: 'bar', value: 'bravo' } 1
 * { type: 'bar', value: 'delta' } 2
 * { type: 'bar', value: 'charlie' } 3
 */

console.log(parent);
/*
 * { type: 'foo',
 *   children:
 *    [ { type: 'bar', value: 'alpha' },
 *      { type: 'bar', value: 'bravo' },
 *      { type: 'bar', value: 'delta' },
 *      { type: 'bar', value: 'charlie' } ] }
 */
```

## API

### modifyChildren(fn)

**Parameters**

*   `fn` ([`Function`](#function-fnchild-index-parent))
    — Function to wrap.

**Return**

[`Function`](#function-pluginparent) — Wrapped `fn`.

#### function fn(child, index, parent)

Modifier for children of `parent`.

**Parameters**

*   `child` ([`Node`](https://github.com/wooorm/unist##unist-nodes))
    — Current iteration;

*   `index` (`number`) — Position of `child` in `parent`;

*   `parent` ([`Node`](https://github.com/wooorm/unist##unist-nodes))
    — Parent node of `child`.

**Returns**

`number` (optional) — Next position to iterate.

#### function plugin(parent)

Function invoking `fn` for each child of `parent`.

**Parameters**

*   `parent` ([`Node`](https://github.com/wooorm/unist##unist-nodes))
    — Node with children.

**Throws**

*   `Error` — When not given a parent node.

## License

[MIT](LICENSE) © [Titus Wormer](http://wooorm.com)
