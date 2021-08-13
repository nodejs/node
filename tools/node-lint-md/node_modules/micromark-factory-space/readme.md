# micromark-factory-space

[![Build][build-badge]][build]
[![Coverage][coverage-badge]][coverage]
[![Downloads][downloads-badge]][downloads]
[![Size][bundle-size-badge]][bundle-size]
[![Sponsors][sponsors-badge]][opencollective]
[![Backers][backers-badge]][opencollective]
[![Chat][chat-badge]][chat]

micromark factory to parse [markdown space][markdown-space] (found in lots of
places).

## Contents

*   [Install](#install)
*   [Use](#use)
*   [API](#api)
    *   [`factorySpace(…)`](#factoryspace)
*   [Security](#security)
*   [Contribute](#contribute)
*   [License](#license)

## Install

[npm][]:

```sh
npm install micromark-factory-space
```

## Use

```js
import {factorySpace} from 'micromark-factory-space'
import {codes} from 'micromark-util-symbol/codes'
import {types} from 'micromark-util-symbol/types'

// A micromark tokenizer that uses the factory:
/** @type {Tokenizer} */
function tokenizeCodeFenced(effects, ok, nok) {
  return start

  // …

  /** @type {State} */
  function info(code) {
    if (code === codes.eof || markdownLineEndingOrSpace(code)) {
      effects.exit(types.chunkString)
      effects.exit(types.codeFencedFenceInfo)
      return factorySpace(effects, infoAfter, types.whitespace)(code)
    }

    if (code === codes.graveAccent && code === marker) return nok(code)
    effects.consume(code)
    return info
  }

  // …
}
```

## API

This module exports the following identifiers: `factorySpace`.
There is no default export.

### `factorySpace(…)`

Note that there is no `nok` parameter:

*   spaces in markdown are often optional, in which case this factory can be
    used and `ok` will be switched to whether spaces were found or not,
*   One space character can be detected with
    [markdownSpace(code)][markdown-space] right before using `factorySpace`

###### Parameters

*   `effects` (`Effects`) — Context
*   `ok` (`State`) — State switched to when successful
*   `type` (`string`) — Token type for whole (`' \t'`)
*   `max` (`number`, default: `Infinity`) — Max size of whitespace

###### Returns

`State`.

###### Examples

Where `␉` represents a tab (plus how much it expands) and `␠` represents a
single space.

```markdown
␉
␠␠␠␠
␉␠
```

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

[downloads-badge]: https://img.shields.io/npm/dm/micromark-factory-space.svg

[downloads]: https://www.npmjs.com/package/micromark-factory-space

[bundle-size-badge]: https://img.shields.io/bundlephobia/minzip/micromark-factory-space.svg

[bundle-size]: https://bundlephobia.com/result?p=micromark-factory-space

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

[markdown-space]: https://github.com/micromark/micromark/tree/main/packages/micromark-util-character#markdownspacecode
