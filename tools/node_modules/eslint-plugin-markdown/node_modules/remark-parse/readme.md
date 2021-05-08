# remark-parse

[![Build][build-badge]][build]
[![Coverage][coverage-badge]][coverage]
[![Downloads][downloads-badge]][downloads]
[![Size][size-badge]][size]
[![Sponsors][sponsors-badge]][collective]
[![Backers][backers-badge]][collective]
[![Chat][chat-badge]][chat]

[Parser][] for [**unified**][unified].
Parses Markdown to [**mdast**][mdast] syntax trees.
Used in the [**remark** processor][remark] but can be used on its own as well.
Can be [extended][extend] to change how markdown is parsed.

## Sponsors

<!--lint ignore no-html maximum-line-length-->

<table>
  <tr valign="top">
    <td width="20%" align="center">
      <a href="https://zeit.co"><img src="https://avatars1.githubusercontent.com/u/14985020?s=400&v=4"></a>
      <br><br>🥇
      <a href="https://zeit.co">ZEIT</a>
    </td>
    <td width="20%" align="center">
      <a href="https://www.gatsbyjs.org"><img src="https://avatars1.githubusercontent.com/u/12551863?s=400&v=4"></a>
      <br><br>🥇
      <a href="https://www.gatsbyjs.org">Gatsby</a>
    </td>
    <td width="20%" align="center">
      <a href="https://www.netlify.com"><img src="https://avatars1.githubusercontent.com/u/7892489?s=400&v=4"></a>
      <br><br>🥇
      <a href="https://www.netlify.com">Netlify</a>
    </td>
    <td width="20%" align="center">
      <a href="https://www.holloway.com"><img src="https://avatars1.githubusercontent.com/u/35904294?s=400&v=4"></a>
      <br><br>
      <a href="https://www.holloway.com">Holloway</a>
    </td>
    <td width="20%" align="center">
      <br><br><br><br>
      <a href="https://opencollective.com/unified"><strong>You?</strong>
    </td>
  </tr>
</table>

[**Read more about the unified collective on Medium »**][announcement]

## Install

[npm][]:

```sh
npm install remark-parse
```

## Use

```js
var unified = require('unified')
var createStream = require('unified-stream')
var markdown = require('remark-parse')
var remark2rehype = require('remark-rehype')
var html = require('rehype-stringify')

var processor = unified()
  .use(markdown, {commonmark: true})
  .use(remark2rehype)
  .use(html)

process.stdin.pipe(createStream(processor)).pipe(process.stdout)
```

[See **unified** for more examples »][unified]

## Table of Contents

