// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --no-harmony-restrictive-declarations

(function() {
  var x = {a: 1}
  assertEquals("undefined", typeof f);
  with (x)
    function f() { return a; }
  assertEquals("function", typeof f);
  assertEquals(1, f());
  x.a = 2;
  assertEquals(2, f());
})();

var y = {b: 1}
assertEquals("undefined", typeof g);
with (y)
  function g() { return b; }
assertEquals("function", typeof g);
assertEquals(1, g());
y.b = 2;
assertEquals(2, g());
