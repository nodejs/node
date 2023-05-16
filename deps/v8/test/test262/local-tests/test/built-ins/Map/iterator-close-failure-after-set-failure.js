// Copyright (C) 2017 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 23.1.1.1
description: >
  The correct error is thrown `Map.prototype.set` throws an error and
  the IteratorClose throws an error.
features: [Symbol.iterator]
---*/

var count = 0;
var iterable = {};
iterable[Symbol.iterator] = function() {
  return {
    next: function() {
      return { value: [], done: false };
    },
    return: function() {
        throw new TypeError('ignore');
    }
  };
};
Map.prototype.set = function() { throw new Test262Error(); }

assert.throws(Test262Error, function() {
  new Map(iterable);
});
