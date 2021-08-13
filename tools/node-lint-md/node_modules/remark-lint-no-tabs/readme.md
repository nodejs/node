<!--This file is generated-->

# remark-lint-no-tabs

[![Build][build-badge]][build]
[![Coverage][coverage-badge]][coverage]
[![Downloads][downloads-badge]][downloads]
[![Size][size-badge]][size]
[![Sponsors][sponsors-badge]][collective]
[![Backers][backers-badge]][collective]
[![Chat][chat-badge]][chat]

Warn when hard tabs (`\t`) are used instead of spaces.

## Fix

[`remark-stringify`](https://github.com/remarkjs/remark/tree/HEAD/packages/remark-stringify)
uses spaces where tabs are used for indentation, but retains tabs used in
content.

See [Using remark to fix your Markdown](https://github.com/remarkjs/remark-lint#using-remark-to-fix-your-markdown)
on how to automatically fix warnings for this rule.

## Presets

This rule is not included in any default preset

## Example

##### `ok.md`

###### In

Note: `·` represents a space.

```markdown
Foo Bar

····Foo
```

###### Out

No messages.

##### `not-ok.md`

###### In

Note: `»` represents a tab.

```markdown
»Here's one before a code block.

Here's a tab:», and here is another:».

And this is in `inline»code`.

>»This is in a block quote.

*»And…

»1.»in a list.

And this is a tab as the last character.»
```

###### Out

```text
1:1: Use spaces instead of tabs
3:14: Use spaces instead of tabs
3:37: Use spaces instead of tabs
5:23: Use spaces instead of tabs
7:2: Use spaces instead of tabs
9:2: Use spaces instead of tabs
11:1: Use spaces instead of tabs
11:4: Use spaces instead of tabs
13:41: Use spaces instead of tabs
```

## Install

This package is [ESM only][esm]:
Node 12+ is needed to use it and it must be `imported`ed instead of `required`d.

[npm][]:

```sh
npm install remark-lint-no-tabs
```

This package exports no identifiers.
The default export is `remarkLintNoTabs`.

## Use

You probably want to use it on the CLI through a config file:

```diff
 …
 "remarkConfig": {
   "plugins": [
     …
     "lint",
+    "lint-no-tabs",
     …
   ]
 }
 …
```

Or use it on the CLI directly

```sh
remark -u lint -u lint-no-tabs readme.md
```

Or use this on the API:

```diff
 import {remark} from 'remark'
 import {reporter} from 'vfile-reporter'
 import remarkLint from 'remark-lint'
 import remarkLintNoTabs from 'remark-lint-no-tabs'

 remark()
   .use(remarkLint)
+  .use(remarkLintNoTabs)
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

[downloads-badge]: https://img.shields.io/npm/dm/remark-lint-no-tabs.svg

[downloads]: https://www.npmjs.com/package/remark-lint-no-tabs

[size-badge]: https://img.shields.io/bundlephobia/minzip/remark-lint-no-tabs.svg

[size]: https://bundlephobia.com/result?p=remark-lint-no-tabs

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
