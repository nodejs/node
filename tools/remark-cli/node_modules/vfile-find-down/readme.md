# vfile-find-down [![Build Status][travis-badge]][travis] [![Coverage Status][codecov-badge]][codecov]

Find [vfile][]s by searching the file system downwards.

## Installation

[npm][npm-install]:

```bash
npm install vfile-find-down
```

## Usage

Dependencies:

```js
var findDown = require('vfile-find-down');
```

Search for files with a `.md` extension from the current working
directory downwards:

```js
findDown.all('.md', console.log);
```

Logs:

```js
null [ VFile {
    data: {},
    messages: [],
    history: [ '/Users/tilde/projects/oss/vfile-find-down/readme.md' ],
    cwd: '/Users/tilde/projects/oss/vfile-find-down' } ]
```

Search for the first file:

```js
findDown.one('.md', console.log);
```

Logs:

```js
null VFile {
  data: {},
  messages: [],
  history: [ '/Users/tilde/projects/oss/vfile-find-down/readme.md' ],
  cwd: '/Users/tilde/projects/oss/vfile-find-down' }
```

## API

### `vfileFindDown.all(tests[, paths], callback)`

Search for `tests` downwards.  Invokes callback with either an error
or an array of files passing `tests`.
Note: Virtual Files are not read (their `contents` is not populated).

###### Parameters

*   `tests` (`string|Function|Array.<tests>`)
    — A test is a [function invoked with a `vfile`][test].
    If an array is passed in, any test must match a given file for it
    to be included.
    If a `string` is passed in, the `basename` or `extname` of files
    must match it for them to be included (and hidden directories and
    `node_modules` will not be searched).
*   `paths` (`Array.<string>` or `string`, default: `process.cwd()`)
    — Place(s) to searching from;
*   `callback` (`function cb(err[, files])`);
    — Function invoked with all matching files.

### `vfileFindDown.one(tests[, paths], callback)`

Like `vfileFindDown.all`, but invokes `callback` with the first found
file, or `null`.

### `function test(file, stats)`

Check whether a virtual file should be included.  Invoked with
a [vfile][] and a [stats][] object.

###### Returns

*   `true` or `vfileFindDown.INCLUDE` — Include the file in the results;
*   `vfileFindDown.SKIP` — Do not search inside this directory;
*   `vfileFindDown.BREAK` — Stop searching for files;
*   anything else is ignored: files are neither included nor skipped.

The different flags can be combined by using the pipe operator:
`vfileFindDown.INCLUDE | vfileFindDown.SKIP`.

## License

[MIT][license] © [Titus Wormer][author]

<!-- Definitions -->

[travis-badge]: https://img.shields.io/travis/wooorm/vfile-find-down.svg

[travis]: https://travis-ci.org/wooorm/vfile-find-down

[codecov-badge]: https://img.shields.io/codecov/c/github/wooorm/vfile-find-down.svg

[codecov]: https://codecov.io/github/wooorm/vfile-find-down

[npm-install]: https://docs.npmjs.com/cli/install

[license]: LICENSE

[author]: http://wooorm.com

[vfile]: https://github.com/wooorm/vfile

[stats]: https://nodejs.org/api/fs.html#fs_class_fs_stats

[test]: #function-testfile-stats
