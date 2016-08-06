# regex-cache [![NPM version](https://img.shields.io/npm/v/regex-cache.svg?style=flat)](https://www.npmjs.com/package/regex-cache) [![NPM downloads](https://img.shields.io/npm/dm/regex-cache.svg?style=flat)](https://npmjs.org/package/regex-cache) [![Build Status](https://img.shields.io/travis/jonschlinkert/regex-cache.svg?style=flat)](https://travis-ci.org/jonschlinkert/regex-cache)

> Memoize the results of a call to the RegExp constructor, avoiding repetitious runtime compilation of the same string and options, resulting in suprising performance improvements.

## Install

Install with [npm](https://www.npmjs.com/):

```sh
$ npm install regex-cache --save
```

* Read [what this does](#what-this-does).
* See [the benchmarks](#benchmarks)

## Usage

Wrap a function like this:

```js
var cache = require('regex-cache');
var someRegex = cache(require('some-regex-lib'));
```

**Caching a regex**

If you want to cache a regex after calling `new RegExp()`, or you're requiring a module that returns a regex, wrap it with a function first:

```js
var cache = require('regex-cache');

function yourRegex(str, opts) {
  // do stuff to str and opts
  return new RegExp(str, opts.flags);
}

var regex = cache(yourRegex);
```

## Recommendations

### Use this when...

* **No options are passed** to the function that creates the regex. Regardless of how big or small the regex is, when zero options are passed, caching will be faster than not.
* **A few options are passed**, and the values are primitives. The limited benchmarks I did show that caching is beneficial when up to 8 or 9 options are passed.

### Do not use this when...

* **The values of options are not primitives**. When non-primitives must be compared for equality, the time to compare the options is most likely as long or longer than the time to just create a new regex.

### Example benchmarks

Performance results, with and without regex-cache:

```bash
# no args passed (defaults)
  with-cache x 8,699,231 ops/sec ±0.86% (93 runs sampled)
  without-cache x 2,777,551 ops/sec ±0.63% (95 runs sampled)

# string and six options passed
  with-cache x 1,885,934 ops/sec ±0.80% (93 runs sampled)
  without-cache x 1,256,893 ops/sec ±0.65% (97 runs sampled)

# string only
  with-cache x 7,723,256 ops/sec ±0.87% (92 runs sampled)
  without-cache x 2,303,060 ops/sec ±0.47% (99 runs sampled)

# one option passed
  with-cache x 4,179,877 ops/sec ±0.53% (100 runs sampled)
  without-cache x 2,198,422 ops/sec ±0.47% (95 runs sampled)

# two options passed
  with-cache x 3,256,222 ops/sec ±0.51% (99 runs sampled)
  without-cache x 2,121,401 ops/sec ±0.79% (97 runs sampled)

# six options passed
  with-cache x 1,816,018 ops/sec ±1.08% (96 runs sampled)
  without-cache x 1,157,176 ops/sec ±0.53% (100 runs sampled)

# 
# diminishing returns happen about here
# 

# ten options passed
  with-cache x 1,210,598 ops/sec ±0.56% (92 runs sampled)
  without-cache x 1,665,588 ops/sec ±1.07% (100 runs sampled)

# twelve options passed
  with-cache x 1,042,096 ops/sec ±0.68% (92 runs sampled)
  without-cache x 1,389,414 ops/sec ±0.68% (97 runs sampled)

# twenty options passed
  with-cache x 661,125 ops/sec ±0.80% (93 runs sampled)
  without-cache x 1,208,757 ops/sec ±0.65% (97 runs sampled)

# 
# when non-primitive values are compared
# 

# single value on the options is an object
  with-cache x 1,398,313 ops/sec ±1.05% (95 runs sampled)
  without-cache x 2,228,281 ops/sec ±0.56% (99 runs sampled)
```

## Run benchmarks

Install dev dependencies:

```bash
npm i -d && npm run benchmarks
```

## What this does

If you're using `new RegExp('foo')` instead of a regex literal, it's probably because you need to dyamically generate a regex based on user options or some other potentially changing factors.

When your function creates a string based on user inputs and passes it to the `RegExp` constructor, regex-cache caches the results. The next time the function is called if the key of a cached regex matches the user input (or no input was given), the cached regex is returned, avoiding unnecessary runtime compilation.

Using the RegExp constructor offers a lot of flexibility, but the runtime compilation comes at a price - it's slow. Not specifically because of the call to the RegExp constructor, but **because you have to build up the string before `new RegExp()` is even called**.
## Contributing

Pull requests and stars are always welcome. For bugs and feature requests, [please create an issue](https://github.com/jonschlinkert/regex-cache/issues/new).

## Building docs

Generate readme and API documentation with [verb](https://github.com/verbose/verb):

```sh
$ npm install verb && npm run docs
```

Or, if [verb](https://github.com/verbose/verb) is installed globally:

```sh
$ verb
```

## Running tests

Install dev dependencies:

```sh
$ npm install -d && npm test
```

## Author

**Jon Schlinkert**

* [github/jonschlinkert](https://github.com/jonschlinkert)
* [twitter/jonschlinkert](http://twitter.com/jonschlinkert)

## License

Copyright © 2016, [Jon Schlinkert](https://github.com/jonschlinkert).
Released under the [MIT license](https://github.com/jonschlinkert/regex-cache/blob/master/LICENSE).

***

_This file was generated by [verb](https://github.com/verbose/verb), v, on April 01, 2016._