<!--This file is generated-->

# remark-lint-unordered-list-marker-style

[![Build][build-badge]][build]
[![Coverage][coverage-badge]][coverage]
[![Downloads][downloads-badge]][downloads]
[![Size][size-badge]][size]
[![Sponsors][sponsors-badge]][collective]
[![Backers][backers-badge]][collective]
[![Chat][chat-badge]][chat]

Warn when the list item marker style of unordered lists violate a given
style.

Options: `'consistent'`, `'-'`, `'*'`, or `'+'`, default: `'consistent'`.

`'consistent'` detects the first used list style and warns when subsequent
lists use different styles.

## Fix

[`remark-stringify`](https://github.com/remarkjs/remark/tree/HEAD/packages/remark-stringify)
formats unordered lists using `-` (hyphen-minus) by default.
Pass
[`bullet: '*'` or `bullet: '+'`](https://github.com/remarkjs/remark/tree/HEAD/packages/remark-stringify#optionsbullet)
to use `*` (asterisk) or `+` (plus sign) instead.

See [Using remark to fix your Markdown](https://github.com/remarkjs/remark-lint#using-remark-to-fix-your-markdown)
on how to automatically fix warnings for this rule.

## Presets

This rule is included in the following presets:

| Preset | Setting |
| - | - |
| [`remark-preset-lint-markdown-style-guide`](https://github.com/remarkjs/remark-lint/tree/main/packages/remark-preset-lint-markdown-style-guide) | `'-'` |

## Example

##### `ok.md`

###### In

```markdown
By default (`'consistent'`), if the file uses only one marker,
that’s OK.

* Foo
* Bar
* Baz

Ordered lists are not affected.

1. Foo
2. Bar
3. Baz
```

###### Out

No messages.

##### `not-ok.md`

###### In

```markdown
* Foo
- Bar
+ Baz
```

###### Out

```text
2:1-2:6: Marker style should be `*`
3:1-3:6: Marker style should be `*`
```

##### `ok.md`

When configured with `'*'`.

###### In

```markdown
* Foo
```

###### Out

No messages.

##### `ok.md`

When configured with `'-'`.

###### In

```markdown
- Foo
```

###### Out

No messages.

##### `ok.md`

When configured with `'+'`.

###### In

```markdown
+ Foo
```

###### Out

No messages.

##### `not-ok.md`

When configured with `'💩'`.

###### Out

```text
1:1: Incorrect unordered list item marker style `💩`: use either `'-'`, `'*'`, or `'+'`
```

## Install

This package is [ESM only][esm]:
Node 12+ is needed to use it and it must be `imported`ed instead of `required`d.

[npm][]:

```sh
npm install remark-lint-unordered-list-marker-style
```

This package exports no identifiers.
The default export is `remarkLintUnorderedListMarkerStyle`.

## Use

You probably want to use it on the CLI through a config file:

```diff
 …
 "remarkConfig": {
   "plugins": [
     …
     "lint",
+    "lint-unordered-list-marker-style",
     …
   ]
 }
 …
```

Or use it on the CLI directly

```sh
remark -u lint -u lint-unordered-list-marker-style readme.md
```

Or use this on the API:

```diff
 import {remark} from 'remark'
 import {reporter} from 'vfile-reporter'
 import remarkLint from 'remark-lint'
 import remarkLintUnorderedListMarkerStyle from 'remark-lint-unordered-list-marker-style'

 remark()
   .use(remarkLint)
+  .use(remarkLintUnorderedListMarkerStyle)
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

[downloads-badge]: https://img.shields.io/npm/dm/remark-lint-unordered-list-marker-style.svg

[downloads]: https://www.npmjs.com/package/remark-lint-unordered-list-marker-style

[size-badge]: https://img.shields.io/bundlephobia/minzip/remark-lint-unordered-list-marker-style.svg

[size]: https://bundlephobia.com/result?p=remark-lint-unordered-list-marker-style

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
