// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbolev --turbofan
// Flags: --turbolev-non-eager-loop-peeling

// The non-eager loop peeler clones throwable nodes (e.g. a non-inlined call)
// whose exception handler has no live catch edge: either no handler at all, or
// a handler that unwinds via lazy-deopt. The %AssertPeeled() marker is stripped
// only when the surrounding loop is peeled, so these positively assert that
// peeling fires through the throwable node.

// Pure value computation, used to predict results without throwing.
function val(x) {
  let s = x;
  s = s + 1; s = s * 2; s = s ^ 3; s = s + 4; s = s * 5; s = s ^ 6;
  return s & 0xffff;
}
%NeverOptimizeFunction(val);

// A non-inlined, throwable callee. %NeverOptimizeFunction keeps it a call in the
// optimized caller (rather than being inlined away).
let throwAt = -1;
function callee(x) {
  if (x === throwAt) throw "boom@" + x;
  return val(x);
}
%NeverOptimizeFunction(callee);

// No try/catch: the throwable call's handler is the no-exception-handler
// sentinel (still registered as an inline candidate).
function no_try_catch(n) {
  let acc = 0;
  for (let i = 0; i < n; i++) {
    %AssertPeeled();
    acc += callee(i);
  }
  return acc;
}
throwAt = -1;
%PrepareFunctionForOptimization(no_try_catch);
const ntc = no_try_catch(20);
no_try_catch(20);
%OptimizeFunctionOnNextCall(no_try_catch);
assertEquals(ntc, no_try_catch(20));
assertEquals(0, no_try_catch(0));
assertOptimized(no_try_catch);

// Loop inside the try block. The catch (outside the loop) is never taken during
// warmup, so it is not a live handler: the throwing call unwinds via lazy-deopt.
function loop_in_try(n) {
  let acc = 0;
  try {
    for (let i = 0; i < n; i++) {
      %AssertPeeled();
      acc += callee(i);
    }
  } catch (e) {
    return "caught:" + e;
  }
  return acc;
}
throwAt = -1;
%PrepareFunctionForOptimization(loop_in_try);
const lit = loop_in_try(20);
loop_in_try(20);
%OptimizeFunctionOnNextCall(loop_in_try);
assertEquals(lit, loop_in_try(20));
assertOptimized(loop_in_try);
// A throw in the peeled (first) iteration must still reach the catch.
throwAt = 0;
assertEquals("caught:boom@0", loop_in_try(20));
// A throw in a steady-state iteration too.
throwAt = 7;
assertEquals("caught:boom@7", loop_in_try(20));

// Catch inside the loop body: a per-iteration try/catch around the call. The
// catch is never taken during warmup, so it is not a live handler (the throwing
// call unwinds via lazy-deopt). A caught iteration adds 1000 and skips that
// iteration's contribution.
function catch_in_loop(n) {
  let acc = 0;
  for (let i = 0; i < n; i++) {
    %AssertPeeled();
    try {
      acc += callee(i);
    } catch (e) {
      acc += 1000;
    }
  }
  return acc;
}
function catch_in_loop_expected(n, at) {
  let acc = 0;
  for (let i = 0; i < n; i++) acc += (i === at) ? 1000 : val(i);
  return acc;
}
throwAt = -1;
%PrepareFunctionForOptimization(catch_in_loop);
const cil = catch_in_loop(20);
catch_in_loop(20);
%OptimizeFunctionOnNextCall(catch_in_loop);
assertEquals(cil, catch_in_loop(20));
assertOptimized(catch_in_loop);
// Throw in the peeled (first) iteration is caught locally; the loop continues.
throwAt = 0;
assertEquals(catch_in_loop_expected(20, 0), catch_in_loop(20));
// Throw in a steady-state iteration is caught locally too.
throwAt = 5;
assertEquals(catch_in_loop_expected(20, 5), catch_in_loop(20));
