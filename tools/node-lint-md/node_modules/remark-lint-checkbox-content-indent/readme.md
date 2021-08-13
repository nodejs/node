<!--This file is generated-->

# remark-lint-checkbox-content-indent

[![Build][build-badge]][build]
[![Coverage][coverage-badge]][coverage]
[![Downloads][downloads-badge]][downloads]
[![Size][size-badge]][size]
[![Sponsors][sponsors-badge]][collective]
[![Backers][backers-badge]][collective]
[![Chat][chat-badge]][chat]

Warn when list item checkboxes are followed by too much whitespace.

## Presets

This rule is not included in any default preset

## Example

##### `ok.md`

###### In

Note: this example uses [GFM][].

```markdown
- [ ] List item
+  [x] List Item
*   [X] List item
-    [ ] List item
```

###### Out

No messages.

##### `not-ok.md`

###### In

Note: this example uses [GFM][].

```markdown
- [ ] List item
+ [x]  List item
* [X]   List item
- [ ]    List item
```

###### Out

```text
2:7-2:8: Checkboxes should be followed by a single character
3:7-3:9: Checkboxes should be followed by a single character
4:7-4:10: Checkboxes should be followed by a single character
```

## Install

This package is [ESM only][esm]:
Node 12+ is needed to use it and it must be `imported`ed instead of `required`d.

[npm][]:

```sh
npm install remark-lint-checkbox-content-indent
```

This package exports no identifiers.
The default export is `remarkLintCheckboxContentIndent`.

## Use

You probably want to use it on the CLI through a config file:

```diff
 …
 "remarkConfig": {
   "plugins": [
     …
     "lint",
+    "lint-checkbox-content-indent",
     …
   ]
 }
 …
```

Or use it on the CLI directly

```sh
remark -u lint -u lint-checkbox-content-indent readme.md
```

Or use this on the API:

```diff
 import {remark} from 'remark'
 import {reporter} from 'vfile-reporter'
 import remarkLint from 'remark-lint'
 import remarkLintCheckboxContentIndent from 'remark-lint-checkbox-content-indent'

 remark()
   .use(remarkLint)
+  .use(remarkLintCheckboxContentIndent)
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

[downloads-badge]: https://img.shields.io/npm/dm/remark-lint-checkbox-content-indent.svg

[downloads]: https://www.npmjs.com/package/remark-lint-checkbox-content-indent

[size-badge]: https://img.shields.io/bundlephobia/minzip/remark-lint-checkbox-content-indent.svg

[size]: https://bundlephobia.com/result?p=remark-lint-checkbox-content-indent

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
