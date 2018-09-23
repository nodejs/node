// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function Module(stdlib, foreign, buffer) {
  "use asm";
  function f(i) {
    var j;
    i = i|0;
    do {
      if (i > 0) {
        j = i != 0;
        i = (i - 1) | 0;
      } else {
        j = 0;
      }
    } while (j);
    return i;
  }
  return {f:f};
}

var m = Module(this, {}, new ArrayBuffer(64*1024));

assertEquals(-1, m.f("-1"));
assertEquals(0, m.f(-Math.infinity));
assertEquals(0, m.f(undefined));
assertEquals(0, m.f(0));
assertEquals(0, m.f(1));
assertEquals(0, m.f(100));
