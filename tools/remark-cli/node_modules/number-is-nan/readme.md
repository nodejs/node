# number-is-nan [![Build Status](https://travis-ci.org/sindresorhus/number-is-nan.svg?branch=master)](https://travis-ci.org/sindresorhus/number-is-nan)

> ES6 [`Number.isNaN()`](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Number/isNaN) ponyfill

> Ponyfill: A polyfill that doesn't overwrite the native method


## Install

```
$ npm install --save number-is-nan
```


## Usage

```js
var numberIsNan = require('number-is-nan');

numberIsNan(NaN);
//=> true

numberIsNan('unicorn');
//=> false
```


## License

MIT © [Sindre Sorhus](http://sindresorhus.com)
