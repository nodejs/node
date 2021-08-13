<!--This file is generated-->

# remark-lint-table-cell-padding

[![Build][build-badge]][build]
[![Coverage][coverage-badge]][coverage]
[![Downloads][downloads-badge]][downloads]
[![Size][size-badge]][size]
[![Sponsors][sponsors-badge]][collective]
[![Backers][backers-badge]][collective]
[![Chat][chat-badge]][chat]

Warn when table cells are incorrectly padded.

Options: `'consistent'`, `'padded'`, or `'compact'`, default: `'consistent'`.

`'consistent'` detects the first used cell padding style and warns when
subsequent cells use different styles.

## Fix

[`remark-stringify`](https://github.com/remarkjs/remark/tree/HEAD/packages/remark-stringify)
formats tables with padding by default.
Pass
[`spacedTable: false`](https://github.com/remarkjs/remark/tree/HEAD/packages/remark-stringify#optionsspacedtable)
to not use padding.

See [Using remark to fix your Markdown](https://github.com/remarkjs/remark-lint#using-remark-to-fix-your-markdown)
on how to automatically fix warnings for this rule.

## Presets

This rule is included in the following presets:

| Preset | Setting |
| - | - |
| [`remark-preset-lint-consistent`](https://github.com/remarkjs/remark-lint/tree/main/packages/remark-preset-lint-consistent) | `'consistent'` |
| [`remark-preset-lint-markdown-style-guide`](https://github.com/remarkjs/remark-lint/tree/main/packages/remark-preset-lint-markdown-style-guide) | `'padded'` |

## Example

##### `ok.md`

When configured with `'padded'`.

###### In

Note: this example uses [GFM][].

```markdown
| A     | B     |
| ----- | ----- |
| Alpha | Bravo |
```

###### Out

No messages.

##### `not-ok.md`

When configured with `'padded'`.

###### In

Note: this example uses [GFM][].

```markdown
| A    |    B |
| :----|----: |
| Alpha|Bravo |

| C      |    D |
| :----- | ---: |
|Charlie | Delta|

Too much padding isn’t good either:

| E     | F        |   G    |      H |
| :---- | -------- | :----: | -----: |
| Echo  | Foxtrot  |  Golf  |  Hotel |
```

###### Out

```text
3:8: Cell should be padded
3:9: Cell should be padded
7:2: Cell should be padded
7:17: Cell should be padded
13:9: Cell should be padded with 1 space, not 2
13:20: Cell should be padded with 1 space, not 2
13:21: Cell should be padded with 1 space, not 2
13:29: Cell should be padded with 1 space, not 2
13:30: Cell should be padded with 1 space, not 2
```

##### `empty.md`

When configured with `'padded'`.

###### In

Note: this example uses [GFM][].

```markdown
<!-- Empty cells are OK, but those surrounding them may not be. -->

|        | Alpha | Bravo|
| ------ | ----- | ---: |
| Charlie|       |  Echo|
```

###### Out

```text
3:25: Cell should be padded
5:10: Cell should be padded
5:25: Cell should be padded
```

##### `missing-body.md`

When configured with `'padded'`.

###### In

Note: this example uses [GFM][].

```markdown
<!-- Missing cells are fine as well. -->

| Alpha | Bravo   | Charlie |
| ----- | ------- | ------- |
| Delta |
| Echo  | Foxtrot |
```

###### Out

No messages.

##### `ok.md`

When configured with `'compact'`.

###### In

Note: this example uses [GFM][].

```markdown
|A    |B    |
|-----|-----|
|Alpha|Bravo|
```

###### Out

No messages.

##### `not-ok.md`

When configured with `'compact'`.

###### In

Note: this example uses [GFM][].

```markdown
|   A    | B    |
|   -----| -----|
|   Alpha| Bravo|

|C      |     D|
|:------|-----:|
|Charlie|Delta |
```

###### Out

```text
3:2: Cell should be compact
3:11: Cell should be compact
7:16: Cell should be compact
```

##### `ok-padded.md`

When configured with `'consistent'`.

###### In

Note: this example uses [GFM][].

```markdown
| A     | B     |
| ----- | ----- |
| Alpha | Bravo |

| C       | D     |
| ------- | ----- |
| Charlie | Delta |
```

###### Out

No messages.

##### `not-ok-padded.md`

When configured with `'consistent'`.

###### In

Note: this example uses [GFM][].

```markdown
| A     | B     |
| ----- | ----- |
| Alpha | Bravo |

| C      |     D |
| :----- | ----: |
|Charlie | Delta |
```

###### Out

```text
7:2: Cell should be padded
```

##### `ok-compact.md`

When configured with `'consistent'`.

###### In

Note: this example uses [GFM][].

```markdown
|A    |B    |
|-----|-----|
|Alpha|Bravo|

|C      |D    |
|-------|-----|
|Charlie|Delta|
```

###### Out

No messages.

##### `not-ok-compact.md`

When configured with `'consistent'`.

###### In

Note: this example uses [GFM][].

```markdown
|A    |B    |
|-----|-----|
|Alpha|Bravo|

|C      |     D|
|:------|-----:|
|Charlie|Delta |
```

###### Out

```text
7:16: Cell should be compact
```

##### `not-ok.md`

When configured with `'💩'`.

###### Out

```text
1:1: Incorrect table cell padding style `💩`, expected `'padded'`, `'compact'`, or `'consistent'`
```

## Install

This package is [ESM only][esm]:
Node 12+ is needed to use it and it must be `imported`ed instead of `required`d.

[npm][]:

```sh
npm install remark-lint-table-cell-padding
```

This package exports no identifiers.
The default export is `remarkLintTableCellPadding`.

## Use

You probably want to use it on the CLI through a config file:

```diff
 …
 "remarkConfig": {
   "plugins": [
     …
     "lint",
+    "lint-table-cell-padding",
     …
   ]
 }
 …
```

Or use it on the CLI directly

```sh
remark -u lint -u lint-table-cell-padding readme.md
```

Or use this on the API:

```diff
 import {remark} from 'remark'
 import {reporter} from 'vfile-reporter'
 import remarkLint from 'remark-lint'
 import remarkLintTableCellPadding from 'remark-lint-table-cell-padding'

 remark()
   .use(remarkLint)
+  .use(remarkLintTableCellPadding)
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

[downloads-badge]: https://img.shields.io/npm/dm/remark-lint-table-cell-padding.svg

[downloads]: https://www.npmjs.com/package/remark-lint-table-cell-padding

[size-badge]: https://img.shields.io/bundlephobia/minzip/remark-lint-table-cell-padding.svg

[size]: https://bundlephobia.com/result?p=remark-lint-table-cell-padding

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
