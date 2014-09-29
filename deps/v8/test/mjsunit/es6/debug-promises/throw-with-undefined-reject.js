// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-debug-as debug --allow-natives-syntax

// Test debug events when an exception is thrown inside a Promise, which is
// caught by a custom promise, which has no reject handler.
// We expect two Exception debug events:
//  1) when the exception is thrown in the promise q.
//  2) when calling the undefined custom reject closure in MyPromise throws.

Debug = debug.Debug;

var expected_events = 2;
var log = [];

var p = new Promise(function(resolve, reject) {
  log.push("resolve");
  resolve();
});

function MyPromise(resolver) {
  var reject = undefined;
  var resolve = function() { };
  log.push("construct");
  resolver(resolve, reject);
};

MyPromise.prototype = new Promise(function() {});
p.constructor = MyPromise;

var q = p.chain(
  function() {
    log.push("throw caught");
    throw new Error("caught");  // event
  });

function listener(event, exec_state, event_data, data) {
  try {
    if (event == Debug.DebugEvent.Exception) {
      expected_events--;
      assertTrue(expected_events >= 0);
      if (expected_events == 1) {
        assertTrue(
            exec_state.frame(0).sourceLineText().indexOf('// event') > 0);
        assertEquals("caught", event_data.exception().message);
      } else if (expected_events == 0) {
        // All of the frames on the stack are from native Javascript.
        assertEquals(0, exec_state.frameCount());
        assertEquals("undefined is not a function",
                     event_data.exception().message);
      } else {
        assertUnreachable();
      }
      assertSame(q, event_data.promise());
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
        assertEquals(["resolve", "construct", "end main", "throw caught"], log);
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
