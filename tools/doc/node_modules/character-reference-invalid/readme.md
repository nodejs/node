# character-reference-invalid [![Build Status][travis-badge]][travis] [![Coverage Status][codecov-badge]][codecov]

HTML invalid numeric character reference information.

## Installation

[npm][npm-install]:

```bash
npm install character-reference-invalid
```

## Usage

```js
console.log(characterReferenceInvalid[0x80]); // €
console.log(characterReferenceInvalid[0x89]); // ‰
console.log(characterReferenceInvalid[0x99]); // ™
```

## API

### `characterReferenceInvalid`

Mapping between invalid numeric character reference to replacements.

## Support

See [html.spec.whatwg.org][html].

## License

[MIT][license] © [Titus Wormer][author]

<!-- Definitions -->

[travis-badge]: https://img.shields.io/travis/wooorm/character-reference-invalid.svg

[travis]: https://travis-ci.org/wooorm/character-reference-invalid

[codecov-badge]: https://img.shields.io/codecov/c/github/wooorm/character-reference-invalid.svg

[codecov]: https://codecov.io/github/wooorm/character-reference-invalid

[npm-install]: https://docs.npmjs.com/cli/install

[license]: LICENSE

[author]: http://wooorm.com

[html]: https://html.spec.whatwg.org/multipage/syntax.html#table-charref-overrides
