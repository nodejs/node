# vfile-statistics

[![Build][build-badge]][build]
[![Coverage][coverage-badge]][coverage]
[![Downloads][downloads-badge]][downloads]
[![Size][size-badge]][size]
[![Sponsors][sponsors-badge]][collective]
[![Backers][backers-badge]][collective]
[![Chat][chat-badge]][chat]

Count [vfile][] messages per category (fatal, warn, info, nonfatal and total).

## Install

This package is [ESM only](https://gist.github.com/sindresorhus/a39789f98801d908bbc7ff3ecc99d99c):
Node 12+ is needed to use it and it must be `import`ed instead of `require`d.

[npm][]:

```sh
npm install vfile-statistics
```

## Use

```js
import {VFile} from 'vfile'
import {statistics} from 'vfile-statistics'

var file = new VFile({path: '~/example.md'})

file.message('This could be better')
file.message('That could be better')

try {
  file.fail('This is terribly wrong')
} catch (err) {}

file.info('This is perfect')

console.log(statistics(file))
```

Yields:

```js
{fatal: 1, nonfatal: 3, warn: 2, info: 1, total: 4}
```

## API

This package exports the following identifiers: `statistics`.
There is no default export.

### `statistics(file)`

Pass a [vfile][], list of vfiles, or a list of messages (`file.messages`), get
counts per category.

###### Returns

`Object`:

*   `fatal`: fatal errors (`fatal: true`)
*   `warn`: warning messages (`fatal: false`)
*   `info`: informational messages (`fatal: null` or `fatal: undefined`)
*   `nonfatal`: warning or info messages
*   `total`: all messages

## Contribute

See [`contributing.md`][contributing] in [`vfile/.github`][health] for ways to
get started.
See [`support.md`][support] for ways to get help.

This project has a [code of conduct][coc].
By interacting with this repository, organization, or community you agree to
abide by its terms.

## License

[MIT][license] © [Titus Wormer][author]

<!-- Definitions -->

[build-badge]: https://github.com/vfile/vfile-statistics/workflows/main/badge.svg

[build]: https://github.com/vfile/vfile-statistics/actions

[coverage-badge]: https://img.shields.io/codecov/c/github/vfile/vfile-statistics.svg

[coverage]: https://codecov.io/github/vfile/vfile-statistics

[downloads-badge]: https://img.shields.io/npm/dm/vfile-statistics.svg

[downloads]: https://www.npmjs.com/package/vfile-statistics

[size-badge]: https://img.shields.io/bundlephobia/minzip/vfile-statistics.svg

[size]: https://bundlephobia.com/result?p=vfile-statistics

[sponsors-badge]: https://opencollective.com/unified/sponsors/badge.svg

[backers-badge]: https://opencollective.com/unified/backers/badge.svg

[collective]: https://opencollective.com/unified

[chat-badge]: https://img.shields.io/badge/chat-discussions-success.svg

[chat]: https://github.com/vfile/vfile/discussions

[npm]: https://docs.npmjs.com/cli/install

[contributing]: https://github.com/vfile/.github/blob/HEAD/contributing.md

[support]: https://github.com/vfile/.github/blob/HEAD/support.md

[health]: https://github.com/vfile/.github

[coc]: https://github.com/vfile/.github/blob/HEAD/code-of-conduct.md

[license]: license

[author]: https://wooorm.com

[vfile]: https://github.com/vfile/vfile
