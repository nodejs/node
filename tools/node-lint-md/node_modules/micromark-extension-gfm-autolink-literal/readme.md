# micromark-extension-gfm-autolink-literal

[![Build][build-badge]][build]
[![Coverage][coverage-badge]][coverage]
[![Downloads][downloads-badge]][downloads]
[![Size][size-badge]][size]
[![Sponsors][sponsors-badge]][collective]
[![Backers][backers-badge]][collective]
[![Chat][chat-badge]][chat]

**[micromark][]** extension to support GitHub flavored markdown (GFM) [literal
autolinks][].

This syntax extension matches the GFM spec and how literal autolinks work
in several places on github.com.
GitHub employs two algorithms to autolink: one at parse time and one at
transform time (similar to how @mentions are done at transform time).
This difference can be observed because character references and escapes are
handled differently.
But also because issues/PRs/comments omit (perhaps by accident?) the second
algorithm for `www.`, `http://`, and `https://` links (but not for email links).

As this is a syntax extension, it focuses on the first algorithm.
The `html` part of this extension does not operate on an AST and hence can’t
perform the second algorithm.
[`mdast-util-gfm-autolink-literal`][mdast-util-gfm-autolink-literal] adds
support for the second.

## When to use this

You should probably use [`micromark-extension-gfm`][micromark-extension-gfm],
which combines this package with other GFM features, instead.
Alternatively, if you don’t want all of GFM, use this package.

## Install

This package is [ESM only](https://gist.github.com/sindresorhus/a39789f98801d908bbc7ff3ecc99d99c):
Node 12+ is needed to use it and it must be `import`ed instead of `require`d.

[npm][]:

```sh
npm install micromark-extension-gfm-autolink-literal
```

## Use

```js
import {micromark} from 'micromark'
import {
  gfmAutolinkLiteral,
  gfmAutolinkLiteralHtml
} from 'micromark-extension-gfm-autolink-literal'

const output = micromark('Just a URL: www.example.com.', {
  extensions: [gfmAutolinkLiteral],
  htmlExtensions: [gfmAutolinkLiteralHtml]
})

console.log(output)
```

Yields:

```html
<p>Just a URL: <a href="http://www.example.com">www.example.com</a>.</p>
```

## API

This package exports the following identifiers: `gfmAutolinkLiteral`,
`gfmAutolinkLiteralHtml`.
There is no default export.

The export map supports the endorsed
[`development` condition](https://nodejs.org/api/packages.html#packages_resolving_user_conditions).
Run `node --conditions development module.js` to get instrumented dev code.
Without this condition, production code is loaded.

### `gfmAutolinkLiteral`

### `gfmAutolinkLiteralHtml`

An extension for the micromark parser (to parse; can be passed in
`extensions`) and one for the HTML compiler (to compile as `<a>` elements; can
be passed in `htmlExtensions`).

## Related

*   [`remarkjs/remark`][remark]
    — markdown processor powered by plugins
*   [`remarkjs/remark-gfm`](https://github.com/remarkjs/remark-gfm)
    — remark plugin using this and other GFM features
*   [`micromark/micromark`][micromark]
    — the smallest commonmark-compliant markdown parser that exists
*   [`micromark/micromark-extension-gfm`][micromark-extension-gfm]
    — micromark extension combining this with other GFM features
*   [`syntax-tree/mdast-util-gfm-autolink-literal`](https://github.com/syntax-tree/mdast-util-gfm-autolink-literal)
    — mdast utility to support autolink literals
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

[build-badge]: https://github.com/micromark/micromark-extension-gfm-autolink-literal/workflows/main/badge.svg

[build]: https://github.com/micromark/micromark-extension-gfm-autolink-literal/actions

[coverage-badge]: https://img.shields.io/codecov/c/github/micromark/micromark-extension-gfm-autolink-literal.svg

[coverage]: https://codecov.io/github/micromark/micromark-extension-gfm-autolink-literal

[downloads-badge]: https://img.shields.io/npm/dm/micromark-extension-gfm-autolink-literal.svg

[downloads]: https://www.npmjs.com/package/micromark-extension-gfm-autolink-literal

[size-badge]: https://img.shields.io/bundlephobia/minzip/micromark-extension-gfm-autolink-literal.svg

[size]: https://bundlephobia.com/result?p=micromark-extension-gfm-autolink-literal

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

[mdast-util-gfm-autolink-literal]: https://github.com/syntax-tree/mdast-util-gfm-autolink-literal

[literal autolinks]: https://github.github.com/gfm/#autolinks-extension-

[micromark-extension-gfm]: https://github.com/micromark/micromark-extension-gfm
