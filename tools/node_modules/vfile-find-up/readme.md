# vfile-find-up [![Build Status](https://img.shields.io/travis/wooorm/vfile-find-up.svg)](https://travis-ci.org/wooorm/vfile-find-up) [![Coverage Status](https://img.shields.io/codecov/c/github/wooorm/vfile-find-up.svg)](https://codecov.io/github/wooorm/vfile-find-up)

Find one or more files (exposed as [VFile](https://github.com/wooorm/vfile)s)
by searching the file system upwards.

## Installation

[npm](https://docs.npmjs.com/cli/install):

```bash
npm install vfile-find-up
```

## Usage

Require dependencies:

```js
var findUp = require('vfile-find-up');
```

Search for files named `package.json` from the current working directory
upwards:

```js
findUp.all('package.json', console.log);
/* null [ VFile {
 *     contents: '',
 *     messages: [],
 *     history: [ '/Users/foo/bar/baz/package.json' ],
 *     directory: '/Users/foo/bar/baz',
 *     filename: 'package',
 *     extension: 'json' },
 *   VFile {
 *     contents: '',
 *     messages: [],
 *     history: [ '/Users/foo/package.json' ],
 *     directory: '/Users/foo',
 *     filename: 'package',
 *     extension: 'json' } ]
 */
```

Search for the first file:

```js
findUp.one('package.json', console.log);
/* null VFile {
 *   contents: '',
 *   messages: [],
 *   history: [ '/Users/foo/bar/baz/package.json' ],
 *   directory: '/Users/foo/bar/baz',
 *   filename: 'package',
 *   extension: 'json' }
 */
```

### findUp.one(test\[, directory\], [callback](#function-callbackerr-file))

Find a file or a directory upwards.

**Example**

```js
findUp.one('package.json', console.log);
/* null VFile {
 *   contents: '',
 *   messages: [],
 *   history: [ '/Users/foo/bar/baz/package.json' ],
 *   directory: '/Users/foo/bar/baz',
 *   filename: 'package',
 *   extension: 'json' }
 */
```

**Signatures**

*   `findUp.one(filePath[, directory], callback)`;
*   `findUp.one(extension[, directory], callback)`;
*   `findUp.one(test[, directory], callback)`;
*   `findUp.one(tests[, directory], callback)`.

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

*   `directory` (`string`, optional, default: `process.cwd()`)
    — Place to start searching from;

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

### findUp.all(test\[, directory\], [callback](#function-callbackerr-files))

Find files or directories upwards.

**Example**

```js
findUp.all('package.json', console.log);
/* null [ VFile {
 *     contents: '',
 *     messages: [],
 *     history: [ '/Users/foo/bar/baz/package.json' ],
 *     directory: '/Users/foo/bar/baz',
 *     filename: 'package',
 *     extension: 'json' },
 *   VFile {
 *     contents: '',
 *     messages: [],
 *     history: [ '/Users/foo/package.json' ],
 *     directory: '/Users/foo',
 *     filename: 'package',
 *     extension: 'json' } ]
 */
```

**Signatures**

*   `findUp.all(filePath[, directory], callback)`;
*   `findUp.all(extension[, directory], callback)`;
*   `findUp.all(test[, directory], callback)`;
*   `findUp.all(tests[, directory], callback)`.

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

*   `directory` (`string`, optional, default: `process.cwd()`)
    — Place to start searching from;

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

**Returns**:

*   `boolean` — When truthy, the file is included;

*   `number` — Bitmask ([`findUp.INCLUDE`](#findupinclude) and
    [`findUp.EXIT`](#findupexit)) to control searching behavior.

### findUp.INCLUDE

Flag used as an alternative to returning `true` from
[`test`](#function-testfile), ensuring the tested file
is included and passed to `callback`.

### findUp.EXIT

Flag used to stop searching for files returned to [`test`](#function-testfile).

## License

[MIT](LICENSE) © [Titus Wormer](http://wooorm.com)
