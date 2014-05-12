// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --harmony-promises --expose-debug-as debug

// Test debug events when we only listen to uncaught exceptions and
// there is a catch handler for the exception thrown in a Promise.
// We expect no debug event to be triggered.

Debug = debug.Debug;

var p = new Promise(function(resolve, reject) {
  resolve();
});

var q = p.chain(
  function() {
    throw new Error("caught");
  });

q.catch(
  function(e) {
    assertEquals("caught", e.message);
  });

function listener(event, exec_state, event_data, data) {
  try {
    assertTrue(event != Debug.DebugEvent.Exception);
  } catch (e) {
    // Signal a failure with exit code 1.  This is necessary since the
    // debugger swallows exceptions and we expect the chained function
    // and this listener to be executed after the main script is finished.
    print("Unexpected exception: " + e + "\n" + e.stack);
    quit(1);
  }
}

Debug.setBreakOnUncaughtException();
Debug.setListener(listener);
