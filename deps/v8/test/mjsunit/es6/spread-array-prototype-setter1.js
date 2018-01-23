// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax


(function TestPrototypeSetter1() {
  Object.defineProperty(Array.prototype, 3, {set() {throw 666}})
  Object.defineProperty(Array.prototype, 4, {set() {throw 666}})

  function f() {
    return ['a', ...['b', 'c', 'd'], 'e']
  }

  assertArrayEquals(['a', 'b', 'c', 'd', 'e'], f());
  %OptimizeFunctionOnNextCall(f);
  assertArrayEquals(['a', 'b', 'c', 'd', 'e'], f());

  delete Array.prototype[3];
  delete Array.prototype[4];
})();
