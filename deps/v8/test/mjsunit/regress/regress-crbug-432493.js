// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-debug-as debug

function f() {
  var a = 1;
  var b = 2;
  return a + b;
}

var exception = null;
var break_count = 0;
var throw_count = 0;

function listener(event, exec_state, event_data, data) {
  try {
    if (event == Debug.DebugEvent.Break) {
      break_count++;
      // Disable all breakpoints from within the debug event callback.
      Debug.debuggerFlags().breakPointsActive.setValue(false);
    } else if (event = Debug.DebugEvent.Exception) {
      throw_count++;
      // Enable all breakpoints from within the debug event callback.
      Debug.debuggerFlags().breakPointsActive.setValue(true);
    }
  } catch (e) {
    exception = e;
  }
}

Debug = debug.Debug;

Debug.setListener(listener);
Debug.setBreakOnException();
Debug.setBreakPoint(f, 2);

f();
f();

assertEquals(1, break_count);
assertEquals(0, throw_count);

// Trigger exception event.
try { throw 1; } catch (e) {}

f();
f();

Debug.setListener(null);
Debug.clearBreakOnException();
Debug.debuggerFlags().breakPointsActive.setValue(true);

assertEquals(2, break_count);
assertEquals(1, throw_count);
assertNull(exception);
