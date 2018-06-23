# detab [![Build Status][travis-badge]][travis] [![Coverage Status][codecov-badge]][codecov]

Detab: tabs → spaces.

## Installation

[npm][]:

```bash
npm install detab
```

## Usage

```javascript
var detab = require('detab');

var four = detab('\tfoo\nbar\tbaz');
var two = detab('\tfoo\nbar\tbaz', 2);
var eight = detab('\tfoo\nbar\tbaz', 8);
```

Yields:

```text
    foo
bar baz
```

```text
  foo
bar baz
```

```text
        foo
bar     baz
```

## API

### `detab(value[, size=4])`

Replace tabs with spaces in `value` (`string`), being smart about which
column the tab is at and which `size` (`number`, default: `4`) should be used.

## License

[MIT][license] © [Titus Wormer][author]

<!-- Definitions -->

[travis-badge]: https://img.shields.io/travis/wooorm/detab.svg

[travis]: https://travis-ci.org/wooorm/detab

[codecov-badge]: https://img.shields.io/codecov/c/github/wooorm/detab.svg

[codecov]: https://codecov.io/github/wooorm/detab

[npm]: https://docs.npmjs.com/cli/install

[license]: LICENSE

[author]: http://wooorm.com
