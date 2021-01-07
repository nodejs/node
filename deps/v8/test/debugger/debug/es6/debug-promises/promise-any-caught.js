// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Test debug events when we only listen to uncaught exceptions and a
// Promise p3 created by Promise.any has a catch handler, and is rejected
// because the Promise p2 passed to Promise.any is rejected. We
// expect no Exception debug event to be triggered, since p3 and by
// extension p2 have a catch handler.

let Debug = debug.Debug;

let expected_events = 2;

let p1 = Promise.resolve();
p1.name = "p1";

let p2 = p1.then(function() {
  throw new Error("caught");
});
p2.name = "p2";

let p3 = Promise.any([p2]);
p3.name = "p3";

p3.catch(function(e) {});

function listener(event, exec_state, event_data, data) {
  try {
    assertTrue(event != Debug.DebugEvent.Exception)
  } catch (e) {
    %AbortJS(e + "\n" + e.stack);
  }
}

Debug.setBreakOnUncaughtException();
Debug.setListener(listener);
