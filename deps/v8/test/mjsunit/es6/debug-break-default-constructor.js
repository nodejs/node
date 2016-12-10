// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-debug-as debug

"use strict";

var Debug = debug.Debug;
var exception = null;
var super_called = false;
var step_count = 0;

function listener(event, execState, eventData, data) {
  if (event != Debug.DebugEvent.Break) return;
  try {
    execState.prepareStep(Debug.StepAction.StepIn);
    var s = execState.frame().sourceLineText();
    step_count++;
    assertTrue(s.indexOf('// ' + step_count + '.') >= 0);
  } catch (e) {
    exception = e;
  }
}

class Base {
  constructor() {
    var x = 1;     // 2.
  }                // 3.
}

class Derived extends Base {}  // 1. // 4.

Debug.setListener(listener);
var bp = Debug.setBreakPoint(Derived, 0);

new Derived();

Debug.setListener(null);  // 5.

assertNull(exception);
assertEquals(5, step_count);
