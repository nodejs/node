# character-entities-html4 [![Build Status][travis-badge]][travis]

HTML4 character entity information.

## Installation

[npm][]:

```bash
npm install character-entities-html4
```

## Usage

```js
console.log(characterEntities.AElig); //=> 'Æ'
console.log(characterEntities.aelig); //=> 'æ'
console.log(characterEntities.amp); //=> '&'
console.log(characterEntities.apos); //=> undefined
```

## API

### `characterEntitiesHTML4`

Mapping between (case-sensitive) character entity names to replacements.

## Support

See [w3.org][html].

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

[travis-badge]: https://img.shields.io/travis/wooorm/character-entities-html4.svg

[travis]: https://travis-ci.org/wooorm/character-entities-html4

[npm]: https://docs.npmjs.com/cli/install

[license]: LICENSE

[author]: http://wooorm.com

[html]: http://www.w3.org/TR/html4/sgml/entities.html
