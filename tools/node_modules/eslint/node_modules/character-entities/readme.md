# character-entities [![Build Status][travis-badge]][travis]

HTML character entity information.

## Installation

[npm][]:

```bash
npm install character-entities
```

## Usage

```js
var characterEntities = require('character-entities')

console.log(characterEntities.AElig) // => 'Æ'
console.log(characterEntities.aelig) // => 'æ'
console.log(characterEntities.amp) // => '&'
```

## API

### characterEntities

Mapping between (case-sensitive) character entity names to replacements.

## Support

See [html.spec.whatwg.org][html].

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

[travis-badge]: https://img.shields.io/travis/wooorm/character-entities.svg

[travis]: https://travis-ci.org/wooorm/character-entities

[npm]: https://docs.npmjs.com/cli/install

[license]: LICENSE

[author]: http://wooorm.com

[html]: https://html.spec.whatwg.org/multipage/syntax.html#named-character-references
