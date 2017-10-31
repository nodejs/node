# mdast-util-to-string [![Build Status][build-badge]][build-status] [![Coverage Status][coverage-badge]][coverage-status] [![Chat][chat-badge]][chat]

Get the plain text content of an [MDAST][] node.

## Installation

[npm][]:

```bash
npm install mdast-util-to-string
```

## Usage

```js
var unified = require('unified');
var parse = require('remark-parse');
var toString = require('mdast-util-to-string');

var tree = unified()
  .use(parse)
  .parse('Some _emphasis_, **importance**, and `code`.');

console.log(toString(tree)); //=> 'Some emphasis, importance, and code.'
```

## API

### `toString(node)`

Get the text content of a node.

The algorithm checks `value` of `node`, then `alt`, and finally `title`.
If no value is found, the algorithm checks the children of `node` and
joins them (without spaces or newlines).

> This is not a markdown to plain-text library.
> Use [`strip-markdown`][strip-markdown] for that.

## Related

*   [`nlcst-to-string`](https://github.com/syntax-tree/nlcst-to-string)
    — Get text content in NLCST
*   [`hast-util-to-string`](https://github.com/wooorm/rehype-minify/tree/master/packages/hast-util-to-string)
    — Get text content in HAST
*   [`hast-util-from-string`](https://github.com/wooorm/rehype-minify/tree/master/packages/hast-util-from-string)
    — Set text content in HAST

## License

[MIT][license] © [Titus Wormer][author]

<!-- Definitions -->

[build-badge]: https://img.shields.io/travis/syntax-tree/mdast-util-to-string.svg

[build-status]: https://travis-ci.org/syntax-tree/mdast-util-to-string

[coverage-badge]: https://img.shields.io/codecov/c/github/syntax-tree/mdast-util-to-string.svg

[coverage-status]: https://codecov.io/github/syntax-tree/mdast-util-to-string

[chat-badge]: https://img.shields.io/gitter/room/wooorm/remark.svg

[chat]: https://gitter.im/wooorm/remark

[license]: LICENSE

[author]: http://wooorm.com

[npm]: https://docs.npmjs.com/cli/install

[mdast]: https://github.com/syntax-tree/mdast

[strip-markdown]: https://github.com/wooorm/strip-markdown
