// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function f(){
   assertEquals("function", typeof f);
})();

(function f(){
   var f;  // Variable shadows function name.
   assertEquals("undefined", typeof f);
})();

(function f(){
   var f;
   assertEquals("undefined", typeof f);
   with ({});  // Force context allocation of both variable and function name.
})();

assertEquals("undefined", typeof f);

// var initialization is intercepted by with scope.
(function() {
  var o = { a: 1 };
  with (o) {
    var a = 2;
  }
  assertEquals("undefined", typeof a);
  assertEquals(2, o.a);
})();

// const initialization is not intercepted by with scope.
(function() {
  var o = { a: 1 };
  with (o) {
    const a = 2;
  }
  assertEquals(2, a);
  assertEquals(1, o.a);
})();
