# micromark-util-classify-character

[![Build][build-badge]][build]
[![Coverage][coverage-badge]][coverage]
[![Downloads][downloads-badge]][downloads]
[![Size][bundle-size-badge]][bundle-size]
[![Sponsors][sponsors-badge]][opencollective]
[![Backers][backers-badge]][opencollective]
[![Chat][chat-badge]][chat]

micromark utility to classify whether a character is whitespace or punctuation.

## Contents

*   [Install](#install)
*   [Use](#use)
*   [API](#api)
    *   [`classifyCharacter(code)`](#classifycharactercode)
*   [Security](#security)
*   [Contribute](#contribute)
*   [License](#license)

## Install

[npm][]:

```sh
npm install micromark-util-classify-character
```

## Use

```js
/** @type {Tokenizer} */
function tokenizeAttention(effects, ok) {
  return start

  // …

  /** @type {State} */
  function sequence(code) {
    if (code === marker) {
      // …
    }

    const token = effects.exit('attentionSequence')
    const after = classifyCharacter(code)
    const open =
      !after || (after === constants.characterGroupPunctuation && before)
    const close =
      !before || (before === constants.characterGroupPunctuation && after)
    // …
  }

  // …
}
```

## API

This module exports the following identifiers: `classifyCharacter`.
There is no default export.

### `classifyCharacter(code)`

Classify whether a
[character code](https://github.com/micromark/micromark#preprocess)
represents whitespace, punctuation, or
something else.
Used for attention (emphasis, strong), whose sequences can open or close based
on the class of surrounding characters.

Note that eof (`null`) is seen as whitespace.

###### Returns

`constants.characterGroupWhitespace`, `constants.characterGroupPunctuation`,
or `undefined.`

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

[downloads-badge]: https://img.shields.io/npm/dm/micromark-util-classify-character.svg

[downloads]: https://www.npmjs.com/package/micromark-util-classify-character

[bundle-size-badge]: https://img.shields.io/bundlephobia/minzip/micromark-util-classify-character.svg

[bundle-size]: https://bundlephobia.com/result?p=micromark-util-classify-character

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
