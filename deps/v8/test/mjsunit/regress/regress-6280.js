// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function Module(stdlib, imports, buffer) {
  "use asm";
  var x = new stdlib.Int8Array(buffer);
  function f() {
    return x[0] | 0;
  }
  return { f:f };
}

var b = new ArrayBuffer(1024);
var m1 = Module({ Int8Array:Int8Array }, {}, b);
assertEquals(0, m1.f());

var was_called = 0;
function observer() { was_called++; return [23]; }
var m2 = Module({ Int8Array:observer }, {}, b);
assertEquals(1, was_called);
assertEquals(23, m2.f());
