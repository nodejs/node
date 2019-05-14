// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax


(function TestPrototypeSetter2() {
  Object.defineProperty(Array.prototype.__proto__, 3, {set() {throw 666}})
  Object.defineProperty(Array.prototype.__proto__, 4, {set() {throw 666}})

  function f() {
    return ['a', ...['b', 'c', 'd'], 'e']
  }

  assertArrayEquals(['a', 'b', 'c', 'd', 'e'], f());
  %OptimizeFunctionOnNextCall(f);
  assertArrayEquals(['a', 'b', 'c', 'd', 'e'], f());

  delete Array.prototype.__proto__[3];
  delete Array.prototype.__proto__[4];
})();
