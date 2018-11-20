// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-debug-as debug --allow-natives-syntax

// Test debug events when a Promise is rejected, which is caught by a custom
// promise, which has undefined for reject closure.  We expect an Exception
// debug even calling the (undefined) custom rejected closure.

Debug = debug.Debug;

var expected_events = 1;
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
    log.push("reject caught");
    return Promise.reject(new Error("caught"));
  });

function listener(event, exec_state, event_data, data) {
  try {
    if (event == Debug.DebugEvent.Exception) {
      expected_events--;
      assertTrue(expected_events >= 0);
      assertEquals("caught", event_data.exception().message);
      // All of the frames on the stack are from native Javascript.
      assertEquals(0, exec_state.frameCount());
    }
  } catch (e) {
    %AbortJS(e + "\n" + e.stack);
  }
}

Debug.setBreakOnUncaughtException();
Debug.setListener(listener);

function testDone(iteration) {
  function checkResult() {
    try {
      assertTrue(iteration < 10);
      if (expected_events === 0) {
        assertEquals(["resolve", "construct", "end main", "reject caught"],
                     log);
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

log.push("end main");
