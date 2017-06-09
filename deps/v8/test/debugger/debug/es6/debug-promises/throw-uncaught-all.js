// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


// Test debug events when we listen to all exceptions and
// there is no catch handler for the exception thrown in a Promise.
// We expect an Exception debug event with a promise to be triggered.

Debug = debug.Debug;

var expected_events = 1;
var log = [];

var p = new Promise(function(resolve, reject) {
  log.push("resolve");
  resolve();
});

var q = p.then(
  function() {
    log.push("throw");
    throw new Error("uncaught");  // event
  });

function listener(event, exec_state, event_data, data) {
  try {
    // Ignore exceptions during startup in stress runs.
    if (event == Debug.DebugEvent.Exception) {
      expected_events--;
      assertTrue(expected_events >= 0);
      assertEquals("uncaught", event_data.exception().message);
      assertTrue(event_data.uncaught());
      // Assert that the debug event is triggered at the throw site.
      assertTrue(exec_state.frame(0).sourceLineText().indexOf("// event") > 0);
    }
  } catch (e) {
    %AbortJS(e + "\n" + e.stack);
  }
}

Debug.setBreakOnException();
Debug.setListener(listener);

log.push("end main");

function testDone(iteration) {
  function checkResult() {
    try {
      assertTrue(iteration < 10);
      if (expected_events === 0) {
        assertEquals(["resolve", "end main", "throw"], log);
      } else {
        testDone(iteration + 1);
      }
    } catch (e) {
      %AbortJS(e + "\n" + e.stack);
    }
  }

  %EnqueueMicrotask(checkResult);
}

testDone(0);
