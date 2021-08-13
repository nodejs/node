# character-entities-legacy

[![Build][build-badge]][build]
[![Coverage][coverage-badge]][coverage]
[![Downloads][downloads-badge]][downloads]
[![Size][size-badge]][size]

HTML legacy character entity information: for legacy reasons some character
entities are not required to have a trailing semicolon: `&copy` is perfectly
okay for `©`.

## Install

This package is ESM only: Node 12+ is needed to use it and it must be `import`ed
instead of `require`d.

[npm][]:

```sh
npm install character-entities-legacy
```

## Use

```js
import {characterEntitiesLegacy} from 'character-entities-legacy'

console.log(characterEntitiesLegacy.copy) // => '©'
console.log(characterEntitiesLegacy.frac34) // => '¾'
console.log(characterEntitiesLegacy.sup1) // => '¹'
```

## API

This package exports the following identifiers: `characterEntitiesLegacy`.
There is no default export.

### `characterEntitiesLegacy`

Mapping between (case-sensitive) legacy character entity names to replacements.

## Support

See [`whatwg/html`][html].

## Related

*   [`character-entities`](https://github.com/wooorm/character-entities)
    — HTML character entity info
*   [`character-entities-html4`](https://github.com/wooorm/character-entities-html4)
    — HTML 4 character entity info
*   [`parse-entities`](https://github.com/wooorm/parse-entities)
    — Parse HTML character references
*   [`stringify-entities`](https://github.com/wooorm/stringify-entities)
    — Serialize HTML character references

## License

[MIT][license] © [Titus Wormer][author]

<!-- Definitions -->

[build-badge]: https://github.com/wooorm/character-entities-legacy/workflows/main/badge.svg

[build]: https://github.com/wooorm/character-entities-legacy/actions

[coverage-badge]: https://img.shields.io/codecov/c/github/wooorm/character-entities-legacy.svg

[coverage]: https://codecov.io/github/wooorm/character-entities-legacy

[downloads-badge]: https://img.shields.io/npm/dm/character-entities-legacy.svg

[downloads]: https://www.npmjs.com/package/character-entities-legacy

[size-badge]: https://img.shields.io/bundlephobia/minzip/character-entities-legacy.svg

[size]: https://bundlephobia.com/result?p=character-entities-legacy

[npm]: https://docs.npmjs.com/cli/install

[license]: license

[author]: https://wooorm.com

[html]: https://github.com/whatwg/html-build/blob/HEAD/entities/json-entities-legacy.inc
