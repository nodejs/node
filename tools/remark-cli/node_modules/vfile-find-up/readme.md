# vfile-find-up [![Build Status][travis-badge]][travis] [![Coverage Status][codecov-badge]][codecov]

Find [vfile][]s by searching the file system upwards.

## Installation

[npm][npm-install]:

```bash
npm install vfile-find-up
```

## Usage

Dependencies:

```js
var findUp = require('vfile-find-up');
```

Search for files named `package.json` from the current working directory
upwards:

```js
findUp.all('package.json', console.log);
```

Logs:

```js
null [ VFile {
    data: {},
    messages: [],
    history: [ '/Users/tilde/projects/oss/vfile-find-up/package.json' ],
    cwd: '/Users/tilde/projects/oss/vfile-find-up' } ]
```

Search for the first file:

```js
findUp.one('package.json', console.log);
```

Logs:

```js
null VFile {
  data: {},
  messages: [],
  history: [ '/Users/tilde/projects/oss/vfile-find-up/package.json' ],
  cwd: '/Users/tilde/projects/oss/vfile-find-up' }
```

## API

### `vfileFindUp.all(tests[, path], callback)`

Search for `tests` upwards.  Invokes callback with either an error
or an array of files passing `tests`.
Note: Virtual Files are not read (their `contents` is not populated).

###### Parameters

*   `tests` (`string|Function|Array.<tests>`)
    — A test is a [function invoked with a `vfile`][test].
    If an array is passed in, any test must match a given file for it
    to be included.
    If a `string` is passed in, the `basename` or `extname` of files
    must match it for them to be included.
*   `path` (`string`, default: `process.cwd()`)
    — Place to searching from;
*   `callback` (`function cb(err[, files])`);
    — Function invoked with all matching files.

### `vfileFindUp.one(tests[, path], callback)`

Like `vfileFindUp.all`, but invokes `callback` with the first found
file, or `null`.

### `function test(file)`

Check whether a virtual file should be included.  Invoked with a
[vfile][].

###### Returns

*   `true` or `vfileFindUp.INCLUDE` — Include the file in the results;
*   `vfileFindUp.BREAK` — Stop searching for files;
*   anything else is ignored: the file is not included.

The different flags can be combined by using the pipe operator:
`vfileFindUp.INCLUDE | vfileFindUp.BREAK`.

## License

[MIT][license] © [Titus Wormer][author]

<!-- Definitions -->

[travis-badge]: https://img.shields.io/travis/wooorm/vfile-find-up.svg

[travis]: https://travis-ci.org/wooorm/vfile-find-up

[codecov-badge]: https://img.shields.io/codecov/c/github/wooorm/vfile-find-up.svg

[codecov]: https://codecov.io/github/wooorm/vfile-find-up

[npm-install]: https://docs.npmjs.com/cli/install

[license]: LICENSE

[author]: http://wooorm.com

[vfile]: https://github.com/wooorm/vfile

[test]: #function-testfile
