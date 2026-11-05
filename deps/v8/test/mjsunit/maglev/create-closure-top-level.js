// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax --maglev

// Closures created outside of a function scope (e.g. in top-level code) don't
// use the FastNewClosure builtin. OSR the top-level code into Maglev to
// exercise the CreateClosure node, which calls the runtime instead.

const topLevel = %GetFunctionForCurrentFrame();
%PrepareFunctionForOptimization(topLevel);

let v = 7;
let x = 4;
let o;
let created_in_maglev = false;
// Declare the loop variable outside of the loop: a `for (let ...)` loop that
// creates closures is desugared into nested loops.
let i = 0;
for (; i < 3; i++) {
  o = {};
  o.x = (c) => v + x++ + 2 + c;
  if (topFrameIsMaglevved(%GetOptimizationStatus(topLevel))) {
    created_in_maglev = true;
  }
  if (i == 1) %OptimizeOsr();
}
assertTrue(created_in_maglev);
assertEquals(7 + 4 + 2 + 2, o.x(2));
assertEquals(7 + 5 + 2 + 5, o.x(5));
