# vfile-sort [![Build Status](https://img.shields.io/travis/wooorm/vfile-sort.svg)](https://travis-ci.org/wooorm/vfile-sort) [![Coverage Status](https://img.shields.io/codecov/c/github/wooorm/vfile-sort.svg)](https://codecov.io/github/wooorm/vfile-sort)

Sorts [`VFile`](https://github.com/wooorm/vfile) [`messages`](https://github.com/wooorm/vfile#vfilemessages).

## Installation

[npm](https://docs.npmjs.com/cli/install):

```bash
npm install vfile-sort
```

**vfile-sort** is also available for [bower](http://bower.io/#install-packages),
[component](https://github.com/componentjs/component), and
[duo](http://duojs.org/#getting-started), and as an AMD, CommonJS, and globals
module, [uncompressed](vfile-sort.js) and
[compressed](vfile-sort.min.js).

## Usage

```js
var vfile = require('vfile');
var sort = require('vfile-sort');

var file = vfile();

file.warn('Error!', {
    'line': 3,
    'column': 1
});

file.warn('Another!', {
    'line': 2,
    'column': 2
});

sort(file);

console.log(file.messages.map(String));
/*
 * [
 *   '2:2: Another!',
 *   '3:1: Error!'
 * ]
 */
```

## API

### sort(vfile)

Sorts [`messages`](https://github.com/wooorm/vfile#vfilemessages) in the given
[`VFile`](https://github.com/wooorm/vfile).

## License

[MIT](LICENSE) © [Titus Wormer](http://wooorm.com)
