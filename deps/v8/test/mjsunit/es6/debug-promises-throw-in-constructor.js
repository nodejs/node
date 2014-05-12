// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --harmony-promises --expose-debug-as debug

// Test debug events when we only listen to uncaught exceptions and
// an exception is thrown in the the Promise constructor.
// We expect an Exception debug event with a promise to be triggered.

Debug = debug.Debug;

var step = 0;
var exception = null;

function listener(event, exec_state, event_data, data) {
  try {
    // Ignore exceptions during startup in stress runs.
    if (step >= 1) return;
    if (event == Debug.DebugEvent.Exception) {
      assertEquals(0, step);
      assertEquals("uncaught", event_data.exception().message);
      assertTrue(event_data.promise() instanceof Promise);
      assertTrue(event_data.uncaught());
      // Assert that the debug event is triggered at the throw site.
      assertTrue(exec_state.frame(0).sourceLineText().indexOf("// event") > 0);
      step++;
    }
  } catch (e) {
    // Signal a failure with exit code 1.  This is necessary since the
    // debugger swallows exceptions and we expect the chained function
    // and this listener to be executed after the main script is finished.
    print("Unexpected exception: " + e + "\n" + e.stack);
    exception = e;
  }
}

Debug.setBreakOnUncaughtException();
Debug.setListener(listener);

var p = new Promise(function(resolve, reject) {
  throw new Error("uncaught");  // event
});

assertEquals(1, step);
assertNull(exception);
