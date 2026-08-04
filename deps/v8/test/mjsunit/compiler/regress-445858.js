// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var foo = (function module(stdlib, foreign, heap) {
  "use asm";
  var MEM = new stdlib.Int8Array(heap);
  function foo(i) {
    i = i|0;
    i[0] = i;
    return MEM[i + 1 >> 0]|0;
  }
  return { foo: foo };
})(this, {}, new ArrayBuffer(64 * 1024)).foo;
foo(-1);
