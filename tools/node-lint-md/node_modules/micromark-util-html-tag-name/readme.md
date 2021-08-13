# micromark-util-html-tag-name

[![Build][build-badge]][build]
[![Coverage][coverage-badge]][coverage]
[![Downloads][downloads-badge]][downloads]
[![Size][bundle-size-badge]][bundle-size]
[![Sponsors][sponsors-badge]][opencollective]
[![Backers][backers-badge]][opencollective]
[![Chat][chat-badge]][chat]

micromark utility with list of html tag names.

## Contents

*   [Install](#install)
*   [Use](#use)
*   [API](#api)
    *   [`htmlBlockNames`](#htmlblocknames)
    *   [`htmlRawNames`](#htmlrawnames)
*   [Security](#security)
*   [Contribute](#contribute)
*   [License](#license)

## Install

[npm][]:

```sh
npm install micromark-util-html-tag-name
```

## Use

```js
import {htmlBlockNames, htmlRawNames} from 'micromark-util-html-tag-name'

console.log(htmlBlockNames) // ['address', 'article', …]
console.log(htmlRawNames) // ['pre', 'script', …]
```

## API

This module exports the following identifiers: `htmlBlockNames`,
`htmlRawNames`.
There is no default export.

### `htmlBlockNames`

List of lowercase HTML tag names (`string[]`) which when parsing HTML (flow),
result in more relaxed rules (condition 6): because they are known blocks, the
HTML-like syntax doesn’t have to be strictly parsed.
For tag names not in this list, a more strict algorithm (condition 7) is used
to detect whether the HTML-like syntax is seen as HTML (flow) or not.

This is copied from: <https://spec.commonmark.org/0.29/#html-blocks>.

### `htmlRawNames`

List of lowercase HTML tag names (`string[]`) which when parsing HTML (flow),
result in HTML that can include lines w/o exiting, until a closing tag also in
this list is found (condition 1).

This is copied from:
<https://spec.commonmark.org/0.29/#html-blocks>.

Note that `textarea` is not available in `CommonMark@0.29` but has been merged
to the primary branch and is slated to be released in the next release of
CommonMark.

## Security

See [`security.md`][securitymd] in [`micromark/.github`][health] for how to
submit a security report.

## Contribute

See [`contributing.md`][contributing] in [`micromark/.github`][health] for ways
to get started.
See [`support.md`][support] for ways to get help.

This project has a [code of conduct][coc].
By interacting with this repository, organisation, or community you agree to
abide by its terms.

## License

[MIT][license] © [Titus Wormer][author]

<!-- Definitions -->

[build-badge]: https://github.com/micromark/micromark/workflows/main/badge.svg

[build]: https://github.com/micromark/micromark/actions

[coverage-badge]: https://img.shields.io/codecov/c/github/micromark/micromark.svg

[coverage]: https://codecov.io/github/micromark/micromark

[downloads-badge]: https://img.shields.io/npm/dm/micromark-util-html-tag-name.svg

[downloads]: https://www.npmjs.com/package/micromark-util-html-tag-name

[bundle-size-badge]: https://img.shields.io/bundlephobia/minzip/micromark-util-html-tag-name.svg

[bundle-size]: https://bundlephobia.com/result?p=micromark-util-html-tag-name

[sponsors-badge]: https://opencollective.com/unified/sponsors/badge.svg

[backers-badge]: https://opencollective.com/unified/backers/badge.svg

[opencollective]: https://opencollective.com/unified

[npm]: https://docs.npmjs.com/cli/install

[chat-badge]: https://img.shields.io/badge/chat-discussions-success.svg

[chat]: https://github.com/micromark/micromark/discussions

[license]: https://github.com/micromark/micromark/blob/main/license

[author]: https://wooorm.com

[health]: https://github.com/micromark/.github

[securitymd]: https://github.com/micromark/.github/blob/HEAD/security.md

[contributing]: https://github.com/micromark/.github/blob/HEAD/contributing.md

[support]: https://github.com/micromark/.github/blob/HEAD/support.md

[coc]: https://github.com/micromark/.github/blob/HEAD/code-of-conduct.md
