# character-reference-invalid

[![Build][build-badge]][build]
[![Coverage][coverage-badge]][coverage]
[![Downloads][downloads-badge]][downloads]
[![Size][size-badge]][size]

HTML invalid numeric character reference information.

## Install

This package is ESM only: Node 12+ is needed to use it and it must be `import`ed
instead of `require`d.

[npm][]:

```sh
npm install character-reference-invalid
```

## Use

```js
import {characterReferenceInvalid} from 'character-reference-invalid'

console.log(characterReferenceInvalid[0x80]) // => '€'
console.log(characterReferenceInvalid[0x89]) // => '‰'
console.log(characterReferenceInvalid[0x99]) // => '™'
```

## API

This package exports the following identifiers: `characterReferenceInvalid`.
There is no default export.

### `characterReferenceInvalid`

Mapping between invalid numeric character reference to replacements.

## Support

See [`html.spec.whatwg.org`][html].

## Related

*   [`character-entities`](https://github.com/wooorm/character-entities)
    — HTML character entity info
*   [`character-entities-html4`](https://github.com/wooorm/character-entities-html4)
    — HTML 4 character entity info
*   [`character-entities-legacy`](https://github.com/wooorm/character-entities-legacy)
    — Legacy character entity info
*   [`parse-entities`](https://github.com/wooorm/parse-entities)
    — Parse HTML character references
*   [`stringify-entities`](https://github.com/wooorm/stringify-entities)
    — Serialize HTML character references

## License

[MIT][license] © [Titus Wormer][author]

<!-- Definitions -->

[build-badge]: https://github.com/wooorm/character-reference-invalid/workflows/main/badge.svg

[build]: https://github.com/wooorm/character-reference-invalid/actions

[coverage-badge]: https://img.shields.io/codecov/c/github/wooorm/character-reference-invalid.svg

[coverage]: https://codecov.io/github/wooorm/character-reference-invalid

[downloads-badge]: https://img.shields.io/npm/dm/character-reference-invalid.svg

[downloads]: https://www.npmjs.com/package/character-reference-invalid

[size-badge]: https://img.shields.io/bundlephobia/minzip/character-reference-invalid.svg

[size]: https://bundlephobia.com/result?p=character-reference-invalid

[npm]: https://docs.npmjs.com/cli/install

[license]: license

[author]: https://wooorm.com

[html]: https://html.spec.whatwg.org/multipage/parsing.html#table-charref-overrides
