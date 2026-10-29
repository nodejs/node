// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function AsmModule(stdlib, foreign, heap) {
  "use asm";
  var HEAP32 = new stdlib.Int32Array(heap);
  function g() { return 1.25; }
  function f(a,b,c) {
    a = a | 0;
    b = b | 0;
    c = c | 0;
    HEAP32[a << (b >> 2) + ~~+g()] = c;
    return c | 0;
  }
  return {f:f};
}

var heap = new ArrayBuffer(0x10000);
var m = AsmModule({Int32Array:Int32Array}, {}, heap);
assertEquals(3, m.f(1,2,3));
