// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-debug-as debug --allow-natives-syntax

// Test debug events when we only listen to uncaught exceptions and
// there is a catch handler for the to-be-rejected Promise.
// We expect an Exception debug event with a promise to be triggered.

Debug = debug.Debug;

var expected_events = 1;
var log = [];

var reject_closure;

var p = new Promise(function(resolve, reject) {
  log.push("postpone p");
  reject_closure = reject;
});

var q = new Promise(function(resolve, reject) {
  log.push("resolve q");
  resolve();
});

q.then(function() {
  log.push("reject p");
  reject_closure(new Error("uncaught reject p"));  // event
})


function listener(event, exec_state, event_data, data) {
  try {
    if (event == Debug.DebugEvent.Exception) {
      expected_events--;
      assertTrue(expected_events >= 0);
      assertEquals("uncaught reject p", event_data.exception().message);
      assertTrue(event_data.promise() instanceof Promise);
      assertSame(p, event_data.promise());
      assertTrue(event_data.uncaught());
      // Assert that the debug event is triggered at the throw site.
      assertTrue(exec_state.frame(0).sourceLineText().indexOf("// event") > 0);
    }
  } catch (e) {
    %AbortJS(e + "\n" + e.stack);
  }
}

Debug.setBreakOnUncaughtException();
Debug.setListener(listener);

log.push("end main");

function testDone(iteration) {
  function checkResult() {
    try {
      assertTrue(iteration < 10);
      if (expected_events === 0) {
        assertEquals(["postpone p", "resolve q", "end main", "reject p"], log);
      } else {
        testDone(iteration + 1);
      }
    } catch (e) {
      %AbortJS(e + "\n" + e.stack);
    }
  }

  // Run testDone through the Object.observe processing loop.
  var dummy = {};
  Object.observe(dummy, checkResult);
  dummy.dummy = dummy;
}

testDone(0);
