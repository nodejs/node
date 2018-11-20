// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-debug-as debug --allow-natives-syntax

// Test debug events when we only listen to uncaught exceptions and
// there is a catch handler for the to-be-rejected Promise.
// We expect no Exception debug events, since the default reject handler passes
// the rejection on to a user-defined reject handler.

Debug = debug.Debug;

var resolve, reject;
var p0 = new Promise(function(res, rej) { resolve = res; reject = rej; });

var p1 = p0.then(function() {
  throw new Error();
});

var p2 = p1.then(function() { });
var p3 = p2.catch(function() { });

var q = new Promise(function(res, rej) {
  res();
});

q.then(function() {
  resolve();
})


function listener(event, exec_state, event_data, data) {
  try {
    assertTrue(event != Debug.DebugEvent.Exception);
  } catch (e) {
    %AbortJS(e + "\n" + e.stack);
  }
}

Debug.setBreakOnUncaughtException();
Debug.setListener(listener);
