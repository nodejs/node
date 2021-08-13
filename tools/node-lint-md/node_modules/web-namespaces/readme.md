# web-namespaces

[![Build][build-badge]][build]
[![Coverage][coverage-badge]][coverage]
[![Downloads][downloads-badge]][downloads]
[![Size][size-badge]][size]

Map of web namespaces.

## Install

This package is ESM only: Node 12+ is needed to use it and it must be `import`ed
instead of `require`d.

[npm][]:

```sh
npm install web-namespaces
```

## Use

```js
import {webNamespaces} from 'web-namespaces'

console.log(webNamespaces)
```

Yields:

```js
{
  html: 'http://www.w3.org/1999/xhtml',
  mathml: 'http://www.w3.org/1998/Math/MathML',
  svg: 'http://www.w3.org/2000/svg',
  xlink: 'http://www.w3.org/1999/xlink',
  xml: 'http://www.w3.org/XML/1998/namespace',
  xmlns: 'http://www.w3.org/2000/xmlns/'
}
```

## API

This package exports the following identifiers: `webNamespaces`.
There is no default export.

### `webNamespaces`

`Record.<string, string>` — Map of name to namespaces.

## Related

*   [`html-tag-names`](https://github.com/wooorm/html-tag-names)
    — List of HTML tags
*   [`mathml-tag-names`](https://github.com/wooorm/mathml-tag-names)
    — List of MathML tags
*   [`svg-tag-names`](https://github.com/wooorm/svg-tag-names)
    — List of SVG tags
*   [`svg-element-attributes`](https://github.com/wooorm/svg-element-attributes)
    — Map of SVG elements to allowed attributes
*   [`html-element-attributes`](https://github.com/wooorm/html-element-attributes)
    — Map of HTML elements to allowed attributes
*   [`aria-attributes`](https://github.com/wooorm/aria-attributes)
    — List of ARIA attributes

## License

[MIT][license] © [Titus Wormer][author]

<!-- Definition -->

[build-badge]: https://github.com/wooorm/web-namespaces/workflows/main/badge.svg

[build]: https://github.com/wooorm/web-namespaces/actions

[coverage-badge]: https://img.shields.io/codecov/c/github/wooorm/web-namespaces.svg

[coverage]: https://codecov.io/github/wooorm/web-namespaces

[downloads-badge]: https://img.shields.io/npm/dm/web-namespaces.svg

[downloads]: https://www.npmjs.com/package/web-namespaces

[size-badge]: https://img.shields.io/bundlephobia/minzip/web-namespaces.svg

[size]: https://bundlephobia.com/result?p=web-namespaces

[npm]: https://docs.npmjs.com/cli/install

[license]: license

[author]: https://wooorm.com
