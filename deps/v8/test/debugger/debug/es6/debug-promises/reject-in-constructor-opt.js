// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


// Test exercises code paths for catching exceptions in the promise constructor
// in conjunction with deoptimization.

Debug = debug.Debug;

var expected_events = 4;

function listener(event, exec_state, event_data, data) {
  try {
    if (event == Debug.DebugEvent.Exception) {
      expected_events--;
      assertTrue(expected_events >= 0);
      assertEquals("uncaught", event_data.exception().message);
      assertTrue(event_data.uncaught());
      // The frame comes from the Promise.reject call
      assertNotNull(/\/\/ EXCEPTION/.exec(event_data.sourceLineText()));
      assertTrue(event_data.uncaught());
    }
  } catch (e) {
    %AbortJS(e + "\n" + e.stack);
  }
}

Debug.setBreakOnException();
Debug.setListener(listener);

function foo(a,b) {
    let P = new Promise((resolve, reject) => { bar(a,b); resolve()})
    return P;
}

function bar(a,b) {
  %DeoptimizeFunction(foo);
  throw new Error("uncaught"); // EXCEPTION
}

foo();
%RunMicrotasks();

foo();
%RunMicrotasks();

%OptimizeFunctionOnNextCall(foo);

// bar likely gets inlined into foo.
foo();
%RunMicrotasks();

%NeverOptimizeFunction(bar);
%OptimizeFunctionOnNextCall(foo);

// bar does not get inlined into foo.
foo();
%RunMicrotasks();

assertEquals(0, expected_events);
