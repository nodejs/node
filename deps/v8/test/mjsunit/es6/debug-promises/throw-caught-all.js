// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-debug-as debug --allow-natives-syntax

// Test debug events when we listen to all exceptions and
// there is a catch handler for the exception thrown in a Promise.
// We expect a normal Exception debug event to be triggered.

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
    throw new Error("caught");
  });

q.catch(
  function(e) {
    assertEquals("caught", e.message);
  });

function listener(event, exec_state, event_data, data) {
  try {
    if (event == Debug.DebugEvent.Exception) {
      expected_events--;
      assertTrue(expected_events >= 0);
      assertEquals("caught", event_data.exception().message);
      assertSame(q, event_data.promise());
      assertFalse(event_data.uncaught());
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
