// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function getScopeObject(gen, index = 0) {
  const details = %GetGeneratorScopeDetails(gen, index);
  return details ? details[1] : undefined;
}

function* genWithDeadTemps(x) {
  let live1 = x + 1;
  let resumed = yield live1;
  let live2 = live1 + resumed;
  // live1 is dead across this second yield.
  let final_in = yield live2;
  return live2 + final_in;
}

%PrepareFunctionForOptimization(genWithDeadTemps);
for (let i = 0; i < 5; ++i) {
  let g = genWithDeadTemps(i);
  g.next();
  g.next(i);
  g.next(10);
}
%OptimizeFunctionOnNextCall(genWithDeadTemps);

const g = genWithDeadTemps(1);
assertEquals({value: 2, done: false}, g.next());
assertEquals(2, getScopeObject(g, 0).live1);

assertEquals({value: 5, done: false}, g.next(3));
const scopeObj = getScopeObject(g, 0);
if (isOptimized(genWithDeadTemps)) {
  assertThrows(() => typeof scopeObj.live1, ReferenceError);
  assertFalse(%SetGeneratorScopeVariableValue(g, 0, 'live1', 99));
} else {
  assertEquals(2, scopeObj.live1);
}
assertEquals({value: 15, done: true}, g.next(10));
