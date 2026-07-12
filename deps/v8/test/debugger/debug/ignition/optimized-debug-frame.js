// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


// Test that PC in optimized frame would correctly translate into
// unoptimized frame when retrieving frame information in the debugger.

function f() {
  debugger;
}

function g(x) {
  return f();
}

var break_count = 0;

function listener(event, exec_state, event_data, data) {
  if (event != Debug.DebugEvent.Break) return;
  assertEquals(14, exec_state.frame(1).sourceLine());
  assertEquals(9, exec_state.frame(1).sourceColumn());
  break_count++;
}

%PrepareFunctionForOptimization(g);
g();
g();
%OptimizeFunctionOnNextCall(g);

var Debug = debug.Debug;
Debug.setListener(listener);

g();

Debug.setListener(null);

assertEquals(1, break_count);
