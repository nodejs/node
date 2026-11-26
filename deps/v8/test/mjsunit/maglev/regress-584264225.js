// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev-non-eager-inlining
// Flags: --max-maglev-eager-inlined-bytecode-size=0

const dv = new DataView(new ArrayBuffer(16));

function isZero(a) {
  return a === 0;
}

function inner(init) {
  switch (init) {
    case 1:
      let le = 1;
    case 0:
      while (le) {
        do {} while (init > 1);
        if (isZero(init)) throw 1;
        le = 0;
      }
      return dv.getInt32(0, le);
  }
}

function test() {
  return inner(0);
}

%PrepareFunctionForOptimization(isZero);
%PrepareFunctionForOptimization(inner);
%PrepareFunctionForOptimization(test);
assertEquals(0, inner(1));
assertThrows(() => test(), ReferenceError);
%OptimizeFunctionOnNextCall(test);
assertThrows(() => test(), ReferenceError);
