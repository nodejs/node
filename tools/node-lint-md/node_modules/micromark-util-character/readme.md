# micromark-util-character

[![Build][build-badge]][build]
[![Coverage][coverage-badge]][coverage]
[![Downloads][downloads-badge]][downloads]
[![Size][bundle-size-badge]][bundle-size]
[![Sponsors][sponsors-badge]][opencollective]
[![Backers][backers-badge]][opencollective]
[![Chat][chat-badge]][chat]

micromark utility to handle [character codes](https://github.com/micromark/micromark#preprocess).

## Contents

*   [Install](#install)
*   [Use](#use)
*   [API](#api)
    *   [`asciiAlpha(code)`](#asciialphacode)
    *   [`asciiDigit(code)`](#asciidigitcode)
    *   [`asciiHexDigit(code)`](#asciihexdigitcode)
    *   [`asciiAlphanumeric(code)`](#asciialphanumericcode)
    *   [`asciiPunctuation(code)`](#asciipunctuationcode)
    *   [`asciiAtext(code)`](#asciiatextcode)
    *   [`asciiControl(code)`](#asciicontrolcode)
    *   [`markdownLineEndingOrSpace(code)`](#markdownlineendingorspacecode)
    *   [`markdownLineEnding(code)`](#markdownlineendingcode)
    *   [`markdownSpace(code)`](#markdownspacecode)
    *   [`unicodeWhitespace(code)`](#unicodewhitespacecode)
    *   [`unicodePunctuation(code)`](#unicodepunctuationcode)
*   [Security](#security)
*   [Contribute](#contribute)
*   [License](#license)

## Install

[npm][]:

```sh
npm install micromark-util-character
```

## Use

```js
import {asciiAlpha} from 'micromark-util-character'

console.log(asciiAlpha(64)) // false
console.log(asciiAlpha(65)) // true
```

## API

This module exports the following identifiers: `asciiAlpha`,
`asciiAlphanumeric`, `asciiAtext`, `asciiControl`, `asciiDigit`,
`asciiHexDigit`, `asciiPunctuation`, `markdownLineEnding`,
`markdownLineEndingOrSpace`, `markdownSpace`, `unicodePunctuation`,
`unicodeWhitespace`.
There is no default export.

### `asciiAlpha(code)`

Check whether the
[character code](https://github.com/micromark/micromark#preprocess)
represents an ASCII alpha (`a` through `z`,
case insensitive).

An **ASCII alpha** is an ASCII upper alpha or ASCII lower alpha.

An **ASCII upper alpha** is a character in the inclusive range U+0041 (`A`)
to U+005A (`Z`).

An **ASCII lower alpha** is a character in the inclusive range U+0061 (`a`)
to U+007A (`z`).

### `asciiDigit(code)`

Check whether the
[character code](https://github.com/micromark/micromark#preprocess)
represents an ASCII digit (`0` through `9`).

An **ASCII digit** is a character in the inclusive range U+0030 (`0`) to
U+0039 (`9`).

### `asciiHexDigit(code)`

Check whether the
[character code](https://github.com/micromark/micromark#preprocess)
represents an ASCII hex digit (`a` through `f`, case insensitive, or `0` through
`9`).

An **ASCII hex digit** is an ASCII digit (see `asciiDigit`), ASCII upper hex
digit, or an ASCII lower hex digit.

An **ASCII upper hex digit** is a character in the inclusive range U+0041
(`A`) to U+0046 (`F`).

An **ASCII lower hex digit** is a character in the inclusive range U+0061
(`a`) to U+0066 (`f`).

### `asciiAlphanumeric(code)`

Check whether the
[character code](https://github.com/micromark/micromark#preprocess)
represents an ASCII alphanumeric (`a` through `z`, case insensitive, or `0`
through `9`).

An **ASCII alphanumeric** is an ASCII digit (see `asciiDigit`) or ASCII alpha
(see `asciiAlpha`).

### `asciiPunctuation(code)`

Check whether the
[character code](https://github.com/micromark/micromark#preprocess)
represents ASCII punctuation.

An **ASCII punctuation** is a character in the inclusive ranges U+0021
EXCLAMATION MARK (`!`) to U+002F SLASH (`/`), U+003A COLON (`:`) to U+0040 AT
SIGN (`@`), U+005B LEFT SQUARE BRACKET (`[`) to U+0060 GRAVE ACCENT
(`` ` ``), or U+007B LEFT CURLY BRACE (`{`) to U+007E TILDE (`~`).

### `asciiAtext(code)`

Check whether the
[character code](https://github.com/micromark/micromark#preprocess)
represents an ASCII atext.

atext is an ASCII alphanumeric (see `asciiAlphanumeric`), or a character in
the inclusive ranges U+0023 NUMBER SIGN (`#`) to U+0027 APOSTROPHE (`'`),
U+002A ASTERISK (`*`), U+002B PLUS SIGN (`+`), U+002D DASH (`-`), U+002F
SLASH (`/`), U+003D EQUALS TO (`=`), U+003F QUESTION MARK (`?`), U+005E
CARET (`^`) to U+0060 GRAVE ACCENT (`` ` ``), or U+007B LEFT CURLY BRACE
(`{`) to U+007E TILDE (`~`) (**\[RFC5322]**).

See **\[RFC5322]**:\
[Internet Message Format](https://tools.ietf.org/html/rfc5322).\
P. Resnick.\
IETF.

### `asciiControl(code)`

Check whether a
[character code](https://github.com/micromark/micromark#preprocess)
is an ASCII control character.

An **ASCII control** is a character in the inclusive range U+0000 NULL (NUL)
to U+001F (US), or U+007F (DEL).

### `markdownLineEndingOrSpace(code)`

Check whether a
[character code](https://github.com/micromark/micromark#preprocess)
is a markdown line ending (see `markdownLineEnding`) or markdown space (see
`markdownSpace`).

### `markdownLineEnding(code)`

Check whether a
[character code](https://github.com/micromark/micromark#preprocess)
is a markdown line ending.

A **markdown line ending** is the virtual characters M-0003 CARRIAGE RETURN
LINE FEED (CRLF), M-0004 LINE FEED (LF) and M-0005 CARRIAGE RETURN (CR).

In micromark, the actual character U+000A LINE FEED (LF) and U+000D CARRIAGE
RETURN (CR) are replaced by these virtual characters depending on whether
they occurred together.

### `markdownSpace(code)`

Check whether a
[character code](https://github.com/micromark/micromark#preprocess)
is a markdown space.

A **markdown space** is the concrete character U+0020 SPACE (SP) and the
virtual characters M-0001 VIRTUAL SPACE (VS) and M-0002 HORIZONTAL TAB (HT).

In micromark, the actual character U+0009 CHARACTER TABULATION (HT) is
replaced by one M-0002 HORIZONTAL TAB (HT) and between 0 and 3 M-0001 VIRTUAL
SPACE (VS) characters, depending on the column at which the tab occurred.

### `unicodeWhitespace(code)`

Check whether the
[character code](https://github.com/micromark/micromark#preprocess)
represents Unicode whitespace.

Note that this does handle micromark specific markdown whitespace characters.
See `markdownLineEndingOrSpace` to check that.

A **Unicode whitespace** is a character in the Unicode `Zs` (Separator,
Space) category, or U+0009 CHARACTER TABULATION (HT), U+000A LINE FEED (LF),
U+000C (FF), or U+000D CARRIAGE RETURN (CR) (**\[UNICODE]**).

See **\[UNICODE]**:\
[The Unicode Standard](https://www.unicode.org/versions/).\
Unicode Consortium.

### `unicodePunctuation(code)`

Check whether the
[character code](https://github.com/micromark/micromark#preprocess)
represents Unicode punctuation.

A **Unicode punctuation** is a character in the Unicode `Pc` (Punctuation,
Connector), `Pd` (Punctuation, Dash), `Pe` (Punctuation, Close), `Pf`
(Punctuation, Final quote), `Pi` (Punctuation, Initial quote), `Po`
(Punctuation, Other), or `Ps` (Punctuation, Open) categories, or an ASCII
punctuation (see `asciiPunctuation`) (**\[UNICODE]**).

See **\[UNICODE]**:\
[The Unicode Standard](https://www.unicode.org/versions/).\
Unicode Consortium.

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

[downloads-badge]: https://img.shields.io/npm/dm/micromark-util-character.svg

[downloads]: https://www.npmjs.com/package/micromark-util-character

[bundle-size-badge]: https://img.shields.io/bundlephobia/minzip/micromark-util-character.svg

[bundle-size]: https://bundlephobia.com/result?p=micromark-util-character

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
