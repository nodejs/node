// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


var Debug = debug.Debug;

var break_count = 0;
var exception = null;

function listener(event, exec_state, event_data, data) {
  if (event != Debug.DebugEvent.Break) return;
  try {
    break_count++;
    var line = exec_state.frame(0).sourceLineText();
    assertTrue(line.indexOf(`B${break_count}`) > 0);
  } catch (e) {
    exception = e;
  }
}

Debug.setListener(listener);

function g() {
  throw 1;
}

function f() {
  try {
    g();                         // B1
  } catch (e) {}
  assertEquals(2, break_count);  // B2
  return 1;                      // B3
}

Debug.setBreakPoint(f, 2, 0);
Debug.setBreakPoint(f, 4, 1);
Debug.setBreakPoint(f, 5, 1);

f();

assertEquals(3, break_count);
assertNull(exception);

Debug.setListener(null);
