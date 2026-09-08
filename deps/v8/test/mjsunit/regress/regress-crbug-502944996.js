// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

// Regression test for crbug.com/502944996. Maglev-compiled Array.prototype.sort
// must not mutate the backing store of the array literal in the callee (makeArr)
// when the comparator push/pops to exhaust slack capacity and triggers a
// right-trim, which aliases the literal's FixedArray.

function makeArr() { return [3, 1, 2]; }

function trigger() {
  let a = makeArr();
  let first = true;
  a.sort(function(x, y) {
    if (first) {
      first = false;
      for (let i = 0; i < 100; i++) a.push(0);
      for (let i = 0; i < 100; i++) a.pop();
    }
    return x - y;
  });
}

%PrepareFunctionForOptimization(makeArr);
%PrepareFunctionForOptimization(trigger);
for (let i = 0; i < 10; i++) trigger();
%OptimizeMaglevOnNextCall(trigger);
trigger();

// The literal in makeArr must not have been sorted in-place.
assertEquals(3, makeArr()[0]);
assertEquals(1, makeArr()[1]);
assertEquals(2, makeArr()[2]);
