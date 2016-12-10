// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-debug-as debug --allow-natives-syntax

// Test debug events when a Promise is rejected, which is caught by a
// custom promise, which throws a new exception in its reject handler.
// We expect two Exception debug events:
//  1) when promise q is rejected.
//  2) when the custom reject closure in MyPromise throws an exception.

Debug = debug.Debug;

var expected_events = 1;
var log = [];

var p = new Promise(function(resolve, reject) {
  log.push("resolve");
  resolve();
});

function MyPromise(resolver) {
  var reject = function() {
    log.push("throw in reject");
    throw new Error("reject");  // event
  };
  var resolve = function() { };
  log.push("construct");
  resolver(resolve, reject);
};

MyPromise.prototype = new Promise(function() {});
p.constructor = MyPromise;

var q = p.then(
  function() {
    log.push("reject caught");
    return Promise.reject(new Error("caught"));
  });

function listener(event, exec_state, event_data, data) {
  try {
    if (event == Debug.DebugEvent.Exception) {
      expected_events--;
      assertTrue(expected_events >= 0);
      assertEquals("reject", event_data.exception().message);
      // Assert that the debug event is triggered at the throw site.
      assertTrue(
          exec_state.frame(0).sourceLineText().indexOf("// event") > 0);
    }
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

log.push("end main");

function testDone(iteration) {
  function checkResult() {
    try {
      assertTrue(iteration < 10);
      if (expected_events === 0) {
        assertEquals(["resolve", "construct", "end main",
                      "reject caught", "throw in reject"], log);
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
