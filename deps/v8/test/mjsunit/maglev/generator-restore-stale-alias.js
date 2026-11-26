// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --expose-gc

function asyncTimeout(timeout) {
  return new Promise((resolve) => {
    setTimeout(resolve, timeout);
  });
}

// Keep enough locals live across `yield` so no unblocked free GPRs remain when
// restoring `obj`, while `0 == obj` hints `obj`'s `GeneratorRestoreRegister`
// to `rbx` (matching `StaleInput`), causing `stale == result_reg`.
function* gen(p) {
  let l0 = p + 1;
  let l1 = p + 2;
  let l2 = p + 3;
  let l3 = p + 4;
  let l4 = p + 5;
  let l5 = p + 6;
  let l6 = p + 7;
  let l7 = p + 8;
  let obj = {x: 42};
  yield obj;
  // `0 == obj` uses `Builtin::kEqual` (`RightInput` fixed to `rbx`), giving
  // `obj`'s `GeneratorRestoreRegister` a register hint matching `StaleInput`.
  let eq = (0 == obj) ? 1 : 0;
  let sum = l0 + l1 + l2 + l3 + l4 + l5 + l6 + l7 + eq;
  obj = null;
  return sum;
}

%PrepareFunctionForOptimization(gen);
for (let i = 0; i < 10; i++) {
  let g = gen(0);
  g.next();
  g.next();
}
%OptimizeFunctionOnNextCall(gen);

assertPromiseResult((async function() {
  globalThis.active_gen = gen(0);
  let weak_obj = new WeakRef(globalThis.active_gen.next().value);
  assertEquals(36, globalThis.active_gen.next().value);

  // End the microtask turn and run stackless async GC while `active_gen` (and
  // its internal `parameters_and_registers` FixedArray) stays reachable.
  await asyncTimeout(0);
  await gc({type: 'major', execution: 'async'});

  assertEquals(undefined, weak_obj.deref());
})());
