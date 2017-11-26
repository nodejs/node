# vfile-reporter [![Build Status][travis-badge]][travis] [![Coverage Status][codecov-badge]][codecov]

Format [**vfile**][vfile]s using a stylish reporter.

![Example screenshot of **vfile-reporter**][screenshot]

## Features

*   [x] Ranges (`3:2` and `3:2-3:6`)
*   [x] Stack-traces to show where awful stuff occurs
*   [x] Successful files (configurable)
*   [x] All of [**VFile**][vfile]’s awesomeness

## Installation

[npm][]:

```bash
npm install vfile-reporter
```

## Usage

Say `example.js` contains:

```javascript
var vfile = require('vfile');
var reporter = require('vfile-reporter');

var one = vfile({path: 'test/fixture/1.js'});
var two = vfile({path: 'test/fixture/2.js'});

one.message('Warning!', {line: 2, column: 4});

console.error(reporter([one, two]));
```

Now, running `node example` yields:

```txt
test/fixture/1.js
  2:4  warning  Warning!

test/fixture/2.js: no issues found

⚠ 1 warning
```

## API

### `reporter(files[, options])`

Generate a stylish report from the given [`vfile`][vfile], `Array.<VFile>`,
or `Error`.

##### `options`

###### `options.quiet`

Do not output anything for a file which has no warnings or errors (`boolean`,
default: `false`).  The default behaviour is to show a success message.

###### `options.silent`

Do not output messages without `fatal` set to true (`boolean`, default:
`false`).  Also sets `quiet` to `true`.

###### `options.color`

Whether to use colour (`boolean`, default: depends).  The default behaviour
is the check if [colour is supported][support].

###### `options.defaultName`

Label to use for files without file-path (`string`, default: `'<stdin>'`).
If one file and no `defaultName` is given, no name will show up in the report.

## Related

*   [`vfile-reporter-json`](https://github.com/vfile/vfile-reporter-json)
    — JSON reporter
*   [`vfile-reporter-pretty`](https://github.com/vfile/vfile-reporter-pretty)
    — Pretty reporter
*   [`convert-vinyl-to-vfile`](https://github.com/dustinspecker/convert-vinyl-to-vfile)
    — Convert from [Vinyl][]
*   [`vfile-statistics`](https://github.com/vfile/vfile-statistics)
    — Count messages per category
*   [`vfile-sort`](https://github.com/vfile/vfile-sort)
    — Sort messages by line/column

## License

[MIT][license] © [Titus Wormer][author]

Forked from [ESLint][]’s stylish reporter
(originally created by Sindre Sorhus), which is Copyright (c) 2013
Nicholas C. Zakas, and licensed under MIT.

<!-- Definitions -->

[travis-badge]: https://img.shields.io/travis/vfile/vfile-reporter.svg

[travis]: https://travis-ci.org/vfile/vfile-reporter

[codecov-badge]: https://img.shields.io/codecov/c/github/vfile/vfile-reporter.svg

[codecov]: https://codecov.io/github/vfile/vfile-reporter

[npm]: https://docs.npmjs.com/cli/install

[license]: LICENSE

[author]: http://wooorm.com

[eslint]: https://github.com/eslint/eslint

[vfile]: https://github.com/vfile/vfile

[screenshot]: ./screenshot.png

[support]: https://github.com/chalk/supports-color

[vinyl]: https://github.com/gulpjs/vinyl
