# vfile-reporter

[![Build][build-badge]][build]
[![Coverage][coverage-badge]][coverage]
[![Downloads][downloads-badge]][downloads]
[![Sponsors][sponsors-badge]][collective]
[![Backers][backers-badge]][collective]
[![Chat][chat-badge]][chat]

Create a report for a **[vfile][]**.

![Example screenshot of vfile-reporter][screenshot]

## Features

*   [x] Ranges (`3:2` and `3:2-3:6`)
*   [x] Stack-traces to show where awful stuff occurs
*   [x] Successful files (configurable)
*   [x] All of [**VFile**][vfile]’s awesomeness

## Install

This package is [ESM only](https://gist.github.com/sindresorhus/a39789f98801d908bbc7ff3ecc99d99c):
Node 12+ is needed to use it and it must be `import`ed instead of `require`d.

[npm][]:

```sh
npm install vfile-reporter
```

## Use

Say `example.js` contains:

```js
import {VFile} from 'vfile'
import {reporter} from 'vfile-reporter'

var one = new VFile({path: 'test/fixture/1.js'})
var two = new VFile({path: 'test/fixture/2.js'})

one.message('Warning!', {line: 2, column: 4})

console.error(reporter([one, two]))
```

Now, running `node example` yields:

```txt
test/fixture/1.js
  2:4  warning  Warning!

test/fixture/2.js: no issues found

⚠ 1 warning
```

## API

This package exports the following identifiers: `reporter`.
That identifier is also the default export.

### `reporter(files[, options])`

Generate a stylish report from the given [`vfile`][vfile], `Array.<VFile>`,
or `Error`.

##### `options`

###### `options.verbose`

Output long form descriptions of messages, if applicable (`boolean`, default:
`false`).

###### `options.quiet`

Do not output anything for a file which has no warnings or errors (`boolean`,
default: `false`).
The default behavior is to show a success message.

###### `options.silent`

Do not output messages without `fatal` set to true (`boolean`, default:
`false`).
Also sets `quiet` to `true`.

###### `options.color`

Whether to use color (`boolean`, default: depends).
The default behavior is the check if [color is supported][supports-color].

###### `options.defaultName`

Label to use for files without file-path (`string`, default: `'<stdin>'`).
If one file and no `defaultName` is given, no name will show up in the report.

## Related

*   [`vfile-reporter-json`](https://github.com/vfile/vfile-reporter-json)
    — JSON reporter
*   [`vfile-reporter-pretty`](https://github.com/vfile/vfile-reporter-pretty)
    — Pretty reporter
*   [`convert-vinyl-to-vfile`](https://github.com/dustinspecker/convert-vinyl-to-vfile)
    — Convert from [Vinyl][]
*   [`vfile-statistics`](https://github.com/vfile/vfile-statistics)
    — Count messages per category
*   [`vfile-sort`](https://github.com/vfile/vfile-sort)
    — Sort messages by line/column

## Contribute

See [`contributing.md`][contributing] in [`vfile/.github`][health] for ways to
get started.
See [`support.md`][support] for ways to get help.

This project has a [code of conduct][coc].
By interacting with this repository, organization, or community you agree to
abide by its terms.

## License

[MIT][license] © [Titus Wormer][author]

Forked from [ESLint][]’s stylish reporter
(originally created by Sindre Sorhus), which is Copyright (c) 2013
Nicholas C. Zakas, and licensed under MIT.

<!-- Definitions -->

[build-badge]: https://github.com/vfile/vfile-reporter/workflows/main/badge.svg

[build]: https://github.com/vfile/vfile-reporter/actions

[coverage-badge]: https://img.shields.io/codecov/c/github/vfile/vfile-reporter.svg

[coverage]: https://codecov.io/github/vfile/vfile-reporter

[downloads-badge]: https://img.shields.io/npm/dm/vfile-reporter.svg

[downloads]: https://www.npmjs.com/package/vfile-reporter

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

[eslint]: https://github.com/eslint/eslint

[vfile]: https://github.com/vfile/vfile

[screenshot]: ./screenshot.png

[supports-color]: https://github.com/chalk/supports-color

[vinyl]: https://github.com/gulpjs/vinyl
