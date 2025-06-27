// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


'use strict';

var Debug = debug.Debug

var done = false;
var stepCount = 0;

function listener(event, execState, eventData, data) {
  if (event == Debug.DebugEvent.Break) {
    if (!done) {
      execState.prepareStep(Debug.StepAction.StepInto);
      var s = execState.frame().sourceLineText();
      assertTrue(s.indexOf('// ' + stepCount + '.') !== -1);
      stepCount++;
    }
  }
};

Debug.setListener(listener);

function GetBase() {
  var x = 1;   // 1.
  var y = 2;   // 2.
  done = true; // 3.
  return null;
}

function f() {
  class Derived extends GetBase() {} // 0.
}

var bp = Debug.setBreakPoint(f, 1, 20);
f();
assertEquals(4, stepCount);

Debug.setListener(null);
