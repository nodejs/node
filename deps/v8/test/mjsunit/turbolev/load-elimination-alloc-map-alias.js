// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --no-maglev-loop-peeling

function testLoopStackCheckPreservesMaps(o1, o2, n) {
  let v1 = +o1.x;
  for (let i = 0; i < n; i++) {
    o2.x = i;
  }
  %TurbofanStaticAssert(v1 == o1.x);
}

function testFreshAllocationStoreMapPreservesMaps(o1, o2, sink) {
  let v1 = +o1.x;
  let box = {z: 42};
  sink[0] = box;
  o2.x = 99;
  %TurbofanStaticAssert(v1 == o1.x);
}

let o1 = {x: 5};
o1.x++;
let o2 = {x: 1, y: 2};
o2.x++;
let sink = [null];
sink[0] = {};  // Make {sink} non-COW.

%PrepareFunctionForOptimization(testLoopStackCheckPreservesMaps);
testLoopStackCheckPreservesMaps(o1, o2, 3);
testLoopStackCheckPreservesMaps(o1, o2, 3);
%OptimizeFunctionOnNextCall(testLoopStackCheckPreservesMaps);
testLoopStackCheckPreservesMaps(o1, o2, 3);

%PrepareFunctionForOptimization(testFreshAllocationStoreMapPreservesMaps);
testFreshAllocationStoreMapPreservesMaps(o1, o2, sink);
testFreshAllocationStoreMapPreservesMaps(o1, o2, sink);
%OptimizeFunctionOnNextCall(testFreshAllocationStoreMapPreservesMaps);
testFreshAllocationStoreMapPreservesMaps(o1, o2, sink);
