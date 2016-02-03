# vfile-reporter [![Build Status](https://img.shields.io/travis/wooorm/vfile-reporter.svg)](https://travis-ci.org/wooorm/vfile-reporter) [![Coverage Status](https://img.shields.io/codecov/c/github/wooorm/vfile-reporter.svg)](https://codecov.io/github/wooorm/vfile-reporter)

Format [**VFile**](https://github.com/wooorm/vfile)s using a stylish reporter.

Originally forked from ESLint’s stylish reporter, but with some coolness
added.

![Example screen shot of **vfile-reporter**](./screenshot.png)

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

[npm](https://docs.npmjs.com/cli/install):

```bash
npm install vfile-report
```

## Usage

```js
var toVFile = require('to-vfile');
var reporter = require('vfile-reporter');

var one = toVFile('test/fixture/1.js');
var two = toVFile('test/fixture/2.js');

/*
 * See VFile’s docs for more info on how to warn.
 */

one.warn('Warning!', {
    'line': 2,
    'column': 4
});

console.log(reporter([one, two]));
```

Yields:

```text
test/fixture/1.js
        2:4  warning  Warning!

test/fixture/2.js: no issues found

⚠ 1 warning
```

## API

### reporter(vfiles\[, options\])

Generate a stylish report from the given files.

**Signatures**

*   `report = reporter(file[, options])`
*   `report = reporter(files[, options])`

**Parameters**

*   `options` (`Object`, optional)

    *   `quiet` (`boolean`, default: `false`)
        — Do not output anything for a file which has no warnings or errors.
        The default behaviour is to show a success message.

    *   `silent` (`boolean`, default: `false`)
        — Do not output messages without `fatal` set to true.
        Also sets `quiet` to `true`.

**Returns**: `string`, a stylish report.

## License

[MIT](LICENSE) © [Titus Wormer](http://wooorm.com)

Forked from [ESLint](https://github.com/eslint/eslint)’s stylish reporter
(originally created by Sindre Sorhus), which is Copyright (c) 2013
Nicholas C. Zakas, and licensed under MIT.
