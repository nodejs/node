// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-debug-as debug

function f0() {
  var v00 = 0;              // Break 1
  var v01 = 1;
  // Normal function call in a catch scope.
  try {
    throw 1;
  } catch (e) {
    try{
      f1();
    } catch (e) {
      var v02 = 2;          // Break 13
    }
  }
  var v03 = 3;
  var v04 = 4;
}

function f1() {
  var v10 = 0;              // Break 2
  var v11 = 1;
  // Getter call.
  var v12 = o.get;
  var v13 = 3               // Break 4
  // Setter call.
  o.set = 2;
  var v14 = 4;              // Break 6
  // Function.prototype.call.
  f2.call();
  var v15 = 5;              // Break 12
  var v16 = 6;
  // Exit function by throw.
  throw 1;
  var v17 = 7;
}

function get() {
  var g0 = 0;               // Break 3
  var g1 = 1;
  return 3;
}

function set() {
  var s0 = 0;               // Break 5
  return 3;
}

function f2() {
  var v20 = 0;              // Break 7
  // Construct call.
  var v21 = new c0();
  var v22 = 2;              // Break 9
  // Bound function.
  b0();
  return 2;                 // Break 11
}

function c0() {
  this.v0 = 0;              // Break 8
  this.v1 = 1;
}

function f3() {
  var v30 = 0;              // Break 10
  var v31 = 1;
  return 3;
}

var b0 = f3.bind(o);

var o = {};
Object.defineProperty(o, "get", { get : get });
Object.defineProperty(o, "set", { set : set });

Debug = debug.Debug;
var break_count = 0
var exception = null;
var step_size;

function listener(event, exec_state, event_data, data) {
  if (event != Debug.DebugEvent.Break) return;
  try {
    var line = exec_state.frame(0).sourceLineText();
    print(line);
    var match = line.match(/\/\/ Break (\d+)$/);
    assertEquals(2, match.length);
    assertEquals(break_count, parseInt(match[1]));
    break_count += step_size;
    exec_state.prepareStep(Debug.StepAction.StepFrame, step_size);
  } catch (e) {
    print(e + e.stack);
    exception = e;
  }
}

for (step_size = 1; step_size < 6; step_size++) {
  print("step size = " + step_size);
  break_count = 0;
  Debug.setListener(listener);
  debugger;                 // Break 0
  f0();
  Debug.setListener(null);  // Break 14
  assertTrue(break_count > 14);
}

assertNull(exception);
