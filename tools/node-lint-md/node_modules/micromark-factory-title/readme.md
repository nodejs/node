# micromark-factory-title

[![Build][build-badge]][build]
[![Coverage][coverage-badge]][coverage]
[![Downloads][downloads-badge]][downloads]
[![Size][bundle-size-badge]][bundle-size]
[![Sponsors][sponsors-badge]][opencollective]
[![Backers][backers-badge]][opencollective]
[![Chat][chat-badge]][chat]

micromark factory to parse markdown titles (found in resources, definitions).

## Contents

*   [Install](#install)
*   [Use](#use)
*   [API](#api)
    *   [`factoryTitle(…)`](#factorytitle)
*   [Security](#security)
*   [Contribute](#contribute)
*   [License](#license)

## Install

[npm][]:

```sh
npm install micromark-factory-title
```

## Use

```js
import {factoryTitle} from 'micromark-factory-title'
import {codes} from 'micromark-util-symbol/codes'
import {types} from 'micromark-util-symbol/types'

// A micromark tokenizer that uses the factory:
/** @type {Tokenizer} */
function tokenizeDefinition(effects, ok, nok) {
  return start

  // …

  /** @type {State} */
  function before(code) {
    if (
      code === codes.quotationMark ||
      code === codes.apostrophe ||
      code === codes.leftParenthesis
    ) {
      return factoryTitle(
        effects,
        factorySpace(effects, after, types.whitespace),
        nok,
        types.definitionTitle,
        types.definitionTitleMarker,
        types.definitionTitleString
      )(code)
    }

    return nok(code)
  }

  // …
}
```

## API

This module exports the following identifiers: `factoryTitle`.
There is no default export.

### `factoryTitle(…)`

###### Parameters

*   `effects` (`Effects`) — Context
*   `ok` (`State`) — State switched to when successful
*   `nok` (`State`) — State switched to when not successful
*   `type` (`string`) — Token type for whole (`"a"`, `'b'`, `(c)`)
*   `markerType` (`string`) — Token type for the markers (`"`, `'`, `(`, and
    `)`)
*   `stringType` (`string`) — Token type for the value (`a`)

###### Returns

`State`.

###### Examples

```markdown
"a"
'b'
(c)
"a
b"
'a
    b'
(a\)b)
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

[downloads-badge]: https://img.shields.io/npm/dm/micromark-factory-title.svg

[downloads]: https://www.npmjs.com/package/micromark-factory-title

[bundle-size-badge]: https://img.shields.io/bundlephobia/minzip/micromark-factory-title.svg

[bundle-size]: https://bundlephobia.com/result?p=micromark-factory-title

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
