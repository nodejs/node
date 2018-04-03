# unist-util-remove-position [![Build Status][travis-badge]][travis] [![Coverage Status][codecov-badge]][codecov]

Remove [`position`][position]s from a [Unist][] tree.

## Installation

[npm][]:

```bash
npm install unist-util-remove-position
```

## Usage

```javascript
var remark = require('remark');
var removePosition = require('unist-util-remove-position');

var tree = remark().parse('Some _emphasis_, **importance**, and `code`.');

console.dir(removePosition(tree, true), {depth: null});
```

Yields:

```js
{ type: 'root',
  children:
   [ { type: 'paragraph',
       children:
        [ { type: 'text', value: 'Some ' },
          { type: 'emphasis',
            children: [ { type: 'text', value: 'emphasis' } ] },
          { type: 'text', value: ', ' },
          { type: 'strong',
            children: [ { type: 'text', value: 'importance' } ] },
          { type: 'text', value: ', and ' },
          { type: 'inlineCode', value: 'code' },
          { type: 'text', value: '.' } ] } ] }
```

## API

### `removePosition(node[, force])`

Remove [`position`][position]s from [`node`][node].  If `force` is given,
uses `delete`, otherwise, sets `position`s to `undefined`.

###### Returns

The given `node`.

## License

[MIT][license] © [Titus Wormer][author]

<!-- Definitions -->

[travis-badge]: https://img.shields.io/travis/syntax-tree/unist-util-remove-position.svg

[travis]: https://travis-ci.org/syntax-tree/unist-util-remove-position

[codecov-badge]: https://img.shields.io/codecov/c/github/syntax-tree/unist-util-remove-position.svg

[codecov]: https://codecov.io/github/syntax-tree/unist-util-remove-position

[npm]: https://docs.npmjs.com/cli/install

[license]: LICENSE

[author]: http://wooorm.com

[unist]: https://github.com/syntax-tree/unist

[position]: https://github.com/syntax-tree/unist#position

[node]: https://github.com/syntax-tree/unist#node
