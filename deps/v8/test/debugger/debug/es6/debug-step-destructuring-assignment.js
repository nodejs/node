// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


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
    a,                                            // B2
    b,                                            // B3
    c = 3                                         // B4
  ] = [1, 2];
  assertEquals({a:1,b:2,c:3}, {a, b, c});         // B5

  [                                               // B6
    a,                                            // B7
    [
      b,                                          // B8
      c                                           // B9
    ],
    d                                             // B10
  ] = [5, [6, 7], 8];
  assertEquals({a:5,b:6,c:7,d:8}, {a, b, c, d});  // B11

  [                                               // B12
    a,                                            // B13
    b,                                            // B14
    ...c                                          // B15
  ] = [1, 2, 3, 4];
  assertEquals({a:1,b:2,c:[3,4]}, {a, b, c});     // B16

  ({                                              // B17
    a,                                            // B18
    b,                                            // B19
    c = 7                                         // B20
  } = {a: 5, b: 6});
  assertEquals({a:5,b:6,c:7}, {a, b, c});         // B21

  ({                                              // B22
    a,                                            // B23
    b = return1(),                                // B24
    c = return1()                                 // B25
  } = {a: 5, b: 6});
  assertEquals({a:5,b:6,c:1}, {a, b, c});         // B28

  ({                                              // B29
    x : a,                                        // B30
    y : b,                                        // B31
    z : c = 3                                     // B32
  } = {x: 1, y: 2});
  assertEquals({a:1,b:2,c:3}, {a, b, c});         // B33
}                                                 // B34

function return1() {
  return 1;                                       // B26
}                                                 // B27

f();
Debug.setListener(null);                          // B35
assertNull(exception);
