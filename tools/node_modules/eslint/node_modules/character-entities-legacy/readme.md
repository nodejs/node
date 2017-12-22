# character-entities-legacy [![Build Status][travis-badge]][travis]

HTML legacy character entity information: for legacy reasons some
character entities are not required to have a trailing semicolon:
`&copy` is perfectly okay for `©`.

## Installation

[npm][]:

```bash
npm install character-entities-legacy
```

## Usage

```js
console.log(characterEntitiesLegacy.copy); //=> '©'
console.log(characterEntitiesLegacy.frac34); //=> '¾'
console.log(characterEntitiesLegacy.sup1); //=> '¹'
```

## API

### `characterEntitiesLegacy`

Mapping between (case-sensitive) legacy character entity names to
replacements.

## Support

See [whatwg/html][html].

## Related

*   [`character-entities`](https://github.com/wooorm/character-entities)
    — HTML character entity info
*   [`character-entities-html4`](https://github.com/wooorm/character-entities-html4)
    — HTML 4 character entity info
*   [`parse-entities`](https://github.com/wooorm/parse-entities)
    — Parse HTML character references
*   [`stringify-entities`](https://github.com/wooorm/stringify-entities)
    — Stringify HTML character references

## License

[MIT][license] © [Titus Wormer][author]

<!-- Definitions -->

[travis-badge]: https://img.shields.io/travis/wooorm/character-entities-legacy.svg

[travis]: https://travis-ci.org/wooorm/character-entities-legacy

[npm]: https://docs.npmjs.com/cli/install

[license]: LICENSE

[author]: http://wooorm.com

[html]: https://raw.githubusercontent.com/whatwg/html/master/json-entities-legacy.inc
