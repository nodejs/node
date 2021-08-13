# character-entities-html4

[![Build][build-badge]][build]
[![Coverage][coverage-badge]][coverage]
[![Downloads][downloads-badge]][downloads]
[![Size][size-badge]][size]

HTML4 character entity information.

## Install

This package is ESM only: Node 12+ is needed to use it and it must be `import`ed
instead of `require`d.

[npm][]:

```sh
npm install character-entities-html4
```

## Use

```js
import {characterEntitiesHtml4} from 'character-entities-html4'

console.log(characterEntitiesHtml4.AElig) // => 'Æ'
console.log(characterEntitiesHtml4.aelig) // => 'æ'
console.log(characterEntitiesHtml4.amp) // => '&'
console.log(characterEntitiesHtml4.apos) // => undefined
```

## API

This package exports the following identifiers: `characterEntitiesHtml4`.
There is no default export.

### `characterEntitiesHtml4`

Mapping between (case-sensitive) character entity names to replacements.

## Support

See [`w3.org`][html].

## Related

*   [`character-entities`](https://github.com/wooorm/character-entities)
    — HTML character entity info
*   [`character-entities-legacy`](https://github.com/wooorm/character-entities-legacy)
    — Legacy character entity info
*   [`parse-entities`](https://github.com/wooorm/parse-entities)
    — Parse HTML character references
*   [`stringify-entities`](https://github.com/wooorm/stringify-entities)
    — Stringify HTML character references

## License

[MIT][license] © [Titus Wormer][author]

<!-- Definitions -->

[build-badge]: https://github.com/wooorm/character-entities-html4/workflows/main/badge.svg

[build]: https://github.com/wooorm/character-entities-html4/actions

[coverage-badge]: https://img.shields.io/codecov/c/github/wooorm/character-entities-html4.svg

[coverage]: https://codecov.io/github/wooorm/character-entities-html4

[downloads-badge]: https://img.shields.io/npm/dm/character-entities-html4.svg

[downloads]: https://www.npmjs.com/package/character-entities-html4

[size-badge]: https://img.shields.io/bundlephobia/minzip/character-entities-html4.svg

[size]: https://bundlephobia.com/result?p=character-entities-html4

[npm]: https://docs.npmjs.com/cli/install

[license]: license

[author]: https://wooorm.com

[html]: https://www.w3.org/TR/html4/sgml/entities.html
