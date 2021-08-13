# stringify-entities

[![Build Status][build-badge]][build]
[![Coverage Status][coverage-badge]][coverage]
[![Downloads][downloads-badge]][downloads]
[![Size][size-badge]][size]

Encode HTML character references.

*   [x] Very fast
*   [x] Just the encoding part
*   [x] Has either all the options you need for a minifier/prettifier, or a tiny
    size w/ `stringifyEntitiesLight`
*   [x] Reliable: ``'`'`` characters are escaped to ensure no scripts
    run in Internet Explorer 6 to 8.
    Additionally, only named references recognized by HTML4 are encoded, meaning
    the infamous `&apos;` (which people think is a [virus][]) won’t show up

## Algorithm

By default, all dangerous, non-ASCII, and non-printable ASCII characters are
encoded.
A [subset][] of characters can be given to encode just those characters.
Alternatively, pass [`escapeOnly`][escapeonly] to escape just the dangerous
characters (`"`, `'`, `<`, `>`, `&`, `` ` ``).
By default, hexadecimal character references are used.
Pass [`useNamedReferences`][named] to use named character references when
possible, or [`useShortestReferences`][short] to use whichever is shortest:
decimal, hexadecimal, or named.
There is also a `stringifyEntitiesLight` export, which works just like
`stringifyEntities` but without the formatting options: it’s much smaller but
always outputs hexadecimal character references.

## Install

This package is [ESM only](https://gist.github.com/sindresorhus/a39789f98801d908bbc7ff3ecc99d99c):
Node 12+ is needed to use it and it must be `import`ed instead of `require`d.

[npm][]:

```sh
npm install stringify-entities
```

## Use

```js
import {stringifyEntities} from 'stringify-entities'

stringifyEntities('alpha © bravo ≠ charlie 𝌆 delta')
// => 'alpha &#xA9; bravo &#x2260; charlie &#x1D306; delta'

stringifyEntities('alpha © bravo ≠ charlie 𝌆 delta', {useNamedReferences: true})
// => 'alpha &copy; bravo &ne; charlie &#x1D306; delta'
```

## API

This package exports the following identifiers: `stringifyEntities`,
`stringifyEntitiesLight`.
There is no default export.

### `stringifyEntities(value[, options])`

Encode special characters in `value`.

##### `options`

##### Core options

###### `options.escapeOnly`

Whether to only escape possibly dangerous characters (`boolean`, default:
`false`).
Those characters are `"`, `&`, `'`, `<`, `>`, and `` ` ``.

###### `options.subset`

Whether to only escape the given subset of characters (`Array.<string>`).
Note that only BMP characters are supported here (so no emoji).

##### Formatting options

If you do not care about these, use `stringifyEntitiesLight`, which always
outputs hexadecimal character references.

###### `options.useNamedReferences`

Prefer named character references (`&amp;`) where possible (`boolean?`, default:
`false`).

###### `options.useShortestReferences`

Prefer the shortest possible reference, if that results in less bytes
(`boolean?`, default: `false`).
**Note**: `useNamedReferences` can be omitted when using
`useShortestReferences`.

###### `options.omitOptionalSemicolons`

Whether to omit semicolons when possible (`boolean?`, default: `false`).
**Note**: This creates what HTML calls “parse errors” but is otherwise still
valid HTML — don’t use this except when building a minifier.

Omitting semicolons is possible for [legacy][] named references in
[certain][dangerous] cases, and numeric references in some cases.

###### `options.attribute`

Only needed when operating dangerously with `omitOptionalSemicolons: true`.
Create character references which don’t fail in attributes (`boolean?`, default:
`false`).

## Related

*   [`parse-entities`](https://github.com/wooorm/parse-entities)
    — Parse HTML character references
*   [`character-entities`](https://github.com/wooorm/character-entities)
    — Info on character entities
*   [`character-entities-html4`](https://github.com/wooorm/character-entities-html4)
    — Info on HTML 4 character entities
*   [`character-entities-legacy`](https://github.com/wooorm/character-entities-legacy)
    — Info on legacy character entities
*   [`character-reference-invalid`](https://github.com/wooorm/character-reference-invalid)
    — Info on invalid numeric character references

## License

[MIT][license] © [Titus Wormer][author]

<!-- Definitions -->

[build-badge]: https://github.com/wooorm/stringify-entities/workflows/main/badge.svg

[build]: https://github.com/wooorm/stringify-entities/actions

[coverage-badge]: https://img.shields.io/codecov/c/github/wooorm/stringify-entities.svg

[coverage]: https://codecov.io/github/wooorm/stringify-entities

[downloads-badge]: https://img.shields.io/npm/dm/stringify-entities.svg

[downloads]: https://www.npmjs.com/package/stringify-entities

[size-badge]: https://img.shields.io/bundlephobia/minzip/stringify-entities.svg

[size]: https://bundlephobia.com/result?p=stringify-entities

[license]: license

[author]: https://wooorm.com

[npm]: https://docs.npmjs.com/cli/install

[virus]: https://www.telegraph.co.uk/technology/advice/10516839/Why-do-some-apostrophes-get-replaced-with-andapos.html

[dangerous]: lib/constant/dangerous.js

[legacy]: https://github.com/wooorm/character-entities-legacy

[subset]: #optionssubset

[escapeonly]: #optionsescapeonly

[named]: #optionsusenamedreferences

[short]: #optionsuseshortestreferences
