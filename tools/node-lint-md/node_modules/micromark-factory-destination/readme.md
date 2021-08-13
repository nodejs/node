# micromark-factory-destination

[![Build][build-badge]][build]
[![Coverage][coverage-badge]][coverage]
[![Downloads][downloads-badge]][downloads]
[![Size][bundle-size-badge]][bundle-size]
[![Sponsors][sponsors-badge]][opencollective]
[![Backers][backers-badge]][opencollective]
[![Chat][chat-badge]][chat]

micromark factory to parse destinations (found in resources, definitions).

## Contents

*   [Install](#install)
*   [Use](#use)
*   [API](#api)
    *   [`factoryDestination(…)`](#factorydestination)
*   [Security](#security)
*   [Contribute](#contribute)
*   [License](#license)

## Install

[npm][]:

```sh
npm install micromark-factory-destination
```

## Use

```js
import {factoryDestination} from 'micromark-factory-destination'
import {codes} from 'micromark-util-symbol/codes'
import {types} from 'micromark-util-symbol/types'

// A micromark tokenizer that uses the factory:
/** @type {Tokenizer} */
function tokenizeResource(effects, ok, nok) {
  return start

  // …

  /** @type {State} */
  function open(code) {
    if (code === codes.rightParenthesis) {
      return end(code)
    }

    return factoryDestination(
      effects,
      destinationAfter,
      nok,
      types.resourceDestination,
      types.resourceDestinationLiteral,
      types.resourceDestinationLiteralMarker,
      types.resourceDestinationRaw,
      types.resourceDestinationString,
      constants.linkResourceDestinationBalanceMax
    )(code)
  }

  // …
}
```

## API

This module exports the following identifiers: `factoryDestination`.
There is no default export.

### `factoryDestination(…)`

###### Parameters

*   `effects` (`Effects`) — Context
*   `ok` (`State`) — State switched to when successful
*   `nok` (`State`) — State switched to when not successful
*   `type` (`string`) — Token type for whole (`<a>` or `b`)
*   `literalType` (`string`) — Token type when enclosed (`<a>`)
*   `literalMarkerType` (`string`) — Token type for enclosing (`<` and `>`)
*   `rawType` (`string`) — Token type when not enclosed (`b`)
*   `stringType` (`string`) — Token type for the URI (`a` or `b`)
*   `max` (`number`, default: `Infinity`) — Max depth of nested parens

###### Returns

`State`.

###### Examples

```markdown
<a>
<a\>b>
<a b>
<a)>
a
a\)b
a(b)c
a(b)
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

[downloads-badge]: https://img.shields.io/npm/dm/micromark-factory-destination.svg

[downloads]: https://www.npmjs.com/package/micromark-factory-destination

[bundle-size-badge]: https://img.shields.io/bundlephobia/minzip/micromark-factory-destination.svg

[bundle-size]: https://bundlephobia.com/result?p=micromark-factory-destination

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
