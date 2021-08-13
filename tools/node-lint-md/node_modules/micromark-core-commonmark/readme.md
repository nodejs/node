# micromark-core-commonmark

[![Build][build-badge]][build]
[![Coverage][coverage-badge]][coverage]
[![Downloads][downloads-badge]][downloads]
[![Size][bundle-size-badge]][bundle-size]
[![Sponsors][sponsors-badge]][opencollective]
[![Backers][backers-badge]][opencollective]
[![Chat][chat-badge]][chat]

The core CommonMark constructs needed to tokenize markdown.
Some of these can be [turned off][disable], but they are often essential to
markdown and weird things might happen.

## Contents

*   [Install](#install)
*   [Use](#use)
*   [API](#api)
*   [Security](#security)
*   [Contribute](#contribute)
*   [License](#license)

## Install

[npm][]:

```sh
npm install micromark-core-commonmark
```

## Use

```js
import {autolink} from 'micromark-core-commonmark'

console.log(autolink) // Do things with `autolink`.
```

## API

This module exports the following identifiers: `attention`, `autolink`,
`blankLine`, `blockQuote`, `characterEscape`, `characterReference`,
`codeFenced`, `codeIndented`, `codeText`, `content`, `definition`,
`hardBreakEscape`, `headingAtx`, `htmlFlow`, `htmlText`, `labelEnd`,
`labelStartImage`, `labelStartLink`, `lineEnding`, `list`, `setextUnderline`,
`thematicBreak`.
There is no default export.

Each identifier refers to a [construct](https://github.com/micromark/micromark#constructs).

See the code for more on the exported constructs.

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

[downloads-badge]: https://img.shields.io/npm/dm/micromark-core-commonmark.svg

[downloads]: https://www.npmjs.com/package/micromark-core-commonmark

[bundle-size-badge]: https://img.shields.io/bundlephobia/minzip/micromark-core-commonmark.svg

[bundle-size]: https://bundlephobia.com/result?p=micromark-core-commonmark

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

[disable]: https://github.com/micromark/micromark#case-turn-off-constructs
