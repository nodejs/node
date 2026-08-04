// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --ignore-unhandled-promises

// Test debug events when we only listen to uncaught exceptions and
// the Promise is rejected in the Promise constructor.
// We expect an Exception debug event with a promise to be triggered.

Debug = debug.Debug;

var steps = 0;
var exception = null;

function listener(event, exec_state, event_data, data) {
  try {
    if (event == Debug.DebugEvent.Exception) {
      steps++;
      assertEquals("uncaught", event_data.exception().message);
      assertTrue(event_data.uncaught());
      // Assert that the debug event is triggered at the throw site.
      assertTrue(exec_state.frame(0).sourceLineText().indexOf("// event") > 0);
    }
  } catch (e) {
    exception = e;
  }
}

Debug.setBreakOnUncaughtException();
Debug.setListener(listener);

var p = new Promise(function(resolve, reject) {
  reject(new Error("uncaught"));  // event
});

assertEquals(1, steps);
assertNull(exception);
