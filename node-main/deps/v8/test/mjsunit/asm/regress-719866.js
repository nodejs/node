// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function Module(stdlib) {
  "use asm";
  function f(a,b,c) {
    a = +a;
    b = +b;
    c = +c;
    var r = 0.0;
    r = a / b * c;
    return +r;
  }
  return { f:f }
}
var m = Module(this);
assertEquals(16, m.f(32, 4, 2));
