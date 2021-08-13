# hastscript

[![Build][build-badge]][build]
[![Coverage][coverage-badge]][coverage]
[![Downloads][downloads-badge]][downloads]
[![Size][size-badge]][size]
[![Sponsors][sponsors-badge]][collective]
[![Backers][backers-badge]][collective]
[![Chat][chat-badge]][chat]

[**hast**][hast] utility to create [*trees*][tree] in HTML or SVG.

Similar to [`hyperscript`][hyperscript], [`virtual-dom/h`][virtual-hyperscript],
[`React.createElement`][react], and [Vue’s `createElement`][vue],
but for [**hast**][hast].

Use [`unist-builder`][u] to create any [**unist**][unist] tree.

## Install

This package is [ESM only](https://gist.github.com/sindresorhus/a39789f98801d908bbc7ff3ecc99d99c):
Node 12+ is needed to use it and it must be `import`ed instead of `require`d.

[npm][]:

```sh
npm install hastscript
```

## Use

```js
import {h, s} from 'hastscript'

// Children as an array:
console.log(
  h('.foo#some-id', [
    h('span', 'some text'),
    h('input', {type: 'text', value: 'foo'}),
    h('a.alpha', {class: 'bravo charlie', download: 'download'}, [
      'delta',
      'echo'
    ])
  ])
)

// Children as arguments:
console.log(
  h(
    'form',
    {method: 'POST'},
    h('input', {type: 'text', name: 'foo'}),
    h('input', {type: 'text', name: 'bar'}),
    h('input', {type: 'submit', value: 'send'})
  )
)

// SVG:
console.log(
  s('svg', {xmlns: 'http://www.w3.org/2000/svg', viewbox: '0 0 500 500'}, [
    s('title', 'SVG `<circle>` element'),
    s('circle', {cx: 120, cy: 120, r: 100})
  ])
)
```

Yields:

```js
{
  type: 'element',
  tagName: 'div',
  properties: {className: ['foo'], id: 'some-id'},
  children: [
    {
      type: 'element',
      tagName: 'span',
      properties: {},
      children: [{type: 'text', value: 'some text'}]
    },
    {
      type: 'element',
      tagName: 'input',
      properties: {type: 'text', value: 'foo'},
      children: []
    },
    {
      type: 'element',
      tagName: 'a',
      properties: {className: ['alpha', 'bravo', 'charlie'], download: true},
      children: [{type: 'text', value: 'delta'}, {type: 'text', value: 'echo'}]
    }
  ]
}
{
  type: 'element',
  tagName: 'form',
  properties: {method: 'POST'},
  children: [
    {
      type: 'element',
      tagName: 'input',
      properties: {type: 'text', name: 'foo'},
      children: []
    },
    {
      type: 'element',
      tagName: 'input',
      properties: {type: 'text', name: 'bar'},
      children: []
    },
    {
      type: 'element',
      tagName: 'input',
      properties: {type: 'submit', value: 'send'},
      children: []
    }
  ]
}
{
  type: 'element',
  tagName: 'svg',
  properties: {xmlns: 'http://www.w3.org/2000/svg', viewBox: '0 0 500 500'},
  children: [
    {
      type: 'element',
      tagName: 'title',
      properties: {},
      children: [{type: 'text', value: 'SVG `<circle>` element'}]
    },
    {
      type: 'element',
      tagName: 'circle',
      properties: {cx: 120, cy: 120, r: 100},
      children: []
    }
  ]
}
```

## API

This package exports the following identifiers: `h` and `s`.
There is no default export.

### `h(selector?[, properties][, …children])`

### `s(selector?[, properties][, …children])`

Create virtual [**hast**][hast] [*trees*][tree] for HTML or SVG.

##### Signatures

*   `h(): root`
*   `h(null[, …children]): root`
*   `h(name[, properties][, …children]): element`

(and the same for `s`).

##### Parameters

###### `selector`

Simple CSS selector (`string`, optional).
Can contain a tag name (`foo`), IDs (`#bar`), and classes (`.baz`).
If the selector is a string but there is no tag name in it, `h` defaults to
build a `div` element, and `s` to a `g` element.
`selector` is parsed by [`hast-util-parse-selector`][parse-selector].
When string, builds an [`Element`][element].
When nullish, builds a [`Root`][root] instead.

###### `properties`

Map of properties (`Object.<*>`, optional).
Keys should match either the HTML attribute name, or the DOM property name, but
are case-insensitive.
Cannot be given when building a [`Root`][root].

###### `children`

(Lists of) children (`string`, `number`, `Node`, `Array.<children>`, optional).
When strings or numbers are encountered, they are mapped to [`Text`][text]
nodes.
If [`Root`][root] nodes are given, their children are used instead.

##### Returns

[`Element`][element] or [`Root`][root].

## JSX

`hastscript` can be used with JSX.
Either use the automatic runtime set to `hastscript/html`, `hastscript/svg`,
or `hastscript` (shortcut for HTML).

Or import `h` or `s` yourself and define it as the pragma (plus set the fragment
to `null`).

The example above can then be written like so, using inline pragmas, so
that SVG can be used too:

`example-html.jsx`:

```jsx
/** @jsxImportSource hastscript */

console.log(
  <div class="foo" id="some-id">
    <span>some text</span>
    <input type="text" value="foo" />
    <a class="alpha bravo charlie" download>
      deltaecho
    </a>
  </div>
)

console.log(
  <form method="POST">
    <input type="text" name="foo" />
    <input type="text" name="bar" />
    <input type="submit" name="send" />
  </form>
)
```

`example-svg.jsx`:

```jsx
/** @jsxImportSource hastscript/svg */
console.log(
  <svg xmlns="http://www.w3.org/2000/svg" viewbox="0 0 500 500">
    <title>SVG `&lt;circle&gt;` element</title>
    <circle cx={120} cy={120} r={100} />
  </svg>
)
```

Because JSX does not allow dots (`.`) or number signs (`#`) in tag names, you
have to pass class names and IDs in as attributes.

You can use [`estree-util-build-jsx`][build-jsx] to compile JSX away.

You could also use [bublé][], but it’s not ideal (`jsxFragment` is currently
only available on the API, not the CLI, and it only allows a single pragma).

For [Babel][], use [`@babel/plugin-transform-react-jsx`][babel-jsx] and either
pass `pragma: 'h'` and `pragmaFrag: 'null'`, or pass `importSource:
'hastscript'`.
This is less ideal because it allows a single pragma.

Babel also lets you configure this in a script:

```jsx
/** @jsx s @jsxFrag null */
import {s} from 'hastscript'

console.log(<rect />)
```

This is useful because it allows using *both* `html` and `svg`, although in
different files.

## Security

Use of `hastscript` can open you up to a [cross-site scripting (XSS)][xss]
attack as values are injected into the syntax tree.
The following example shows how a script is injected that runs when loaded in a
browser.

```js
const tree = {type: 'root', children: []}

tree.children.push(h('script', 'alert(1)'))
```

Yields:

```html
<script>alert(1)</script>
```

The following example shows how an image is injected that fails loading and
therefore runs code in a browser.

```js
const tree = {type: 'root', children: []}

// Somehow someone injected these properties instead of an expected `src` and
// `alt`:
const otherProps = {src: 'x', onError: 'alert(2)'}

tree.children.push(h('img', {src: 'default.png', ...otherProps}))
```

Yields:

```html
<img src="x" onerror="alert(2)">
```

The following example shows how code can run in a browser because someone stored
an object in a database instead of the expected string.

```js
const tree = {type: 'root', children: []}

// Somehow this isn’t the expected `'wooorm'`.
const username = {
  type: 'element',
  tagName: 'script',
  children: [{type: 'text', value: 'alert(3)'}]
}

tree.children.push(h('span.handle', username))
```

Yields:

```html
<span class="handle"><script>alert(3)</script></span>
```

Either do not use user input in `hastscript` or use
[`hast-util-santize`][sanitize].

## Related

*   [`unist-builder`](https://github.com/syntax-tree/unist-builder)
    — Create any unist tree
*   [`xastscript`](https://github.com/syntax-tree/xastscript)
    — Create a xast tree
*   [`hast-to-hyperscript`](https://github.com/syntax-tree/hast-to-hyperscript)
    — Convert a Node to React, Virtual DOM, Hyperscript, and more
*   [`hast-util-from-dom`](https://github.com/syntax-tree/hast-util-from-dom)
    — Transform a DOM tree to hast
*   [`hast-util-select`](https://github.com/syntax-tree/hast-util-select)
    — `querySelector`, `querySelectorAll`, and `matches`
*   [`hast-util-to-html`](https://github.com/syntax-tree/hast-util-to-html)
    — Stringify nodes to HTML
*   [`hast-util-to-dom`](https://github.com/syntax-tree/hast-util-to-dom)
    — Transform to a DOM tree

## Contribute

See [`contributing.md` in `syntax-tree/.github`][contributing] for ways to get
started.
See [`support.md`][support] for ways to get help.

This project has a [code of conduct][coc].
By interacting with this repository, organization, or community you agree to
abide by its terms.

## License

[MIT][license] © [Titus Wormer][author]

<!-- Definitions -->

[build-badge]: https://github.com/syntax-tree/hastscript/workflows/main/badge.svg

[build]: https://github.com/syntax-tree/hastscript/actions

[coverage-badge]: https://img.shields.io/codecov/c/github/syntax-tree/hastscript.svg

[coverage]: https://codecov.io/github/syntax-tree/hastscript

[downloads-badge]: https://img.shields.io/npm/dm/hastscript.svg

[downloads]: https://www.npmjs.com/package/hastscript

[size-badge]: https://img.shields.io/bundlephobia/minzip/hastscript.svg

[size]: https://bundlephobia.com/result?p=hastscript

[sponsors-badge]: https://opencollective.com/unified/sponsors/badge.svg

[backers-badge]: https://opencollective.com/unified/backers/badge.svg

[collective]: https://opencollective.com/unified

[chat-badge]: https://img.shields.io/badge/chat-discussions-success.svg

[chat]: https://github.com/syntax-tree/unist/discussions

[npm]: https://docs.npmjs.com/cli/install

[license]: license

[author]: https://wooorm.com

[contributing]: https://github.com/syntax-tree/.github/blob/HEAD/contributing.md

[support]: https://github.com/syntax-tree/.github/blob/HEAD/support.md

[coc]: https://github.com/syntax-tree/.github/blob/HEAD/code-of-conduct.md

[hyperscript]: https://github.com/dominictarr/hyperscript

[virtual-hyperscript]: https://github.com/Matt-Esch/virtual-dom/tree/HEAD/virtual-hyperscript

[react]: https://reactjs.org/docs/glossary.html#react-elements

[vue]: https://vuejs.org/v2/guide/render-function.html#createElement-Arguments

[unist]: https://github.com/syntax-tree/unist

[tree]: https://github.com/syntax-tree/unist#tree

[hast]: https://github.com/syntax-tree/hast

[element]: https://github.com/syntax-tree/hast#element

[root]: https://github.com/syntax-tree/xast#root

[text]: https://github.com/syntax-tree/hast#text

[u]: https://github.com/syntax-tree/unist-builder

[build-jsx]: https://github.com/wooorm/estree-util-build-jsx

[bublé]: https://github.com/Rich-Harris/buble

[babel]: https://github.com/babel/babel

[babel-jsx]: https://github.com/babel/babel/tree/main/packages/babel-plugin-transform-react-jsx

[parse-selector]: https://github.com/syntax-tree/hast-util-parse-selector

[xss]: https://en.wikipedia.org/wiki/Cross-site_scripting

[sanitize]: https://github.com/syntax-tree/hast-util-sanitize
