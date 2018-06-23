# character-reference-invalid [![Build Status][travis-badge]][travis]

HTML invalid numeric character reference information.

## Installation

[npm][]:

```bash
npm install character-reference-invalid
```

## Usage

```js
var characterReferenceInvalid = require('character-reference-invalid')

console.log(characterReferenceInvalid[0x80]) // => '€'
console.log(characterReferenceInvalid[0x89]) // => '‰'
console.log(characterReferenceInvalid[0x99]) // => '™'
```

## API

### `characterReferenceInvalid`

Mapping between invalid numeric character reference to replacements.

## Support

See [html.spec.whatwg.org][html].

## Related

*   [`character-entities`](https://github.com/wooorm/character-entities)
    — HTML character entity info
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

[travis-badge]: https://img.shields.io/travis/wooorm/character-reference-invalid.svg

[travis]: https://travis-ci.org/wooorm/character-reference-invalid

[npm]: https://docs.npmjs.com/cli/install

[license]: LICENSE

[author]: http://wooorm.com

[html]: https://html.spec.whatwg.org/multipage/syntax.html#table-charref-overrides
