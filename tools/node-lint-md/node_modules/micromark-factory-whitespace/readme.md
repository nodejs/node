# micromark-factory-whitespace

[![Build][build-badge]][build]
[![Coverage][coverage-badge]][coverage]
[![Downloads][downloads-badge]][downloads]
[![Size][bundle-size-badge]][bundle-size]
[![Sponsors][sponsors-badge]][opencollective]
[![Backers][backers-badge]][opencollective]
[![Chat][chat-badge]][chat]

micromark factory to parse [markdown line endings or spaces][ws] (found in lots
of places).

## Contents

*   [Install](#install)
*   [Use](#use)
*   [API](#api)
    *   [`factoryWhitespace(…)`](#factorywhitespace)
*   [Security](#security)
*   [Contribute](#contribute)
*   [License](#license)

## Install

[npm][]:

```sh
npm install micromark-factory-whitespace
```

## Use

```js
import {factoryWhitespace} from 'micromark-factory-whitespace'
import {codes} from 'micromark-util-symbol/codes'
import {types} from 'micromark-util-symbol/types'

// A micromark tokenizer that uses the factory:
/** @type {Tokenizer} */
function tokenizeTitle(effects, ok, nok) {
  return start

  /** @type {State} */
  function start(code) {
    return markdownLineEndingOrSpace(code)
      ? factoryWhitespace(effects, before)(code)
      : nok(code)
  }

  // …
}
```

## API

This module exports the following identifiers: `factoryWhitespace`.
There is no default export.

### `factoryWhitespace(…)`

Note that there is no `nok` parameter:

*   line endings or spaces in markdown are often optional, in which case this
    factory can be used and `ok` will be switched to whether spaces were found
    or not,
*   One line ending or space can be detected with
    [markdownLineEndingOrSpace(code)][ws] right before using `factoryWhitespace`

###### Parameters

*   `effects` (`Effects`) — Context
*   `ok` (`State`) — State switched to when successful

###### Returns

`State`.

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

[downloads-badge]: https://img.shields.io/npm/dm/micromark-factory-whitespace.svg

[downloads]: https://www.npmjs.com/package/micromark-factory-whitespace

[bundle-size-badge]: https://img.shields.io/bundlephobia/minzip/micromark-factory-whitespace.svg

[bundle-size]: https://bundlephobia.com/result?p=micromark-factory-whitespace

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

[ws]: https://github.com/micromark/micromark/tree/main/packages/micromark-util-character#markdownlineendingorspacecode
