<!--This file is generated-->

# remark-lint-ordered-list-marker-style

[![Build][build-badge]][build]
[![Coverage][coverage-badge]][coverage]
[![Downloads][downloads-badge]][downloads]
[![Size][size-badge]][size]
[![Sponsors][sponsors-badge]][collective]
[![Backers][backers-badge]][collective]
[![Chat][chat-badge]][chat]

Warn when the list item marker style of ordered lists violate a given style.

Options: `'consistent'`, `'.'`, or `')'`, default: `'consistent'`.

`'consistent'` detects the first used list style and warns when subsequent
lists use different styles.

## Presets

This rule is included in the following presets:

| Preset | Setting |
| - | - |
| [`remark-preset-lint-consistent`](https://github.com/remarkjs/remark-lint/tree/main/packages/remark-preset-lint-consistent) | `'consistent'` |
| [`remark-preset-lint-markdown-style-guide`](https://github.com/remarkjs/remark-lint/tree/main/packages/remark-preset-lint-markdown-style-guide) | `'.'` |
| [`remark-preset-lint-recommended`](https://github.com/remarkjs/remark-lint/tree/main/packages/remark-preset-lint-recommended) | `'.'` |

## Example

##### `ok.md`

###### In

```markdown
1.  Foo


1.  Bar

Unordered lists are not affected by this rule.

* Foo
```

###### Out

No messages.

##### `not-ok.md`

###### In

```markdown
1.  Foo

2)  Bar
```

###### Out

```text
3:1-3:8: Marker style should be `.`
```

##### `ok.md`

When configured with `'.'`.

###### In

```markdown
1.  Foo

2.  Bar
```

###### Out

No messages.

##### `ok.md`

When configured with `')'`.

###### In

```markdown
1)  Foo

2)  Bar
```

###### Out

No messages.

##### `not-ok.md`

When configured with `'💩'`.

###### Out

```text
1:1: Incorrect ordered list item marker style `💩`: use either `'.'` or `')'`
```

## Install

This package is [ESM only][esm]:
Node 12+ is needed to use it and it must be `imported`ed instead of `required`d.

[npm][]:

```sh
npm install remark-lint-ordered-list-marker-style
```

This package exports no identifiers.
The default export is `remarkLintOrderedListMarkerStyle`.

## Use

You probably want to use it on the CLI through a config file:

```diff
 …
 "remarkConfig": {
   "plugins": [
     …
     "lint",
+    "lint-ordered-list-marker-style",
     …
   ]
 }
 …
```

Or use it on the CLI directly

```sh
remark -u lint -u lint-ordered-list-marker-style readme.md
```

Or use this on the API:

```diff
 import {remark} from 'remark'
 import {reporter} from 'vfile-reporter'
 import remarkLint from 'remark-lint'
 import remarkLintOrderedListMarkerStyle from 'remark-lint-ordered-list-marker-style'

 remark()
   .use(remarkLint)
+  .use(remarkLintOrderedListMarkerStyle)
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

[downloads-badge]: https://img.shields.io/npm/dm/remark-lint-ordered-list-marker-style.svg

[downloads]: https://www.npmjs.com/package/remark-lint-ordered-list-marker-style

[size-badge]: https://img.shields.io/bundlephobia/minzip/remark-lint-ordered-list-marker-style.svg

[size]: https://bundlephobia.com/result?p=remark-lint-ordered-list-marker-style

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
