// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function Module(stdlib, foreign, heap) {
  "use asm";
  function f1(i) {
    i = i>>>0;
    return i % 3;
  }
  function f2(i) {
    i = i>>>0;
    return i % 11;
  }
  function f3(i) {
    i = i>>>0;
    return i % 1024;
  }
  function f4(i) {
    i = i>>>0;
    return i % 3333337;
  }
  return { f1: f1, f2: f2, f3: f3, f4: f4 };
}

var m = Module(this, {}, new ArrayBuffer(1024));

for (var i = 0; i < 4294967296; i += 3999777) {
  assertEquals(i % 3, m.f1(i));
  assertEquals(i % 11, m.f2(i));
  assertEquals(i % 1024, m.f3(i));
  assertEquals(i % 3333337, m.f4(i));
}
