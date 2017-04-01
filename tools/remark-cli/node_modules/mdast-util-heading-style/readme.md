# mdast-util-heading-style [![Build Status][build-badge]][build-status] [![Coverage Status][coverage-badge]][coverage-status] [![Chat][chat-badge]][chat]

<!--lint disable list-item-spacing heading-increment list-item-indent-->

Get the style of an [**MDAST**][mdast] heading.

## Installation

[npm][]:

```bash
npm install mdast-util-heading-style
```

**mdast-util-heading-style** is also available as an AMD, CommonJS, and
globals module, [uncompressed and compressed][releases].

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

*   `node` ([`Node`][node]) — Node to parse;
*   `relative` (`string`, optional) — Style to use for ambiguous headings
    (atx-headings with a level of three or more could also be setext).

###### Returns

`string` (`'atx'`, `'atx-closed'`, or `'setext'`) — When an ambiguous
heading is found, either `relative` or `null` is returned.

### `Marker`

A comment marker.

###### Properties

*   `name` (`string`) — Name of marker;
*   `attributes` (`string`) — Value after name;
*   `parameters` (`Object`) — Parsed attributes, tries to convert
    values to numbers and booleans when possible;
*   `node` ([`Node`][node]) — Reference to given node.

## License

[MIT][license] © [Titus Wormer][author]

<!-- Definitions -->

[build-badge]: https://img.shields.io/travis/wooorm/mdast-util-heading-style.svg

[build-status]: https://travis-ci.org/wooorm/mdast-util-heading-style

[coverage-badge]: https://img.shields.io/codecov/c/github/wooorm/mdast-util-heading-style.svg

[coverage-status]: https://codecov.io/github/wooorm/mdast-util-heading-style

[chat-badge]: https://img.shields.io/gitter/room/wooorm/remark.svg

[chat]: https://gitter.im/wooorm/remark

[releases]: https://github.com/wooorm/mdast-util-heading-style/releases

[license]: LICENSE

[author]: http://wooorm.com

[npm]: https://docs.npmjs.com/cli/install

[mdast]: https://github.com/wooorm/mdast

[node]: https://github.com/wooorm/mdast#node
