<!--This file is generated-->

# remark-lint-maximum-line-length

[![Build][build-badge]][build]
[![Coverage][coverage-badge]][coverage]
[![Downloads][downloads-badge]][downloads]
[![Size][size-badge]][size]
[![Sponsors][sponsors-badge]][collective]
[![Backers][backers-badge]][collective]
[![Chat][chat-badge]][chat]

Warn when lines are too long.

Options: `number`, default: `80`.

Ignores nodes that cannot be wrapped, such as headings, tables, code,
definitions, HTML, and JSX.

Ignores images, links, and inline code if they start before the wrap, end
after the wrap, and there’s no whitespace after them.

## Presets

This rule is included in the following presets:

| Preset | Setting |
| - | - |
| [`remark-preset-lint-markdown-style-guide`](https://github.com/remarkjs/remark-lint/tree/main/packages/remark-preset-lint-markdown-style-guide) | `80` |

## Example

##### `ok-mixed-line-endings.md`

When configured with `10`.

###### In

Note: `␍␊` represents a carriage return and a line feed.

Note: `␊` represents a line feed.

```markdown
0123456789␍␊
0123456789␊
01234␍␊
01234␊
```

###### Out

No messages.

##### `not-ok-mixed-line-endings.md`

When configured with `10`.

###### In

Note: `␍␊` represents a carriage return and a line feed.

Note: `␊` represents a line feed.

```markdown
012345678901␍␊
012345678901␊
01234567890␍␊
01234567890␊
```

###### Out

```text
1:13: Line must be at most 10 characters
2:13: Line must be at most 10 characters
3:12: Line must be at most 10 characters
4:12: Line must be at most 10 characters
```

##### `not-ok.md`

When configured with `80`.

###### In

```markdown
This line is simply not tooooooooooooooooooooooooooooooooooooooooooooooooooooooo
long.

Just like thiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiis one.

And this one is also very wrong: because the link starts aaaaaaafter the column: <http://line.com>

<http://this-long-url-with-a-long-domain-is-not-ok.co.uk/a-long-path?query=variables> and such.

And this one is also very wrong: because the code starts aaaaaaafter the column: `alpha.bravo()`

`alphaBravoCharlieDeltaEchoFoxtrotGolfHotelIndiaJuliettKiloLimaMikeNovemberOscar.papa()` and such.
```

###### Out

```text
4:86: Line must be at most 80 characters
6:99: Line must be at most 80 characters
8:96: Line must be at most 80 characters
10:97: Line must be at most 80 characters
12:99: Line must be at most 80 characters
```

##### `ok.md`

###### In

Note: this example uses [GFM][].

```markdown
This line is simply not toooooooooooooooooooooooooooooooooooooooooooo
long.

This is also fine: <http://this-long-url-with-a-long-domain.co.uk/a-long-path?query=variables>

<http://this-link-is-fine.com>

`alphaBravoCharlieDeltaEchoFoxtrotGolfHotelIndiaJuliettKiloLimaMikeNovemberOscarPapaQuebec.romeo()`

[foo](http://this-long-url-with-a-long-domain-is-ok.co.uk/a-long-path?query=variables)

<http://this-long-url-with-a-long-domain-is-ok.co.uk/a-long-path?query=variables>

![foo](http://this-long-url-with-a-long-domain-is-ok.co.uk/a-long-path?query=variables)

| An | exception | is | line | length | in | long | tables | because | those | can’t | just |
| -- | --------- | -- | ---- | ------ | -- | ---- | ------ | ------- | ----- | ----- | ---- |
| be | helped    |    |      |        |    |      |        |         |       |       | .    |

<a><b><i><p><q><s><u>alpha bravo charlie delta echo foxtrot golf</u></s></q></p></i></b></a>

The following is also fine, because there is no whitespace.

<http://this-long-url-with-a-long-domain-is-ok.co.uk/a-long-path?query=variables>.

In addition, definitions are also fine:

[foo]: <http://this-long-url-with-a-long-domain-is-ok.co.uk/a-long-path?query=variables>
```

###### Out

No messages.

## Install

This package is [ESM only][esm]:
Node 12+ is needed to use it and it must be `imported`ed instead of `required`d.

[npm][]:

```sh
npm install remark-lint-maximum-line-length
```

This package exports no identifiers.
The default export is `remarkLintMaximumLineLength`.

## Use

You probably want to use it on the CLI through a config file:

```diff
 …
 "remarkConfig": {
   "plugins": [
     …
     "lint",
+    "lint-maximum-line-length",
     …
   ]
 }
 …
```

Or use it on the CLI directly

```sh
remark -u lint -u lint-maximum-line-length readme.md
```

Or use this on the API:

```diff
 import {remark} from 'remark'
 import {reporter} from 'vfile-reporter'
 import remarkLint from 'remark-lint'
 import remarkLintMaximumLineLength from 'remark-lint-maximum-line-length'

 remark()
   .use(remarkLint)
+  .use(remarkLintMaximumLineLength)
   .process('_Emphasis_ and **importance**')
   .then((file) => {
     console.error(reporter(file))
   })
```

## Contribute

See [`contributing.md`][contributing] in [`remarkjs/.github`][health] for ways
to get started.
See [`support.md`][support] for ways to get help.

This project has a [code of conduct][coc].
By interacting with this repository, organization, or community you agree to
abide by its terms.

## License

[MIT][license] © [Titus Wormer][author]

[build-badge]: https://github.com/remarkjs/remark-lint/workflows/main/badge.svg

[build]: https://github.com/remarkjs/remark-lint/actions

[coverage-badge]: https://img.shields.io/codecov/c/github/remarkjs/remark-lint.svg

[coverage]: https://codecov.io/github/remarkjs/remark-lint

[downloads-badge]: https://img.shields.io/npm/dm/remark-lint-maximum-line-length.svg

[downloads]: https://www.npmjs.com/package/remark-lint-maximum-line-length

[size-badge]: https://img.shields.io/bundlephobia/minzip/remark-lint-maximum-line-length.svg

[size]: https://bundlephobia.com/result?p=remark-lint-maximum-line-length

[sponsors-badge]: https://opencollective.com/unified/sponsors/badge.svg

[backers-badge]: https://opencollective.com/unified/backers/badge.svg

[collective]: https://opencollective.com/unified

[chat-badge]: https://img.shields.io/badge/chat-discussions-success.svg

[chat]: https://github.com/remarkjs/remark/discussions

[esm]: https://gist.github.com/sindresorhus/a39789f98801d908bbc7ff3ecc99d99c

[npm]: https://docs.npmjs.com/cli/install

[health]: https://github.com/remarkjs/.github

[contributing]: https://github.com/remarkjs/.github/blob/HEAD/contributing.md

[support]: https://github.com/remarkjs/.github/blob/HEAD/support.md

[coc]: https://github.com/remarkjs/.github/blob/HEAD/code-of-conduct.md

[license]: https://github.com/remarkjs/remark-lint/blob/main/license

[author]: https://wooorm.com

[gfm]: https://github.com/remarkjs/remark-gfm
