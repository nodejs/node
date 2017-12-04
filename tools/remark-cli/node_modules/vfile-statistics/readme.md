# vfile-statistics [![Build Status][travis-badge]][travis] [![Coverage Status][codecov-badge]][codecov]

Count [vfile][] messages per category (fatal, warn, info, nonfatal and total).

## Installation

[npm][]:

```bash
npm install vfile-statistics
```

## Usage

```js
var vfile = require('vfile');
var statistics = require('vfile-statistics');

var file = vfile({path: '~/example.md'});

file.message('This could be better');
file.message('That could be better');

try {
  file.fail('This is terribly wrong');
} catch (err) {}

file.info('This is perfect');

console.log(statistics(file));
```

Yields:

```js
{ fatal: 1, nonfatal: 3, warn: 2, info: 1, total: 4 }
```

## API

### `statistics(file)`

Pass a [vfile][], list of vfiles, or a list of messages
(`file.messages`), get counts per category.

###### Returns

`Object`:

*   `fatal`: fatal errors (`fatal: true`)
*   `warn`: warning messages (`fatal: false`)
*   `info`: informational messages (`fatal: null` or `fatal: undefined`)
*   `nonfatal`: warning or info messages
*   `total`: all messages

## License

[MIT][license] © [Titus Wormer][author]

<!-- Definitions -->

[travis-badge]: https://img.shields.io/travis/vfile/vfile-statistics.svg

[travis]: https://travis-ci.org/vfile/vfile-statistics

[codecov-badge]: https://img.shields.io/codecov/c/github/vfile/vfile-statistics.svg

[codecov]: https://codecov.io/github/vfile/vfile-statistics

[npm]: https://docs.npmjs.com/cli/install

[license]: LICENSE

[author]: http://wooorm.com

[vfile]: https://github.com/vfile/vfile
