// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function f1() {
  var sum = 0;
  for (var i = 0; i < 1000; i++) {
    sum += arguments[0] + arguments[1] + arguments[2] + arguments[3];
    if (i == 18) %OptimizeOsr();
  }
  return sum;
}

let result = f1(1, 1, 2, 3);
assertEquals(7000, result);
