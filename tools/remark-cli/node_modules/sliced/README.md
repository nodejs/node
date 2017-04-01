#sliced
==========

A faster alternative to `[].slice.call(arguments)`.

[![Build Status](https://secure.travis-ci.org/aheckmann/sliced.png)](http://travis-ci.org/aheckmann/sliced)

Example output from [benchmark.js](https://github.com/bestiejs/benchmark.js)

    Array.prototype.slice.call x 1,401,820 ops/sec ±2.16% (90 runs sampled)
    [].slice.call x 1,313,116 ops/sec ±2.04% (96 runs sampled)
    cached slice.call x 10,297,910 ops/sec ±1.81% (96 runs sampled)
    sliced x 19,906,019 ops/sec ±1.23% (89 runs sampled)
    fastest is sliced

    Array.prototype.slice.call(arguments, 1) x 1,373,238 ops/sec ±1.84% (95 runs sampled)
    [].slice.call(arguments, 1) x 1,395,336 ops/sec ±1.36% (93 runs sampled)
    cached slice.call(arguments, 1) x 9,926,018 ops/sec ±1.67% (92 runs sampled)
    sliced(arguments, 1) x 20,747,990 ops/sec ±1.16% (93 runs sampled)
    fastest is sliced(arguments, 1)

    Array.prototype.slice.call(arguments, -1) x 1,319,908 ops/sec ±2.12% (91 runs sampled)
    [].slice.call(arguments, -1) x 1,336,170 ops/sec ±1.33% (97 runs sampled)
    cached slice.call(arguments, -1) x 10,078,718 ops/sec ±1.21% (98 runs sampled)
    sliced(arguments, -1) x 20,471,474 ops/sec ±1.81% (92 runs sampled)
    fastest is sliced(arguments, -1)

    Array.prototype.slice.call(arguments, -2, -10) x 1,369,246 ops/sec ±1.68% (97 runs sampled)
    [].slice.call(arguments, -2, -10) x 1,387,935 ops/sec ±1.70% (95 runs sampled)
    cached slice.call(arguments, -2, -10) x 9,593,428 ops/sec ±1.23% (97 runs sampled)
    sliced(arguments, -2, -10) x 23,178,931 ops/sec ±1.70% (92 runs sampled)
    fastest is sliced(arguments, -2, -10)

    Array.prototype.slice.call(arguments, -2, -1) x 1,441,300 ops/sec ±1.26% (98 runs sampled)
    [].slice.call(arguments, -2, -1) x 1,410,326 ops/sec ±1.96% (93 runs sampled)
    cached slice.call(arguments, -2, -1) x 9,854,419 ops/sec ±1.02% (97 runs sampled)
    sliced(arguments, -2, -1) x 22,550,801 ops/sec ±1.86% (91 runs sampled)
    fastest is sliced(arguments, -2, -1)

_Benchmark  [source](https://github.com/aheckmann/sliced/blob/master/bench.js)._

##Usage

`sliced` accepts the same arguments as `Array#slice` so you can easily swap it out.

```js
function zing () {
  var slow = [].slice.call(arguments, 1, 8);
  var args = slice(arguments, 1, 8);

  var slow = Array.prototype.slice.call(arguments);
  var args = slice(arguments);
  // etc
}
```

## install

    npm install sliced


[LICENSE](https://github.com/aheckmann/sliced/blob/master/LICENSE)
