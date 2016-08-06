# ![vfile][]

[![Build Status][build-badge]][build-status]
[![Coverage Status][coverage-badge]][coverage-status]

> A lot has changed recently, other tools may still use the [1.0.0][]
> API.

**VFile** is a virtual file format used by [**unified**][unified],
a text processing umbrella (it powers [**retext**][retext] for
natural language, [**remark**][remark] for markdown, and
[**rehype**][rehype] for HTML).  Each processors that parse, transform,
and compile text, and need a virtual representation of files and a
place to store [messages][] about them.  Plus, they work in the browser.
**VFile** provides these requirements at a small size, in IE 9 and up.

> **VFile** is different from the excellent [**vinyl**][vinyl]
> in that it has a smaller API, a smaller size, and focusses on
> [messages][].

## Installation

[npm][]:

```bash
npm install vfile
```

## Table of Contents

*   [Usage](#usage)
*   [List of Utilities](#list-of-utilities)
*   [API](#api)
    *   [VFile(\[options\])](#vfileoptions)
    *   [vfile.contents](#vfilecontents)
    *   [vfile.cwd](#vfilecwd)
    *   [vfile.path](#vfilepath)
    *   [vfile.basename](#vfilebasename)
    *   [vfile.stem](#vfilestem)
    *   [vfile.extname](#vfileextname)
    *   [vfile.dirname](#vfiledirname)
    *   [vfile.history](#vfilehistory)
    *   [vfile.messages](#vfilemessages)
    *   [vfile.data](#vfiledata)
    *   [VFile#toString(\[encoding='utf8'\])](#vfiletostringencodingutf8)
    *   [VFile#message(reason\[, position\[, ruleId\]\])](#vfilemessagereason-position-ruleid)
    *   [VFile#fail(reason\[, position\[, ruleId\]\])](#vfilefailreason-position-ruleid)
    *   [VFileMessage](#vfilemessage)
*   [License](#license)

## Usage

```js
var vfile = require('vfile');

var file = vfile({path: '~/example.txt', contents: 'Alpha *braavo* charlie.'});

console.log(file.path);
// '~/example.txt'

console.log(file.dirname);
// '~'

file.extname = '.md';

console.log(file.basename);
// 'example.md'

file.basename = 'index.text';

console.log(file.history);
// [ '~/example.txt', '~/example.md', '~/index.text' ]

file.message('`braavo` is misspelt; did you mean `bravo`?', {line: 1, column: 8});
// { [~/index.text:1:8: `braavo` is misspelt; did you mean `bravo`?]
//   message: '`braavo` is misspelt; did you mean `bravo`?',
//   name: '~/index.text:1:8',
//   file: '~/index.text',
//   reason: '`braavo` is misspelt; did you mean `bravo`?',
//   line: 1,
//   column: 8,
//   location:
//    { start: { line: 1, column: 8 },
//      end: { line: null, column: null } },
//   ruleId: null,
//   source: null,
//   fatal: false }
```

## List of Utilities

The following list of projects includes tools for working with virtual
files.  See [**Unist**][unist] for projects working with nodes.

*   [`dustinspecker/convert-vinyl-to-vfile`](https://github.com/dustinspecker/convert-vinyl-to-vfile)
    — Convert from [Vinyl][];
*   [`shinnn/is-vfile-message`](https://github.com/shinnn/is-vfile-message)
    — Check if a value is a `VFileMessage` object;
*   [`wooorm/to-vfile`](https://github.com/wooorm/to-vfile)
    — Create a virtual file from a file-path (and optionally read it);
*   [`wooorm/vfile-find-down`](https://github.com/wooorm/vfile-find-down)
    — Find files by searching the file system downwards;
*   [`wooorm/vfile-find-up`](https://github.com/wooorm/vfile-find-up)
    — Find files by searching the file system upwards;
*   [`wooorm/vfile-location`](https://github.com/wooorm/vfile-location)
    — Convert between line/column- and range-based locations;
*   [`wooorm/vfile-statistics`](https://github.com/wooorm/vfile-statistics)
    — Count messages per category;
*   [`shinnn/vfile-messages-to-vscode-diagnostics`](https://github.com/shinnn/vfile-messages-to-vscode-diagnostics)
    — Convert to VS Code diagnostics;
*   [`wooorm/vfile-reporter`](https://github.com/wooorm/vfile-reporter)
    — Stylish reporter.
*   [`wooorm/vfile-sort`](https://github.com/wooorm/vfile-sort)
    — Sort messages by line/column;
*   [`sindresorhus/vfile-to-eslint`](https://github.com/sindresorhus/vfile-to-eslint)
    — Convert VFiles to ESLint formatter compatible output;
*   [`sindresorhus/vfile-reporter-pretty`](https://github.com/sindresorhus/vfile-reporter-pretty)
    — Pretty reporter;

## API

### `VFile([options])`

Create a new virtual file.  If `options` is `string` or `Buffer`, treats
it as `{contents: options}`.  If `options` is a `VFile`, returns it.
All other options are set on the newly created `vfile`.

Path related properties are set in the following order (least specific
to most specific): `history`, `path`, `basename`, `stem`, `extname`,
`dirname`.

It’s not possible to either `dirname` or `extname` without setting
either `history`, `path`, `basename`, or `stem` as well.

###### Example

```js
vfile();
vfile('console.log("alpha");');
vfile(Buffer.from('exit 1'));
vfile({path: path.join(__dirname, 'readme.md')});
vfile({stem: 'readme', extname: '.md', dirname: __dirname});
vfile({other: 'properties', are: 'copied', ov: {e: 'r'}});
```

### `vfile.contents`

`Buffer`, `string`, `null` — Raw value.

### `vfile.cwd`

`string` — Base of `path`.  Defaults to `process.cwd()`.

### `vfile.path`

`string?` — Path of `vfile`.  Cannot be nullified.

### `vfile.basename`

`string?` — Current name (including extension) of `vfile`.  Cannot
contain path separators.  Cannot be nullified either (use
`file.path = file.dirname` instead).

### `vfile.stem`

`string?` — Name (without extension) of `vfile`.  Cannot be nullified,
and cannot contain path separators.

### `vfile.extname`

`string?` — Extension (with dot) of `vfile`.  Cannot be set if
there’s no `path` yet and cannot contain path separators.

### `vfile.dirname`

`string?` — Path to parent directory of `vfile`.  Cannot be set if
there’s no `path` yet.

### `vfile.history`

`Array.<string>` — List of file-paths the file moved between.

### `vfile.messages`

`Array.<VFileMessage>` — List of messages associated with the file.

### `vfile.data`

`Object` — Place to store custom information.  It’s OK to store custom
data directly on the `vfile`, moving it to `data` gives a _little_ more
privacy.

### `VFile#toString([encoding='utf8'])`

Convert contents of `vfile` to string.  If `contents` is a buffer,
`encoding` is used to stringify buffers (default: `'utf8'`).

### `VFile#message(reason[, position[, ruleId]])`

Associates a message with the file for `reason` at `position`.  When an
error is passed in as `reason`, copies the stack.

*   `reason` (`string` or `Error`)
    — Reason for message, uses the stack and message of the error if given;
*   `position` (`Node`, `Location`, or `Position`, optional)
    — Place at which the message occurred in `vfile`.
*   `ruleId` (`string`, optional)
    — Category of warning.

###### Returns

[`VFileMessage`][message].

### `VFile#fail(reason[, position[, ruleId]])`

Associates a fatal message with the file, then immediately throws it.
Note: fatal errors mean a file is no longer processable.
Calls [`#message()`][messages] internally.

###### Throws

[`VFileMessage`][message].

### `VFileMessage`

File-related message describing something at certain position (extends
`Error`).

###### Properties

*   `file` (`string`) — File-path (when the message was triggered);
*   `reason` (`string`) — Reason for message;
*   `ruleId` (`string?`) — Category of message;
*   `source` (`string?`) — Namespace of warning;
*   `stack` (`string?`) — Stack of message;
*   `fatal` (`boolean?`) — If `true`, marks associated file as no longer
    processable;
*   `line` (`number?`) — Starting line of error;
*   `column` (`number?`) — Starting column of error;
*   `location` (`object`) — Full range information, when available.  Has
    `start` and `end` properties, both set to an object with `line` and
    `column`, set to `number?`.

## License

[MIT][license] © [Titus Wormer][author]

<!-- Definitions -->

[build-badge]: https://img.shields.io/travis/wooorm/vfile.svg

[build-status]: https://travis-ci.org/wooorm/vfile

[coverage-badge]: https://img.shields.io/codecov/c/github/wooorm/vfile.svg

[coverage-status]: https://codecov.io/github/wooorm/vfile

[npm]: https://docs.npmjs.com/cli/install

[license]: LICENSE

[author]: http://wooorm.com

[vfile]: https://cdn.rawgit.com/wooorm/vfile/master/logo.svg

[unified]: https://github.com/wooorm/unified

[retext]: https://github.com/wooorm/retext

[remark]: https://github.com/wooorm/remark

[rehype]: https://github.com/wooorm/rehype

[vinyl]: https://github.com/wearefractal/vinyl

[unist]: https://github.com/wooorm/unist#list-of-utilities

[messages]: #vfilemessagereason-position-ruleid

[message]: #vfilemessage

[1.0.0]: https://github.com/wooorm/vfile/tree/d5abd71
