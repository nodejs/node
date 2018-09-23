// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-debug-as debug --allow-natives-syntax

// Test debug events when we listen to all exceptions and
// there is a catch handler for the exception thrown in a Promise.
// We expect a normal Exception debug event to be triggered.

Debug = debug.Debug;

var events = [];

function listener(event, exec_state, event_data, data) {
  if (event == Debug.DebugEvent.PromiseEvent) events.push(event_data.status());
}

Debug.setListener(listener);

var p = new Promise(function(resolve, reject) {
  try {
    throw new Error("reject");
  } finally {
    // Implicit rethrow.
  }
  resolve();
});

assertEquals([0 /* create */, -1 /* rethrown */], events);
