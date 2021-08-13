# micromark-util-symbol

[![Build][build-badge]][build]
[![Coverage][coverage-badge]][coverage]
[![Downloads][downloads-badge]][downloads]
[![Size][bundle-size-badge]][bundle-size]
[![Sponsors][sponsors-badge]][opencollective]
[![Backers][backers-badge]][opencollective]
[![Chat][chat-badge]][chat]

micromark utility with symbols.

It’s useful to reference these by name instead of value while developing.
[`micromark-build`][micromark-build] compiles them away for production code.

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
npm install micromark-util-symbol
```

## Use

```js
import {codes} from 'micromark-util-symbol/codes'
import {constants} from 'micromark-util-symbol/constants'
import {types} from 'micromark-util-symbol/types'
import {values} from 'micromark-util-symbol/values'

console.log(codes.atSign) // 64
console.log(constants.characterReferenceNamedSizeMax) // 31
console.log(types.definitionDestinationRaw) // 'definitionDestinationRaw'
console.log(values.atSign) // '@'
```

## API

This package has four entries in its export map: `micromark-util-symbol/codes`,
`micromark-util-symbol/constants`, `micromark-util-symbol/types`,
`micromark-util-symbol/values`.

Each module exports an identifier with the same name (for example,
`micromark-util-symbol/codes` has `codes`), which is an object mapping strings
to other values.

Take a peek at the code to learn more!

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

[downloads-badge]: https://img.shields.io/npm/dm/micromark-util-symbol.svg

[downloads]: https://www.npmjs.com/package/micromark-util-symbol

[bundle-size-badge]: https://img.shields.io/bundlephobia/minzip/micromark-util-symbol.svg

[bundle-size]: https://bundlephobia.com/result?p=micromark-util-symbol

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

[micromark-build]: https://github.com/micromark/micromark/tree/main/packages/micromark-build
