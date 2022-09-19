// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Debug = debug.Debug
var exception = null;
var breaks = 0;

function listener(event, exec_state, event_data, data) {
  if (event != Debug.DebugEvent.Break) return;
  try {
    var source = exec_state.frame(0).sourceLineText();
    assertTrue(RegExp(`B${breaks++}`).test(source));
    if (/stop/.test(source)) return;
    if (/step out/.test(source)) {
      exec_state.prepareStep(Debug.StepAction.StepOut);
    } else if (/step in/.test(source)) {
      exec_state.prepareStep(Debug.StepAction.StepInto);
    } else {
      exec_state.prepareStep(Debug.StepAction.StepOver);
    }
  } catch (e) {
    print(e, e.stack);
    exception = e;
  }
}

Debug.setListener(listener);

function * g() {
  debugger;  // B0
  yield 1;   // B1
  yield 2;   // B2 step out
  yield 3;   // B5
  yield 4;   // B6 step out
  return 2 * (yield 5);
}

var i = g();
assertEquals(1, i.next().value);
assertEquals(2, i.next().value);   // B3
assertEquals(3, i.next().value);   // B4 step in
assertEquals(4, i.next().value);   // B7
assertEquals(5, i.next().value);   // B8
assertEquals(6, i.next(3).value);  // B9 stop

assertNull(exception);
assertEquals(10, breaks);
