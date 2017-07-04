# remark-parse [![Build Status][build-badge]][build-status] [![Coverage Status][coverage-badge]][coverage-status] [![Chat][chat-badge]][chat]

[Parser][] for [**unified**][unified].  Parses markdown to an
[**MDAST**][mdast] syntax tree.  Used in the [**remark**
processor][processor].  Can be [extended][extend] to change how
markdown is parsed.

## Installation

[npm][]:

```sh
npm install remark-parse
```

## Usage

```js
var unified = require('unified');
var createStream = require('unified-stream');
var markdown = require('remark-parse');
var html = require('remark-html');

var processor = unified()
  .use(markdown, {commonmark: true})
  .use(html)

process.stdin
  .pipe(createStream(processor))
  .pipe(process.stdout);
```

## Table of Contents

*   [API](#api)
    *   [processor.use(parse)](#processoruseparse)
    *   [parse.Parser](#parseparser)
*   [Extending the Parser](#extending-the-parser)
    *   [Parser#blockTokenizers](#parserblocktokenizers)
    *   [Parser#blockMethods](#parserblockmethods)
    *   [Parser#inlineTokenizers](#parserinlinetokenizers)
    *   [Parser#inlineMethods](#parserinlinemethods)
    *   [function tokenizer(eat, value, silent)](#function-tokenizereat-value-silent)
    *   [tokenizer.locator(value, fromIndex)](#tokenizerlocatorvalue-fromindex)
    *   [eat(subvalue)](#eatsubvalue)
    *   [add(node\[, parent\])](#addnode-parent)
    *   [add.test()](#addtest)
    *   [add.reset(node\[, parent\])](#addresetnode-parent)
*   [License](#license)

## API

### `processor.use(parse)`

Configure the `processor` to read markdown as input and process an
[**MDAST**][mdast] syntax tree.

#### `options`

Options are passed later through [`processor.parse()`][parse],
[`processor.process()`][process], or [`processor.pipe()`][pipe].
The following settings are supported:

*   [`gfm`][options-gfm] (`boolean`, default: `true`)
*   [`yaml`][options-yaml] (`boolean`, default: `true`)
*   [`commonmark`][options-commonmark] (`boolean`, default: `false`)
*   [`footnotes`][options-footnotes] (`boolean`, default: `false`)
*   [`pedantic`][options-pedantic] (`boolean`, default: `false`)
*   [`breaks`][options-breaks] (`boolean`, default: `false`)
*   [`blocks`][options-blocks] (`Array.<string>`, default: list of block HTML
    elements)

##### `options.gfm`

```md
hello ~~hi~~ world
```

GFM mode (default: `true`) turns on:

*   [Fenced code blocks](https://help.github.com/articles/github-flavored-markdown/#fenced-code-blocks)
*   [Autolinking of URLs](https://help.github.com/articles/github-flavored-markdown/#url-autolinking)
*   [Deletions (strikethrough)](https://help.github.com/articles/github-flavored-markdown/#strikethrough)
*   [Task lists](https://help.github.com/articles/writing-on-github/#task-lists)
*   [Tables](https://help.github.com/articles/github-flavored-markdown/#tables)

##### `options.yaml`

```md
---
title: YAML is Cool
---

# YAML is Cool
```

YAML mode (default: `true`) enables raw YAML front matter to be detected
at the top.

##### `options.commonmark`

```md
This is a paragraph
    and this is also part of the preceding paragraph.
```

CommonMark mode (default: `false`) allows:

*   Empty lines to split blockquotes
*   Parentheses (`(` and `)`) around for link and image titles
*   Any escaped [ASCII-punctuation][escapes] character
*   Closing parenthesis (`)`) as an ordered list marker
*   URL definitions (and footnotes, when enabled) in blockquotes

CommonMark mode disallows:

*   Code directly following a paragraph
*   ATX-headings (`# Hash headings`) without spacing after opening hashes
    or and before closing hashes
*   Setext headings (`Underline headings\n---`) when following a paragraph
*   Newlines in link and image titles
*   White space in link and image URLs in auto-links (links in brackets,
    `<` and `>`)
*   Lazy blockquote continuation, lines not preceded by a closing angle
    bracket (`>`), for lists, code, and thematicBreak

##### `options.footnotes`

```md
Something something[^or something?].

And something else[^1].

[^1]: This reference footnote contains a paragraph...

    * ...and a list
```

Footnotes mode (default: `false`) enables reference footnotes and inline
footnotes.  Both are wrapped in square brackets and preceded by a caret
(`^`), and can be referenced from inside other footnotes.

##### `options.breaks`

```md
This is a
paragraph.
```

Breaks mode (default: `false`) exposes newline characters inside
paragraphs as breaks.

##### `options.blocks`

```md
<block>foo
</block>
```

Blocks (default: a list of HTML block elements) exposes
let’s users define block-level HTML elements.

##### `options.pedantic`

```md
Check out some_file_name.txt
```

Pedantic mode (default: `false`) turns on:

*   Emphasis (`_alpha_`) and importance (`__bravo__`) with underscores
    in words
*   Unordered lists with different markers (`*`, `-`, `+`)
*   If `commonmark` is also turned on, ordered lists with different
    markers (`.`, `)`)
*   And pedantic mode removes less spaces in list-items (at most four,
    instead of the whole indent)

### `parse.Parser`

Access to the [parser][], if you need it.

## Extending the Parser

Most often, using transformers to manipulate a syntax tree produces
the desired output.  Sometimes, mainly when introducing new syntactic
entities with a certain level of precedence, interfacing with the parser
is necessary.

If this plug-in is used, it adds a [`Parser`][parser] constructor to
the `processor`.  Other plug-ins can add tokenizers to the parser’s
prototype to change how markdown is parsed.

The below plug-in adds a [tokenizer][] for at-mentions.

```js
module.exports = mentions;

function mentions() {
  var Parser = this.Parser;
  var tokenizers = Parser.prototype.inlineTokenizers;
  var methods = Parser.prototype.inlineMethods;

  /* Add an inline tokenizer (defined in the following example). */
  tokenizers.mention = tokenizeMention;

  /* Run it just before `text`. */
  methods.splice(methods.indexOf('text'), 0, 'mention');
}
```

### `Parser#blockTokenizers`

An object mapping tokenizer names to [tokenizer][]s.  These
tokenizers (for example: `fencedCode`, `table`, and `paragraph`) eat
from the start of a value to a line ending.

### `Parser#blockMethods`

Array of `blockTokenizers` names (`string`) specifying the order in
which they run.

### `Parser#inlineTokenizers`

An object mapping tokenizer names to [tokenizer][]s.  These tokenizers
(for example: `url`, `reference`, and `emphasis`) eat from the start
of a value.  To increase performance, they depend on [locator][]s.

### `Parser#inlineMethods`

Array of `inlineTokenizers` names (`string`) specifying the order in
which they run.

### `function tokenizer(eat, value, silent)`

```js
tokenizeMention.notInLink = true;
tokenizeMention.locator = locateMention;

function tokenizeMention(eat, value, silent) {
  var match = /^@(\w+)/.exec(value);

  if (match) {
    if (silent) {
      return true;
    }

    return eat(match[0])({
      type: 'link',
      url: 'https://social-network/' + match[1],
      children: [{type: 'text', value: match[0]}]
    });
  }
}
```

The parser knows two types of tokenizers: block level and inline level.
Block level tokenizers are the same as inline level tokenizers, with
the exception that the latter must have a [locator][].

Tokenizers _test_ whether a document starts with a certain syntactic
entity.  In _silent_ mode, they return whether that test passes.
In _normal_ mode, they consume that token, a process which is called
“eating”.  Locators enable tokenizers to function faster by providing
information on where the next entity may occur.

###### Signatures

*   `Node? = tokenizer(eat, value)`
*   `boolean? = tokenizer(eat, value, silent)`

###### Parameters

*   `eat` ([`Function`][eat]) — Eat, when applicable, an entity
*   `value` (`string`) — Value which may start an entity
*   `silent` (`boolean`, optional) — Whether to detect or consume

###### Properties

*   `locator` ([`Function`][locator])
    — Required for inline tokenizers
*   `onlyAtStart` (`boolean`)
    — Whether nodes can only be found at the beginning of the document
*   `notInBlock` (`boolean`)
    — Whether nodes cannot be in blockquotes, lists, or footnote
    definitions
*   `notInList` (`boolean`)
    — Whether nodes cannot be in lists
*   `notInLink` (`boolean`)
    — Whether nodes cannot be in links

###### Returns

*   In _silent_ mode, whether a node can be found at the start of `value`
*   In _normal_ mode, a node if it can be found at the start of `value`

### `tokenizer.locator(value, fromIndex)`

```js
function locateMention(value, fromIndex) {
  return value.indexOf('@', fromIndex);
}
```

Locators are required for inline tokenization to keep the process
performant.  Locators enable inline tokenizers to function faster by
providing information on the where the next entity occurs.  Locators
may be wrong, it’s OK if there actually isn’t a node to be found at
the index they return, but they must skip any nodes.

###### Parameters

*   `value` (`string`) — Value which may contain an entity
*   `fromIndex` (`number`) — Position to start searching at

###### Returns

Index at which an entity may start, and `-1` otherwise.

### `eat(subvalue)`

```js
var add = eat('foo');
```

Eat `subvalue`, which is a string at the start of the
[tokenize][tokenizer]d `value` (it’s tracked to ensure the correct
value is eaten).

###### Parameters

*   `subvalue` (`string`) - Value to eat.

###### Returns

[`add`][add].

### `add(node[, parent])`

```js
var add = eat('foo');
add({type: 'text', value: 'foo'});
```

Add [positional information][location] to `node` and add it to `parent`.

###### Parameters

*   `node` ([`Node`][node]) - Node to patch position on and insert
*   `parent` ([`Node`][node], optional) - Place to add `node` to in
    the syntax tree.  Defaults to the currently processed node

###### Returns

The given `node`.

### `add.test()`

Get the [positional information][location] which would be patched on
`node` by `add`.

###### Returns

[`Location`][location].

### `add.reset(node[, parent])`

`add`, but resets the internal location.  Useful for example in
lists, where the same content is first eaten for a list, and later
for list items

###### Parameters

*   `node` ([`Node`][node]) - Node to patch position on and insert
*   `parent` ([`Node`][node], optional) - Place to add `node` to in
    the syntax tree.  Defaults to the currently processed node

###### Returns

The given `node`.

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

[parse]: https://github.com/wooorm/unified#processorparsefilevalue-options

[process]: https://github.com/wooorm/unified#processorprocessfilevalue-options-done

[pipe]: https://github.com/wooorm/unified#processorpipestream-options

[processor]: https://github.com/wooorm/remark/blob/master/packages/remark

[mdast]: https://github.com/wooorm/mdast

[escapes]: http://spec.commonmark.org/0.25/#backslash-escapes

[node]: https://github.com/wooorm/unist#node

[location]: https://github.com/wooorm/unist#location

[options-gfm]: #optionsgfm

[options-yaml]: #optionsyaml

[options-commonmark]: #optionscommonmark

[options-footnotes]: #optionsfootnotes

[options-pedantic]: #optionspedantic

[options-breaks]: #optionsbreaks

[options-blocks]: #optionsblocks

[parser]: https://github.com/wooorm/unified#processorparser

[extend]: #extending-the-parser

[tokenizer]: #function-tokenizereat-value-silent

[locator]: #tokenizerlocatorvalue-fromindex

[eat]: #eatsubvalue

[add]: #addnode-parent
