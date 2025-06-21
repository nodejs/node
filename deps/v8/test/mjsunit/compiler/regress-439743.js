// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function module(stdlib, foreign, heap) {
    "use asm";
    var MEM32 = new stdlib.Int32Array(heap);
    function foo(i) {
      i = i|0;
      MEM32[0] = i;
      return MEM32[i + 4 >> 2]|0;
    }
    return { foo: foo };
}

var foo = module(this, {}, new ArrayBuffer(64*1024)).foo;
assertEquals(-4, foo(-4));
