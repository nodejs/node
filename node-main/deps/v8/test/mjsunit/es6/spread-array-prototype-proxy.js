// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax


(function TestPrototypeProxy() {
  const backup = Array.prototype.__proto__;
  Array.prototype.__proto__ = new Proxy({}, {set() {throw 666}});

  function f() {
    return ['a', ...['b', 'c', 'd'], 'e']
  }

  %PrepareFunctionForOptimization(f);
  assertArrayEquals(['a', 'b', 'c', 'd', 'e'], f());
  %OptimizeFunctionOnNextCall(f);
  assertArrayEquals(['a', 'b', 'c', 'd', 'e'], f());

  Object.setPrototypeOf(Array.prototype, backup);
})();
