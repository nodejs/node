// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


var Debug = debug.Debug;
var break_count = 0;
var exception_count = 0;

function assertCount(expected_breaks, expected_exceptions) {
  assertEquals(expected_breaks, break_count);
  assertEquals(expected_exceptions, exception_count);
}

function listener(event, exec_state, event_data, data) {
  if (event == Debug.DebugEvent.Break) {
    break_count++;
  } else if (event == Debug.DebugEvent.Exception) {
    exception_count++;
  }
}

function f(x) {
  debugger;
  return x + 1;
}

function g(x) {
  try {
    throw x;
  } catch (e) {
  }
}

function h(x) {
  var a = undefined;
  try {
    var x = a();
  } catch (e) {
  }
}

Debug.setListener(listener);

assertCount(0, 0);
f(0);
assertCount(1, 0);
g(0);
assertCount(1, 0);

Debug.setBreakOnException();
f(0);
assertCount(2, 0);
g(0);
assertCount(2, 1);

Debug.setBreakPoint(f, 1, 0, "x == 1");
f(1);
assertCount(3, 1);
f(2);
assertCount(3, 1);
f(1);
assertCount(4, 1);

assertThrows(() => Debug.setBreakPoint(f, 1, 0, "x > 0"));
f(1);
assertCount(5, 2);
f(0);
assertCount(5, 2);

Debug.setBreakPoint(g, 2, 0, "1 == 2");
g(1);
assertCount(5, 2);

assertThrows(() => Debug.setBreakPoint(g, 2, 0, "x == 1"));
g(1);
assertCount(5, 3);
g(2);
assertCount(5, 3);
g(1);
assertCount(5, 3);

assertThrows(() => Debug.setBreakPoint(g, 2, 0, "x > 0"));
g(1);
assertCount(5, 4);
g(0);
assertCount(5, 4);

h(0);
assertCount(5, 5);
Debug.setBreakPoint(h, 3, 0, "x > 0");
h(1);
assertCount(6, 6);
h(0);
assertCount(6, 6);

Debug.clearBreakOnException();
Debug.setListener(null);
