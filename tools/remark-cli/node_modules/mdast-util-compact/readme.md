# mdast-util-compact [![Build Status][travis-badge]][travis] [![Coverage Status][codecov-badge]][codecov]

<!--lint disable heading-increment list-item-spacing-->

Make an [MDAST][] tree compact: collapse text nodes (when possible),
and blockquotes (in commonmark mode).

## Installation

[npm][npm-install]:

```bash
npm install mdast-util-compact
```

## Usage

Dependencies:

```javascript
var u = require('unist-builder');
var compact = require('mdast-util-compact');
```

Tree:

```javascript
var tree = u('strong', [
  u('text', 'alpha'),
  u('text', ' '),
  u('text', 'bravo')
]);
```

Compact:

```javascript
compact(tree);
```

`tree` now yields:

```js
{ type: 'strong',
  children: [ { type: 'text', value: 'alpha bravo' } ] }
```

## API

### `compact(tree[, commonmark])`

Visit the tree and collapse nodes.  Combines adjacent text nodes (but
not when they represent entities or escapes).  If `commonmark` is `true`,
collapses `blockquote` nodes.

Handles positional information properly.

###### Returns

The given `tree`.

## License

[MIT][license] © [Titus Wormer][author]

<!-- Definitions -->

[travis-badge]: https://img.shields.io/travis/wooorm/mdast-util-compact.svg

[travis]: https://travis-ci.org/wooorm/mdast-util-compact

[codecov-badge]: https://img.shields.io/codecov/c/github/wooorm/mdast-util-compact.svg

[codecov]: https://codecov.io/github/wooorm/mdast-util-compact

[npm-install]: https://docs.npmjs.com/cli/install

[license]: LICENSE

[author]: http://wooorm.com

[mdast]: https://github.com/wooorm/mdast
