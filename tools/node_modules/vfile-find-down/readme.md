# vfile-find-down [![Build Status](https://img.shields.io/travis/wooorm/vfile-find-down.svg)](https://travis-ci.org/wooorm/vfile-find-down) [![Coverage Status](https://img.shields.io/codecov/c/github/wooorm/vfile-find-down.svg)](https://codecov.io/github/wooorm/vfile-find-down)

Find one or more files (exposed as [VFile](https://github.com/wooorm/vfile)s)
by searching the file system downwards.

## Installation

[npm](https://docs.npmjs.com/cli/install):

```bash
npm install vfile-find-down
```

## Usage

Require dependencies:

```js
var findDown = require('vfile-find-down');
```

Search for files with a `.md` extension from the current working directory
downwards:

```js
findDown.all('.md', console.log);
/* null [ VFile {
 *     contents: '',
 *     messages: [],
 *     history: [ '/Users/foo/bar/baz/example.md' ],
 *     directory: '/Users/foo/bar/baz',
 *     filename: 'example',
 *     extension: 'md' },
 *   VFile {
 *     contents: '',
 *     messages: [],
 *     directory: '/Users/foo/bar/baz',
 *     filename: 'readme',
 *     extension: 'md' } ]
 */
```

Search for the first file:

```js
findDown.one('.md', console.log);
/* null VFile {
 *   contents: '',
 *   messages: [],
 *   directory: '/Users/foo/bar/baz',
 *   filename: 'example',
 *   extension: 'md' }
 */
```

## API

### findDown.one(test\[, paths\], [callback](#function-callbackerr-file))

Find a file or a directory downwards.

**Example**

```js
findDown.one('readme.md', console.log);
/* null VFile {
 *   contents: '',
 *   messages: [],
 *   history: [ '/Users/foo/bar/baz/readme.md' ],
 *   directory: '/Users/foo/bar/baz',
 *   filename: 'readme',
 *   extension: 'md' }
 */
```

**Signatures**

*   `findDown.one(filePath[, paths], callback)`;
*   `findDown.one(extension[, paths], callback)`;
*   `findDown.one(test[, paths], callback)`;
*   `findDown.one(tests[, paths], callback)`.

**Parameters**

*   `filePath` (`string`)
    — Filename (including extension) to search for;

*   `extension` (`string`)
    — File extension to search for (must start with a `.`);

*   `test` ([`Function`](#function-testfile))
    — Function invoked to check whether a virtual file should be included;

*   `tests` (`Array.<Function|filePath|extension>`)
    — List of tests, any of which should match a given file for it to
    be included.

*   `paths` (`Array.<string>` or `string`, optional, default: `process.cwd()`)
    — Place(s) to start searching from;

*   `callback` ([`Function`](#function-callbackerr-file));
    — Function invoked when a matching file or the top of the volume
    is reached.

**Notes**

*   Virtual Files are not read (their `content` is not populated).

#### function callback(err, file)

Invoked when a matching file or the top of the volume is reached.

**Parameters**

*   `err` (`Error`, optional);
*   `file` ([`VFile`](https://github.com/wooorm/vfile), optional).

**Notes**

*   The `err` parameter is never populated.

### findDown.all(test\[, paths\], [callback](#function-callbackerr-files))

Find files or directories downwards.

**Example**

```js
findDown.all('.md', console.log);
/* null [ VFile {
 *     contents: '',
 *     messages: [],
 *     directory: '/Users/foo/bar/baz',
 *     filename: 'example',
 *     extension: 'md' },
 *   VFile {
 *     contents: '',
 *     messages: [],
 *     directory: '/Users/foo',
 *     filename: 'readme',
 *     extension: 'md' } ]
 */
```

**Signatures**

*   `findDown.all(filePath[, paths], callback)`;
*   `findDown.all(extension[, paths], callback)`;
*   `findDown.all(test[, paths], callback)`;
*   `findDown.all(tests[, paths], callback)`.

**Parameters**

*   `filePath` (`string`)
    — Filename (including extension) to search for;

*   `extension` (`string`)
    — File extension to search for (must start with a `.`);

*   `test` ([`Function`](#function-testfile))
    — Function invoked to check whether a virtual file should be included;

*   `tests` (`Array.<Function|filePath|extension>`)
    — List of tests, any of which should match a given file for it to
    be included.

*   `paths` (`Array.<string>` or `string`, optional, default: `process.cwd()`)
    — Place(s) to start searching from;

*   `callback` ([`Function`](#function-callbackerr-files));
    — Function invoked when matching files or the top of the volume
    is reached.

**Notes**

*   Virtual Files are not read (their `content` is not populated).

#### function callback(err, files)

Invoked when files or the top of the volume is reached.

**Parameters**

*   `err` (`Error`, optional);
*   `files` ([`Array.<VFile>`](https://github.com/wooorm/vfile), optional).

**Notes**

*   The `err` parameter is never populated.

### function test(file)

Check whether a virtual file should be included.

**Parameters**

*   `file` ([`VFile`](https://github.com/wooorm/vfile)).

**Returns**: `boolean`, when truthy, the file is included.

### findDown.INCLUDE

Flag used as an alternative to returning `true` from
[`test`](#function-testfile), ensuring the tested file
is included and passed to `callback`.

### findDown.SKIP

Flag used to skip searching a directory from [`test`](#function-testfile).

### findDown.EXIT

Flag used to stop searching for files from [`test`](#function-testfile).

## License

[MIT](LICENSE) © [Titus Wormer](http://wooorm.com)
