// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function Module(stdlib, foreign, heap) {
  "use asm";
  function f1(i) {
    i = i|0;
    return ((i | 0) % 3) | 0;
  }
  function f2(i) {
    i = i|0;
    return ((i | 0) % 9) | 0;
  }
  function f3(i) {
    i = i|0;
    return ((i | 0) % 1024) | 0;
  }
  function f4(i) {
    i = i|0;
    return ((i | 0) % 3133335) | 0;
  }
  return { f1: f1, f2: f2, f3: f3, f4: f4 };
}

var m = Module(this, {}, new ArrayBuffer(1024));

for (var i = -2147483648; i < 2147483648; i += 3999773) {
  assertEquals((i % 3) | 0, m.f1(i));
  assertEquals((i % 9) | 0, m.f2(i));
  assertEquals((i % 1024) | 0, m.f3(i));
  assertEquals((i % 3133335) | 0, m.f4(i));
}
