// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-debug-as debug --allow-natives-syntax

// Test debug events when we only listen to uncaught exceptions, the Promise
// is rejected, and a catch handler is installed right before the rejection.
// We expect no debug event to be triggered.

Debug = debug.Debug;

var p = new Promise(function(resolve, reject) {
  resolve();
});

var q = p.then(
  function() {
    q.catch(function(e) {
      assertEquals("caught", e.message);
    });
    return Promise.reject(Error("caught"));
  });

function listener(event, exec_state, event_data, data) {
  try {
    assertTrue(event != Debug.DebugEvent.Exception);
  } catch (e) {
    %AbortJS(e + "\n" + e.stack);
  }
}

Debug.setBreakOnUncaughtException();
Debug.setListener(listener);
