// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --turbo-filter=g --allow-natives-syntax

// Test that Debug::PrepareForBreakPoints can deal with turbofan code (g)
// on the stack.  Without deoptimization support, we will not be able to
// replace optimized code for g by unoptimized code with debug break slots.
// This would cause stepping to fail (V8 issue 3660).

function f(x) {
  g(x);
  var a = 0;              // Break 6
  return a;               // Break 7
}                         // Break 8

function g(x) {
  if (x) h();
  var a = 0;              // Break 2
  var b = 1;              // Break 3
  return a + b;           // Break 4
}                         // Break 5

function h() {
  debugger;               // Break 0
}                         // Break 1

Debug = debug.Debug;
var exception = null;
var break_count = 0;

function listener(event, exec_state, event_data, data) {
  if (event != Debug.DebugEvent.Break) return;
  try {
    exec_state.prepareStep(Debug.StepAction.StepNext, 1);
    print(exec_state.frame(0).sourceLineText());
    var match = exec_state.frame(0).sourceLineText().match(/Break (\d)/);
    assertNotNull(match);
    assertEquals(break_count++, parseInt(match[1]));
  } catch (e) {
    print(e + e.stack);
    exception = e;
  }
}

f(0);
f(0);
%OptimizeFunctionOnNextCall(g);

Debug.setListener(listener);

f(1);

Debug.setListener(null);  // Break 9
assertNull(exception);
assertEquals(10, break_count);
