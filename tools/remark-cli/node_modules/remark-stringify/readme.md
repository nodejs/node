# remark-stringify [![Build Status][build-badge]][build-status] [![Coverage Status][coverage-badge]][coverage-status] [![Chat][chat-badge]][chat]

[Compiler][] for [**unified**][unified].  Stringifies an
[**MDAST**][mdast] syntax tree to markdown.  Used in the [**remark**
processor][processor].  Can be [extended][extend] to change how
markdown is compiled.

## Installation

[npm][]:

```sh
npm install remark-stringify
```

## Usage

```js
var unified = require('unified');
var createStream = require('unified-stream');
var parse = require('remark-parse');
var toc = require('remark-toc');
var stringify = require('remark-stringify');

var processor = unified()
  .use(parse)
  .use(toc)
  .use(stringify, {
    bullet: '*',
    fence: '~',
    fences: true,
    incrementListMarker: false
  });

process.stdin
  .pipe(createStream(processor))
  .pipe(process.stdout);
```

## Table of Contents

*   [API](#api)
    *   [processor.use(stringify)](#processorusestringify)
    *   [stringify.Compiler](#stringifycompiler)
*   [Extending the Compiler](#extending-the-compiler)
    *   [Compiler#visitors](#compilervisitors)
    *   [function visitor(node\[, parent\])](#function-visitornode-parent)
*   [License](#license)

## API

### `processor.use(stringify)`

Configure the `processor` to stringify [**MDAST**][mdast] syntax trees
to markdown.

###### `options`

Options are passed later through [`processor.stringify()`][stringify],
[`processor.process()`][process], or [`processor.pipe()`][pipe].
The following settings are supported:

*   `gfm` (`boolean`, default: `true`):
    *   Escape pipes (`|`, for tables)
    *   Escape colons (`:`, for literal URLs)
    *   Escape tildes (`~`, for strike-through)
*   `commonmark` (`boolean`, default: `false`):
    *   Compile adjacent blockquotes separately
    *   Escape more characters using slashes, instead of as entities
*   `pedantic` (`boolean`, default: `false`):
    *   Escape underscores in words
*   `entities` (`string` or `boolean`, default: `false`):

    *   `true` — Entities are generated for special HTML characters
        (`&` > `&amp;`) and non-ASCII characters (`©` > `&copy;`).
        If named entities are not (widely) supported, numbered character
        references are used (`’` > `&#x2019;`)

    *   `'numbers'` — Numbered entities are generated (`&` > `&#x26;`)
        for special HTML characters and non-ASCII characters

    *   `'escape'` — Special HTML characters are encoded (`&` >
        `&amp;`, `’` > `&#x2019;`), non-ASCII characters not (ö persists)

*   `setext` (`boolean`, default: `false`)
    — Compile headings, when possible, in Setext-style: using `=` for
    level one headings and `-` for level two headings.  Other heading
    levels are compiled as ATX (respecting `closeAtx`)
*   `closeAtx` (`boolean`, default: `false`)
    — Compile ATX headings with the same amount of closing hashes as
    opening hashes
*   `looseTable` (`boolean`, default: `false`)
    — Create tables without fences (initial and final pipes)
*   `spacedTable` (`boolean`, default: `true`)
    — Create tables without spacing between pipes and content
*   `paddedTable` (`boolean`, default: `true`)
    — Create tables with padding in each cell so that they are the same size
*   `fence` (`'~'` or ``'`'``, default: ``'`'``)
    — Fence marker to use for code blocks
*   `fences` (`boolean`, default: `false`)
    — Stringify code blocks without language with fences
*   `bullet` (`'-'`, `'*'`, or `'+'`, default: `'-'`)
    — Bullet marker to use for unordered list items
*   `listItemIndent` (`'tab'`, `'mixed'` or `'1'`, default: `'tab'`)

    How to indent the content from list items:

    *   `'tab'`: use tab stops (4 spaces)
    *   `'1'`: use one space
    *   `'mixed'`: use `1` for tight and `tab` for loose list items

*   `incrementListMarker` (`boolean`, default: `true`)
    — Whether to increment ordered list item bullets
*   `rule` (`'-'`, `'*'`, or `'_'`, default: `'*'`)
    — Marker to use for thematic breaks (horizontal rules)
*   `ruleRepetition` (`number`, default: `3`)
    — Number of markers to use for thematic breaks (horizontal rules).
    Should be `3` or more
*   `ruleSpaces` (`boolean`, default `true`)
    — Whether to pad thematic break (horizontal rule) markers with
    spaces
*   `strong` (`'_'` or `'*'`, default `'*'`)
    — Marker to use for importance
*   `emphasis` (`'_'` or `'*'`, default `'_'`)
    — Marker to use for emphasis

### `stringify.Compiler`

Access to the raw [compiler][], if you need it.

## Extending the Compiler

If this plug-in is used, it adds a [`Compiler`][compiler] constructor
to the `processor`.  Other plug-ins can change and add visitors on
the compiler’s prototype to change how markdown is stringified.

The below plug-in modifies a [visitor][] to add an extra blank line
before level two headings.

```js
module.exports = gap;

function gap() {
  var Compiler = this.Compiler;
  var visitors = Compiler.prototype.visitors;
  var heading = visitors.heading;

  visitors.heading = heading;

  function heading(node) {
    return (node.depth === 2 ? '\n' : '') + heading.apply(this, arguments);
  }
}
```

### `Compiler#visitors`

An object mapping [node][] types to [`visitor`][visitor]s.

### `function visitor(node[, parent])`

Stringify `node`.

###### Parameters

*   `node` ([`Node`][node]) — Node to compile
*   `parent` ([`Node`][node], optional) — Parent of `node`

###### Returns

`string`, the compiled given `node`.

## License

[MIT][license] © [Titus Wormer][author]

<!-- Definitions -->

[build-badge]: https://img.shields.io/travis/wooorm/remark.svg

[build-status]: https://travis-ci.org/wooorm/remark

[coverage-badge]: https://img.shields.io/codecov/c/github/wooorm/remark.svg

[coverage-status]: https://codecov.io/github/wooorm/remark

[chat-badge]: https://img.shields.io/gitter/room/wooorm/remark.svg

[chat]: https://gitter.im/wooorm/remark

[license]: https://github.com/wooorm/remark/blob/master/LICENSE

[author]: http://wooorm.com

[npm]: https://docs.npmjs.com/cli/install

[unified]: https://github.com/wooorm/unified

[processor]: https://github.com/wooorm/remark

[stringify]: https://github.com/wooorm/unified#processorstringifynode-filevalue-options

[process]: https://github.com/wooorm/unified#processorprocessfilevalue-options-done

[pipe]: https://github.com/wooorm/unified#processorpipestream-options

[compiler]: https://github.com/wooorm/unified#processorcompiler

[mdast]: https://github.com/wooorm/mdast

[node]: https://github.com/wooorm/unist#node

[extend]: #extending-the-compiler

[visitor]: #function-visitornode-parent
