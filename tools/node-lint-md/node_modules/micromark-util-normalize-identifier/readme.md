# micromark-util-normalize-identifier

[![Build][build-badge]][build]
[![Coverage][coverage-badge]][coverage]
[![Downloads][downloads-badge]][downloads]
[![Size][bundle-size-badge]][bundle-size]
[![Sponsors][sponsors-badge]][opencollective]
[![Backers][backers-badge]][opencollective]
[![Chat][chat-badge]][chat]

micromark utility normalize identifiers (as found in references, definitions).

## Contents

*   [Install](#install)
*   [Use](#use)
*   [API](#api)
    *   [`normalizeIdentifier(value)`](#normalizeidentifiervalue)
*   [Security](#security)
*   [Contribute](#contribute)
*   [License](#license)

## Install

[npm][]:

```sh
npm install micromark-util-normalize-identifier
```

## Use

```js
import {normalizeIdentifier} from 'micromark-util-normalize-identifier'

normalizeIdentifier(' a ') // 'A'
normalizeIdentifier('a\t\r\nb') // 'A B'
normalizeIdentifier('ТОЛПОЙ') // 'ТОЛПОЙ'
normalizeIdentifier('Толпой') // 'ТОЛПОЙ'
```

## API

This module exports the following identifiers: `normalizeIdentifier`.
There is no default export.

### `normalizeIdentifier(value)`

Normalize an identifier (such as used in definitions).
Collapse Markdown whitespace, trim, and then lower- and uppercase.

Some characters are considered “uppercase”, such as U+03F4 (`ϴ`), but if their
lowercase counterpart (U+03B8 (`θ`)) is uppercased will result in a different
uppercase character (U+0398 (`Θ`)).
Hence, to get that form, we perform both lower- and uppercase.

Using uppercase last makes sure keys will not interact with default prototypal
methods: no method is uppercase.

###### Parameters

*   `value` (`string`) — Identifier to normalize.

###### Returns

`string` — Normalized value.

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

[downloads-badge]: https://img.shields.io/npm/dm/micromark-util-normalize-identifier.svg

[downloads]: https://www.npmjs.com/package/micromark-util-normalize-identifier

[bundle-size-badge]: https://img.shields.io/bundlephobia/minzip/micromark-util-normalize-identifier.svg

[bundle-size]: https://bundlephobia.com/result?p=micromark-util-normalize-identifier

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
