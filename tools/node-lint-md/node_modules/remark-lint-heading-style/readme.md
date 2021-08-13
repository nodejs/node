<!--This file is generated-->

# remark-lint-heading-style

[![Build][build-badge]][build]
[![Coverage][coverage-badge]][coverage]
[![Downloads][downloads-badge]][downloads]
[![Size][size-badge]][size]
[![Sponsors][sponsors-badge]][collective]
[![Backers][backers-badge]][collective]
[![Chat][chat-badge]][chat]

Warn when a heading does not conform to a given style.

Options: `'consistent'`, `'atx'`, `'atx-closed'`, or `'setext'`,
default: `'consistent'`.

`'consistent'` detects the first used heading style and warns when
subsequent headings use different styles.

## Fix

[`remark-stringify`](https://github.com/remarkjs/remark/tree/HEAD/packages/remark-stringify)
formats headings as ATX by default.
This can be configured with the
[`setext`](https://github.com/remarkjs/remark/tree/HEAD/packages/remark-stringify#optionssetext)
and
[`closeAtx`](https://github.com/remarkjs/remark/tree/HEAD/packages/remark-stringify#optionscloseatx)
options.

See [Using remark to fix your Markdown](https://github.com/remarkjs/remark-lint#using-remark-to-fix-your-markdown)
on how to automatically fix warnings for this rule.

## Presets

This rule is included in the following presets:

| Preset | Setting |
| - | - |
| [`remark-preset-lint-consistent`](https://github.com/remarkjs/remark-lint/tree/main/packages/remark-preset-lint-consistent) | `'consistent'` |
| [`remark-preset-lint-markdown-style-guide`](https://github.com/remarkjs/remark-lint/tree/main/packages/remark-preset-lint-markdown-style-guide) | `'atx'` |

## Example

##### `ok.md`

When configured with `'atx'`.

###### In

```markdown
# Alpha

## Bravo

### Charlie
```

###### Out

No messages.

##### `ok.md`

When configured with `'atx-closed'`.

###### In

```markdown
# Delta ##

## Echo ##

### Foxtrot ###
```

###### Out

No messages.

##### `ok.md`

When configured with `'setext'`.

###### In

```markdown
Golf
====

Hotel
-----

### India
```

###### Out

No messages.

##### `not-ok.md`

###### In

```markdown
Juliett
=======

## Kilo

### Lima ###
```

###### Out

```text
4:1-4:8: Headings should use setext
6:1-6:13: Headings should use setext
```

##### `not-ok.md`

When configured with `'💩'`.

###### Out

```text
1:1: Incorrect heading style type `💩`: use either `'consistent'`, `'atx'`, `'atx-closed'`, or `'setext'`
```

## Install

This package is [ESM only][esm]:
Node 12+ is needed to use it and it must be `imported`ed instead of `required`d.

[npm][]:

```sh
npm install remark-lint-heading-style
```

This package exports no identifiers.
The default export is `remarkLintHeadingStyle`.

## Use

You probably want to use it on the CLI through a config file:

```diff
 …
 "remarkConfig": {
   "plugins": [
     …
     "lint",
+    "lint-heading-style",
     …
   ]
 }
 …
```

Or use it on the CLI directly

```sh
remark -u lint -u lint-heading-style readme.md
```

Or use this on the API:

```diff
 import {remark} from 'remark'
 import {reporter} from 'vfile-reporter'
 import remarkLint from 'remark-lint'
 import remarkLintHeadingStyle from 'remark-lint-heading-style'

 remark()
   .use(remarkLint)
+  .use(remarkLintHeadingStyle)
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

[downloads-badge]: https://img.shields.io/npm/dm/remark-lint-heading-style.svg

[downloads]: https://www.npmjs.com/package/remark-lint-heading-style

[size-badge]: https://img.shields.io/bundlephobia/minzip/remark-lint-heading-style.svg

[size]: https://bundlephobia.com/result?p=remark-lint-heading-style

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