*   [API](#api)
    *   [`processor().use(parse[, options])`](#processoruseparse-options)
    *   [`parse.Parser`](#parseparser)
*   [Extending the Parser](#extending-the-parser)
    *   [`Parser#blockTokenizers`](#parserblocktokenizers)
    *   [`Parser#blockMethods`](#parserblockmethods)
    *   [`Parser#inlineTokenizers`](#parserinlinetokenizers)
    *   [`Parser#inlineMethods`](#parserinlinemethods)
    *   [`function tokenizer(eat, value, silent)`](#function-tokenizereat-value-silent)
    *   [`tokenizer.locator(value, fromIndex)`](#tokenizerlocatorvalue-fromindex)
    *   [`eat(subvalue)`](#eatsubvalue)
    *   [`add(node[, parent])`](#addnode-parent)
    *   [`add.test()`](#addtest)
    *   [`add.reset(node[, parent])`](#addresetnode-parent)
    *   [Turning off a tokenizer](#turning-off-a-tokenizer)
*   [Security](#security)
*   [Contribute](#contribute)
*   [License](#license)

## API

[See **unified** for API docs »][unified]

### `processor().use(parse[, options])`

Configure the `processor` to read Markdown as input and process
[**mdast**][mdast] syntax trees.

##### `options`

Options can be passed directly, or passed later through
[`processor.data()`][data].

###### `options.gfm`

GFM mode (`boolean`, default: `true`).

```markdown
hello ~~hi~~ world
```

Turns on:

*   [Fenced code blocks](https://help.github.com/articles/creating-and-highlighting-code-blocks#fenced-code-blocks)
*   [Autolinking of URLs](https://help.github.com/articles/autolinked-references-and-urls)
*   [Deletions (strikethrough)](https://help.github.com/articles/basic-writing-and-formatting-syntax#styling-text)
*   [Task lists](https://help.github.com/articles/basic-writing-and-formatting-syntax#task-lists)
*   [Tables](https://help.github.com/articles/organizing-information-with-tables)

###### `options.commonmark`

CommonMark mode (`boolean`, default: `false`).

```markdown
This is a paragraph
    and this is also part of the preceding paragraph.
```

Allows:

*   Empty lines to split blockquotes
*   Parentheses (`(` and `)`) around link and image titles
*   Any escaped [ASCII punctuation][escapes] character
*   Closing parenthesis (`)`) as an ordered list marker
*   URL definitions (and footnotes, when enabled) in blockquotes

Disallows:

*   Indented code blocks directly following a paragraph
*   ATX headings (`# Hash headings`) without spacing after opening hashes or and
    before closing hashes
*   Setext headings (`Underline headings\n---`) when following a paragraph
*   Newlines in link and image titles
*   White space in link and image URLs in auto-links (links in brackets, `<` and
    `>`)
*   Lazy blockquote continuation, lines not preceded by a greater than character
    (`>`), for lists, code, and thematic breaks

###### `options.footnotes`

Footnotes mode (`boolean`, default: `false`).

```markdown
Something something[^or something?].

And something else[^1].

[^1]: This reference footnote contains a paragraph...

    * ...and a list
```

Enables reference footnotes and inline footnotes.
Both are wrapped in square brackets and preceded by a caret (`^`), and can be
referenced from inside other footnotes.

###### `options.pedantic`

Pedantic mode (`boolean`, default: `false`).

```markdown
Check out some_file_name.txt
```

Turns on:

*   Emphasis (`_alpha_`) and importance (`__bravo__`) with underscores in words
*   Unordered lists with different markers (`*`, `-`, `+`)
*   If `commonmark` is also turned on, ordered lists with different markers
    (`.`, `)`)
*   And removes less spaces in list items (at most four, instead of the whole
    indent)

###### `options.blocks`

Blocks (`Array.<string>`, default: list of [block HTML elements][blocks]).

```markdown
<block>foo
</block>
```

Defines which HTML elements are seen as block level.

### `parse.Parser`

Access to the [parser][], if you need it.

## Extending the Parser

Typically, using [*transformers*][transformer] to manipulate a syntax tree
produces the desired output.
Sometimes, such as when introducing new syntactic entities with a certain
precedence, interfacing with the parser is necessary.

If the `remark-parse` plugin is used, it adds a [`Parser`][parser] constructor
function to the `processor`.
Other plugins can add tokenizers to its prototype to change how Markdown is
parsed.

The below plugin adds a [tokenizer][] for at-mentions.

```js
module.exports = mentions

function mentions() {
  var Parser = this.Parser
  var tokenizers = Parser.prototype.inlineTokenizers
  var methods = Parser.prototype.inlineMethods

  // Add an inline tokenizer (defined in the following example).
  tokenizers.mention = tokenizeMention

  // Run it just before `text`.
  methods.splice(methods.indexOf('text'), 0, 'mention')
}
```

### `Parser#blockTokenizers`

Map of names to [tokenizer][]s (`Object.<Function>`).
These tokenizers (such as `fencedCode`, `table`, and `paragraph`) eat from the
start of a value to a line ending.

See `#blockMethods` below for a list of methods that are included by default.

### `Parser#blockMethods`

List of `blockTokenizers` names (`Array.<string>`).
Specifies the order in which tokenizers run.

Precedence of default block methods is as follows:

<!--methods-block start-->

*   `newline`
*   `indentedCode`
*   `fencedCode`
*   `blockquote`
*   `atxHeading`
*   `thematicBreak`
*   `list`
*   `setextHeading`
*   `html`
*   `footnote`
*   `definition`
*   `table`
*   `paragraph`

<!--methods-block end-->

### `Parser#inlineTokenizers`

Map of names to [tokenizer][]s (`Object.<Function>`).
These tokenizers (such as `url`, `reference`, and `emphasis`) eat from the start
of a value.
To increase performance, they depend on [locator][]s.

See `#inlineMethods` below for a list of methods that are included by default.

### `Parser#inlineMethods`

List of `inlineTokenizers` names (`Array.<string>`).
Specifies the order in which tokenizers run.

Precedence of default inline methods is as follows:

<!--methods-inline start-->

*   `escape`
*   `autoLink`
*   `url`
*   `html`
*   `link`
*   `reference`
*   `strong`
*   `emphasis`
*   `deletion`
*   `code`
*   `break`
*   `text`

<!--methods-inline end-->

### `function tokenizer(eat, value, silent)`

There are two types of tokenizers: block level and inline level.
Both are functions, and work the same, but inline tokenizers must have a
[locator][].

The following example shows an inline tokenizer that is added by the mentions
plugin above.

```js
tokenizeMention.notInLink = true
tokenizeMention.locator = locateMention

function tokenizeMention(eat, value, silent) {
  var match = /^@(\w+)/.exec(value)

  if (match) {
    if (silent) {
      return true
    }

    return eat(match[0])({
      type: 'link',
      url: 'https://social-network/' + match[1],
      children: [{type: 'text', value: match[0]}]
    })
  }
}
```

Tokenizers *test* whether a document starts with a certain syntactic entity.
In *silent* mode, they return whether that test passes.
In *normal* mode, they consume that token, a process which is called “eating”.

Locators enable inline tokenizers to function faster by providing where the next
entity may occur.

###### Signatures

*   `Node? = tokenizer(eat, value)`
*   `boolean? = tokenizer(eat, value, silent)`

###### Parameters

*   `eat` ([`Function`][eat]) — Eat, when applicable, an entity
*   `value` (`string`) — Value which may start an entity
*   `silent` (`boolean`, optional) — Whether to detect or consume

###### Properties

*   `locator` ([`Function`][locator]) — Required for inline tokenizers
*   `onlyAtStart` (`boolean`) — Whether nodes can only be found at the beginning
    of the document
*   `notInBlock` (`boolean`) — Whether nodes cannot be in blockquotes, lists, or
    footnote definitions
*   `notInList` (`boolean`) — Whether nodes cannot be in lists
*   `notInLink` (`boolean`) — Whether nodes cannot be in links

###### Returns

*   `boolean?`, in *silent* mode — whether a node can be found at the start of
    `value`
*   [`Node?`][node], In *normal* mode — If it can be found at the start of
    `value`

### `tokenizer.locator(value, fromIndex)`

Locators are required for inline tokenizers.
Their role is to keep parsing performant.

The following example shows a locator that is added by the mentions tokenizer
above.

```js
function locateMention(value, fromIndex) {
  return value.indexOf('@', fromIndex)
}
```

Locators enable inline tokenizers to function faster by providing information on
where the next entity *may* occur.
Locators may be wrong, it’s OK if there actually isn’t a node to be found at the
index they return.

###### Parameters

*   `value` (`string`) — Value which may contain an entity
*   `fromIndex` (`number`) — Position to start searching at

###### Returns

`number` — Index at which an entity may start, and `-1` otherwise.

### `eat(subvalue)`

```js
var add = eat('foo')
```

Eat `subvalue`, which is a string at the start of the [tokenized][tokenizer]
`value`.

###### Parameters

*   `subvalue` (`string`) - Value to eat

###### Returns

[`add`][add].

### `add(node[, parent])`

```js
var add = eat('foo')

add({type: 'text', value: 'foo'})
```

Add [positional information][position] to `node` and add `node` to `parent`.

###### Parameters

*   `node` ([`Node`][node]) - Node to patch position on and to add
*   `parent` ([`Parent`][parent], optional) - Place to add `node` to in the
    syntax tree.
    Defaults to the currently processed node

###### Returns

[`Node`][node] — The given `node`.

### `add.test()`

Get the [positional information][position] that would be patched on `node` by
`add`.

###### Returns

[`Position`][position].

### `add.reset(node[, parent])`

`add`, but resets the internal position.
Useful for example in lists, where the same content is first eaten for a list,
and later for list items.

###### Parameters

*   `node` ([`Node`][node]) - Node to patch position on and insert
*   `parent` ([`Node`][node], optional) - Place to add `node` to in
    the syntax tree.
    Defaults to the currently processed node

###### Returns

[`Node`][node] — The given node.

### Turning off a tokenizer

In some situations, you may want to turn off a tokenizer to avoid parsing that
syntactic feature.

Preferably, use the [`remark-disable-tokenizers`][remark-disable-tokenizers]
plugin to turn off tokenizers.

Alternatively, this can be done by replacing the tokenizer from
`blockTokenizers` (or `blockMethods`) or `inlineTokenizers` (or
`inlineMethods`).

The following example turns off indented code blocks:

```js
remarkParse.Parser.prototype.blockTokenizers.indentedCode = indentedCode

function indentedCode() {
  return true
}
```

## Security

As Markdown is sometimes used for HTML, and improper use of HTML can open you up
to a [cross-site scripting (XSS)][xss] attack, use of remark can also be unsafe.
When going to HTML, use remark in combination with the [**rehype**][rehype]
ecosystem, and use [`rehype-sanitize`][sanitize] to make the tree safe.

Use of remark plugins could also open you up to other attacks.
Carefully assess each plugin and the risks involved in using them.

## Contribute

See [`contributing.md`][contributing] in [`remarkjs/.github`][health] for ways
to get started.
See [`support.md`][support] for ways to get help.
Ideas for new plugins and tools can be posted in [`remarkjs/ideas`][ideas].

A curated list of awesome remark resources can be found in [**awesome
remark**][awesome].

This project has a [Code of Conduct][coc].
By interacting with this repository, organisation, or community you agree to
abide by its terms.

## License

[MIT][license] © [Titus Wormer][author]

<!-- Definitions -->

[build-badge]: https://img.shields.io/travis/remarkjs/remark.svg

[build]: https://travis-ci.org/remarkjs/remark

[coverage-badge]: https://img.shields.io/codecov/c/github/remarkjs/remark.svg

[coverage]: https://codecov.io/github/remarkjs/remark

[downloads-badge]: https://img.shields.io/npm/dm/remark-parse.svg

[downloads]: https://www.npmjs.com/package/remark-parse

[size-badge]: https://img.shields.io/bundlephobia/minzip/remark-parse.svg

[size]: https://bundlephobia.com/result?p=remark-parse

[sponsors-badge]: https://opencollective.com/unified/sponsors/badge.svg

[backers-badge]: https://opencollective.com/unified/backers/badge.svg

[collective]: https://opencollective.com/unified

[chat-badge]: https://img.shields.io/badge/join%20the%20community-on%20spectrum-7b16ff.svg

[chat]: https://spectrum.chat/unified/remark

[health]: https://github.com/remarkjs/.github

[contributing]: https://github.com/remarkjs/.github/blob/master/contributing.md

[support]: https://github.com/remarkjs/.github/blob/master/support.md

[coc]: https://github.com/remarkjs/.github/blob/master/code-of-conduct.md

[ideas]: https://github.com/remarkjs/ideas

[awesome]: https://github.com/remarkjs/awesome-remark

[license]: https://github.com/remarkjs/remark/blob/master/license

[author]: https://wooorm.com

[npm]: https://docs.npmjs.com/cli/install

[unified]: https://github.com/unifiedjs/unified

[data]: https://github.com/unifiedjs/unified#processordatakey-value

[remark]: https://github.com/remarkjs/remark/tree/master/packages/remark

[blocks]: https://github.com/remarkjs/remark/blob/master/packages/remark-parse/lib/block-elements.js

[mdast]: https://github.com/syntax-tree/mdast

[escapes]: https://spec.commonmark.org/0.29/#backslash-escapes

[node]: https://github.com/syntax-tree/unist#node

[parent]: https://github.com/syntax-tree/unist#parent

[position]: https://github.com/syntax-tree/unist#position

[parser]: https://github.com/unifiedjs/unified#processorparser

[transformer]: https://github.com/unifiedjs/unified#function-transformernode-file-next

[extend]: #extending-the-parser

[tokenizer]: #function-tokenizereat-value-silent

[locator]: #tokenizerlocatorvalue-fromindex

[eat]: #eatsubvalue

[add]: #addnode-parent

[announcement]: https://medium.com/unifiedjs/collectively-evolving-through-crowdsourcing-22c359ea95cc

[remark-disable-tokenizers]: https://github.com/zestedesavoir/zmarkdown/tree/master/packages/remark-disable-tokenizers

[xss]: https://en.wikipedia.org/wiki/Cross-site_scripting

[rehype]: https://github.com/rehypejs/rehype

[sanitize]: https://github.com/rehypejs/rehype-sanitize
