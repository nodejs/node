// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function() {
  function f() {
    {
      function f() { return 42 }
    }
    function g() { return f }
    return g;
  }

  var g = f();
  var inner_f = g();
  assertEquals(42, inner_f());
})();

(function() {
  var y = 100;
  var z = (function y() { return y; });
  assertEquals(z, z());
})();
