<!--This file is generated-->

# remark-lint-blockquote-indentation

[![Build][build-badge]][build]
[![Coverage][coverage-badge]][coverage]
[![Downloads][downloads-badge]][downloads]
[![Size][size-badge]][size]
[![Sponsors][sponsors-badge]][collective]
[![Backers][backers-badge]][collective]
[![Chat][chat-badge]][chat]

Warn when block quotes are indented too much or too little.

Options: `number` or `'consistent'`, default: `'consistent'`.

`'consistent'` detects the first used indentation and will warn when
other block quotes use a different indentation.

## Presets

This rule is included in the following presets:

| Preset | Setting |
| - | - |
| [`remark-preset-lint-consistent`](https://github.com/remarkjs/remark-lint/tree/main/packages/remark-preset-lint-consistent) | `'consistent'` |
| [`remark-preset-lint-markdown-style-guide`](https://github.com/remarkjs/remark-lint/tree/main/packages/remark-preset-lint-markdown-style-guide) | `2` |

## Example

##### `ok.md`

When configured with `2`.

###### In

```markdown
> Hello

Paragraph.

> World
```

###### Out

No messages.

##### `ok.md`

When configured with `4`.

###### In

```markdown
>   Hello

Paragraph.

>   World
```

###### Out

No messages.

##### `not-ok.md`

###### In

```markdown
>  Hello

Paragraph.

>   World

Paragraph.

> World
```

###### Out

```text
5:5: Remove 1 space between block quote and content
9:3: Add 1 space between block quote and content
```

## Install

This package is [ESM only][esm]:
Node 12+ is needed to use it and it must be `imported`ed instead of `required`d.

[npm][]:

```sh
npm install remark-lint-blockquote-indentation
```

This package exports no identifiers.
The default export is `remarkLintBlockquoteIndentation`.

## Use

You probably want to use it on the CLI through a config file:

```diff
 …
 "remarkConfig": {
   "plugins": [
     …
     "lint",
+    "lint-blockquote-indentation",
     …
   ]
 }
 …
```

Or use it on the CLI directly

```sh
remark -u lint -u lint-blockquote-indentation readme.md
```

Or use this on the API:

```diff
 import {remark} from 'remark'
 import {reporter} from 'vfile-reporter'
 import remarkLint from 'remark-lint'
 import remarkLintBlockquoteIndentation from 'remark-lint-blockquote-indentation'

 remark()
   .use(remarkLint)
+  .use(remarkLintBlockquoteIndentation)
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

[downloads-badge]: https://img.shields.io/npm/dm/remark-lint-blockquote-indentation.svg

[downloads]: https://www.npmjs.com/package/remark-lint-blockquote-indentation

[size-badge]: https://img.shields.io/bundlephobia/minzip/remark-lint-blockquote-indentation.svg

[size]: https://bundlephobia.com/result?p=remark-lint-blockquote-indentation

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
