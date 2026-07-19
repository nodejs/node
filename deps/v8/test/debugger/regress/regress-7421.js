// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Debug = debug.Debug

// Test that the side-effect check is not bypassed in optimized code.

var exception = null;
var counter = 0;

function f1() {
  counter++;
}

function wrapper1() {
  for (var i = 0; i < 4; i++) {
    // Get this function optimized before calling to increment.
    // Check that that call performs the necessary side-effect checks.
    %OptimizeOsr();
    %PrepareFunctionForOptimization(wrapper1);
  }
  f1();
}
%PrepareFunctionForOptimization(wrapper1);

function f2() {
  counter++;
}

function wrapper2(call) {
  if (call) f2();
}

function listener(event, exec_state, event_data, data) {
  if (event != Debug.DebugEvent.Break) return;
  try {
    function success(expectation, source) {
      assertEquals(expectation,
                   exec_state.frame(0).evaluate(source, true).value());
    }
    function fail(source) {
      assertThrows(() => exec_state.frame(0).evaluate(source, true),
                   EvalError);
    }
    wrapper1();
    wrapper1();
    fail("wrapper1()");

    %PrepareFunctionForOptimization(wrapper2);
    wrapper2(true);
    wrapper2(false);
    wrapper2(true);
    %OptimizeFunctionOnNextCall(wrapper2);
    wrapper2(false);
    fail("wrapper2(true)");
    fail("%PrepareFunctionForOptimization(wrapper2); "+
         "%OptimizeFunctionOnNextCall(wrapper2); wrapper2(true)");

    %PrepareFunctionForOptimization(wrapper2);
    %DisableOptimizationFinalization();
    %OptimizeFunctionOnNextCall(wrapper2, "concurrent");
    wrapper2(false);
    fail("%FinalizeOptimization();" +
         "wrapper2(true);");
  } catch (e) {
    exception = e;
    print(e, e.stack);
  }
};

// Add the debug event listener.
Debug.setListener(listener);

function f() {
  debugger;
};

f();

assertNull(exception);
