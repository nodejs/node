// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev-non-eager-inlining
// Flags: --max-maglev-eager-inlined-bytecode-size=0

const holey_doubles = [1.5, , 2.5];
const obj = {a: 1};
Object.prototype.prop = 42;

function id(b) {
  return b;
}

function checkLoop(x, n) {
  let i = 0;
  if (n > 0) {
    while (x.prop && i < n) {
      i++;
      for (let k = 0; k < 1; k++) {}
    }
  }
  return i;
}

function main(idx, n) {
  let x = id(true) ? holey_doubles[idx] : obj;
  return checkLoop(x, n);
}

%PrepareFunctionForOptimization(id);
%PrepareFunctionForOptimization(checkLoop);
%PrepareFunctionForOptimization(main);

id(true);
id(false);
assertEquals(2, checkLoop(1.5, 2));
assertEquals(2, checkLoop(obj, 2));
assertEquals(0, main(0, 0));
assertEquals(0, main(0, 0));

%OptimizeMaglevOnNextCall(main);
assertEquals(2, main(0, 2));
assertEquals(0, main(1, 0));
assertThrows(() => main(1, 2), TypeError);
