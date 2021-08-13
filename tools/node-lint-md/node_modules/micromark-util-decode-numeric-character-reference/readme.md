# micromark-util-decode-numeric-character-reference

[![Build][build-badge]][build]
[![Coverage][coverage-badge]][coverage]
[![Downloads][downloads-badge]][downloads]
[![Size][bundle-size-badge]][bundle-size]
[![Sponsors][sponsors-badge]][opencollective]
[![Backers][backers-badge]][opencollective]
[![Chat][chat-badge]][chat]

micromark utility to decode numeric character references.

## Contents

*   [Install](#install)
*   [Use](#use)
*   [API](#api)
    *   [`decodeNumericCharacterReference(value)`](#decodenumericcharacterreferencevalue)
*   [Security](#security)
*   [Contribute](#contribute)
*   [License](#license)

## Install

[npm][]:

```sh
npm install micromark-util-decode-numeric-character-reference
```

## Use

```js
import {decodeNumericCharacterReference} from 'micromark-util-decode-numeric-character-reference'

decodeNumericCharacterReference('41', 16) // 'A'
decodeNumericCharacterReference('65', 10) // 'A'
decodeNumericCharacterReference('A', 16) // '\n'
decodeNumericCharacterReference('7F', 16) // '�' - Control
decodeNumericCharacterReference('110000', 16) // '�' - Out of range
```

## API

This module exports the following identifiers:
`decodeNumericCharacterReference`.
There is no default export.

### `decodeNumericCharacterReference(value)`

Sort of like `String.fromCharCode(Number.parseInt(value, base))`,
but makes non-characters and control characters safe.

###### Parameters

*   `value` (`string`) — Value to decode.
*   `base` (`number`, probably `10` or `16`) — Numeric base.

###### Returns

`string` — Character code.

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
