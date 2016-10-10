[![only-shallow on npm](https://img.shields.io/npm/v/only-shallow.svg?style=flat)](http://npm.im/only-shallow)
[![Build Status](https://travis-ci.org/othiym23/only-shallow.svg?branch=master)](https://travis-ci.org/othiym23/only-shallow)
[![Coverage Status](https://coveralls.io/repos/othiym23/only-shallow/badge.svg?branch=master&service=github)](https://coveralls.io/github/othiym23/only-shallow?branch=master)
[!["standard" style](https://img.shields.io/badge/code%20style-standard-brightgreen.svg?style=flat)](https://github.com/feross/standard)

# only-shallow

If [`deeper`](http://npm.im/deeper) and [`deepest`](http://npm.im/deepest) are
`assert.deepEqual()`'s strict East Coast siblings with engineering backgrounds,
`only-shallow` is their laid-back California cousin.  `only-shallow` is a
library for structurally comparing JavaScript objects. It supports recursive /
cyclical data structures, is written to avoid try / catch / throw (for speed),
and has no dependencies. It's not particularly strict about matching types.
It's more of a duck squeezer.

It has some optimizations but stresses correctness over raw speed. Unlike
`deepest`, it has no native dependencies, so you can use it, like, wherever.

If you install [Ben Noordhuis](http://github.com/bnoordhuis)'s
[buffertools](https://github.com/bnoordhuis/node-buffertools) into a project
using `only-shallow`, it will use that to speed up comparison of Buffers.

The core algorithm is based on those used by Node's assertion library and the
implementation of cycle detection in
[isEqual](http://underscorejs.org/#isEqual) in
[Underscore.js](http://underscorejs.org/).

I like to think the documentation is pretty OK.

`only-shallow` has this name because [I'm
old](https://www.youtube.com/watch?v=oiomcuNlVjk).

## installation

```
npm install only-shallow
```

## usage

```javascript
var deepEqual = require('only-shallow')

if (!deepEqual(obj1, obj2)) console.log("yay! diversity!");
```

## details

Copied from the source, here are the details of `only-shallow`'s algorithm:

1. Use loose equality (`==`) only for value types (non-objects). This is the
   biggest difference between `only-shallow` and `deeper` / `deepest`.
   `only-shallow` cares more about shape and contents than type. This step will
   also catch functions, with the useful (default) property that only
   references to the same function are considered equal. 'Ware the halting
   problem!
2. `null` *is* an object – a singleton value object, in fact – so if
   either is `null`, return a == b. For the purposes of `only-shallow`,
   loose testing of emptiness makes sense.
3. Since the only way to make it this far is for `a` or `b` to be an object, if
   `a` or `b` is *not* an object, they're clearly not the same.
4. It's much faster to compare dates by numeric value (`.getTime()`) than by
   lexical value.
5. Compare RegExps by their components, not the objects themselves.
6. The parts of an arguments list most people care about are the arguments
   themselves, not the callee, which you shouldn't be looking at anyway.
7. Objects are more complex:
   1. Return `true` if `a` and `b` both have no properties.
   2. Ensure that `a` and `b` have the same number of own properties (which is
      what `Object.keys()` returns).
   3. Ensure that cyclical references don't blow up the stack.
   4. Ensure that all the key names match (faster).
   5. Ensure that all of the associated values match, recursively (slower).

## license

ISC. Go nuts.
