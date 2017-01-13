// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-debug-as debug --allow-natives-syntax

// Test debug events when an exception is thrown inside a Promise, which is
// caught by a custom promise, which throws a new exception in its reject
// handler. We expect two Exception debug events:
//  1) when the exception is thrown in the promise q.
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
MyPromise.__proto__ = Promise;
p.constructor = MyPromise;

var q = p.then(
  function() {
    log.push("throw caught");
    throw new Error("caught");  // event
  });

function listener(event, exec_state, event_data, data) {
  try {
    if (event == Debug.DebugEvent.Exception) {
      expected_events--;
      assertTrue(expected_events >= 0);
      if (expected_events == 0) {
        assertEquals(["resolve", "construct", "end main",
                      "throw caught"], log);
        assertEquals("caught", event_data.exception().message);
      } else {
        assertUnreachable();
      }
      assertSame(q, event_data.promise());
      assertTrue(exec_state.frame(0).sourceLineText().indexOf('// event') > 0);
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
        assertEquals(["resolve", "construct", "end main",
                      "throw caught", "throw in reject"], log);
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
