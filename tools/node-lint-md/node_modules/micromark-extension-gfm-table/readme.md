# micromark-extension-gfm-table

[![Build][build-badge]][build]
[![Coverage][coverage-badge]][coverage]
[![Downloads][downloads-badge]][downloads]
[![Size][size-badge]][size]
[![Sponsors][sponsors-badge]][collective]
[![Backers][backers-badge]][collective]
[![Chat][chat-badge]][chat]

**[micromark][]** extension to support GitHub flavored markdown (GFM)
[tables][].
This syntax extension matches the GFM spec and github.com.

## When to use this

You should probably use [`micromark-extension-gfm`][micromark-extension-gfm]
instead, which combines this package with other GFM features.
Alternatively, if you don’t want all of GFM, use this package.

## Install

This package is [ESM only](https://gist.github.com/sindresorhus/a39789f98801d908bbc7ff3ecc99d99c):
Node 12+ is needed to use it and it must be `import`ed instead of `require`d.

[npm][]:

```sh
npm install micromark-extension-gfm-table
```

## Use

```js
import {micromark} from 'micromark'
import {gfmTable, gfmTableHtml} from 'micromark-extension-gfm-table'

const output = micromark('| a |\n| - |', {
  extensions: [gfmTable],
  htmlExtensions: [gfmTableHtml]
})

console.log(output)
```

Yields:

```html
<table>
<thead>
<tr>
<th>a</th>
</tr>
</thead>
</table>
```

## API

This package exports the following identifiers: `gfmTable`, `gfmTableHtml`.
There is no default export.

The export map supports the endorsed
[`development` condition](https://nodejs.org/api/packages.html#packages_resolving_user_conditions).
Run `node --conditions development module.js` to get instrumented dev code.
Without this condition, production code is loaded.

### `gfmTable`

### `gfmTableHtml`

An extension for micromark to parse tables (can be passed in
`extensions`) and one to compile to `<table>` elements (can be passed in
`htmlExtensions`).

## Related

*   [`remarkjs/remark`][remark]
    — markdown processor powered by plugins
*   [`remarkjs/remark-gfm`](https://github.com/remarkjs/remark-gfm)
    — remark plugin using this and other GFM features
*   [`micromark/micromark`][micromark]
    — the smallest commonmark-compliant markdown parser that exists
*   [`micromark/micromark-extension-gfm`][micromark-extension-gfm]
    — micromark extension combining this with other GFM features
*   [`syntax-tree/mdast-util-gfm-table`](https://github.com/syntax-tree/mdast-util-gfm-table)
    — mdast utility to support tables
*   [`syntax-tree/mdast-util-gfm`](https://github.com/syntax-tree/mdast-util-gfm)
    — mdast utility to support GFM
*   [`syntax-tree/mdast-util-from-markdown`][from-markdown]
    — mdast parser using `micromark` to create mdast from markdown
*   [`syntax-tree/mdast-util-to-markdown`][to-markdown]
    — mdast serializer to create markdown from mdast

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

[build-badge]: https://github.com/micromark/micromark-extension-gfm-table/workflows/main/badge.svg

[build]: https://github.com/micromark/micromark-extension-gfm-table/actions

[coverage-badge]: https://img.shields.io/codecov/c/github/micromark/micromark-extension-gfm-table.svg

[coverage]: https://codecov.io/github/micromark/micromark-extension-gfm-table

[downloads-badge]: https://img.shields.io/npm/dm/micromark-extension-gfm-table.svg

[downloads]: https://www.npmjs.com/package/micromark-extension-gfm-table

[size-badge]: https://img.shields.io/bundlephobia/minzip/micromark-extension-gfm-table.svg

[size]: https://bundlephobia.com/result?p=micromark-extension-gfm-table

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

[tables]: https://github.github.com/gfm/#tables-extension-

[micromark-extension-gfm]: https://github.com/micromark/micromark-extension-gfm
