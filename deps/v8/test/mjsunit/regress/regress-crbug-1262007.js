// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbofan

function foo(...args) {
  class C {}
  C(...args);
}
Object.getPrototypeOf([])[Symbol.iterator] = () => {};
%PrepareFunctionForOptimization(foo);
assertThrows(foo, TypeError, 'Result of the Symbol.iterator method is not an object');
assertThrows(foo, TypeError, 'Result of the Symbol.iterator method is not an object');
%OptimizeFunctionOnNextCall(foo);
assertThrows(foo, TypeError, 'Result of the Symbol.iterator method is not an object');
