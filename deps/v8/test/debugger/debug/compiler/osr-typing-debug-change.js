// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


var Debug = debug.Debug;

var changed = false;

function listenerSetJToResult(
    event, exec_state, event_data, data) {

  if (event == Debug.DebugEvent.Break) {
    var scope = exec_state.frame(1).scope(0);
    var newval = "result";
    try {
      scope.setVariableValue("j", newval);
      changed = true;
    } catch(e) {
      changed = false;
    }
  }
}

Debug.setListener(listenerSetJToResult);

function g() { debugger; }
%NeverOptimizeFunction(g);

function ChangeSmiConstantAndOsr() {
  var j = 1;
  for (var i = 0; i < 4; i++) {
    if (i == 2) {
      %OptimizeOsr();
      g();
    }
  }
  return j;
}
%PrepareFunctionForOptimization(ChangeSmiConstantAndOsr);
var r1 = ChangeSmiConstantAndOsr();
if (changed) {
  assertEquals("result", r1);
} else {
  assertEquals(1, r1);
}

function ChangeFloatConstantAndOsr() {
  var j = 0.1;
  for (var i = 0; i < 4; i++) {
    if (i == 2) {
      %OptimizeOsr();
      g();
    }
  }
  return j;
}
%PrepareFunctionForOptimization(ChangeFloatConstantAndOsr);
var r2 = ChangeFloatConstantAndOsr();
if (changed) {
  assertEquals("result", r2);
} else {
  assertEquals(0.1, r2);
}

function ChangeFloatVarAndOsr() {
  var j = 0.1;
  for (var i = 0; i < 4; i++) {
    j = j + 0.1;
    if (i == 2) {
      %OptimizeOsr();
      g();
    }
  }
  return j;
}
%PrepareFunctionForOptimization(ChangeFloatVarAndOsr);
var r3 = ChangeFloatVarAndOsr();
if (changed) {
  assertEquals("result0.1", r3);
} else {
  assertEquals(0.5, r3);
}

function listenerSetJToObject(
    event, exec_state, event_data, data) {
  if (event == Debug.DebugEvent.Break) {
    var scope = exec_state.frame(1).scope(0);
    try {
      scope.setVariableValue("j", 100);
      changed = true;
    } catch(e) {
      changed = false;
    }
  }
}

Debug.setListener(listenerSetJToObject);

function ChangeIntVarAndOsr() {
  var j = 1;
  for (var i = 0; i < 4; i++) {
    j = j + 1|0;
    if (i == 2) {
      %OptimizeOsr();
      g();
    }
  }
  return j;
}
%PrepareFunctionForOptimization(ChangeIntVarAndOsr);

var r4 = ChangeIntVarAndOsr();
if (changed) {
  assertEquals(101, r4);
} else {
  assertEquals(5, r4);
}
