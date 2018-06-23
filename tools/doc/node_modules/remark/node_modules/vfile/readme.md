# ![vfile](https://cdn.rawgit.com/wooorm/vfile/master/logo.svg)

[![Build Status](https://img.shields.io/travis/wooorm/vfile.svg)](https://travis-ci.org/wooorm/vfile) [![Coverage Status](https://img.shields.io/codecov/c/github/wooorm/vfile.svg)](https://codecov.io/github/wooorm/vfile)

**VFile** is a virtual file format used by [**retext**](https://github.com/wooorm/retext)
(natural language) and [**remark**](https://github.com/wooorm/remark)
(markdown). Two processors which parse, transform, and compile text. Both need
a virtual representation of files and a place to store metadata and messages.
And, they work in the browser. **VFile** provides these requirements.

Also, **VFile** exposes a warning mechanism compatible with [**ESLint**](https://github.com/eslint/eslint)s
formatters, making it easy to expose [stylish](https://github.com/eslint/eslint/blob/master/lib/formatters/stylish.js)
warnings, or export [tap](https://github.com/eslint/eslint/blob/master/lib/formatters/tap.js)
compliant messages.

> **VFile** is different from (the excellent :+1:) [**vinyl**](https://github.com/wearefractal/vinyl)
> in that it does not include file-system or node-only functionality. No
> buffers, streams, or stats. In addition, the focus on
> [metadata](#vfilenamespacekey) and [messages](#vfilemessagereason-position-ruleid)
> are useful when processing a file through a
> [middleware](https://github.com/segmentio/ware) pipeline.

## Installation

[npm](https://docs.npmjs.com/cli/install):

```bash
npm install vfile
```

**VFile** is also available for [duo](http://duojs.org/#getting-started),
and as an AMD, CommonJS, and globals module, [uncompressed and
compressed](https://github.com/wooorm/vfile/releases).

## Table of Contents

*   [Usage](#usage)

*   [Related Tools](#related-tools)

*   [API](#api)

    *   [VFile()](#vfile-1)
    *   [VFile#contents](#vfilecontents)
    *   [VFile#directory](#vfiledirectory)
    *   [VFile#filename](#vfilefilename)
    *   [VFile#extension](#vfileextension)
    *   [VFile#basename()](#vfilebasename)
    *   [VFile#quiet](#vfilequiet)
    *   [VFile#messages](#vfilemessages)
    *   [VFile#history](#vfilehistory)
    *   [VFile#toString()](#vfiletostring)
    *   [VFile#filePath()](#vfilefilepath)
    *   [VFile#move(options)](#vfilemoveoptions)
    *   [VFile#namespace(key)](#vfilenamespacekey)
    *   [VFile#message(reason\[, position\[, ruleId\]\])](#vfilemessagereason-position-ruleid)
    *   [VFile#warn(reason\[, position\[, ruleId\]\])](#vfilewarnreason-position-ruleid)
    *   [VFile#fail(reason\[, position\[, ruleId\]\])](#vfilefailreason-position-ruleid)
    *   [VFile#hasFailed()](#vfilehasfailed)
    *   [VFileMessage](#vfilemessage)

*   [License](#license)

## Usage

```js
var VFile = require('vfile');

var file = new VFile({
  'directory': '~',
  'filename': 'example',
  'extension': 'txt',
  'contents': 'Foo *bar* baz'
});

file.toString(); // 'Foo *bar* baz'
file.filePath(); // '~/example.txt'

file.move({'extension': 'md'});
file.filePath(); // '~/example.md'

file.warn('Something went wrong', {'line': 1, 'column': 3});
// { [~/example.md:1:3: Something went wrong]
//   name: '~/example.md:1:3',
//   file: '~/example.md',
//   reason: 'Something went wrong',
//   line: 1,
//   column: 3,
//   fatal: false }
```

## Related Tools

[**VFile**](#api)s are used by both [**retext**](https://github.com/wooorm/retext)
and [**remark**](https://github.com/wooorm/remark).

In addition, here’s a list of useful tools:

*   [`dustinspecker/convert-vinyl-to-vfile`](https://github.com/dustinspecker/convert-vinyl-to-vfile)
    — Convert a [Vinyl](https://github.com/wearefractal/vinyl) file to a VFile;

*   [`shinnn/is-vfile-message`](https://github.com/shinnn/is-vfile-message)
    — Check if a value is a `VFileMessage` object;

*   [`wooorm/to-vfile`](https://github.com/wooorm/to-vfile)
    — Create a virtual file from a file-path;

*   [`wooorm/vfile-find-down`](https://github.com/wooorm/vfile-find-down)
    — Find one or more files by searching the file system downwards;

*   [`wooorm/vfile-find-up`](https://github.com/wooorm/vfile-find-up)
    — Find one or more files by searching the file system upwards;

*   [`wooorm/vfile-location`](https://github.com/wooorm/vfile-location)
    — Convert between positions (line and column-based) and offsets
    (range-based) locations;

*   [`shinnn/vfile-messages-to-vscode-diagnostics`](https://github.com/shinnn/vfile-messages-to-vscode-diagnostics)
    — Convert `VFileMessage`s into an array of VS Code diagnostics;

*   [`wooorm/vfile-reporter`](https://github.com/wooorm/vfile-reporter)
    — Stylish reporter for virtual files.

*   [`wooorm/vfile-sort`](https://github.com/wooorm/vfile-sort)
    — Sort virtual file messages by line/column;

## API

### `VFile()`

**VFile** objects make it easy to move files, to trigger warnings and
errors, and to store supplementary metadata relating to files, all without
accessing the file-system.

**Example**:

```js
var file = new VFile({
  'directory': '~',
  'filename': 'example',
  'extension': 'txt',
  'contents': 'Foo *bar* baz'
});

file === VFile(file); // true
file === new VFile(file); // true

VFile('foo') instanceof VFile; // true
```

**Signatures**:

*   `file = VFile(contents|options|vFile?)`.

**Parameters**:

*   `contents` (`string`) — Contents of the file;

*   `vFile` (`VFile`) — Existing representation, returned without modification;

*   `options` (`Object`):

    *   `directory` (`string?`, default: `''`)
        — Parent directory;

    *   `filename` (`string?`, default: `''`)
        — Name, without extension;

    *   `extension` (`string?`, default: `''`)
        — Extension(s), without initial dot;

    *   `contents` (`string?`, default: `''`)
        — Raw value.

**Returns**:

`vFile` — Instance.

**Notes**:

`VFile` exposes an interface compatible with ESLint’s formatters.  For example,
to expose warnings using ESLint’s `compact` formatter, execute the following:

```javascript
var compact = require('eslint/lib/formatters/compact');
var VFile = require('vfile');

var vFile = new VFile({
    'directory': '~',
    'filename': 'hello',
    'extension': 'txt'
});

vFile.warn('Whoops, something happened!');

console.log(compact([vFile]));
```

Which would yield the following:

```text
~/hello.txt: line 0, col 0, Warning - Whoops, something happened!

1 problem
```

### `VFile#contents`

`string` — Content of file.

### `VFile#directory`

`string` — Path to parent directory.

### `VFile#filename`

`string` — Filename. A file-path can still be generated when no filename exists.

### `VFile#extension`

`string` — Extension. A file-path can still be generated when no extension
exists.

### `VFile#basename()`

Get the filename, with extension, if applicable.

**Example**:

```js
var file = new VFile({
  'directory': '~',
  'filename': 'example',
  'extension': 'txt'
});

file.basename() // example.txt
```

**Signatures**:

*   `string = vFile.basename()`.

**Returns**:

`string`— Returns the file path without a directory, if applicable.
Otherwise,an empty string is returned.

### `VFile#quiet`

`boolean?` — Whether an error created by [`VFile#fail()`](#vfilemessagereason-position-ruleid)
is returned (when truthy) or thrown (when falsey).

Ensure all `messages` associated with a file are handled properly when setting
this to `true`.

### `VFile#messages`

`Array.<VFileMessage>` — List of associated messages.

**Notes**:

`VFile#message()`, and in turn `VFile#warn()` and `VFile#fail()`, return
`Error` objects that adhere to the [`VFileMessage`](#vfilemessage) schema.
Its results can populate `messages`.

### `VFile#history`

`Array.<String>` — List of file-paths the file [`move`](#vfilemoveoptions)d
between.

### `VFile#toString()`

Get the value of the file.

**Example**:

```js
var vFile = new VFile('Foo');
String(vFile); // 'Foo'
```

**Signatures**:

*   `string = vFile.toString()`.

**Returns**:

`string` — Contents.

### `VFile#filePath()`

Get the filename, with extension and directory, if applicable.

**Example**:

```js
var file = new VFile({
  'directory': '~',
  'filename': 'example',
  'extension': 'txt'
});

String(file.filePath); // ~/example.txt
file.filePath() // ~/example.txt
```

**Signatures**:

*   `string = vFile.filePath()`.

**Returns**:

`string` — If the `vFile` has a `filename`, it will be prefixed with the
directory (slashed), if applicable, and suffixed with the (dotted) extension
(if applicable).  Otherwise, an empty string is returned.

### `VFile#move(options)`

Move a file by passing a new directory, filename, and extension.  When these
are not given, the default values are kept.

**Example**:

```js
var file = new VFile({
  'directory': '~',
  'filename': 'example',
  'extension': 'txt',
  'contents': 'Foo *bar* baz'
});

file.move({'directory': '/var/www'});
file.filePath(); // '/var/www/example.txt'

file.move({'extension': 'md'});
file.filePath(); // '/var/www/example.md'
```

**Signatures**:

*   `vFile = vFile.move(options?)`.

**Parameters**:

*   `options` (`Object`):

    *   `directory` (`string`, default: `''`)
        — Parent directory;

    *   `filename` (`string?`, default: `''`)
        — Name, without extension;

    *   `extension` (`string`, default: `''`)
        — Extension(s), without initial dot.

**Returns**:

`vFile` — Context object (chainable).

### `VFile#namespace(key)`

Access metadata.

**Example**:

```js
var file = new VFile('Foo');

file.namespace('foo').bar = 'baz';

console.log(file.namespace('foo').bar) // 'baz';
```

**Parameters**:

*   `key` (`string`) — Namespace key.

**Returns**:

`Object` — Private namespace for metadata.

### `VFile#message(reason[, position[, ruleId]])`

Create a message with `reason` at `position`. When an error is passed in as
`reason`, copies the stack. This does not add a message to `messages`.

**Example**:

```js
var file = new VFile();

file.message('Something went wrong');
// { [1:1: Something went wrong]
//   name: '1:1',
//   file: '',
//   reason: 'Something went wrong',
//   line: null,
//   column: null }
```

**Signatures**:

*   `VFileMessage = vFile.message(err|reason, node|location|position?,
    ruleId?)`.

**Parameters**:

*   `err` (`Error`) — Original error, whose stack and message are used;

*   `reason` (`string`) — Reason for message;

*   `node` (`Node`) — Syntax tree object;

*   `location` (`Object`) — Syntax tree location (found at `node.position`);

*   `position` (`Object`) — Syntax tree position (found at
    `node.position.start` or `node.position.end`).

*   `ruleId` (`string`) — Category of warning.

**Returns**:

[`VFileMessage`](#vfilemessage) — File-related message with location
information.

### `VFile#warn(reason[, position[, ruleId]])`

Warn. Creates a non-fatal message (see [`VFile#message()`](#vfilemessagereason-position-ruleid)),
and adds it to the file's [`messages`](#vfilemessages) list.

**Example**:

```js
var file = new VFile();

file.warn('Something went wrong');
// { [1:1: Something went wrong]
//   name: '1:1',
//   file: '',
//   reason: 'Something went wrong',
//   line: null,
//   column: null,
//   fatal: false }
```

**See**:

*   [`VFile#message`](#vfilemessagereason-position-ruleid)

### `VFile#fail(reason[, position[, ruleId]])`

Fail. Creates a fatal message (see `VFile#message()`), sets `fatal: true`,
adds it to the file's `messages` list.

If `quiet` is not `true`, throws the error.

**Example**:

```js
var file = new VFile();

file.fail('Something went wrong');
// 1:1: Something went wrong
//     at VFile.exception (vfile/index.js:296:11)
//     at VFile.fail (vfile/index.js:360:20)
//     at repl:1:6

file.quiet = true;
file.fail('Something went wrong');
// { [1:1: Something went wrong]
//   name: '1:1',
//   file: '',
//   reason: 'Something went wrong',
//   line: null,
//   column: null,
//   fatal: true }
```

**See**:

*   [`VFile#message`](#vfilemessagereason-position-ruleid)

### `VFile#hasFailed()`

Check if a fatal message occurred making the file no longer processable.

**Example**:

```js
var file = new VFile();
file.quiet = true;

file.hasFailed(); // false

file.fail('Something went wrong');
file.hasFailed(); // true
```

**Signatures**:

*   `boolean = vFile.hasFailed()`.

**Returns**:

`boolean` — `true` if at least one of file’s `messages` has a `fatal`
property set to `true`.

### `VFileMessage`

`Error` — File-related message with location information.

**Properties**:

*   `name` (`string`)
    — (Starting) location of the message, preceded by its file-path when
    available, and joined by `':'`. Used by the native
    [`Error#toString()`](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Error/name);

*   `file` (`string`) — File-path;

*   `reason` (`string`) — Reason for message;

*   `line` (`number?`) — Line of error, when available;

*   `column` (`number?`) — Column of error, when available;

*   `stack` (`string?`) — Stack of message, when available;

*   `fatal` (`boolean?`) — Whether the associated file is still processable.

*   `location` (`object`) — Full range information, when available. Has
    `start` and `end` properties, both set to an object with `line` and
    `column`, set to `number?`.

## License

[MIT](LICENSE) © [Titus Wormer](http://wooorm.com)
