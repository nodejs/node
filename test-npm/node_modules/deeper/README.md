[![deeper on npm](https://img.shields.io/npm/v/deeper.svg?style=flat)](http://npm.im/deeper)
[![Build Status](https://travis-ci.org/othiym23/node-deeper.svg?branch=master)](https://travis-ci.org/othiym23/node-deeper)
[![Coverage Status](https://coveralls.io/repos/othiym23/node-deeper/badge.svg?branch=master&service=github)](https://coveralls.io/github/othiym23/node-deeper?branch=master)
[!["standard" style](https://img.shields.io/badge/code%20style-standard-brightgreen.svg?style=flat)](https://github.com/feross/standard)


# deeper

`deeper` is a library for structurally comparing the equality of JavaScript
values. It supports recursive / cyclical data structures, is written to avoid
try / catch / throw (for speed), and has no dependencies by default.

If you're running Node 0.12+ or io.js, `deeper` will use the built-in
`Buffer.equals()`.  If you're running an older version of Node and you install
[Ben Noordhuis](http://github.com/bnoordhuis)'s
[buffertools](https://github.com/bnoordhuis/node-buffertools) into a project
using `deeper`, it will use that to speed up comparison of Buffers. This used
to be installed as an optional dependency, but it gets in the way of
browserification and also makes using `deeper` in your own projects harder, so
I changed it to just try to use it if it's there.

It has some optimizations, but stresses correctness over raw speed (unless
you're testing objects with lots of Buffers attached to them, in which case it
plus `buffertools` is likely to be the fastest general-purpose deep-comparison
tool available).

The core algorithm is based on those used by Node's assertion library and the
implementation of cycle detection in
[isEqual](http://underscorejs.org/#isEqual) in
[Underscore.js](http://underscorejs.org/).

I like to think the documentation is pretty OK.

## installation

```
npm install deeper
```

## usage

```javascript
// vanilla
var deepEqual = require('deeper')

if (!deepEqual(obj1, obj2)) console.log("yay! diversity!");
```

## details

Copied from the source, here are the details of `deeper`'s algorithm:

1. `===` only tests objects and functions by reference. `null` is an object.
   Any pairs of identical entities failing this test are therefore objects
   (including `null`), which need to be recursed into and compared attribute by
   attribute.
2. Since the only entities to get to this test must be objects, if `a` or `b`
   is not an object, they're clearly not the same. All unfiltered `a` and `b`
   getting past this are objects (including `null`).
3. `null` is an object, but `null === null.` All unfiltered `a` and `b` are
   non-null `Objects`.
4. Buffers need to be special-cased because they live partially on the wrong
   side of the C++ / JavaScript barrier. Still, calling this on structures
   that can contain Buffers is a bad idea, because they can contain
   multiple megabytes of data and comparing them byte-by-byte is hella
   expensive.
5. It's much faster to compare dates by numeric value (`.getTime()`) than by
   lexical value.
6. Compare `RegExps` by their components, not the objects themselves.
7. Treat argumens objects like arrays. The parts of an arguments list most
   people care about are the arguments themselves, not `callee`, which you
   shouldn't be looking at anyway.
8. Objects are more complex:
    1. Ensure that `a` and `b` are on the same constructor chain.
    2. Ensure that `a` and `b` have the same number of own properties (which is
       what `Object.keys()` returns).
    3. Ensure that cyclical references don't blow up the stack.
    4. Ensure that all the key names match (faster).
    5. Ensure that all of the associated values match, recursively (slower).

### (somewhat untested) assumptions:

- Functions are only considered identical if they unify to the same reference.
  To anything else is to invite the wrath of the halting problem.
- V8 is smart enough to optimize treating an Array like any other kind of
  object.
- Users of this function are cool with mutually recursive data structures that
  are otherwise identical being treated as the same.

## license
BSD. Go nuts.
