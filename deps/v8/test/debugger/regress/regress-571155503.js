// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const Debug = debug.Debug;

// Case 1: Clearing the last breakpoint during an interrupt at a Return bytecode
// after a one-shot step/debug break.
(function TestClearBreakPointDuringInterruptAtReturn() {
  let bp;
  function bar() {
    %ScheduleBreak();
  }
  function foo() {
    bar();
  }

  let step = 0;
  Debug.setListener(function(event, exec_state, event_data, data) {
    if (event != Debug.DebugEvent.Break) return;
    step++;
    if (step === 1) {
      exec_state.prepareStep(Debug.StepAction.StepOut);
    } else if (step === 2) {
      Debug.clearBreakPoint(bp);
    }
  });

  bp = Debug.setBreakPoint(foo, 0);
  foo();
  Debug.setListener(null);
})();

// Case 2: Clearing a breakpoint in a callee and setting a new breakpoint in
// the caller during an interrupt at a Return bytecode.
(function TestSetBreakPointDuringInterruptAtReturn() {
  let bp;
  function bar() {
    Debug.clearBreakPoint(bp);
    %ScheduleBreak();
  }
  function foo() {
    bar();
  }

  let step = 0;
  Debug.setListener(function(event, exec_state, event_data, data) {
    if (event != Debug.DebugEvent.Break) return;
    step++;
    if (step === 1) {
      exec_state.prepareStep(Debug.StepAction.StepOut);
    } else if (step === 2) {
      bp = Debug.setBreakPoint(foo, 0);
    }
  });

  bp = Debug.setBreakPoint(foo, 0);
  foo();
  Debug.clearBreakPoint(bp);
  Debug.setListener(null);
})();

// Case 3: Lazy deoptimization of an active optimized frame after
// Debug::PerformSideEffectCheck creates a DebugInfo with debug bytecode
// (without BreakInfo), followed by setting and clearing a breakpoint.
(function TestLazyDeoptAfterSideEffectCheck() {
  let trigger = false;
  function side_effect(o) {
    o.x = 1;
  }
  %NeverOptimizeFunction(side_effect);

  function bar(o, do_se) {
    if (do_se) side_effect(o);
    if (trigger) {
      debugger;
    }
  }
  %NeverOptimizeFunction(bar);

  function foo(o, do_se) {
    bar(o, do_se);
    return o.x;
  }

  %PrepareFunctionForOptimization(foo);
  let obj = {x: 0};
  for (let i = 0; i < 10; i++) foo(obj, false);
  %OptimizeFunctionOnNextCall(foo);
  foo(obj, false);

  Debug.setListener(function(event, exec_state, event_data, data) {
    if (event != Debug.DebugEvent.Break) return;
    try {
      exec_state.frame(0).evaluate("foo({x: 0}, true)", true);
    } catch (e) {
    }
    %DeoptimizeFunction(foo);
    let bp = Debug.setBreakPoint(foo, 0);
    Debug.clearBreakPoint(bp);
  });

  trigger = true;
  foo(obj, false);
  Debug.setListener(null);
})();

// Case 4: Async generator SuspendGenerator bytecode during side-effect check
// evaluation followed by clearing breakpoints.
(function TestAsyncGeneratorSuspendDuringSideEffectCheck() {
  let bp;
  async function* gen() {
    yield 1;
  }

  let in_eval = false;
  Debug.setListener(function(event, exec_state, event_data, data) {
    if (event != Debug.DebugEvent.Break) return;
    if (!in_eval) {
      in_eval = true;
      try {
        exec_state.frame(0).evaluate("gen().next()", true);
      } catch (e) {
      }
      Debug.clearBreakPoint(bp);
    }
  });

  bp = Debug.setBreakPoint(gen, 0);
  gen().next();
  Debug.setListener(null);
})();
