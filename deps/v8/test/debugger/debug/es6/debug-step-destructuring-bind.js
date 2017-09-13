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
    print(source, break_count);
    assertTrue(source.indexOf(`B${break_count++}`) > 0);
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

var id = x => x;                                  // B9 B10 B36 B37

function test() {
  debugger;                                       // B0
  function fx1([
                a,                                // B2
                b                                 // B3
              ]) {
    assertEquals([1, 2], [a, b]);                 // B4
  }                                               // B5
  fx1([1, 2, 3]);                                 // B1

  function f2([
                a,                                // B7
                b = id(3)                         // B8
              ]) {
    assertEquals([4, 3], [a, b]);                 // B11
  }                                               // B12
  f2([4]);                                        // B6

  function f3({
                x: a,                             // B14
                y: b                              // B15
              }) {
    assertEquals([5, 6], [a, b]);                 // B16
  }                                               // B17
  f3({y: 6, x: 5});                               // B13

  function f4([
                a,                                // B19
                {
                  b,                              // B20
                  c,                              // B21
                }
              ]) {
    assertEquals([2, 4, 6], [a, b, c]);           // B22
  }                                               // B23
  f4([2, {c: 6, b: 4}]);                          // B18

  function f5([
                {
                  a,                              // B25
                  b = 7                           // B26
                },
                c = 3                             // B27
              ] = [{a:1}]) {
    assertEquals([1, 7, 3], [a, b, c]);           // B28
  }                                               // B29
  f5();                                           // B24

  var name = "x";                                 // B30
  function f6({
                [id(name)]: a,                    // B34 B35
                b = a                             // B38
              }) {
    assertEquals([9, 9], [a, b]);                 // B39
  }                                               // B40
  var o6 = {};                                    // B31
  o6[name] = 9;                                   // B32
  f6(o6);                                         // B33

  try {
    throw [3, 4];                                 // B41
  } catch ([
             a,                                   // B42
             b,                                   // B43
             c = 6                                // B44
           ]) {
    assertEquals([3, 4, 6], [a, b, c]);           // B45
  }

  var {
    x: a,
    y: b = 9
  } = { x: 4 };                                   // B46
  assertEquals([4, 9], [a, b]);                   // B47
}                                                 // B48

test();
Debug.setListener(null);                          // B49
assertNull(exception);
