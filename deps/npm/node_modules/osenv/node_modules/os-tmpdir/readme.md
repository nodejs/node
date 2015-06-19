# os-tmpdir [![Build Status](https://travis-ci.org/sindresorhus/os-tmpdir.svg?branch=master)](https://travis-ci.org/sindresorhus/os-tmpdir)

> Node.js [`os.tmpdir()`](https://nodejs.org/api/os.html#os_os_tmpdir) ponyfill

> Ponyfill: A polyfill that doesn't overwrite the native method

Use this instead of `require('os').tmpdir()` to get a consistent behaviour on different Node.js versions (even 0.8).

*This is actually taken from io.js 2.0.2 as it contains some fixes that haven't bubbled up to Node.js yet.*


## Install

```
$ npm install --save os-tmpdir
```


## Usage

```js
var osTmpdir = require('os-tmpdir');

osTmpdir();
//=> /var/folders/m3/5574nnhn0yj488ccryqr7tc80000gn/T
```


## API

See the [`os.tmpdir()` docs](https://nodejs.org/api/os.html#os_os_tmpdir).


## License

MIT © [Sindre Sorhus](http://sindresorhus.com)
