// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --sparkplug --stack-size=120
// Flags: --max-inlined-bytecode-size=999999
// Flags: --max-inlined-bytecode-size-cumulative=999999
// Flags: --max-maglev-inlined-bytecode-size=999999
// Flags: --max-maglev-inlined-bytecode-size-cumulative=999999

// Transition the ContextCell to mutable before optimization so flipping
// `deopt_now` at runtime does not eagerly invalidate `outer`.
let deopt_now = true;
deopt_now = false;

// Neither branch returns normally during compilation: the untaken `if` branch
// emits an unconditional soft deopt and the `else` branch throws, so when
// inlined at the start of `mid`, the rest of `mid` is unreachable in `outer`
// and `mid` only appears as a parent frame in `outer`'s deopt frame chains.
function inner_deopt() {
  if (deopt_now) {
    return deopt_now.prop;
  }
  throw "warmup";
}

function nop() {}

// After `inner_deopt` deoptimizes and returns into the materialized `mid`
// frame, `mid` OSRs to Sparkplug on its loop backedge and pushes 2,000
// outgoing call arguments without a per-call stack check.
const many_args = new Array(2000).fill("0").join(",");
const mid = new Function("call_inner", "inner_deopt", "nop", `
  if (call_inner) {
    inner_deopt();
  }
  for (let i = 0; i < 2; i++) {}
  nop(${many_args});
`);

function outer() {
  mid(true, inner_deopt, nop);
}

%CompileBaseline(mid);
%PrepareFunctionForOptimization(inner_deopt);
%PrepareFunctionForOptimization(mid);
%PrepareFunctionForOptimization(outer);

mid(false, inner_deopt, nop);
try {
  outer();
} catch (e) {}

%OptimizeFunctionOnNextCall(outer);
try {
  outer();
} catch (e) {}
deopt_now = true;

function run_near_stack_limit() {
  try {
    run_near_stack_limit();
  } catch (e) {
    outer();
  }
}
try {
  run_near_stack_limit();
} catch (e) {}
