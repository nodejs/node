# require-uncached [![Build Status](https://travis-ci.org/sindresorhus/require-uncached.svg?branch=master)](https://travis-ci.org/sindresorhus/require-uncached)

> Require a module bypassing the [cache](http://nodejs.org/api/modules.html#modules_caching)

Useful for testing purposes when you need to freshly require a module.


## Install

```sh
$ npm install --save require-uncached
```


## Usage

```js
// foo.js
var i = 0;
module.exports = function () {
	return ++i;
};
```

```js
var requireUncached = require('require-uncached');

require('./foo')();
//=> 1

require('./foo')();
//=> 2

requireUncached('./foo')();
//=> 1

requireUncached('./foo')();
//=> 1
```


## License

MIT © [Sindre Sorhus](http://sindresorhus.com)
