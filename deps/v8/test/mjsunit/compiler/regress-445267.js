// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var foo = (function Module(stdlib, foreign, heap) {
  "use asm";
  var MEM16 = new stdlib.Int16Array(heap);
  function foo(i) {
    i = i|0;
    i = MEM16[i + 2147483650 >> 1]|0;
    return i;
  }
  return { foo: foo };
})(this, {}, new ArrayBuffer(64 * 1024)).foo;

foo(0);
