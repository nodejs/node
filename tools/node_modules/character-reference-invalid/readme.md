# character-reference-invalid [![Build Status](https://img.shields.io/travis/wooorm/character-reference-invalid.svg?style=flat)](https://travis-ci.org/wooorm/character-reference-invalid) [![Coverage Status](https://img.shields.io/codecov/c/github/wooorm/character-reference-invalid.svg)](https://codecov.io/github/wooorm/character-reference-invalid)

HTML invalid numeric character reference information.

## Installation

[npm](https://docs.npmjs.com/cli/install):

```bash
npm install character-reference-invalid
```

**character-reference-invalid** is also available for
[bower](http://bower.io/#install-packages), [duo](http://duojs.org/#getting-started),
and for AMD, CommonJS, and globals ([uncompressed](character-reference-invalid.js) and
[compressed](character-reference-invalid.min.js)).

## Usage

```js
console.log(characterReferenceInvalid[0x80]); // €
console.log(characterReferenceInvalid[0x89]); // ‰
console.log(characterReferenceInvalid[0x99]); // ™
```

## API

### characterReferenceInvalid

Mapping between invalid numeric character reference to replacements.

## Support

See [html.spec.whatwg.org](https://html.spec.whatwg.org/multipage/syntax.html#table-charref-overrides).

## License

[MIT](LICENSE) © [Titus Wormer](http://wooorm.com)
