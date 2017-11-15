# ![vfile][]

[![Build Status][build-badge]][build-status]
[![Coverage Status][coverage-badge]][coverage-status]

**VFile** is a virtual file format used by [**unified**][unified],
a text processing umbrella (it powers [**retext**][retext] for
natural language, [**remark**][remark] for markdown, and
[**rehype**][rehype] for HTML).  Each processors that parse, transform,
and compile text, and need a virtual representation of files and a
place to store [messages][] about them.  Plus, they work in the browser.
**VFile** provides these requirements at a small size, in IE 9 and up.

> **VFile** is different from the excellent [**vinyl**][vinyl]
> in that it has a smaller API, a smaller size, and focuses on
> [messages][].

## Installation

[npm][]:

```bash
npm install vfile
```

## Table of Contents

*   [Usage](#usage)
*   [Utilities](#utilities)
*   [Reporters](#reporters)
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
    *   [VFile#toString(\[encoding\])](#vfiletostringencoding)
    *   [VFile#message(reason\[, position\[, ruleId\]\])](#vfilemessagereason-position-ruleid)
    *   [VFile#fail(reason\[, position\[, ruleId\]\])](#vfilefailreason-position-ruleid)
    *   [VFileMessage](#vfilemessage)
*   [License](#license)

## Usage

```js
var vfile = require('vfile');

var file = vfile({path: '~/example.txt', contents: 'Alpha *braavo* charlie.'});

file.path; //=> '~/example.txt'
file.dirname; //=> '~'

file.extname = '.md';

file.basename; //=> 'example.md'

file.basename = 'index.text';

file.history; //=> ['~/example.txt', '~/example.md', '~/index.text']

file.message('`braavo` is misspelt; did you mean `bravo`?', {line: 1, column: 8});

console.log(file.messages);
```

Yields:

```js
[ { [~/index.text:1:8: `braavo` is misspelt; did you mean `bravo`?]
    message: '`braavo` is misspelt; did you mean `bravo`?',
    name: '~/index.text:1:8',
    file: '~/index.text',
    reason: '`braavo` is misspelt; did you mean `bravo`?',
    line: 1,
    column: 8,
    location: { start: [Object], end: [Object] },
    ruleId: null,
    source: null,
    fatal: false } ]
```

## Utilities

The following list of projects includes tools for working with virtual
files.  See [**Unist**][unist] for projects working with nodes.

*   [`convert-vinyl-to-vfile`](https://github.com/dustinspecker/convert-vinyl-to-vfile)
    — Convert from [Vinyl][]
*   [`is-vfile-message`](https://github.com/shinnn/is-vfile-message)
    — Check if a value is a `VFileMessage` object
*   [`to-vfile`](https://github.com/vfile/to-vfile)
    — Create a virtual file from a file-path (and optionally read it)
*   [`vfile-find-down`](https://github.com/vfile/vfile-find-down)
    — Find files by searching the file system downwards
*   [`vfile-find-up`](https://github.com/vfile/vfile-find-up)
    — Find files by searching the file system upwards
*   [`vfile-location`](https://github.com/vfile/vfile-location)
    — Convert between line/column- and range-based locations
*   [`vfile-statistics`](https://github.com/vfile/vfile-statistics)
    — Count messages per category
*   [`vfile-messages-to-vscode-diagnostics`](https://github.com/shinnn/vfile-messages-to-vscode-diagnostics)
    — Convert to VS Code diagnostics
*   [`vfile-sort`](https://github.com/vfile/vfile-sort)
    — Sort messages by line/column
*   [`vfile-to-eslint`](https://github.com/vfile/vfile-to-eslint)
    — Convert VFiles to ESLint formatter compatible output

## Reporters

The following list of projects show linting results for given virtual files.
Reporters _must_ accept `Array.<VFile>` as their first argument, and return
`string`.  Reporters _may_ accept other values too, in which case it’s suggested
to stick to `vfile-reporter`s interface.

*   [`vfile-reporter`](https://github.com/vfile/vfile-reporter)
    — Stylish reporter
*   [`vfile-reporter-json`](https://github.com/vfile/vfile-reporter-json)
    — JSON reporter
*   [`vfile-reporter-pretty`](https://github.com/vfile/vfile-reporter-pretty)
    — Pretty reporter

## API

### `VFile([options])`

Create a new virtual file.  If `options` is `string` or `Buffer`, treats
it as `{contents: options}`.  If `options` is a `VFile`, returns it.
All other options are set on the newly created `vfile`.

Path related properties are set in the following order (least specific
to most specific): `history`, `path`, `basename`, `stem`, `extname`,
`dirname`.

It’s not possible to set either `dirname` or `extname` without setting
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

### `VFile#toString([encoding])`

Convert contents of `vfile` to string.  If `contents` is a buffer,
`encoding` is used to stringify buffers (default: `'utf8'`).

### `VFile#message(reason[, position[, ruleId]])`

Associates a message with the file for `reason` at `position`.  When an
error is passed in as `reason`, copies the stack.

###### Parameters

*   `reason` (`string` or `Error`)
    — Reason for message, uses the stack and message of the error if given
*   `position` (`Node`, `Location`, or `Position`, optional)
    — Place at which the message occurred in `vfile`
*   `ruleId` (`string`, optional)
    — Category of warning

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

*   `file` (`string`) — File-path (when the message was triggered)
*   `reason` (`string`) — Reason for message
*   `ruleId` (`string?`) — Category of message
*   `source` (`string?`) — Namespace of warning
*   `stack` (`string?`) — Stack of message
*   `fatal` (`boolean?`) — If `true`, marks associated file as no longer
    processable
*   `line` (`number?`) — Starting line of error
*   `column` (`number?`) — Starting column of error
*   `location` (`object`) — Full range information, when available.  Has
    `start` and `end` properties, both set to an object with `line` and
    `column`, set to `number?`

## License

[MIT][license] © [Titus Wormer][author]

<!-- Definitions -->

[build-badge]: https://img.shields.io/travis/vfile/vfile.svg

[build-status]: https://travis-ci.org/vfile/vfile

[coverage-badge]: https://img.shields.io/codecov/c/github/vfile/vfile.svg

[coverage-status]: https://codecov.io/github/vfile/vfile

[npm]: https://docs.npmjs.com/cli/install

[license]: LICENSE

[author]: http://wooorm.com

[vfile]: https://cdn.rawgit.com/vfile/vfile/a20a566/logo.svg

[unified]: https://github.com/unifiedjs/unified

[retext]: https://github.com/wooorm/retext

[remark]: https://github.com/wooorm/remark

[rehype]: https://github.com/wooorm/rehype

[vinyl]: https://github.com/gulpjs/vinyl

[unist]: https://github.com/syntax-tree/unist#list-of-utilities

[messages]: #vfilemessagereason-position-ruleid

[message]: #vfilemessage
