# lazy-req [![Build Status](https://travis-ci.org/sindresorhus/lazy-req.svg?branch=master)](https://travis-ci.org/sindresorhus/lazy-req)

> Require modules lazily


## Install

```
$ npm install --save lazy-req
```


## Usage

```js
// pass in `require` or a custom require function
const lazyReq = require('lazy-req')(require);
const _ = lazyReq('lodash');

// Where you would normally do
_.isNumber(2);

// You now instead call it as a function
_().isNumber(2);

// It's cached on consecutive calls
_().isString('unicorn');

// Extract lazy variations of the props you need
const members = lazyReq('lodash')('isNumber', 'isString');

// Useful when using destructuring assignment in ES2015
const {isNumber, isString} = lazyReq('lodash')('isNumber', 'isString');

// Works out of the box for functions and regular properties
const stuff = lazyReq('./math-lib')('sum', 'PHI');
console.log(stuff.sum(1, 2)); // => 3
console.log(stuff.PHI); // => 1.618033
```

### Proxy support in Node.js 6 or later

If you use Node.js 6 or later, you can take advantage of ES2015 proxies and don't need to call it as a function.

```js
const lazyReq = require('lazy-req').proxy(require);
const _ = lazyReq('lodash');

// No need to call it as a function but still lazily required
_.isNumber(2);
```

## Related

- [resolve-from](https://github.com/sindresorhus/resolve-from) - Resolve the path of a module from a given path
- [req-from](https://github.com/sindresorhus/req-from) - Require a module from a given path
- [resolve-pkg](https://github.com/sindresorhus/resolve-pkg) - Resolve the path of a package regardless of it having an entry point
- [lazy-value](https://github.com/sindresorhus/lazy-value) - Create a lazily evaluated value
- [define-lazy-prop](https://github.com/sindresorhus/define-lazy-prop) - Define a lazily evaluated property on an object


## License

MIT © [Sindre Sorhus](https://sindresorhus.com)
