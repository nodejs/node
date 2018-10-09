// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --use-osr --allow-natives-syntax

function f(x) {
  var sum = 0;
  var outer = 10;
  while (outer > 0) {
    var inner = 10;
    while (inner > 0) {
      sum += x;
      inner--;
      if (inner == 5) {
        %OptimizeOsr();
      }
    }
    outer--;
  }
  return sum;
}

assertEquals(500, f(5));
