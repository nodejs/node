// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

var Debug = debug.Debug;
var exception = null;

Debug.setListener(function (event, exec_state, event_data, data) {
  if (event == Debug.DebugEvent.Break) {
    try {
      var sab2 = new SharedArrayBuffer(4);
      var i32a2 = new Int32Array(sab2);
      Atomics.wait(i32a2, 0, 0, 10);
    } catch (e) {
      exception = e;
    }
  }
});

let sab = new SharedArrayBuffer(4);
let i32a = new Int32Array(sab);

let timeout = {
  valueOf: function() {
    %ScheduleBreak();
    return 10;
  }
};

Atomics.wait(i32a, 0, 0, timeout);

assertNotNull(exception);
assertTrue(exception.message.includes("cannot be called in this context"));
