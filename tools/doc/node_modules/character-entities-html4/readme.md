# character-entities-html4 [![Build Status][travis-badge]][travis] [![Coverage Status][codecov-badge]][codecov]

HTML4 character entity information.

## Installation

[npm][npm-install]:

```bash
npm install character-entities-html4
```

## Usage

```js
console.log(characterEntities.AElig); // Æ
console.log(characterEntities.aelig); // æ
console.log(characterEntities.amp); // &
console.log(characterEntities.apos); // undefined
```

## API

### `characterEntitiesHTML4`

Mapping between (case-sensitive) character entity names to replacements.

## Support

See [w3.org][html].

## License

[MIT][license] © [Titus Wormer][author]

<!-- Definitions -->

[travis-badge]: https://img.shields.io/travis/wooorm/character-entities-html4.svg

[travis]: https://travis-ci.org/wooorm/character-entities-html4

[codecov-badge]: https://img.shields.io/codecov/c/github/wooorm/character-entities-html4.svg

[codecov]: https://codecov.io/github/wooorm/character-entities-html4

[npm-install]: https://docs.npmjs.com/cli/install

[license]: LICENSE

[author]: http://wooorm.com

[html]: http://www.w3.org/TR/html4/sgml/entities.html
