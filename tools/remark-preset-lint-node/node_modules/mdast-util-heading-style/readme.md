# mdast-util-heading-style [![Build Status][build-badge]][build-status] [![Coverage Status][coverage-badge]][coverage-status] [![Chat][chat-badge]][chat]

Get the style of an [**MDAST**][mdast] heading.

## Installation

[npm][]:

```bash
npm install mdast-util-heading-style
```

## Usage

```js
var style = require('mdast-util-heading-style');
var remark = require('remark')();

style(remark.parse('# ATX').children[0]); // 'atx'
style(remark.parse('# ATX #\n').children[0]); // 'atx-closed'
style(remark.parse('ATX\n===').children[0]); // 'setext'

style(remark.parse('### ATX').children[0]); // null
style(remark.parse('### ATX').children[0], 'setext'); // 'setext'
```

## API

### `style(node[, relative])`

Get the heading style of a node.

###### Parameters

*   `node` ([`Node`][node]) — Node to parse
*   `relative` (`string`, optional) — Style to use for ambiguous headings
    (atx-headings with a level of three or more could also be setext)

###### Returns

`string` (`'atx'`, `'atx-closed'`, or `'setext'`) — When an ambiguous
heading is found, either `relative` or `null` is returned.

## License

[MIT][license] © [Titus Wormer][author]

<!-- Definitions -->

[build-badge]: https://img.shields.io/travis/syntax-tree/mdast-util-heading-style.svg

[build-status]: https://travis-ci.org/syntax-tree/mdast-util-heading-style

[coverage-badge]: https://img.shields.io/codecov/c/github/syntax-tree/mdast-util-heading-style.svg

[coverage-status]: https://codecov.io/github/syntax-tree/mdast-util-heading-style

[chat-badge]: https://img.shields.io/gitter/room/wooorm/remark.svg

[chat]: https://gitter.im/wooorm/remark

[license]: LICENSE

[author]: http://wooorm.com

[npm]: https://docs.npmjs.com/cli/install

[mdast]: https://github.com/syntax-tree/mdast

[node]: https://github.com/syntax-tree/unist#node
