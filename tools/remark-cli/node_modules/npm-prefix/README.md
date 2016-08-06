[![npm](https://nodei.co/npm/npm-prefix.png)](https://npmjs.com/package/npm-prefix)

# npm-prefix

[![Build Status][travis-badge]][travis] [![Dependency Status][david-badge]][david]

Get global npm prefix. Respects npmrc configs.

[travis]: https://travis-ci.org/eush77/npm-prefix
[travis-badge]: https://travis-ci.org/eush77/npm-prefix.svg?branch=master
[david]: https://david-dm.org/eush77/npm-prefix
[david-badge]: https://david-dm.org/eush77/npm-prefix.png

## Example

```js
var npmPrefix = require('npm-prefix');

npmPrefix()
//=> '/usr/local'
```

## API

#### `npmPrefix()`

Returns the npm prefix.

## Install

```
npm install npm-prefix
```

## License

MIT
