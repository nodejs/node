# micromark-util-encode

[![Build][build-badge]][build]
[![Coverage][coverage-badge]][coverage]
[![Downloads][downloads-badge]][downloads]
[![Size][bundle-size-badge]][bundle-size]
[![Sponsors][sponsors-badge]][opencollective]
[![Backers][backers-badge]][opencollective]
[![Chat][chat-badge]][chat]

micromark utility to encode dangerous html characters.

## Contents

*   [Install](#install)
*   [Use](#use)
*   [API](#api)
    *   [`encode(value)`](#encodevalue)
*   [Security](#security)
*   [Contribute](#contribute)
*   [License](#license)

## Install

[npm][]:

```sh
npm install micromark-util-encode
```

## Use

```js
import {encode} from 'micromark-util-encode'

encode('<3') // '&lt;3'
```

## API

This module exports the following identifiers: `encode`.
There is no default export.

### `encode(value)`

Encode only the dangerous HTML characters.

This ensures that certain characters which have special meaning in HTML are
dealt with.
Technically, we can skip `>` and `"` in many cases, but CM includes them.

###### Parameters

*   `value` (`string`) — Value to encode.

###### Returns

`string` — Encoded value.

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

[downloads-badge]: https://img.shields.io/npm/dm/micromark-util-encode.svg

[downloads]: https://www.npmjs.com/package/micromark-util-encode

[bundle-size-badge]: https://img.shields.io/bundlephobia/minzip/micromark-util-encode.svg

[bundle-size]: https://bundlephobia.com/result?p=micromark-util-encode

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
