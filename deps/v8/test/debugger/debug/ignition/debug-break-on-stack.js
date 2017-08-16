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
    print(line);
    assertTrue(line.indexOf(`B${break_count}`) > 0);
  } catch (e) {
    exception = e;
  }
}


function g() {
  setbreaks();
  throw 1;  // B1
}

function f() {
  try {
    g();
  } catch (e) {}
  return 2;  // B2
}

function setbreaks() {
  Debug.setListener(listener);
  Debug.setBreakPoint(g, 2, 0);
  Debug.setBreakPoint(f, 4, 0);
}

f();

assertEquals(2, break_count);
assertNull(exception);

Debug.setListener(null);
