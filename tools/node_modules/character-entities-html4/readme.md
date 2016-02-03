# character-entities-html4 [![Build Status](https://img.shields.io/travis/wooorm/character-entities-html4.svg?style=flat)](https://travis-ci.org/wooorm/character-entities-html4) [![Coverage Status](https://img.shields.io/codecov/c/github/wooorm/character-entities-html4.svg)](https://codecov.io/github/wooorm/character-entities-html4)

HTML4 character entity information.

## Installation

[npm](https://docs.npmjs.com/cli/install):

```bash
npm install character-entities-html4
```

**character-entities-html4** is also available for [duo](http://duojs.org/#getting-started),
and [bundled](https://github.com/wooorm/character-entities-html4/releases) for
AMD, CommonJS, and globals (uncompressed and compressed).

## Usage

```js
console.log(characterEntities.AElig); // Æ
console.log(characterEntities.aelig); // æ
console.log(characterEntities.amp); // &
```

## API

### characterEntitiesHTML4

Mapping between (case-sensitive) character entity names to replacements.

## Support

See [w3.org](http://www.w3.org/TR/html4/sgml/entities.html).

## License

[MIT](LICENSE) © [Titus Wormer](http://wooorm.com)
