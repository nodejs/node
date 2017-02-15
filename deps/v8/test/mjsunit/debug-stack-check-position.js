// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-debug-as debug --allow-natives-syntax

var Debug = debug.Debug;
var exception = null;
var loop = true;

function listener(event, exec_state, event_data, data) {
  if (event != Debug.DebugEvent.Break) return;
  try {
    assertTrue(exec_state.frame(0).sourceLineText().indexOf("BREAK") > 0);
  } catch (e) {
    exception = e;
  }
}

function f() {     // BREAK
  return 1;
}

Debug.setListener(listener);

%ScheduleBreak();  // Break on function entry.
f();

Debug.setListener(null);
assertNull(exception);
