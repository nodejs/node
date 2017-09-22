# groff-escape [![Build Status][build-badge]][build-status]

Map of non-ASCII characters to Groff commands: `'≠'` > `'!='`
(which you’ll need to wrap in like so `\[!=]` to use in Groff).

## Installation

[npm][]:

```bash
npm install groff-escape
```

## Usage

Dependencies:

```javascript
var escape = require('groff-escape');
```

Yields:

```js
{ '≠': '!=',
  'À': '`A',
  'Á': '\'A',
  'Â': '^A',
  'Ã': '~A',
  // ...
  '♥': 'HE',
  '♦': 'DI',
  '✓': 'OK',
  '⟨': 'la',
  '⟩': 'ra' }
```

## License

[MIT][license] © [Titus Wormer][author]

<!-- Definitions -->

[build-badge]: https://img.shields.io/travis/wooorm/groff-escape.svg

[build-status]: https://travis-ci.org/wooorm/groff-escape

[license]: LICENSE

[author]: http://wooorm.com

[npm]: https://docs.npmjs.com/cli/install
