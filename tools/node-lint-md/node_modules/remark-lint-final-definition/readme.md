<!--This file is generated-->

# remark-lint-final-definition

[![Build][build-badge]][build]
[![Coverage][coverage-badge]][coverage]
[![Downloads][downloads-badge]][downloads]
[![Size][size-badge]][size]
[![Sponsors][sponsors-badge]][collective]
[![Backers][backers-badge]][collective]
[![Chat][chat-badge]][chat]

Warn when definitions are placed somewhere other than at the end of
the file.

## Presets

This rule is included in the following presets:

| Preset | Setting |
| - | - |
| [`remark-preset-lint-markdown-style-guide`](https://github.com/remarkjs/remark-lint/tree/main/packages/remark-preset-lint-markdown-style-guide) | |

## Example

##### `ok.md`

###### In

```markdown
Paragraph.

[example]: http://example.com "Example Domain"
```

###### Out

No messages.

##### `not-ok.md`

###### In

```markdown
Paragraph.

[example]: http://example.com "Example Domain"

Another paragraph.
```

###### Out

```text
3:1-3:47: Move definitions to the end of the file (after the node at line `5`)
```

##### `ok-comments.md`

###### In

```markdown
Paragraph.

[example-1]: http://example.com/one/

<!-- Comments are fine between and after definitions -->

[example-2]: http://example.com/two/
```

###### Out

No messages.

## Install

This package is [ESM only][esm]:
Node 12+ is needed to use it and it must be `imported`ed instead of `required`d.

[npm][]:

```sh
npm install remark-lint-final-definition
```

This package exports no identifiers.
The default export is `remarkLintFinalDefinition`.

## Use

You probably want to use it on the CLI through a config file:

```diff
 …
 "remarkConfig": {
   "plugins": [
     …
     "lint",
+    "lint-final-definition",
     …
   ]
 }
 …
```

Or use it on the CLI directly

```sh
remark -u lint -u lint-final-definition readme.md
```

Or use this on the API:

```diff
 import {remark} from 'remark'
 import {reporter} from 'vfile-reporter'
 import remarkLint from 'remark-lint'
 import remarkLintFinalDefinition from 'remark-lint-final-definition'

 remark()
   .use(remarkLint)
+  .use(remarkLintFinalDefinition)
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

[downloads-badge]: https://img.shields.io/npm/dm/remark-lint-final-definition.svg

[downloads]: https://www.npmjs.com/package/remark-lint-final-definition

[size-badge]: https://img.shields.io/bundlephobia/minzip/remark-lint-final-definition.svg

[size]: https://bundlephobia.com/result?p=remark-lint-final-definition

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
