# character-entities [![Build Status][travis-badge]][travis] [![Coverage Status][codecov-badge]][codecov]

<!--lint disable heading-increment list-item-spacing no-duplicate-headings-->

HTML character entity information.

## Installation

[npm][npm-install]:

```bash
npm install character-entities
```

## Usage

```js
console.log(characterEntities.AElig); // Æ
console.log(characterEntities.aelig); // æ
console.log(characterEntities.amp); // &
```

## API

### characterEntities

Mapping between (case-sensitive) character entity names to replacements.

## Support

See [html.spec.whatwg.org][html].

## License

[MIT][license] © [Titus Wormer][author]

<!-- Definitions -->

[travis-badge]: https://img.shields.io/travis/wooorm/character-entities.svg

[travis]: https://travis-ci.org/wooorm/character-entities

[codecov-badge]: https://img.shields.io/codecov/c/github/wooorm/character-entities.svg

[codecov]: https://codecov.io/github/wooorm/character-entities

[npm-install]: https://docs.npmjs.com/cli/install

[license]: LICENSE

[author]: http://wooorm.com

[html]: https://html.spec.whatwg.org/multipage/syntax.html#named-character-references
