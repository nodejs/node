// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --turbo-splitting

function module(stdlib, foreign, heap) {
    "use asm";
    function foo(i) {
      var j = 0;
      i = i|0;
      if (i < 0) {
        j = i+1|0;
      }
      if (i > 0) {
        j = i+1|0;
      }
      return j;
    }
    return { foo: foo };
}

var foo = module(this, {}, new ArrayBuffer(64*1024)).foo;
assertEquals(0, foo(0));
assertEquals(0, foo(-1));
assertEquals(12, foo(11));
