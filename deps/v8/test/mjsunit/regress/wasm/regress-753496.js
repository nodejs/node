// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function Module(stdlib, foreign, heap) {
  "use asm";
  var MEM64 = new stdlib.Float64Array(heap);
  function foo(i) {
    i = i | 0;
    MEM64[032 ] = +(i >>> 7 ) / 2.;
    return +MEM64[0];
  }
  return { foo: foo };
}

var foo = Module(this, {}, new ArrayBuffer( "" ? this : this)).foo;
assertEquals(NaN, foo(1));
