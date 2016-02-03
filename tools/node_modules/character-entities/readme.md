# character-entities [![Build Status](https://img.shields.io/travis/wooorm/character-entities.svg?style=flat)](https://travis-ci.org/wooorm/character-entities) [![Coverage Status](https://img.shields.io/codecov/c/github/wooorm/character-entities.svg)](https://codecov.io/github/wooorm/character-entities)

HTML character entity information.

## Installation

[npm](https://docs.npmjs.com/cli/install):

```bash
npm install character-entities
```

**character-entities** is also available for
[bower](http://bower.io/#install-packages), [duo](http://duojs.org/#getting-started),
and for AMD, CommonJS, and globals ([uncompressed](character-entities.js) and
[compressed](character-entities.min.js)).

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

See [html.spec.whatwg.org](https://html.spec.whatwg.org/multipage/syntax.html#named-character-references).

## License

[MIT](LICENSE) © [Titus Wormer](http://wooorm.com)
