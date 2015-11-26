# pinkie-promise [![Build Status](https://travis-ci.org/floatdrop/pinkie-promise.svg?branch=master)](https://travis-ci.org/floatdrop/pinkie-promise)

> [ES6 Promise](https://people.mozilla.org/~jorendorff/es6-draft.html#sec-promise-objects) ponyfill
>
> Ponyfill: A polyfill that doesn't overwrite the native method

Module exports global Promise object (if available) or [`pinkie`](http://github.com/floatdrop/pinkie) Promise polyfill.

## Install

```
$ npm install --save pinkie-promise
```

## Usage

```js
var Promise = require('pinkie-promise');

new Promise(function (resolve) { resolve('unicorns'); });
//=> Promise { 'unicorns' }
```

## License

MIT © [Vsevolod Strukchinsky](http://github.com/floatdrop)
