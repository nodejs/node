# unified-lint-rule

[![Build][build-badge]][build]
[![Coverage][coverage-badge]][coverage]
[![Downloads][downloads-badge]][downloads]
[![Size][size-badge]][size]
[![Sponsors][sponsors-badge]][collective]
[![Backers][backers-badge]][collective]
[![Chat][chat-badge]][chat]

[**unified**][unified] plugin to make it a bit easier to create linting rules.

Each rule in [`remark-lint`][lint] uses this project, so see that for examples!

## Install

This package is [ESM only](https://gist.github.com/sindresorhus/a39789f98801d908bbc7ff3ecc99d99c):
Node 12+ is needed to use it and it must be `import`ed instead of `require`d.

[npm][]:

```sh
npm install unified-lint-rule
```

## Use

```js
import {lintRule} from 'unified-lint-rule'

const remarkLintFileExtension = lintRule(
  'remark-lint:file-extension',
  (tree, file, option = 'md') => {
    var ext = file.extname

    if (ext && ext.slice(1) !== option) {
      file.message('Incorrect extension: use `' + option + '`')
    }
  }
)

export default remarkLintFileExtension
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

[downloads-badge]: https://img.shields.io/npm/dm/unified-lint-rule.svg

[downloads]: https://www.npmjs.com/package/unified-lint-rule

[size-badge]: https://img.shields.io/bundlephobia/minzip/unified-lint-rule.svg

[size]: https://bundlephobia.com/result?p=unified-lint-rule

[sponsors-badge]: https://opencollective.com/unified/sponsors/badge.svg

[backers-badge]: https://opencollective.com/unified/backers/badge.svg

[collective]: https://opencollective.com/unified

[chat-badge]: https://img.shields.io/badge/chat-discussions-success.svg

[chat]: https://github.com/remarkjs/remark/discussions

[npm]: https://docs.npmjs.com/cli/install

[health]: https://github.com/remarkjs/.github

[contributing]: https://github.com/remarkjs/.github/blob/HEAD/contributing.md

[support]: https://github.com/remarkjs/.github/blob/HEAD/support.md

[coc]: https://github.com/remarkjs/.github/blob/HEAD/code-of-conduct.md

[license]: https://github.com/remarkjs/remark-lint/blob/main/license

[author]: https://wooorm.com

[unified]: https://github.com/unifiedjs/unified

[lint]: https://github.com/remarkjs/remark-lint
