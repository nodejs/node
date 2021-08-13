# character-entities

[![Build][build-badge]][build]
[![Coverage][coverage-badge]][coverage]
[![Downloads][downloads-badge]][downloads]
[![Size][size-badge]][size]

HTML character entity information.

## Install

This package is ESM only: Node 12+ is needed to use it and it must be `import`ed
instead of `require`d.

[npm][]:

```sh
npm install character-entities
```

## Use

This package exports the following identifiers: `characterEntities`.
There is no default export.

```js
import {characterEntities} from 'character-entities'

console.log(characterEntities.AElig) // => 'Æ'
console.log(characterEntities.aelig) // => 'æ'
console.log(characterEntities.amp) // => '&'
```

## API

### characterEntities

Mapping between (case-sensitive) character entity names to replacements.

## Support

See [`html.spec.whatwg.org`][html].

## Related

*   [`character-entities-html4`](https://github.com/wooorm/character-entities-html4)
    — HTML 4 character entity info
*   [`character-entities-legacy`](https://github.com/wooorm/character-entities-legacy)
    — Legacy character entity info
*   [`parse-entities`](https://github.com/wooorm/parse-entities)
    — Parse HTML character references
*   [`stringify-entities`](https://github.com/wooorm/stringify-entities)
    — Stringify HTML character references

## License

[MIT][license] © [Titus Wormer][author]

<!-- Definitions -->

[build-badge]: https://github.com/wooorm/character-entities/workflows/main/badge.svg

[build]: https://github.com/wooorm/character-entities/actions

[coverage-badge]: https://img.shields.io/codecov/c/github/wooorm/longest-streak.svg

[coverage]: https://codecov.io/github/wooorm/longest-streak

[downloads-badge]: https://img.shields.io/npm/dm/character-entities.svg

[downloads]: https://www.npmjs.com/package/character-entities

[size-badge]: https://img.shields.io/bundlephobia/minzip/character-entities.svg

[size]: https://bundlephobia.com/result?p=character-entities

[npm]: https://docs.npmjs.com/cli/install

[license]: license

[author]: https://wooorm.com

[html]: https://html.spec.whatwg.org/multipage/syntax.html#named-character-references
