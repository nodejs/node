# unist-util-remove-position [![Build Status][travis-badge]][travis] [![Coverage Status][codecov-badge]][codecov]

<!--lint disable heading-increment list-item-spacing-->

Remove [`position`][position]s from a [unist][] tree.

## Installation

[npm][npm-install]:

```bash
npm install unist-util-remove-position
```

## Usage

Dependencies:

```javascript
var remark = require('remark');
var removePosition = require('unist-util-remove-position');
```

Tree:

```javascript
var tree = remark().parse('Some **strong**, _emphasis_, and `code`.');
```

Yields:

```js
{ type: 'root',
  children: 
   [ { type: 'paragraph',
       children: 
        [ { type: 'text',
            value: 'Some ',
            position: 
             Position {
               start: { line: 1, column: 1, offset: 0 },
               end: { line: 1, column: 6, offset: 5 },
               indent: [] } },
          { type: 'strong',
            children: 
             [ { type: 'text',
                 value: 'strong',
                 position: 
                  Position {
                    start: { line: 1, column: 8, offset: 7 },
                    end: { line: 1, column: 14, offset: 13 },
                    indent: [] } } ],
            position: 
             Position {
               start: { line: 1, column: 6, offset: 5 },
               end: { line: 1, column: 16, offset: 15 },
               indent: [] } },
          { type: 'text',
            value: ', ',
            position: 
             Position {
               start: { line: 1, column: 16, offset: 15 },
               end: { line: 1, column: 18, offset: 17 },
               indent: [] } },
          { type: 'emphasis',
            children: 
             [ { type: 'text',
                 value: 'emphasis',
                 position: 
                  Position {
                    start: { line: 1, column: 19, offset: 18 },
                    end: { line: 1, column: 27, offset: 26 },
                    indent: [] } } ],
            position: 
             Position {
               start: { line: 1, column: 18, offset: 17 },
               end: { line: 1, column: 28, offset: 27 },
               indent: [] } },
          { type: 'text',
            value: ', and ',
            position: 
             Position {
               start: { line: 1, column: 28, offset: 27 },
               end: { line: 1, column: 34, offset: 33 },
               indent: [] } },
          { type: 'inlineCode',
            value: 'code',
            position: 
             Position {
               start: { line: 1, column: 34, offset: 33 },
               end: { line: 1, column: 40, offset: 39 },
               indent: [] } },
          { type: 'text',
            value: '.',
            position: 
             Position {
               start: { line: 1, column: 40, offset: 39 },
               end: { line: 1, column: 41, offset: 40 },
               indent: [] } } ],
       position: 
        Position {
          start: { line: 1, column: 1, offset: 0 },
          end: { line: 1, column: 41, offset: 40 },
          indent: [] } } ],
  position: 
   { start: { line: 1, column: 1, offset: 0 },
     end: { line: 1, column: 41, offset: 40 } } }
```

Removing position from tree:

```javascript
tree = removePosition(tree, true);
```

Yields:

```js
{ type: 'root',
  children: 
   [ { type: 'paragraph',
       children: 
        [ { type: 'text', value: 'Some ' },
          { type: 'strong',
            children: [ { type: 'text', value: 'strong' } ] },
          { type: 'text', value: ', ' },
          { type: 'emphasis',
            children: [ { type: 'text', value: 'emphasis' } ] },
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

[travis-badge]: https://img.shields.io/travis/wooorm/unist-util-remove-position.svg

[travis]: https://travis-ci.org/wooorm/unist-util-remove-position

[codecov-badge]: https://img.shields.io/codecov/c/github/wooorm/unist-util-remove-position.svg

[codecov]: https://codecov.io/github/wooorm/unist-util-remove-position

[npm-install]: https://docs.npmjs.com/cli/install

[license]: LICENSE

[author]: http://wooorm.com

[unist]: https://github.com/wooorm/unist

[position]: https://github.com/wooorm/unist#position

[node]: https://github.com/wooorm/unist#node
