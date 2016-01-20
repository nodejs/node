// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-debug-as debug --allow-natives-syntax

// Test debug events when we only listen to uncaught exceptions and a
// Promise p3 created by Promise.all has a catch handler, and is rejected
// because one of the Promises p2 passed to Promise.all is rejected. We
// expect no Exception debug event to be triggered, since p3 and by
// extension p2 have a catch handler.

var Debug = debug.Debug;

var expected_events = 2;

var p1 = Promise.resolve();
p1.name = "p1";

var p2 = p1.then(function() {
  throw new Error("caught");
});

p2.name = "p2";

var p3 = Promise.all([p2]);
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
