// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function Module(stdlib, foreign, heap) {
  "use asm";
  var MEM32 = new stdlib.Int32Array(heap);
  function foo(i) {
    i = i|0;
    return +MEM32[i >> 2];
  }
  return {foo: foo};
}

var foo = Module(this, {}, new ArrayBuffer(4)).foo;
assertEquals(NaN, foo(-4));
assertEquals(NaN, foo(4));
