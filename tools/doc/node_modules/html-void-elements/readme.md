# html-void-elements [![Build Status][build-badge]][build-page]

List of known void HTML elements.  Includes ancient (for example,
`nextid` and `basefont`) and modern (for example, `img` and
`meta`) tag-names from both W3C and WHATWG.

**Note**: there’s one special case: `menuitem`.  W3C specifies it to be
void, but WHATWG doesn’t.  I suggest using the void form.

## Installation

[npm][]:

```bash
npm install html-void-elements
```

## Usage

```javascript
var htmlVoidElements = require('html-void-elements')

console.log(htmlVoidElements)
```

Yields:

```js
[ 'area',
  'base',
  'basefont',
  'bgsound',
  'br',
  'col',
  'command',
  'embed',
  'frame',
  'hr',
  'image',
  'img',
  'input',
  'isindex',
  'keygen',
  'link',
  'menuitem',
  'meta',
  'nextid',
  'param',
  'source',
  'track',
  'wbr' ]
```

## API

### `htmlVoidElements`

`Array.<string>` — List of lower-case tag-names.

## License

[MIT][license] © [Titus Wormer][author]

<!-- Definition -->

[build-badge]: https://img.shields.io/travis/wooorm/html-void-elements.svg

[build-page]: https://travis-ci.org/wooorm/html-void-elements

[npm]: https://docs.npmjs.com/cli/install

[license]: LICENSE

[author]: http://wooorm.com
