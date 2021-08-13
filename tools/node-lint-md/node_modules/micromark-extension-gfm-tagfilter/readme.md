# micromark-extension-gfm-tagfilter

[![Build][build-badge]][build]
[![Coverage][coverage-badge]][coverage]
[![Downloads][downloads-badge]][downloads]
[![Size][size-badge]][size]
[![Sponsors][sponsors-badge]][collective]
[![Backers][backers-badge]][collective]
[![Chat][chat-badge]][chat]

**[micromark][]** extension to support GitHub flavored markdown [tag filter][].
This extension matches the GFM spec and github.com.
The [tag filter][] is a rather naïve attempt at XSS protection.
It’s much better to use a proper HTML sanitizing algorithm.

## When to use this

You should probably use [`micromark-extension-gfm`][micromark-extension-gfm],
which combines this package with other GFM features, instead.
If for some weird reason you *have* to match GHs tagfilter, but not all the
other GFM parts, use this package.

## Install

This package is [ESM only](https://gist.github.com/sindresorhus/a39789f98801d908bbc7ff3ecc99d99c):
Node 12+ is needed to use it and it must be `import`ed instead of `require`d.

[npm][]:

```sh
npm install micromark-extension-gfm-tagfilter
```

## Use

```js
import {micromark} from 'micromark'
import {gfmTagfilterHtml} from 'micromark-extension-gfm-tagfilter'

const output = micromark('XSS! <script>alert(1)</script>', {
  allowDangerousHtml: true,
  htmlExtensions: [gfmTagfilterHtml]
})

console.log(output)
```

Yields:

```html
<p>XSS! &lt;script>alert(1)&lt;/script></p>
```

## API

This package exports the following identifiers: `gfmTagfilterHtml`.
There is no default export.

### `gfmTagfilterHtml`

Support a [tag filter][] (protection against script, plaintext, etc).
The export is an extension for the micromark compiler to escape certain tag
names (can be passed in `htmlExtensions`).

## Related

*   [`remarkjs/remark`][remark]
    — markdown processor powered by plugins
*   [`micromark/micromark`][micromark]
    — the smallest commonmark-compliant markdown parser that exists
*   [`micromark/micromark-extension-gfm`][micromark-extension-gfm]
    — micromark extension combining this with other GFM features
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

[build-badge]: https://github.com/micromark/micromark-extension-gfm-tagfilter/workflows/main/badge.svg

[build]: https://github.com/micromark/micromark-extension-gfm-tagfilter/actions

[coverage-badge]: https://img.shields.io/codecov/c/github/micromark/micromark-extension-gfm-tagfilter.svg

[coverage]: https://codecov.io/github/micromark/micromark-extension-gfm-tagfilter

[downloads-badge]: https://img.shields.io/npm/dm/micromark-extension-gfm-tagfilter.svg

[downloads]: https://www.npmjs.com/package/micromark-extension-gfm-tagfilter

[size-badge]: https://img.shields.io/bundlephobia/minzip/micromark-extension-gfm-tagfilter.svg

[size]: https://bundlephobia.com/result?p=micromark-extension-gfm-tagfilter

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

[tag filter]: https://github.github.com/gfm/#disallowed-raw-html-extension-

[micromark-extension-gfm]: https://github.com/micromark/micromark-extension-gfm
