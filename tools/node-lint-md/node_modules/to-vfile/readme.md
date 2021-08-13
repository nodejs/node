# to-vfile

[![Build][build-badge]][build]
[![Coverage][coverage-badge]][coverage]
[![Downloads][downloads-badge]][downloads]
[![Sponsors][sponsors-badge]][collective]
[![Backers][backers-badge]][collective]
[![Chat][chat-badge]][chat]

Create a [`vfile`][vfile] from a filepath.
Optionally populates them from the file system as well.
Can write virtual files to file system too.

## Install

This package is [ESM only](https://gist.github.com/sindresorhus/a39789f98801d908bbc7ff3ecc99d99c):
Node 12+ is needed to use it and it must be `import`ed instead of `require`d.

[npm][]:

```sh
npm install to-vfile
```

## Use

```js
import {toVFile, readSync} from 'to-vfile'

console.log(toVFile('readme.md'))
console.log(toVFile(new URL('./readme.md', import.meta.url)))
console.log(readSync('.git/HEAD'))
console.log(readSync('.git/HEAD', 'utf8'))
```

Yields:

```js
VFile {
  data: {},
  messages: [],
  history: ['readme.md'],
  cwd: '/Users/tilde/projects/oss/to-vfile'
}
VFile {
  data: {},
  messages: [],
  history: ['readme.md'],
  cwd: '/Users/tilde/projects/oss/to-vfile'
}
VFile {
  data: {},
  messages: [],
  history: ['.git/HEAD'],
  cwd: '/Users/tilde/projects/oss/to-vfile',
  value: <Buffer 72 65 66 3a 20 72 65 66 73 2f 68 65 61 64 73 2f 6d 61 73 74 65 72 0a>
}
VFile {
  data: {},
  messages: [],
  history: ['.git/HEAD'],
  cwd: '/Users/tilde/projects/oss/to-vfile',
  value: 'ref: refs/heads/main\n'
}
```

## API

This package exports the following identifiers: `toVFile`, `read`, `readSync`,
`write`, and `writeSync`.
There is no default export.

### `toVFile(options)`

Create a virtual file.
Works like the [vfile][] constructor, except when `options` is `string` or
`Buffer`, in which case it’s treated as `{path: options}` instead of
`{value: options}`, or when `options` is a WHATWG `URL` object, in which case
it’s treated as `{path: fileURLToPath(options)}`.

### `read(options[, encoding][, callback])`

Creates a virtual file from options (`toVFile(options)`), reads the file from
the file system and populates `file.value` with the result.
If `encoding` is specified, it’s passed to `fs.readFile`.
If `callback` is given, invokes it with either an error or the populated virtual
file.
If `callback` is not given, returns a [`Promise`][promise] that is rejected with
an error or resolved with the populated virtual file.

### `readSync(options[, encoding])`

Like `read` but synchronous.
Either throws an error or returns a populated virtual file.

### `write(options[, fsOptions][, callback])`

Creates a virtual file from `options` (`toVFile(options)`), writes the file to
the file system.
`fsOptions` are passed to `fs.writeFile`.
If `callback` is given, invokes it with an error, if any.
If `callback` is not given, returns a [`Promise`][promise] that is rejected with
an error or resolved with the written virtual file.

### `writeSync(options[, fsOptions])`

Like `write` but synchronous.
Either throws an error or returns a populated virtual file.

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

[build-badge]: https://github.com/vfile/to-vfile/workflows/main/badge.svg

[build]: https://github.com/vfile/to-vfile/actions

[coverage-badge]: https://img.shields.io/codecov/c/github/vfile/to-vfile.svg

[coverage]: https://codecov.io/github/vfile/to-vfile

[downloads-badge]: https://img.shields.io/npm/dm/to-vfile.svg

[downloads]: https://www.npmjs.com/package/to-vfile

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

[promise]: https://developer.mozilla.org/Web/JavaScript/Reference/Global_Objects/Promise
