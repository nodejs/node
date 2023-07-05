// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function Module(stdlib, foreign, buffer) {
  "use asm";
  var HEAP32 = new stdlib.Int32Array(buffer);
  function g(a) {
    HEAP32[a] = 9982 * 100;
    return a;
  }
  function f(i1) {
    i1 = i1 | 0;
    var i2 = HEAP32[i1 >> 2] | 0;
    g(i1);
    L2909: {
      L2: {
        if (0) {
          if (0) break L2;
          g(i2);
          break L2909;
        }
      }
      var r = (HEAP32[1] | 0) / 100 | 0;
      g(r);
      return r;
    }
  }
  return {f: f};
}

var f = Module(this, {}, new ArrayBuffer(64 * 1024)).f;
assertEquals(9982, f(1));
