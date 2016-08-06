# to-vfile [![Build Status][travis-badge]][travis] [![Coverage Status][codecov-badge]][codecov]

Create a [vfile][] from a file-path.  Optionally populates them from
the file-system as well.  Can write virtual files to file-system too.

## Installation

[npm][npm-install]:

```bash
npm install to-vfile
```

> **Note:** the file-system stuff is not available in the browser.

## Usage

Dependencies:

```js
var toVFile = require('to-vfile');
```

Create a virtual file by its file-path:

```js
toVFile('readme.md');
```

Yields:

```js
VFile {
  data: {},
  messages: [],
  history: [ 'readme.md' ],
  cwd: '/Users/tilde/projects/oss/to-vfile' }
```

Populate a virtual file:

```js
toVFile.readSync('.git/HEAD', 'utf8');
```

Yields:

```js
VFile {
  data: {},
  messages: [],
  history: [ '.git/HEAD' ],
  cwd: '/Users/tilde/projects/oss/to-vfile',
  contents: 'ref: refs/heads/master\n' }
```

## API

### `toVFile(options)`

Create a virtual file.  Works like the [vfile][] constructor,
except when `options` is `string` or `Buffer`, in which case
it’s treated as `{path: options}` instead of `{contents: options}`.

### `toVFile.read(options[, encoding], callback)`

Creates a virtual file from options (`toVFile(options)`), reads the
file from the file-system and populates `file.contents` with the result.
If `encoding` is specified, it’s passed to `fs.readFile`.
Invokes `callback` with either an error or the populated virtual file.

### `toVFile.readSync(options[, encoding])`

Like `toVFile.read` but synchronous.  Either throws an error or
returns a populated virtual file.

### `toVFile.write(options[, fsOptions], callback)`

Creates a virtual file from `options` (`toVFile(options)`), writes the
file to the file-system.  `fsOptions` are passed to `fs.writeFile`.
Invokes `callback` with an error, if any.

### `toVFile.writeSync(options[, fsOptions])`

Like `toVFile.write` but synchronous.  Throws an error, if any.

## License

[MIT][license] © [Titus Wormer][author]

<!-- Definitions -->

[travis-badge]: https://img.shields.io/travis/wooorm/to-vfile.svg

[travis]: https://travis-ci.org/wooorm/to-vfile

[codecov-badge]: https://img.shields.io/codecov/c/github/wooorm/to-vfile.svg

[codecov]: https://codecov.io/github/wooorm/to-vfile

[npm-install]: https://docs.npmjs.com/cli/install

[license]: LICENSE

[author]: http://wooorm.com

[vfile]: https://github.com/wooorm/vfile
