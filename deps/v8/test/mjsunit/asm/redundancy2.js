// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --turbo-splitting

function module(stdlib, foreign, heap) {
    "use asm";
    function foo(i) {
      i = i|0;
      var j = 0;
      switch (i | 0) {
        case 0:
          j = i+1|0;
          break;
        case 1:
          j = i+1|0;
          break;
        default:
          j = i;
          break;
      }
      return j | 0;
    }
    return { foo: foo };
}

var foo = module(this, {}, new ArrayBuffer(64*1024)).foo;
assertEquals(2, foo(2));
