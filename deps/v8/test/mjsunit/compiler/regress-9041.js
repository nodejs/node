// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

(function() {
class A {}

function foo(a, fn) {
  const C = a.constructor;
  fn(a);
  return a instanceof C;
};
%PrepareFunctionForOptimization(foo);
assertTrue(foo(new A(), a => {}));
assertTrue(foo(new A(), a => {}));
%OptimizeFunctionOnNextCall(foo);
assertTrue(foo(new A(), a => {}));
assertFalse(foo(new A(), a => {
  a.__proto__ = {};
}));
})();
