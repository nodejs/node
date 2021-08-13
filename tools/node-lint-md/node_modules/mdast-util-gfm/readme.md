# mdast-util-gfm

[![Build][build-badge]][build]
[![Coverage][coverage-badge]][coverage]
[![Downloads][downloads-badge]][downloads]
[![Size][size-badge]][size]
[![Sponsors][sponsors-badge]][collective]
[![Backers][backers-badge]][collective]
[![Chat][chat-badge]][chat]

Extension for [`mdast-util-from-markdown`][from-markdown] and/or
[`mdast-util-to-markdown`][to-markdown] to support GitHub flavored markdown in
**[mdast][]**.
When parsing (`from-markdown`), must be combined with
[`micromark-extension-gfm`][extension].

## When to use this

Use this if you’re dealing with the AST manually and need all of GFM.
It’s probably nicer to use [`remark-gfm`][remark-gfm] with
**[remark][]**, which includes this but provides a nicer interface and
makes it easier to combine with hundreds of plugins.

Alternatively, the extensions can be used separately:

*   [`syntax-tree/mdast-util-gfm-autolink-literal`](https://github.com/syntax-tree/mdast-util-gfm-autolink-literal)
    — support GFM autolink literals
*   [`syntax-tree/mdast-util-gfm-strikethrough`](https://github.com/syntax-tree/mdast-util-gfm-strikethrough)
    — support GFM strikethrough
*   [`syntax-tree/mdast-util-gfm-table`](https://github.com/syntax-tree/mdast-util-gfm-table)
    — support GFM tables
*   [`syntax-tree/mdast-util-gfm-task-list-item`](https://github.com/syntax-tree/mdast-util-gfm-task-list-item)
    — support GFM tasklists

## Install

This package is [ESM only](https://gist.github.com/sindresorhus/a39789f98801d908bbc7ff3ecc99d99c):
Node 12+ is needed to use it and it must be `import`ed instead of `require`d.

[npm][]:

```sh
npm install mdast-util-gfm
```

## Use

Say we have the following file, `example.md`:

```markdown
# GFM

## Autolink literals

www.example.com, https://example.com, and contact@example.com.

## Strikethrough

~one~ or ~~two~~ tildes.

## Table

| a | b  |  c |  d  |
| - | :- | -: | :-: |

## Tasklist

* [ ] to do
* [x] done
```

And our module, `example.js`, looks as follows:

```js
import fs from 'node:fs'
import {fromMarkdown} from 'mdast-util-from-markdown'
import {toMarkdown} from 'mdast-util-to-markdown'
import {gfm} from 'micromark-extension-gfm'
import {gfmFromMarkdown, gfmToMarkdown} from 'mdast-util-gfm'

const doc = fs.readFileSync('example.md')

const tree = fromMarkdown(doc, {
  extensions: [gfm()],
  mdastExtensions: [gfmFromMarkdown]
})

console.log(tree)

const out = toMarkdown(tree, {extensions: [gfmToMarkdown()]})

console.log(out)
```

Now, running `node example` yields:

```js
{
  type: 'root',
  children: [
    {type: 'heading', depth: 1, children: [{type: 'text', value: 'GFM'}]},
    {
      type: 'heading',
      depth: 2,
      children: [{type: 'text', value: 'Autolink literals'}]
    },
    {
      type: 'paragraph',
      children: [
        {
          type: 'link',
          title: null,
          url: 'http://www.example.com',
          children: [{type: 'text', value: 'www.example.com'}]
        },
        {type: 'text', value: ', '},
        {
          type: 'link',
          title: null,
          url: 'https://example.com',
          children: [{type: 'text', value: 'https://example.com'}]
        },
        {type: 'text', value: ', and '},
        {
          type: 'link',
          title: null,
          url: 'mailto:contact@example.com',
          children: [{type: 'text', value: 'contact@example.com'}]
        },
        {type: 'text', value: '.'}
      ]
    },
    {
      type: 'heading',
      depth: 2,
      children: [{type: 'text', value: 'Strikethrough'}]
    },
    {
      type: 'paragraph',
      children: [
        {
          type: 'delete',
          children: [{type: 'text', value: 'one'}]
        },
        {type: 'text', value: ' or '},
        {
          type: 'delete',
          children: [{type: 'text', value: 'two'}]
        },
        {type: 'text', value: ' tildes.'}
      ]
    },
    {type: 'heading', depth: 2, children: [{type: 'text', value: 'Table'}]},
    {
      type: 'table',
      align: [null, 'left', 'right', 'center'],
      children: [
        {
          type: 'tableRow',
          children: [
            {type: 'tableCell', children: [{type: 'text', value: 'a'}]},
            {type: 'tableCell', children: [{type: 'text', value: 'b'}]},
            {type: 'tableCell', children: [{type: 'text', value: 'c'}]},
            {type: 'tableCell', children: [{type: 'text', value: 'd'}]}
          ]
        }
      ]
    },
    {type: 'heading', depth: 2, children: [{type: 'text', value: 'Tasklist'}]},
    {
      type: 'list',
      ordered: false,
      start: null,
      spread: false,
      children: [
        {
          type: 'listItem',
          spread: false,
          checked: false,
          children: [
            {type: 'paragraph', children: [{type: 'text', value: 'to do'}]}
          ]
        },
        {
          type: 'listItem',
          spread: false,
          checked: true,
          children: [
            {type: 'paragraph', children: [{type: 'text', value: 'done'}]}
          ]
        }
      ]
    }
  ]
}
```

```markdown
# GFM

## Autolink literals

[www.example.com](http://www.example.com), <https://example.com>, and <contact@example.com>.

## Strikethrough

~~one~~ or ~~two~~ tildes.

## Table

| a | b  |  c |  d  |
| - | :- | -: | :-: |

## Tasklist

*   [ ] to do
*   [x] done
```

## API

This package exports the following identifier: `gfmFromMarkdown`,
`gfmToMarkdown`.
There is no default export.

### `gfmFromMarkdown`

### `gfmToMarkdown(options?)`

Support GFM.
The exports of `fromMarkdown` is an extension for
[`mdast-util-from-markdown`][from-markdown].
The export of `toMarkdown` is a function that can be called with options and
returns an extension for [`mdast-util-to-markdown`][to-markdown].

###### `options`

Passed as `options` to [`mdast-util-gfm-table`][table].

## Related

*   [`remarkjs/remark`][remark]
    — markdown processor powered by plugins
*   [`remarkjs/remark-gfm`][remark-gfm]
    — remark plugin to support GFM
*   [`micromark/micromark`][micromark]
    — the smallest commonmark-compliant markdown parser that exists
*   [`micromark/micromark-extension-gfm`][extension]
    — micromark extension to parse GFM
*   [`syntax-tree/mdast-util-from-markdown`][from-markdown]
    — mdast parser using `micromark` to create mdast from markdown
*   [`syntax-tree/mdast-util-to-markdown`][to-markdown]
    — mdast serializer to create markdown from mdast

## Contribute

See [`contributing.md` in `syntax-tree/.github`][contributing] for ways to get
started.
See [`support.md`][support] for ways to get help.

This project has a [code of conduct][coc].
By interacting with this repository, organization, or community you agree to
abide by its terms.

## License

[MIT][license] © [Titus Wormer][author]

<!-- Definitions -->

[build-badge]: https://github.com/syntax-tree/mdast-util-gfm/workflows/main/badge.svg

[build]: https://github.com/syntax-tree/mdast-util-gfm/actions

[coverage-badge]: https://img.shields.io/codecov/c/github/syntax-tree/mdast-util-gfm.svg

[coverage]: https://codecov.io/github/syntax-tree/mdast-util-gfm

[downloads-badge]: https://img.shields.io/npm/dm/mdast-util-gfm.svg

[downloads]: https://www.npmjs.com/package/mdast-util-gfm

[size-badge]: https://img.shields.io/bundlephobia/minzip/mdast-util-gfm.svg

[size]: https://bundlephobia.com/result?p=mdast-util-gfm

[sponsors-badge]: https://opencollective.com/unified/sponsors/badge.svg

[backers-badge]: https://opencollective.com/unified/backers/badge.svg

[collective]: https://opencollective.com/unified

[chat-badge]: https://img.shields.io/badge/chat-discussions-success.svg

[chat]: https://github.com/syntax-tree/unist/discussions

[npm]: https://docs.npmjs.com/cli/install

[license]: license

[author]: https://wooorm.com

[contributing]: https://github.com/syntax-tree/.github/blob/HEAD/contributing.md

[support]: https://github.com/syntax-tree/.github/blob/HEAD/support.md

[coc]: https://github.com/syntax-tree/.github/blob/HEAD/code-of-conduct.md

[mdast]: https://github.com/syntax-tree/mdast

[remark]: https://github.com/remarkjs/remark

[remark-gfm]: https://github.com/remarkjs/remark-gfm

[from-markdown]: https://github.com/syntax-tree/mdast-util-from-markdown

[to-markdown]: https://github.com/syntax-tree/mdast-util-to-markdown

[micromark]: https://github.com/micromark/micromark

[extension]: https://github.com/micromark/micromark-extension-gfm

[table]: https://github.com/syntax-tree/mdast-util-gfm-table#options
