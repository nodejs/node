// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax --maglev

function test() {
    let obj = { x: 12, y: 23 };
    let sum = 0;
    for (let i = 0; i < 100; i++) {
      let a = obj.x;
      let b = obj.y;
      let c;
      if (i < 33) c = obj.x;
      else if (i < 66) c = obj.y;
      else c = -1;
      obj.x = a + 1;
      obj.y = b + 2;
      if (i === 56) {
        obj.x = Infinity;
        delete obj.y;;
      }
      sum += (a | 0) + (b | 0) + (c | 0);
    }
    return sum;
  }

%PrepareFunctionForOptimization(test);
test();
test();
%OptimizeMaglevOnNextCall(test);
assertEquals(test(), 10361);
