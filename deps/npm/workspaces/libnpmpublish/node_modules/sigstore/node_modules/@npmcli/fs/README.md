# @npmcli/fs

polyfills, and extensions, of the core `fs` module.

## Features

- `fs.cp` polyfill for node < 16.7.0
- `fs.withTempDir` added
- `fs.readdirScoped` added
- `fs.moveFile` added

## `fs.withTempDir(root, fn, options) -> Promise`

### Parameters

- `root`: the directory in which to create the temporary directory
- `fn`: a function that will be called with the path to the temporary directory
- `options`
  - `tmpPrefix`: a prefix to be used in the generated directory name

### Usage

The `withTempDir` function creates a temporary directory, runs the provided
function (`fn`), then removes the temporary directory and resolves or rejects
based on the result of `fn`.

```js
const fs = require('@npmcli/fs')
const os = require('os')

// this function will be called with the full path to the temporary directory
// it is called with `await` behind the scenes, so can be async if desired.
const myFunction = async (tempPath) => {
  return 'done!'
}

const main = async () => {
  const result = await fs.withTempDir(os.tmpdir(), myFunction)
  // result === 'done!'
}

main()
```

## `fs.readdirScoped(root) -> Promise`

### Parameters

- `root`: the directory to read

### Usage

Like `fs.readdir` but handling `@org/module` dirs as if they were
a single entry.

```javascript
const { readdirScoped } = require('@npmcli/fs')
const entries = await readdirScoped('node_modules')
// entries will be something like: ['a', '@org/foo', '@org/bar']
```

## `fs.moveFile(source, dest, options) -> Promise`

A fork of [move-file](https://github.com/sindresorhus/move-file) with
support for Common JS.

### Highlights

- Promise API.
- Supports moving a file across partitions and devices.
- Optionally prevent overwriting an existing file.
- Creates non-existent destination directories for you.
- Automatically recurses when source is a directory.

### Parameters

- `source`: File, or directory, you want to move.
- `dest`: Where you want the file or directory moved.
- `options`
  - `overwrite` (`boolean`, default: `true`): Overwrite existing destination file(s).

### Usage

The built-in
[`fs.rename()`](https://nodejs.org/api/fs.html#fs_fs_rename_oldpath_newpath_callback)
is just a JavaScript wrapper for the C `rename(2)` function, which doesn't
support moving files across partitions or devices. This module is what you
would have expected `fs.rename()` to be.

```js
const { moveFile } = require('@npmcli/fs');

(async () => {
	await moveFile('source/unicorn.png', 'destination/unicorn.png');
	console.log('The file has been moved');
})();
```
