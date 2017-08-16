// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax
'use strict';
function f24(deopt) {
  let x = 1;
  {
    let x = 2;
    {
      let x = 3;
      assertEquals(3, x);
    }
    deopt + 1;
    assertEquals(2, x);
  }
  assertEquals(1, x);
}


for (var j = 0; j < 10; ++j) {
  f24(12);
}
%OptimizeFunctionOnNextCall(f24);
f24({});
