// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

let x = 100;
function f(osr) {
  try { let x; ()=>x; throw "bla" } catch(e) {};
  let y;
  for (let i = 0; i < 10; i++) {
    if (osr) %OptimizeOsr();
    osr = false;
    y = x;
  }
  return y;
}

%PrepareFunctionForOptimization(f);
f(false);
f(false);
f(false);
f(false);
assertEquals(100, f(true));
