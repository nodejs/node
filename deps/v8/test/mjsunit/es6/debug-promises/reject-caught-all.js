// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-debug-as debug --allow-natives-syntax

// Test debug events when we listen to all exceptions and
// there is a catch handler for the to-be-rejected Promise.
// We expect a normal Exception debug event to be triggered.

Debug = debug.Debug;

var log = [];
var expected_events = 1;

var p = new Promise(function(resolve, reject) {
  log.push("resolve");
  resolve();
});

var q = p.chain(
  function(value) {
    log.push("reject");
    return Promise.reject(new Error("reject"));
  });

q.catch(
  function(e) {
    assertEquals("reject", e.message);
  });


function listener(event, exec_state, event_data, data) {
  try {
    if (event == Debug.DebugEvent.Exception) {
      expected_events--;
      assertTrue(expected_events >= 0);
      assertEquals("reject", event_data.exception().message);
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
        assertEquals(["resolve", "end main", "reject"], log);
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
