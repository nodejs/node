# kind-of [![NPM version](https://badge.fury.io/js/kind-of.svg)](http://badge.fury.io/js/kind-of)  [![Build Status](https://travis-ci.org/jonschlinkert/kind-of.svg)](https://travis-ci.org/jonschlinkert/kind-of)

> Get the native type of a value.

[](#optimizations)**What makes this so fast?**

## Install

Install with [npm](https://www.npmjs.com/)

```sh
$ npm i kind-of --save
```

Install with [bower](http://bower.io/)

```sh
$ bower install kind-of --save
```

## Usage

> es5, browser and es6 ready

```js
var kindOf = require('kind-of');

kindOf(undefined);
//=> 'undefined'

kindOf(null);
//=> 'null'

kindOf(true);
//=> 'boolean'

kindOf(false);
//=> 'boolean'

kindOf(new Boolean(true));
//=> 'boolean'

kindOf(new Buffer(''));
//=> 'buffer'

kindOf(42);
//=> 'number'

kindOf(new Number(42));
//=> 'number'

kindOf('str');
//=> 'string'

kindOf(new String('str'));
//=> 'string'

kindOf(arguments);
//=> 'arguments'

kindOf({});
//=> 'object'

kindOf(Object.create(null));
//=> 'object'

kindOf(new Test());
//=> 'object'

kindOf(new Date());
//=> 'date'

kindOf([]);
//=> 'array'

kindOf([1, 2, 3]);
//=> 'array'

kindOf(new Array());
//=> 'array'

kindOf(/[\s\S]+/);
//=> 'regexp'

kindOf(new RegExp('^' + 'foo$'));
//=> 'regexp'

kindOf(function () {});
//=> 'function'

kindOf(function * () {});
//=> 'function'

kindOf(new Function());
//=> 'function'

kindOf(new Map());
//=> 'map'

kindOf(new WeakMap());
//=> 'weakmap'

kindOf(new Set());
//=> 'set'

kindOf(new WeakSet());
//=> 'weakset'

kindOf(Symbol('str'));
//=> 'symbol'
```

## Related projects

* [is-number](https://github.com/jonschlinkert/is-number): Returns true if the value is a number. comprehensive tests.
* [isobject](https://github.com/jonschlinkert/isobject): Returns true if the value is an object and not an array or null.
* [is-primitive](https://github.com/jonschlinkert/is-primitive): Returns `true` if the value is a primitive.
* [is-plain-object](https://github.com/jonschlinkert/is-plain-object): Returns true if an object was created by the `Object` constructor.
* [is-match](https://github.com/jonschlinkert/is-match): Create a matching function from a glob pattern, regex, string, array or function.

## Benchmarks

Benchmarked against [typeof](http://github.com/CodingFu/typeof) and [type-of](https://github.com/ForbesLindesay/type-of).
Note that performaces is slower for es6 features `Map`, `WeakMap`, `Set` and `WeakSet`.

```bash
#1: array
  current x 23,329,397 ops/sec ±0.82% (94 runs sampled)
  lib-type-of x 4,170,273 ops/sec ±0.55% (94 runs sampled)
  lib-typeof x 9,686,935 ops/sec ±0.59% (98 runs sampled)

#2: boolean
  current x 27,197,115 ops/sec ±0.85% (94 runs sampled)
  lib-type-of x 3,145,791 ops/sec ±0.73% (97 runs sampled)
  lib-typeof x 9,199,562 ops/sec ±0.44% (99 runs sampled)

#3: date
  current x 20,190,117 ops/sec ±0.86% (92 runs sampled)
  lib-type-of x 5,166,970 ops/sec ±0.74% (94 runs sampled)
  lib-typeof x 9,610,821 ops/sec ±0.50% (96 runs sampled)

#4: function
  current x 23,855,460 ops/sec ±0.60% (97 runs sampled)
  lib-type-of x 5,667,740 ops/sec ±0.54% (100 runs sampled)
  lib-typeof x 10,010,644 ops/sec ±0.44% (100 runs sampled)

#5: null
  current x 27,061,047 ops/sec ±0.97% (96 runs sampled)
  lib-type-of x 13,965,573 ops/sec ±0.62% (97 runs sampled)
  lib-typeof x 8,460,194 ops/sec ±0.61% (97 runs sampled)

#6: number
  current x 25,075,682 ops/sec ±0.53% (99 runs sampled)
  lib-type-of x 2,266,405 ops/sec ±0.41% (98 runs sampled)
  lib-typeof x 9,821,481 ops/sec ±0.45% (99 runs sampled)

#7: object
  current x 3,348,980 ops/sec ±0.49% (99 runs sampled)
  lib-type-of x 3,245,138 ops/sec ±0.60% (94 runs sampled)
  lib-typeof x 9,262,952 ops/sec ±0.59% (99 runs sampled)

#8: regex
  current x 21,284,827 ops/sec ±0.72% (96 runs sampled)
  lib-type-of x 4,689,241 ops/sec ±0.43% (100 runs sampled)
  lib-typeof x 8,957,593 ops/sec ±0.62% (98 runs sampled)

#9: string
  current x 25,379,234 ops/sec ±0.58% (96 runs sampled)
  lib-type-of x 3,635,148 ops/sec ±0.76% (93 runs sampled)
  lib-typeof x 9,494,134 ops/sec ±0.49% (98 runs sampled)

#10: undef
  current x 27,459,221 ops/sec ±1.01% (93 runs sampled)
  lib-type-of x 14,360,433 ops/sec ±0.52% (99 runs sampled)
  lib-typeof x 23,202,868 ops/sec ±0.59% (94 runs sampled)
```

## Optimizations

In 7 out of 8 cases, this library is 2x-10x faster than other top libraries included in the benchmarks. There are a few things that lead to this performance advantage, none of them hard and fast rules, but all of them simple and repeatable in almost any code library:

1. Optimize around the fastest and most common use cases first. Of course, this will change from project-to-project, but I took some time to understand how and why `typeof` checks were being used in my own libraries and other libraries I use a lot.
2. Optimize around bottlenecks - In other words, the order in which conditionals are implemented is significant, because each check is only as fast as the failing checks that came before it. Here, the biggest bottleneck by far is checking for plain objects (an object that was created by the `Object` constructor). I opted to make this check happen by process of elimination rather than brute force up front (e.g. by using something like `val.constructor.name`), so that every other type check would not be penalized it.
3. Don't do uneccessary processing - why do `.slice(8, -1).toLowerCase();` just to get the word `regex`? It's much faster to do `if (type === '[object RegExp]') return 'regex'`

## Running tests

Install dev dependencies:

```sh
$ npm i -d && npm test
```

## Contributing

Pull requests and stars are always welcome. For bugs and feature requests, [please create an issue](https://github.com/jonschlinkert/kind-of/issues/new)

## Author

**Jon Schlinkert**

+ [github/jonschlinkert](https://github.com/jonschlinkert)
+ [twitter/jonschlinkert](http://twitter.com/jonschlinkert)

## License

Copyright © 2014-2015 [Jon Schlinkert](https://github.com/jonschlinkert)
Released under the MIT license.

***

_This file was generated by [verb-cli](https://github.com/assemble/verb-cli) on May 31, 2015._
