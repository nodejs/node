# character-entities-legacy [![Build Status](https://img.shields.io/travis/wooorm/character-entities-legacy.svg?style=flat)](https://travis-ci.org/wooorm/character-entities-legacy) [![Coverage Status](https://img.shields.io/codecov/c/github/wooorm/character-entities-legacy.svg)](https://codecov.io/github/wooorm/character-entities-legacy)

HTML legacy character entity information: for legacy reasons some character
entities are not required to have a trailing semicolon: `&copy` is perfectly
okay for `©`.

## Installation

[npm](https://docs.npmjs.com/cli/install):

```bash
npm install character-entities-legacy
```

**character-entities-legacy** is also available for
[bower](http://bower.io/#install-packages), [duo](http://duojs.org/#getting-started),
and for AMD, CommonJS, and globals ([uncompressed](character-entities-legacy.js) and
[compressed](character-entities-legacy.min.js)).

## Usage

```js
console.log(characterEntitiesLegacy.copy); // ©
console.log(characterEntitiesLegacy.frac34); // ¾
console.log(characterEntitiesLegacy.sup1); // ¹
```

## API

### characterEntitiesLegacy

Mapping between (case-sensitive) legacy character entity names to replacements.

## Support

See [whatwg/html](https://raw.githubusercontent.com/whatwg/html/master/json-entities-legacy.inc).

## License

[MIT](LICENSE) © [Titus Wormer](http://wooorm.com)
