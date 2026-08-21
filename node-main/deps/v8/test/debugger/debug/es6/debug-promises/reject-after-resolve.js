// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


// Test debug events when we listen to uncaught exceptions and
// the Promise is rejected in a chained closure after it has been resolved.
// We expect no Exception debug event to be triggered.

Debug = debug.Debug;

var log = [];

var p = new Promise(function(resolve, reject) {
  log.push("resolve");
  resolve(reject);
});

var q = p.then(
  function(value) {
    assertEquals(["resolve", "end main"], log);
    value(new Error("reject"));
  });

function listener(event, exec_state, event_data, data) {
  try {
    assertTrue(event != Debug.DebugEvent.Exception);
  } catch (e) {
    %AbortJS(e + "\n" + e.stack);
  }
}

Debug.setBreakOnException();
Debug.setListener(listener);

log.push("end main");
