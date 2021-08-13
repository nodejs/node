# html-void-elements

[![Build][build-badge]][build]
[![Coverage][coverage-badge]][coverage]
[![Downloads][downloads-badge]][downloads]
[![Size][size-badge]][size]

List of known void HTML elements.
Includes ancient (such as `nextid` and `basefont`) and modern (such as `img` and
`meta`) tag names from the HTML living standard.

## Install

This package is ESM only: Node 12+ is needed to use it and it must be `import`ed
instead of `require`d.

[npm][]:

```sh
npm install html-void-elements
```

## Use

```js
import {htmlVoidElements} from 'html-void-elements'

console.log(htmlVoidElements)
```

Yields:

```js
[
  'area',
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
  'wbr'
]
```

## API

This package exports the following identifiers: `htmlVoidElements`.
There is no default export.

### `htmlVoidElements`

`string[]` — List of lowercase tag names.

## License

[MIT][license] © [Titus Wormer][author]

<!-- Definition -->

[build-badge]: https://github.com/wooorm/html-void-elements/workflows/main/badge.svg

[build]: https://github.com/wooorm/html-void-elements/actions

[coverage-badge]: https://img.shields.io/codecov/c/github/wooorm/html-void-elements.svg

[coverage]: https://codecov.io/github/wooorm/html-void-elements

[downloads-badge]: https://img.shields.io/npm/dm/html-void-elements.svg

[downloads]: https://www.npmjs.com/package/html-void-elements

[size-badge]: https://img.shields.io/bundlephobia/minzip/html-void-elements.svg

[size]: https://bundlephobia.com/result?p=html-void-elements

[npm]: https://docs.npmjs.com/cli/install

[license]: license

[author]: https://wooorm.com
