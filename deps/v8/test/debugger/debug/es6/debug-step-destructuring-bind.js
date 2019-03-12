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
  }                                               // B34
};

Debug.setListener(listener);

var id = x => x;                                  // B11 B12 B42 B43

function test() {
  debugger;                                       // B0
  function fx1([
                a,                                // B3
                b                                 // B4
              ]) {                                // B2
    assertEquals([1, 2], [a, b]);                 // B5
  }                                               // B6
  fx1([1, 2, 3]);                                 // B1

  function f2([
                a,                                // B9
                b = id(3)                         // B10
              ]) {                                // B8
    assertEquals([4, 3], [a, b]);                 // B13
  }                                               // B14
  f2([4]);                                        // B7

  function f3({
                x: a,                             // B17
                y: b                              // B18
              }) {                                // B16
    assertEquals([5, 6], [a, b]);                 // B19
  }                                               // B20
  f3({y: 6, x: 5});                               // B15

  function f4([
                a,                                // B23
                {
                  b,                              // B24
                  c,                              // B25
                }
              ]) {                                // B22
    assertEquals([2, 4, 6], [a, b, c]);           // B26
  }                                               // B27
  f4([2, {c: 6, b: 4}]);                          // B21

  function f5([
                {
                  a,                              // B30
                  b = 7                           // B31
                },
                c = 3                             // B32
              ] = [{a:1}]) {                      // B29
    assertEquals([1, 7, 3], [a, b, c]);           // B33
  }                                               // B34
  f5();                                           // B28

  var name = "x";                                 // B35
  function f6({
                [id(name)]: a,                    // B40 B41
                b = a                             // B44
              }) {                                // B39
    assertEquals([9, 9], [a, b]);                 // B45
  }                                               // B46
  var o6 = {};                                    // B36
  o6[name] = 9;                                   // B37
  f6(o6);                                         // B38

  try {
    throw [3, 4];                                 // B47
  } catch ([
             a,                                   // B49
             b,                                   // B50
             c = 6                                // B51
           ]) {                                   // B48
    assertEquals([3, 4, 6], [a, b, c]);           // B52
  }

  var {
    x: a,                                         // B54
    y: b = 9                                      // B55
  } = { x: 4 };                                   // B53
  assertEquals([4, 9], [a, b]);                   // B56
}                                                 // B57

test();
Debug.setListener(null);                          // B58
assertNull(exception);
