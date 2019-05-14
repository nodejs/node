// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --use-osr

function foo() {
  var sum = 0;
  A: for (var i = 0; i < 5; i++) {
    B: for (var j = 0; j < 5; j++) {
      C: for (var k = 0; k < 10; k++) {
        if (k === 5) %OptimizeOsr();
        if (k === 6) break B;
        sum++;
      }
    }
  }
  return sum;
}

assertEquals(30, foo());
assertEquals(30, foo());

function bar(a) {
  var sum = 0;
  A: for (var i = 0; i < 5; i++) {
    B: for (var j = 0; j < 5; j++) {
      C: for (var k = 0; k < 10; k++) {
        sum++;
        %OptimizeOsr();
        if (a === 1) break A;
        if (a === 2) break B;
        if (a === 3) break C;
      }
    }
  }
  return sum;
}

assertEquals(1, bar(1));
assertEquals(1, bar(1));

assertEquals(5, bar(2));
assertEquals(5, bar(2));

assertEquals(25, bar(3));
assertEquals(25, bar(3));
