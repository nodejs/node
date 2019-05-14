// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --no-stress-opt

function f(a, i, v) { a[i] = v; }
f("make it generic", 0, 0);

(function TestIsConcatSpreadableProtector() {
  var o = {length: 1, '0': 99};
  %OptimizeObjectForAddingMultipleProperties(o, 0);
  f(o, Symbol.isConcatSpreadable, true);
  assertEquals([99], [].concat(o));
})();

(function TestSpeciesProtector() {
  function MyArray() {}
  assertTrue(%ArraySpeciesProtector());
  f(Array.prototype, "constructor", MyArray);
  assertFalse(%ArraySpeciesProtector());
})();
