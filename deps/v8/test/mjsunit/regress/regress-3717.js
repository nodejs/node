// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-debug-as debug --no-lazy

Debug = debug.Debug;
var exception = null;
var break_count = 0;

function f() {
  function g(p) {
    return 1;
  }
  g(1);
};

function listener(event, exec_state, event_data, data) {
  try {
    if (event == Debug.DebugEvent.Break) break_count++;
  } catch (e) {
    exception = e;
  }
}

Debug.setListener(listener);
var bp = Debug.setBreakPoint(f, 2);
f();
Debug.clearBreakPoint(bp);
Debug.setListener(null);

assertEquals(1, break_count);
assertNull(exception);
