// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --harmony-promises --expose-debug-as debug

// Test debug events when we listen to all exceptions and
// there is a catch handler for the exception thrown in a Promise.
// We expect a normal Exception debug event to be triggered.

Debug = debug.Debug;

var log = [];
var step = 0;

var p = new Promise(function(resolve, reject) {
  log.push("resolve");
  resolve();
});

var q = p.chain(
  function() {
    log.push("throw");
    throw new Error("caught");
  });

q.catch(
  function(e) {
    assertEquals("caught", e.message);
  });

function listener(event, exec_state, event_data, data) {
  try {
    // Ignore exceptions during startup in stress runs.
    if (step >= 1) return;
    assertEquals(["resolve", "end main", "throw"], log);
    if (event == Debug.DebugEvent.Exception) {
      assertEquals("caught", event_data.exception().message);
      assertEquals(undefined, event_data.promise());
      assertFalse(event_data.uncaught());
      step++;
    }
  } catch (e) {
    // Signal a failure with exit code 1.  This is necessary since the
    // debugger swallows exceptions and we expect the chained function
    // and this listener to be executed after the main script is finished.
    print("Unexpected exception: " + e + "\n" + e.stack);
    quit(1);
  }
}

Debug.setBreakOnException();
Debug.setListener(listener);

log.push("end main");
