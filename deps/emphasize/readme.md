# emphasize

[![Build][badge-build-image]][badge-build-url]
[![Coverage][badge-coverage-image]][badge-coverage-url]
[![Downloads][badge-downloads-image]][badge-downloads-url]
[![Size][badge-size-image]][badge-size-url]

ANSI syntax highlighting for the terminal.

## Contents

*   [What is this?](#what-is-this)
*   [When should I use this?](#when-should-i-use-this)
*   [Install](#install)
*   [Use](#use)
*   [API](#api)
    *   [`all`](#all)
    *   [`common`](#common)
    *   [`createEmphasize(grammars?)`](#createemphasizegrammars)
    *   [`emphasize.highlight(language, value[, options])`](#emphasizehighlightlanguage-value-options)
    *   [`emphasize.highlightAuto(value[, options])`](#emphasizehighlightautovalue-options)
    *   [`emphasize.listLanguages()`](#emphasizelistlanguages)
    *   [`emphasize.register(grammars)`](#emphasizeregistergrammars)
    *   [`emphasize.registerAlias(aliases)`](#emphasizeregisteraliasaliases)
    *   [`emphasize.registered(aliasOrLanguage)`](#emphasizeregisteredaliasorlanguage)
    *   [`AutoOptions`](#autooptions)
    *   [`LanguageFn`](#languagefn)
    *   [`Result`](#result)
    *   [`Sheet`](#sheet)
    *   [`Style`](#style)
*   [Compatibility](#compatibility)
*   [Security](#security)
*   [Contribute](#contribute)
*   [License](#license)

## What is this?

This package wraps [`lowlight`][github-lowlight] to output ANSI syntax
highlighting instead of HTML.
It can support 190+ programming languages.

## When should I use this?

This package is useful when you want to display code on a terminal.

## Install

This package is [ESM only][github-gist-esm].
In Node.js (version 16+),
install with [npm][npm-install]:

```sh
npm install emphasize
```

In Deno with [`esm.sh`][esm-sh]:

```js
import {all, common, createEmphasize} from 'https://esm.sh/emphasize@7'
```

In browsers with [`esm.sh`][esm-sh]:

```html
<script type="module">
  import {all, common, createEmphasize} from 'https://esm.sh/emphasize@7?bundle'
</script>
```

## Use

Say `example.css` looks as follows:

```css
@font-face {
  font-family: Alpha;
  src: url('Bravo.otf');
}

body, .charlie, #delta {
  color: #bada55;
  background-color: rgba(33, 33, 33, 0.33);
  font-family: "Alpha", sans-serif;
}

@import url(echo.css);

@media print {
  a[href^=http]::after {
    content: attr(href)
  }
}
```

…and `example.js` contains the following:

```js
import fs from 'node:fs/promises'
import {emphasize} from 'emphasize'

const doc = String(await fs.readFile('example.css'))

const output = emphasize.highlightAuto(doc).value

console.log(output)
```

…now running `node example.js` yields:

```txt
\x1B[32m@font-face\x1B[39m {
  \x1B[33mfont-family\x1B[39m: Alpha;
  \x1B[33msrc\x1B[39m: \x1B[31murl\x1B[39m(\x1B[36m'Bravo.otf'\x1B[39m);
}

\x1B[32mbody\x1B[39m, \x1B[34m.charlie\x1B[39m, \x1B[34m#delta\x1B[39m {
  \x1B[33mcolor\x1B[39m: \x1B[36m#bada55\x1B[39m;
  \x1B[33mbackground-color\x1B[39m: \x1B[31mrgba\x1B[39m(\x1B[36m33\x1B[39m, \x1B[36m33\x1B[39m, \x1B[36m33\x1B[39m, \x1B[36m0.33\x1B[39m);
  \x1B[33mfont-family\x1B[39m: \x1B[36m"Alpha"\x1B[39m, sans-serif;
}

\x1B[32m@import\x1B[39m url(echo.css);

\x1B[32m@media\x1B[39m print {
  \x1B[32ma\x1B[39m\x1B[35m[href^=http]\x1B[39m\x1B[35m::after\x1B[39m {
    \x1B[33mcontent\x1B[39m: \x1B[31mattr\x1B[39m(href)
  }
}
```

…which looks as follows:

![Screenshot showing the code in terminal](screenshot.png)

## API

This package exports the identifiers
[`all`][api-all],
[`common`][api-common],
and [`createEmphasize`][api-create-emphasize].
There is no default export.

It exports the [TypeScript][] types
[`AutoOptions`][api-auto-options],
[`LanguageFn`][api-language-fn],
[`Result`][api-result],
[`Sheet`][api-sheet],
and [`Style`][api-style].

### `all`

Map of all (±190) grammars ([`Record<string, LanguageFn>`][api-language-fn]).

See [`all` from `lowlight`][github-lowlight-all].

### `common`

Map of common (37) grammars ([`Record<string, LanguageFn>`][api-language-fn]).

See [`common` from `lowlight`][github-lowlight-common].

### `createEmphasize(grammars?)`

Create a `emphasize` instance.

###### Parameters

*   `grammars` ([`Record<string, LanguageFn>`][api-language-fn], optional)
    — grammars to add

###### Returns

Emphasize (`emphasize`).

### `emphasize.highlight(language, value[, options])`

Highlight `value` (code) as `language` (name).

###### Parameters

*   `language` (`string`)
    — programming language [name][github-highlight-names]
*   `value` (`string`)
    — code to highlight
*   `sheet` ([`Sheet`][api-sheet], optional)
    — style sheet

###### Returns

[`Result`][api-result].

### `emphasize.highlightAuto(value[, options])`

Highlight `value` (code) and guess its programming language.

###### Parameters

*   `value` (`string`)
    — code to highlight
*   `options` ([`AutoOptions`][api-auto-options] or [`Sheet`][api-sheet],
    optional)
    — configuration or style sheet

###### Returns

[`Result`][api-result].

### `emphasize.listLanguages()`

List registered languages.

See [`lowlight.listLanguages`][github-lowlight-list-languages].

### `emphasize.register(grammars)`

Register languages.

See [`lowlight.register`][github-lowlight-register].

### `emphasize.registerAlias(aliases)`

Register aliases.

See [`lowlight.registerAlias`][github-lowlight-register-alias].

### `emphasize.registered(aliasOrLanguage)`

Check whether an alias or name is registered.

See [`lowlight.registered`][github-lowlight-registered].

### `AutoOptions`

Configuration for `highlightAuto` (TypeScript type).

###### Fields

*   `sheet` ([`Sheet`][api-sheet], optional)
    — sheet
*   `subset` (`Array<string>`, default: all registered languages)
    — list of allowed languages

### `LanguageFn`

Highlight.js grammar (TypeScript type).

See [`LanguageFn` from `lowlight`][github-lowlight-langauge-fn].

### `Result`

Result (TypeScript type).

###### Fields

*   `language` (`string` or `undefined`)
    — detected programming language.
*   `relevance` (`number` or `undefined`)
    — how sure `lowlight` is that the given code is in the language
*   `value` (`string`)
    — highlighted code

### `Sheet`

Map [`highlight.js` classes][github-highlight-classes] to styles functions
(TypeScript type).

The `hljs-` prefix must not be used in those classes.
The “descendant selector” (a space) is supported.

For convenience [chalk’s chaining of styles][github-chalk-styles] is suggested.
An abbreviated example is as follows:

```js
{
  'comment': chalk.gray,
  'meta meta-string': chalk.cyan,
  'meta keyword': chalk.magenta,
  'emphasis': chalk.italic,
  'strong': chalk.bold,
  'formula': chalk.inverse
}
```

###### Type

```ts
type Sheet = Record<string, Style>
```

### `Style`

Color something (TypeScript type).

###### Parameters

*   `value` (`string`)
    — input

###### Returns

Output (`string`).

## Compatibility

This projects is compatible with maintained versions of Node.js.

When we cut a new major release,
we drop support for unmaintained versions of Node.
This means we try to keep the current release line,
`emphasize@7`,
compatible with Node.js 16.

## Security

This package is safe.

## Contribute

Yes please!
See [How to Contribute to Open Source][open-source-guide-contribute].

## License

[MIT][file-license] © [Titus Wormer][wooorm]

<!-- Definitions -->

[api-all]: #all

[api-auto-options]: #autooptions

[api-common]: #common

[api-create-emphasize]: #createemphasizegrammars

[api-language-fn]: #languagefn

[api-result]: #result

[api-sheet]: #sheet

[api-style]: #style

[badge-build-image]: https://github.com/wooorm/emphasize/workflows/main/badge.svg

[badge-build-url]: https://github.com/wooorm/emphasize/actions

[badge-coverage-image]: https://img.shields.io/codecov/c/github/wooorm/emphasize.svg

[badge-coverage-url]: https://codecov.io/github/wooorm/emphasize

[badge-downloads-image]: https://img.shields.io/npm/dm/emphasize.svg

[badge-downloads-url]: https://www.npmjs.com/package/emphasize

[badge-size-image]: https://img.shields.io/bundlejs/size/emphasize

[badge-size-url]: https://bundlejs.com/?q=emphasize

[npm-install]: https://docs.npmjs.com/cli/install

[esm-sh]: https://esm.sh

[file-license]: license

[github-chalk-styles]: https://github.com/chalk/chalk#styles

[github-gist-esm]: https://gist.github.com/sindresorhus/a39789f98801d908bbc7ff3ecc99d99c

[github-highlight-classes]: https://highlightjs.readthedocs.io/en/latest/css-classes-reference.html

[github-highlight-names]: https://github.com/highlightjs/highlight.js/blob/main/SUPPORTED_LANGUAGES.md

[github-lowlight]: https://github.com/wooorm/lowlight

[github-lowlight-all]: https://github.com/wooorm/lowlight#all

[github-lowlight-common]: https://github.com/wooorm/lowlight#common

[github-lowlight-langauge-fn]: https://github.com/wooorm/lowlight#languagefn

[github-lowlight-list-languages]: https://github.com/wooorm/lowlight#lowlightlistlanguages

[github-lowlight-register]: https://github.com/wooorm/lowlight#lowlightregistergrammars

[github-lowlight-register-alias]: https://github.com/wooorm/lowlight#lowlightregisteraliasaliases

[github-lowlight-registered]: https://github.com/wooorm/lowlight#lowlightregisteredaliasorlanguage

[open-source-guide-contribute]: https://opensource.guide/how-to-contribute/

[typescript]: https://www.typescriptlang.org

[wooorm]: https://wooorm.com
