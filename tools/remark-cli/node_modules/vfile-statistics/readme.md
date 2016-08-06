# vfile-statistics [![Build Status][travis-badge]][travis] [![Coverage Status][codecov-badge]][codecov]

Count [vfile][] messages per category (fatal, nonfatal, and total).

## Installation

[npm][npm-install]:

```bash
npm install vfile-statistics
```

## Usage

```js
var statistics = require('vfile-statistics');
var vfile = require('vfile');

var file = vfile({path: '~/example.md'});

file.message('This could be better');
file.message('That could be better');

try {
  file.fail('This is terribly wrong');
} catch (err) {}

console.log(statistics(file));
```

Yields:

```js
{ fatal: 1, nonfatal: 2, total: 3 }
```

## API

### `statistics(file)`

Pass a [vfile][], list of vfiles, or a list of messages
(`file.messages`), get counts per category.

## License

[MIT][license] © [Titus Wormer][author]

<!-- Definitions -->

[travis-badge]: https://img.shields.io/travis/wooorm/vfile-statistics.svg

[travis]: https://travis-ci.org/wooorm/vfile-statistics

[codecov-badge]: https://img.shields.io/codecov/c/github/wooorm/vfile-statistics.svg

[codecov]: https://codecov.io/github/wooorm/vfile-statistics

[npm-install]: https://docs.npmjs.com/cli/install

[license]: LICENSE

[author]: http://wooorm.com

[vfile]: https://github.com/wooorm/vfile
