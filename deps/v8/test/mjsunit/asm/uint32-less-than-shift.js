// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function Module(stdlib, foreign, heap) {
  'use asm';

  function foo1(i1) {
    i1 = i1 | 0;
    var i10 = 0;
    i10 = (i1 >> 5) | 0;
    if (i10 >>> 0 < 5) {
      return 1;
    } else {
      return 0;
    }
    return 0;
  }

  function foo2(i1) {
    i1 = i1 | 0;
    var i10 = 0;
    i10 = ((i1 | 0) / 32) | 0;
    if (i10 >>> 0 < 5) {
      return 1;
    } else {
      return 0;
    }
    return 0;
  }

  function foo3(i1) {
    i1 = i1 | 0;
    var i10 = 0;
    i10 = (i1 + 32 | 0) / 32 | 0;
    if (i10 >>> 0 < 5) {
      return 1;
    } else {
      return 0;
    }
    return 0;
  }
  return {foo1: foo1, foo2: foo2, foo3: foo3};
}

var m = Module(this, {}, undefined);

for (var i = 0; i < 4 * 32; i++) {
  assertEquals(1, m.foo1(i));
  assertEquals(1, m.foo2(i));
  assertEquals(1, m.foo3(i));
}

for (var i = 4 * 32; i < 5 * 32; i++) {
  assertEquals(1, m.foo1(i));
  assertEquals(1, m.foo2(i));
  assertEquals(0, m.foo3(i));
}

for (var i = 5 * 32; i < 10 * 32; i++) {
  assertEquals(0, m.foo1(i));
  assertEquals(0, m.foo2(i));
  assertEquals(0, m.foo3(i));
}
