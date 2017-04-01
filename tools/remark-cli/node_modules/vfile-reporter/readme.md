# vfile-reporter [![Build Status][travis-badge]][travis] [![Coverage Status][codecov-badge]][codecov]

<!--lint disable heading-increment list-item-spacing-->

Format [**VFile**][vfile]s using a stylish reporter.

![Example screen shot of **vfile-reporter**][screenshot]

## Features

*   [x] Ranges
    — Not just a starting position, such as `3:2`, but `3:2-3:6`.
*   [x] Stack-traces
    — When something awful happens, you want to know **where** it occurred,
    stack-traces help answer that question.
*   [x] Successful files (configurable)
    — Sometimes you want to know if things went okay.
*   [x] And all of [**VFile**](https://github.com/wooorm/vfile)s awesomeness.

## Installation

[npm][npm-install]:

```bash
npm install vfile-reporter
```

**vfile-reporter** is also available as an AMD, CommonJS, and
globals module, [uncompressed and compressed][releases].

## Usage

Dependencies:

```javascript
var vfile = require('vfile');
var reporter = require('vfile-reporter');
```

Files:

```javascript
var one = vfile({path: 'test/fixture/1.js'});
var two = vfile({path: 'test/fixture/2.js'});
```

Trigger a warning:

```javascript
one.message('Warning!', {line: 2, column: 4});
```

Report:

```javascript
var report = reporter([one, two], {color: false});
```

Yields:

```txt
test/fixture/1.js
  2:4  warning  Warning!

test/fixture/2.js: no issues found

⚠ 1 warning
```

## API

### `reporter(files[, options])`

Generate a stylish report from the given files.

###### Parameters

*   `files` (`Error`, [`VFile`][vfile], or `Array.<VFile>`).
*   `options` (`object`, optional):

    *   `quiet` (`boolean`, default: `false`)
        — Do not output anything for a file which has no warnings or
        errors.  The default behaviour is to show a success message.
    *   `silent` (`boolean`, default: `false`)
        — Do not output messages without `fatal` set to true.
        Also sets `quiet` to `true`.
    *   `color` (`boolean`, default: `true`)
        — Whether to use colour.
    *   `defaultName` (`string`, default: `'<stdin>'`)
        — Label to use for files without file-path.
        If one file and no `defaultName` is given, no name
        will show up in the report.

###### Returns

`string`, a stylish report.

## License

[MIT][license] © [Titus Wormer][author]

Forked from [ESLint][]’s stylish reporter
(originally created by Sindre Sorhus), which is Copyright (c) 2013
Nicholas C. Zakas, and licensed under MIT.

<!-- Definitions -->

[travis-badge]: https://img.shields.io/travis/wooorm/vfile-reporter.svg

[travis]: https://travis-ci.org/wooorm/vfile-reporter

[codecov-badge]: https://img.shields.io/codecov/c/github/wooorm/vfile-reporter.svg

[codecov]: https://codecov.io/github/wooorm/vfile-reporter

[npm-install]: https://docs.npmjs.com/cli/install

[releases]: https://github.com/wooorm/retext-intensify/releases

[license]: LICENSE

[author]: http://wooorm.com

[eslint]: https://github.com/eslint/eslint

[vfile]: https://github.com/wooorm/vfile

[screenshot]: ./screenshot.png
