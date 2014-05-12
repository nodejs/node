// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --harmony-promises --expose-debug-as debug

// Test debug events when an exception is thrown inside a Promise, which is
// caught by a custom promise, which has no reject handler.
// We expect an Exception event with a promise to be triggered.

Debug = debug.Debug;

var log = [];
var step = 0;

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

MyPromise.prototype = p;
p.constructor = MyPromise;

var q = p.chain(
  function() {
    log.push("throw caught");
    throw new Error("caught");  // event
  });

function listener(event, exec_state, event_data, data) {
  try {
    if (event == Debug.DebugEvent.Exception) {
      assertEquals(["resolve", "construct", "end main", "throw caught"], log);
      assertEquals("undefined is not a function",
                   event_data.exception().message);
      assertEquals(q, event_data.promise());
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
