// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function TestSloppynessPropagates() {
  let f = (function() {
    function Module() {
      "use asm";
      function f() {}
      return {f: f}
    }
    return Module;
  })()().f;
  let p = Object.getOwnPropertyNames(f);
  assertArrayEquals(["length", "name", "prototype"], p);
  assertEquals(null, f.arguments);
  assertEquals(null, f.caller);
})();

(function TestStrictnessPropagates() {
  let f = (function() {
    "use strict";
    function Module() {
      "use asm";
      function f() {}
      return {f: f}
    }
    return Module;
  })()().f;
  let p = Object.getOwnPropertyNames(f);
  assertArrayEquals(["length", "name", "prototype"], p);
  assertThrows(() => f.arguments, TypeError);
  assertThrows(() => f.caller, TypeError);
})();
