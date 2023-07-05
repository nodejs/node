// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --ignore-unhandled-promises

// Test debug events when we only listen to uncaught exceptions and
// there is only a default reject handler for the to-be-rejected Promise.
// We expect only one debug event: when the first Promise is rejected
// and only has default reject handlers. No event is triggered when
// simply forwarding the rejection with .then's default handler.

Debug = debug.Debug;

var expected_events = 1;
var log = [];

var resolve, reject;
var p0 = new Promise(function(res, rej) { resolve = res; reject = rej; });
var p1 = p0.then(function() {
  log.push("p0.then");
  throw new Error("123");  // event
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
      // p1 is rejected, uncaught except for its default reject handler.
      assertTrue(
          exec_state.frame(0).sourceLineText().indexOf("// event") > 0);
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

  %EnqueueMicrotask(checkResult);
}

testDone(0);
