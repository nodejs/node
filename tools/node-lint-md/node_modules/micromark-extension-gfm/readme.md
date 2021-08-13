# micromark-extension-gfm

[![Build][build-badge]][build]
[![Coverage][coverage-badge]][coverage]
[![Downloads][downloads-badge]][downloads]
[![Size][size-badge]][size]
[![Sponsors][sponsors-badge]][collective]
[![Backers][backers-badge]][collective]
[![Chat][chat-badge]][chat]

**[micromark][]** extension to support GitHub flavored markdown.
This extension matches either the [GFM][] spec or github.com (default).

This package provides the low-level modules for integrating with the micromark
tokenizer and the micromark HTML compiler.

## When to use this

If you’re using [`micromark`][micromark] or
[`mdast-util-from-markdown`][from-markdown], use this package.
Alternatively, if you’re using **[remark][]**, use [`remark-gfm`][remark-gfm].

If you don’t need all of GFM, the extensions can be used separately:

*   [`micromark/micromark-extension-gfm-autolink-literal`](https://github.com/micromark/micromark-extension-gfm-autolink-literal)
    — support GFM [autolink literals][]
*   [`micromark/micromark-extension-gfm-strikethrough`](https://github.com/micromark/micromark-extension-gfm-strikethrough)
    — support GFM [strikethrough][]
*   [`micromark/micromark-extension-gfm-table`](https://github.com/micromark/micromark-extension-gfm-table)
    — support GFM [tables][]
*   [`micromark/micromark-extension-gfm-tagfilter`](https://github.com/micromark/micromark-extension-gfm-tagfilter)
    — support GFM [tagfilter][]
*   [`micromark/micromark-extension-gfm-task-list-item`](https://github.com/micromark/micromark-extension-gfm-task-list-item)
    — support GFM [tasklists][]

## Install

This package is [ESM only](https://gist.github.com/sindresorhus/a39789f98801d908bbc7ff3ecc99d99c):
Node 12+ is needed to use it and it must be `import`ed instead of `require`d.

[npm][]:

```sh
npm install micromark-extension-gfm
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

## Tag filter

<plaintext>

## Tasklist

* [ ] to do
* [x] done
```

And our module, `example.js`, looks as follows:

```js
import fs from 'node:fs'
import {micromark} from 'micromark'
import {gfm, gfmHtml} from 'micromark-extension-gfm'

const output = micromark(fs.readFileSync('example.md'), {
  allowDangerousHtml: true,
  extensions: [gfm()],
  htmlExtensions: [gfmHtml]
})

console.log(output)
```

Now, running `node example` yields:

```html
<h1>GFM</h1>
<h2>Autolink literals</h2>
<p><a href="http://www.example.com">www.example.com</a>, <a href="https://example.com">https://example.com</a>, and <a href="mailto:contact@example.com">contact@example.com</a>.</p>
<h2>Strikethrough</h2>
<p><del>one</del> or <del>two</del> tildes.</p>
<h2>Table</h2>
<table>
<thead>
<tr>
<th>a</th>
<th align="left">b</th>
<th align="right">c</th>
<th align="center">d</th>
</tr>
</thead>
</table>
<h2>Tag filter</h2>
&lt;plaintext>
<h2>Tasklist</h2>
<ul>
<li><input disabled="" type="checkbox"> to do</li>
<li><input checked="" disabled="" type="checkbox"> done</li>
</ul>
```

## API

This package exports the following identifiers: `gfm`, `gfmHtml`.
There is no default export.

### `gfm(options?)`

### `gfmHtml`

Support [GFM][] or markdown on github.com.
`gfm` is a function that can be called with options and returns an extension for
micromark to parse GFM (can be passed in `extensions`).
`gfmHtml` is an extension for micromark to compile as elements (can be passed in
`htmlExtensions`).

##### `options`

###### `options.singleTilde`

Passed as [`singleTilde`][single-tilde] in
[`micromark-extension-gfm-strikethrough`][mm-strikethrough].

## Related

*   [`remarkjs/remark`][remark]
    — markdown processor powered by plugins
*   [`syntax-tree/mdast-util-gfm`](https://github.com/syntax-tree/mdast-util-gfm)
    — mdast utility to support GFM
*   [`syntax-tree/mdast-util-from-markdown`][from-markdown]
    — mdast parser using `micromark` to create mdast from markdown
*   [`syntax-tree/mdast-util-to-markdown`][to-markdown]
    — mdast serializer to create markdown from mdast
*   [`micromark/micromark`][micromark]
    — the smallest commonmark-compliant markdown parser that exists
*   [`micromark/micromark-extension-gfm-autolink-literal`](https://github.com/micromark/micromark-extension-gfm-autolink-literal)
    — support GFM [autolink literals][]
*   [`micromark/micromark-extension-gfm-strikethrough`](https://github.com/micromark/micromark-extension-gfm-strikethrough)
    — support GFM [strikethrough][]
*   [`micromark/micromark-extension-gfm-table`](https://github.com/micromark/micromark-extension-gfm-table)
    — support GFM [tables][]
*   [`micromark/micromark-extension-gfm-tagfilter`](https://github.com/micromark/micromark-extension-gfm-tagfilter)
    — support GFM [tagfilter][]
*   [`micromark/micromark-extension-gfm-task-list-item`](https://github.com/micromark/micromark-extension-gfm-task-list-item)
    — support GFM [tasklists][]

## Contribute

See [`contributing.md` in `micromark/.github`][contributing] for ways to get
started.
See [`support.md`][support] for ways to get help.

This project has a [code of conduct][coc].
By interacting with this repository, organization, or community you agree to
abide by its terms.

## License

[MIT][license] © [Titus Wormer][author]

<!-- Definitions -->

[build-badge]: https://github.com/micromark/micromark-extension-gfm/workflows/main/badge.svg

[build]: https://github.com/micromark/micromark-extension-gfm/actions

[coverage-badge]: https://img.shields.io/codecov/c/github/micromark/micromark-extension-gfm.svg

[coverage]: https://codecov.io/github/micromark/micromark-extension-gfm

[downloads-badge]: https://img.shields.io/npm/dm/micromark-extension-gfm.svg

[downloads]: https://www.npmjs.com/package/micromark-extension-gfm

[size-badge]: https://img.shields.io/bundlephobia/minzip/micromark-extension-gfm.svg

[size]: https://bundlephobia.com/result?p=micromark-extension-gfm

[sponsors-badge]: https://opencollective.com/unified/sponsors/badge.svg

[backers-badge]: https://opencollective.com/unified/backers/badge.svg

[collective]: https://opencollective.com/unified

[chat-badge]: https://img.shields.io/badge/chat-discussions-success.svg

[chat]: https://github.com/micromark/micromark/discussions

[npm]: https://docs.npmjs.com/cli/install

[license]: license

[author]: https://wooorm.com

[contributing]: https://github.com/micromark/.github/blob/HEAD/contributing.md

[support]: https://github.com/micromark/.github/blob/HEAD/support.md

[coc]: https://github.com/micromark/.github/blob/HEAD/code-of-conduct.md

[micromark]: https://github.com/micromark/micromark

[from-markdown]: https://github.com/syntax-tree/mdast-util-from-markdown

[to-markdown]: https://github.com/syntax-tree/mdast-util-to-markdown

[remark]: https://github.com/remarkjs/remark

[remark-gfm]: https://github.com/remarkjs/remark-gfm

[gfm]: https://github.github.com/gfm/

[strikethrough]: https://github.github.com/gfm/#strikethrough-extension-

[tables]: https://github.github.com/gfm/#tables-extension-

[tasklists]: https://github.github.com/gfm/#task-list-items-extension-

[autolink literals]: https://github.github.com/gfm/#autolinks-extension-

[tagfilter]: https://github.github.com/gfm/#disallowed-raw-html-extension-

[single-tilde]: https://github.com/micromark/micromark-extension-gfm-strikethrough#optionssingletilde

[mm-strikethrough]: https://github.com/micromark/micromark-extension-gfm-strikethrough
