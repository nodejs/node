<!--This file is generated-->

# remark-preset-lint-recommended

[![Build][build-badge]][build]
[![Coverage][coverage-badge]][coverage]
[![Downloads][downloads-badge]][downloads]
[![Size][size-badge]][size]
[![Sponsors][sponsors-badge]][collective]
[![Backers][backers-badge]][collective]
[![Chat][chat-badge]][chat]

remark preset to configure `remark-lint` with settings that prevent
mistakes or stuff that fails across vendors.

## Rules

This preset configures [`remark-lint`](https://github.com/remarkjs/remark-lint) with the following rules:

| Rule | Setting |
| - | - |
| [`remark-lint-final-newline`](https://github.com/remarkjs/remark-lint/tree/main/packages/remark-lint-final-newline) | |
| [`remark-lint-list-item-bullet-indent`](https://github.com/remarkjs/remark-lint/tree/main/packages/remark-lint-list-item-bullet-indent) | |
| [`remark-lint-list-item-indent`](https://github.com/remarkjs/remark-lint/tree/main/packages/remark-lint-list-item-indent) | `'tab-size'` |
| [`remark-lint-no-auto-link-without-protocol`](https://github.com/remarkjs/remark-lint/tree/main/packages/remark-lint-no-auto-link-without-protocol) | |
| [`remark-lint-no-blockquote-without-marker`](https://github.com/remarkjs/remark-lint/tree/main/packages/remark-lint-no-blockquote-without-marker) | |
| [`remark-lint-no-literal-urls`](https://github.com/remarkjs/remark-lint/tree/main/packages/remark-lint-no-literal-urls) | |
| [`remark-lint-ordered-list-marker-style`](https://github.com/remarkjs/remark-lint/tree/main/packages/remark-lint-ordered-list-marker-style) | `'.'` |
| [`remark-lint-hard-break-spaces`](https://github.com/remarkjs/remark-lint/tree/main/packages/remark-lint-hard-break-spaces) | |
| [`remark-lint-no-duplicate-definitions`](https://github.com/remarkjs/remark-lint/tree/main/packages/remark-lint-no-duplicate-definitions) | |
| [`remark-lint-no-heading-content-indent`](https://github.com/remarkjs/remark-lint/tree/main/packages/remark-lint-no-heading-content-indent) | |
| [`remark-lint-no-inline-padding`](https://github.com/remarkjs/remark-lint/tree/main/packages/remark-lint-no-inline-padding) | |
| [`remark-lint-no-shortcut-reference-image`](https://github.com/remarkjs/remark-lint/tree/main/packages/remark-lint-no-shortcut-reference-image) | |
| [`remark-lint-no-shortcut-reference-link`](https://github.com/remarkjs/remark-lint/tree/main/packages/remark-lint-no-shortcut-reference-link) | |
| [`remark-lint-no-undefined-references`](https://github.com/remarkjs/remark-lint/tree/main/packages/remark-lint-no-undefined-references) | |
| [`remark-lint-no-unused-definitions`](https://github.com/remarkjs/remark-lint/tree/main/packages/remark-lint-no-unused-definitions) | |

## Install

This package is [ESM only][esm]:
Node 12+ is needed to use it and it must be `imported`ed instead of `required`d.

[npm][]:

```sh
npm install remark-preset-lint-recommended
```

This package exports no identifiers.
The default export is `remarkPresetLintRecommended`.

## Use

You probably want to use it on the CLI through a config file:

```diff
 …
 "remarkConfig": {
+  "plugins": ["preset-lint-recommended"]
 }
 …
```

Or use it on the CLI directly

```sh
remark -u preset-lint-recommended readme.md
```

Or use this on the API:

```diff
 import {remark} from 'remark'
 import {reporter} from 'vfile-reporter'
 import remarkPresetLintRecommended from 'remark-preset-lint-recommended'

 remark()
+  .use(remarkPresetLintRecommended)
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

[downloads-badge]: https://img.shields.io/npm/dm/remark-preset-lint-recommended.svg

[downloads]: https://www.npmjs.com/package/remark-preset-lint-recommended

[size-badge]: https://img.shields.io/bundlephobia/minzip/remark-preset-lint-recommended.svg

[size]: https://bundlephobia.com/result?p=remark-preset-lint-recommended

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
