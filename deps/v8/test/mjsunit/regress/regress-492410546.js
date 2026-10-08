// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --for-of-optimization --turbolev-future --turbolev

const resultWithGetter = {};
Object.defineProperty(resultWithGetter, 'value', {
  get: function () { }, // effectively returns undefined
});

const myIterable = {
  [Symbol.iterator]() {
    return {
      next() {
        return Object.create(resultWithGetter);
      }
    };
  }
};

function foo() {
  try {
    for (const v of myIterable) {
      String.prototype.endsWith.apply();
    }
  } catch (e) {}
}
%PrepareFunctionForOptimization(foo);

// Both calls are needed to repro this bug.
foo();
foo();
%OptimizeFunctionOnNextCall(foo);
foo();
