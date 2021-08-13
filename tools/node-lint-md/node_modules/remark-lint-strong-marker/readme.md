<!--This file is generated-->

# remark-lint-strong-marker

[![Build][build-badge]][build]
[![Coverage][coverage-badge]][coverage]
[![Downloads][downloads-badge]][downloads]
[![Size][size-badge]][size]
[![Sponsors][sponsors-badge]][collective]
[![Backers][backers-badge]][collective]
[![Chat][chat-badge]][chat]

Warn for violating importance (strong) markers.

Options: `'consistent'`, `'*'`, or `'_'`, default: `'consistent'`.

`'consistent'` detects the first used importance style and warns when
subsequent importance sequences use different styles.

## Fix

[`remark-stringify`](https://github.com/remarkjs/remark/tree/HEAD/packages/remark-stringify)
formats importance using an `*` (asterisk) by default.
Pass
[`strong: '_'`](https://github.com/remarkjs/remark/tree/HEAD/packages/remark-stringify#optionsstrong)
to use `_` (underscore) instead.

See [Using remark to fix your Markdown](https://github.com/remarkjs/remark-lint#using-remark-to-fix-your-markdown)
on how to automatically fix warnings for this rule.

## Presets

This rule is included in the following presets:

| Preset | Setting |
| - | - |
| [`remark-preset-lint-consistent`](https://github.com/remarkjs/remark-lint/tree/main/packages/remark-preset-lint-consistent) | `'consistent'` |
| [`remark-preset-lint-markdown-style-guide`](https://github.com/remarkjs/remark-lint/tree/main/packages/remark-preset-lint-markdown-style-guide) | `'*'` |

## Example

##### `ok.md`

###### In

```markdown
**foo** and **bar**.
```

###### Out

No messages.

##### `also-ok.md`

###### In

```markdown
__foo__ and __bar__.
```

###### Out

No messages.

##### `not-ok.md`

###### In

```markdown
**foo** and __bar__.
```

###### Out

```text
1:13-1:20: Strong should use `*` as a marker
```

##### `ok.md`

When configured with `'*'`.

###### In

```markdown
**foo**.
```

###### Out

No messages.

##### `ok.md`

When configured with `'_'`.

###### In

```markdown
__foo__.
```

###### Out

No messages.

##### `not-ok.md`

When configured with `'💩'`.

###### Out

```text
1:1: Incorrect strong marker `💩`: use either `'consistent'`, `'*'`, or `'_'`
```

## Install

This package is [ESM only][esm]:
Node 12+ is needed to use it and it must be `imported`ed instead of `required`d.

[npm][]:

```sh
npm install remark-lint-strong-marker
```

This package exports no identifiers.
The default export is `remarkLintStrongMarker`.

## Use

You probably want to use it on the CLI through a config file:

```diff
 …
 "remarkConfig": {
   "plugins": [
     …
     "lint",
+    "lint-strong-marker",
     …
   ]
 }
 …
```

Or use it on the CLI directly

```sh
remark -u lint -u lint-strong-marker readme.md
```

Or use this on the API:

```diff
 import {remark} from 'remark'
 import {reporter} from 'vfile-reporter'
 import remarkLint from 'remark-lint'
 import remarkLintStrongMarker from 'remark-lint-strong-marker'

 remark()
   .use(remarkLint)
+  .use(remarkLintStrongMarker)
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

[downloads-badge]: https://img.shields.io/npm/dm/remark-lint-strong-marker.svg

[downloads]: https://www.npmjs.com/package/remark-lint-strong-marker

[size-badge]: https://img.shields.io/bundlephobia/minzip/remark-lint-strong-marker.svg

[size]: https://bundlephobia.com/result?p=remark-lint-strong-marker

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
