// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

var stdlib = this;
var foreign = {};
var heap = new ArrayBuffer(64 * 1024 * 1024);

var foo = (function(stdlib, foreign, heap) {
  "use asm";
  var M16 = new stdlib.Int16Array(heap);
  var M32 = new stdlib.Int32Array(heap);
  function foo() {
    var i = 0;
    M32[0] = 0x12341234;
    i = M32[0] | 0;
    return M16[0] | 0;
  }
  return foo;
})(stdlib, foreign, heap);

assertEquals(0x1234, foo());
assertEquals(0x1234, foo());
