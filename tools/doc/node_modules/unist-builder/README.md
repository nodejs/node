[![npm](https://nodei.co/npm/unist-builder.png)](https://npmjs.com/package/unist-builder)

# unist-builder

[![Build Status][travis-badge]][travis] [![Dependency Status][david-badge]][david]

Helper for creating [unist] trees with [hyperscript]-like syntax.

[unist]: https://github.com/wooorm/unist
[hyperscript]: https://github.com/dominictarr/hyperscript

[travis]: https://travis-ci.org/eush77/unist-builder
[travis-badge]: https://travis-ci.org/eush77/unist-builder.svg?branch=master
[david]: https://david-dm.org/eush77/unist-builder
[david-badge]: https://david-dm.org/eush77/unist-builder.png

## Example

```js
var u = require('unist-builder');

u('root', [
  u('subtree', { id: 1 }),
  u('subtree', { id: 2 }, [
    u('node', [
      u('leaf', 'leaf-1'),
      u('leaf', 'leaf-2')
    ]),
    u('leaf', { id: 3 }, 'leaf-3')
  ])
])
```

results in the following tree:

```json
{
  "type": "root",
  "children": [
    {
      "type": "subtree",
      "id": 1
    },
    {
      "type": "subtree",
      "id": 2,
      "children": [
        {
          "type": "node",
          "children": [
            {
              "type": "leaf",
              "value": "leaf-1"
            },
            {
              "type": "leaf",
              "value": "leaf-2"
            }
          ]
        },
        {
          "type": "leaf",
          "id": 3,
          "value": "leaf-3"
        }
      ]
    }
  ]
}
```

## API

#### `u(type, [props], [value])`

`type`: `String`<br>
`props`: `Object`<br>
`value`: `String`<br>

Creates a node from `props` and optional `value`.

#### `u(type, [props], children)`

`type`: `String`<br>
`props`: `Object`<br>
`children`: `Array`<br>

Creates a node from `props` and given child nodes.

## Install

```
npm install unist-builder
```

## License

MIT
