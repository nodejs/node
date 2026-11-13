// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev --no-lazy-feedback-allocation
// Flags: --max-maglev-hard-inline-depth=200

// Deeply nested inlined `forEach` calls create one JS builtin continuation
// frame per level. The Maglev prologue stack check must account for their
// full size (including the JS trampoline register parameters), otherwise an
// eager deopt near the stack limit overshoots it.

const kDepth = 100;
let body = 'return obj.y;';
for (let i = 0; i < kDepth; i++) body = `arr.forEach(() => { ${body} });`;
const target = new Function(
    'arr', 'obj', 'deopt', `const x = obj.x; if (!deopt) return x; ${body}`);

const arr = [1];
const good = {x: 1, y: 1};
const bad = {x: 1};  // Different map: `obj.y` deopts eagerly.

%PrepareFunctionForOptimization(target);
target(arr, good, true);
target(arr, bad, false);
target(arr, good, true);
%OptimizeMaglevOnNextCall(target);
target(arr, good, true);

// Recurse until overflow, then on the way back up try to enter `target` at
// every depth. The deepest successful entry sits right at the prologue stack
// check, where an under-estimated deopt frame size makes the deoptimizer
// exceed the stack limit.
let entered = false;
function probe() {
  try {
    probe();
  } catch (e) {
  }
  if (entered) return;
  try {
    target(arr, bad, true);
    entered = true;
  } catch (e) {
  }
}
%NeverOptimizeFunction(probe);
probe();
assertTrue(entered);
