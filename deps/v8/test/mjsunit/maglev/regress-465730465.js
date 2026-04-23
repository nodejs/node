// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

let glob;

function empty() {}

function foo(a) {
  glob = 0;
  var x;
  switch (a) {
    case 1:
      let y = 1;
    case 2:
      try {
        y = 2;
        empty();
        glob++;
      } catch (e) {}
      try {
        x = y;
      } catch (e) {
        empty();
        glob++;
      }
  }
}

%PrepareFunctionForOptimization(foo);
foo(1);
assertEquals(1, glob);
foo(2);
assertEquals(1, glob);
%OptimizeMaglevOnNextCall(foo);
foo(2);
assertEquals(1, glob);
