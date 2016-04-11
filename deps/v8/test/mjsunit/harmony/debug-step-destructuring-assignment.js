// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-debug-as debug --harmony-destructuring-assignment
// Flags: --harmony-destructuring-bind

var exception = null;
var Debug = debug.Debug;
var break_count = 0;

function listener(event, exec_state, event_data, data) {
  if (event != Debug.DebugEvent.Break) return;
  try {
    var source = exec_state.frame(0).sourceLineText();
    print(source);
    assertTrue(source.indexOf(`// B${break_count++}`) > 0);
    if (source.indexOf("assertEquals") > 0) {
      exec_state.prepareStep(Debug.StepAction.StepNext);
    } else {
      exec_state.prepareStep(Debug.StepAction.StepIn);
    }
  } catch (e) {
    exception = e;
    print(e);
  }
};

Debug.setListener(listener);

function f() {
  var a, b, c, d;
  debugger;                                       // B0
  [                                               // B1
    a,                                            // B3
    b,                                            // B4
    c = 3                                         // B5
  ] = [1, 2];                                     // B2
  assertEquals({a:1,b:2,c:3}, {a, b, c});         // B6

  [                                               // B7
    a,                                            // B9
    [
      b,                                          // B10
      c                                           // B11
    ],
    d                                             // B12
  ] = [5, [6, 7], 8];                             // B8
  assertEquals({a:5,b:6,c:7,d:8}, {a, b, c, d});  // B13

  [                                               // B14
    a,                                            // B16
    b,                                            // B17
    ...c                                          // B18
  ] = [1, 2, 3, 4];                               // B15
  assertEquals({a:1,b:2,c:[3,4]}, {a, b, c});     // B19

  ({                                              // B20
    a,                                            // B22
    b,                                            // B23
    c = 7                                         // B24
  } = {a: 5, b: 6});                              // B21
  assertEquals({a:5,b:6,c:7}, {a, b, c});         // B25

  ({                                              // B26
    a,                                            // B28
    b = return1(),                                // B29
    c = return1()                                 // B30
  } = {a: 5, b: 6});                              // B27
  assertEquals({a:5,b:6,c:1}, {a, b, c});         // B33

  ({                                              // B34
    x : a,                                        // B36
    y : b,                                        // B37
    z : c = 3                                     // B38
  } = {x: 1, y: 2});                              // B35
  assertEquals({a:1,b:2,c:3}, {a, b, c});         // B39
}                                                 // B40

function return1() {
  return 1;                                       // B31
}                                                 // B32

f();
Debug.setListener(null);                          // B41
assertNull(exception);
