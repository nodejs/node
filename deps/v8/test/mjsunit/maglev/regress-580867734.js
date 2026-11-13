// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

// Regression test for crbug.com/580867734: Maglev's inline AsyncFunctionAwait
// fast path aborted (unconditional deopt) after branching to its slow-path
// label, leaving the label unbound. The empty inner loop and `x = 2` keep
// `savedIter` a loop phi whose type is statically known not to be a JSPromise.

function makeIter(a, bail) {
  if (bail) throw 0;
  a.x;
  return a.values();
}

%PrepareFunctionForOptimization(makeIter);
let warmArr = [2, 4];
warmArr.x = 42;
makeIter(warmArr, false);
try { makeIter(2, true); } catch (e) {}

async function trigger(doBreak, bail, n) {
  let x = 2;
  let savedIter;
  for (let j = 0; ; j++) {
    for (let k = 0; k < 0; k++) {}
    if (doBreak) {
      savedIter = makeIter(x, bail);
      break;
    }
    if (j >= n) return 42;
    x = 2;
  }
  return await savedIter;
}

%PrepareFunctionForOptimization(trigger);
trigger(false, false, 1);
trigger(true, true, 0).catch(() => {});
%OptimizeFunctionOnNextCall(trigger);
assertPromiseResult(trigger(false, false, 1), v => assertEquals(42, v));
assertPromiseResult(
    trigger(true, true, 0), () => assertUnreachable(), e => assertEquals(0, e));
