# mdast-util-heading-style

[![Build][build-badge]][build]
[![Coverage][coverage-badge]][coverage]
[![Downloads][downloads-badge]][downloads]
[![Size][size-badge]][size]
[![Sponsors][sponsors-badge]][collective]
[![Backers][backers-badge]][collective]
[![Chat][chat-badge]][chat]

[**mdast**][mdast] utility to get the style of a heading.

## Install

This package is [ESM only](https://gist.github.com/sindresorhus/a39789f98801d908bbc7ff3ecc99d99c):
Node 12+ is needed to use it and it must be `import`ed instead of `require`d.

[npm][]:

```sh
npm install mdast-util-heading-style
```

## Use

```js
import unified from 'unified'
import remarkParse from 'remark-parse'
import {headingStyle} from 'mdast-util-heading-style'

var processor = unified().use(remarkParse)

headingStyle(processor.parse('# ATX').children[0]) // => 'atx'
headingStyle(processor.parse('# ATX #\n').children[0]) // => 'atx-closed'
headingStyle(processor.parse('ATX\n===').children[0]) // => 'setext'

headingStyle(processor.parse('### ATX').children[0]) // => null
headingStyle(processor.parse('### ATX').children[0], 'setext') // => 'setext'
```

## API

This package exports the following identifiers: `headingStyle`.
There is no default export.

### `headingStyle(node[, relative])`

Get the heading style of a node.

###### Parameters

*   `node` ([`Node`][node]) — Node to parse
*   `relative` (`string`, optional) — Style to use for ambiguous headings
    (atx-headings with a level of three or more could also be setext)

###### Returns

`string` (`'atx'`, `'atx-closed'`, or `'setext'`) — When an ambiguous
heading is found, either `relative` or `null` is returned.

## Security

Use of `mdast-util-heading-style` does not involve [**hast**][hast] so there are
no openings for [cross-site scripting (XSS)][xss] attacks.

## Related

*   [`mdast-normalize-headings`](https://github.com/syntax-tree/mdast-normalize-headings)
    — make sure there is no more than a single top-level heading
*   [`mdast-util-heading-range`](https://github.com/syntax-tree/mdast-util-heading-range)
    — use headings as ranges

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

[build-badge]: https://github.com/syntax-tree/mdast-util-heading-style/workflows/main/badge.svg

[build]: https://github.com/syntax-tree/mdast-util-heading-style/actions

[coverage-badge]: https://img.shields.io/codecov/c/github/syntax-tree/mdast-util-heading-style.svg

[coverage]: https://codecov.io/github/syntax-tree/mdast-util-heading-style

[downloads-badge]: https://img.shields.io/npm/dm/mdast-util-heading-style.svg

[downloads]: https://www.npmjs.com/package/mdast-util-heading-style

[size-badge]: https://img.shields.io/bundlephobia/minzip/mdast-util-heading-style.svg

[size]: https://bundlephobia.com/result?p=mdast-util-heading-style

[sponsors-badge]: https://opencollective.com/unified/sponsors/badge.svg

[backers-badge]: https://opencollective.com/unified/backers/badge.svg

[collective]: https://opencollective.com/unified

[chat-badge]: https://img.shields.io/badge/chat-discussions-success.svg

[chat]: https://github.com/syntax-tree/unist/discussions

[license]: license

[author]: https://wooorm.com

[npm]: https://docs.npmjs.com/cli/install

[contributing]: https://github.com/syntax-tree/.github/blob/HEAD/contributing.md

[support]: https://github.com/syntax-tree/.github/blob/HEAD/support.md

[coc]: https://github.com/syntax-tree/.github/blob/HEAD/code-of-conduct.md

[mdast]: https://github.com/syntax-tree/mdast

[node]: https://github.com/syntax-tree/unist#node

[xss]: https://en.wikipedia.org/wiki/Cross-site_scripting

[hast]: https://github.com/syntax-tree/hast
