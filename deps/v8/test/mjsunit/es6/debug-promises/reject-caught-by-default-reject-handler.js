// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-debug-as debug --allow-natives-syntax

// Test debug events when we only listen to uncaught exceptions and
// there is only a default reject handler for the to-be-rejected Promise.
// We expect two Exception debug events:
//  - when the first Promise is rejected and only has default reject handlers.
//  - when the default reject handler passes the rejection on.

Debug = debug.Debug;

var expected_events = 2;
var log = [];

var resolve, reject;
var p0 = new Promise(function(res, rej) { resolve = res; reject = rej; });
var p1 = p0.then(function() {
  log.push("p0.then");
  return Promise.reject(new Error("123"));
});
var p2 = p1.then(function() {
  log.push("p1.then");
});

var q = new Promise(function(res, rej) {
  log.push("resolve q");
  res();
});

q.then(function() {
  log.push("resolve p");
  resolve();
})


function listener(event, exec_state, event_data, data) {
  try {
    if (event == Debug.DebugEvent.Exception) {
      expected_events--;
      assertTrue(expected_events >= 0);
      assertTrue(event_data.uncaught());
      assertTrue(event_data.promise() instanceof Promise);
      if (expected_events == 1) {
        // p1 is rejected, uncaught except for its default reject handler.
        assertEquals(0, exec_state.frameCount());
        assertSame(p1, event_data.promise());
      } else {
        // p2 is rejected by p1's default reject handler.
        assertEquals(0, exec_state.frameCount());
        assertSame(p2, event_data.promise());
      }
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
        assertEquals(["resolve q", "end main", "resolve p", "p0.then"], log);
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
