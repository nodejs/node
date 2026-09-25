// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var stdlib = this;
var buffer = new ArrayBuffer(64 * 1024);
var foreign = {}

function Module(stdlib, foreign, heap) {
  "use asm";
  function foo(i) {
    i = i|0;
    if ((i | 0) > 0) {
      i = (i | 0) == 1;
    } else {
      i = 1;
    }
    return i & 1|0;
  }
  return { foo: foo };
}

var m = Module(stdlib, foreign, buffer);

assertEquals(1, m.foo(-1));
assertEquals(1, m.foo(-0));
assertEquals(1, m.foo(0));
assertEquals(1, m.foo(1));
assertEquals(0, m.foo(2));
assertEquals(1, m.foo(true));
assertEquals(1, m.foo(false));
