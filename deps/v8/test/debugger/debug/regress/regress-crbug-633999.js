// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --opt --no-turbo

var Debug = debug.Debug
var exception = null;
var step = 0;

function listener(event, exec_state, event_data, data) {
  if (event != Debug.DebugEvent.Exception) return;
  try {
    step++;
  } catch (e) {
    exception = e;
  }
}

Debug.setBreakOnException();
Debug.setListener(listener);

(function () {
  "use asm";
  function f() {
    try {
      throw 666;
    } catch (e) {
    }
  }
  f();
  f();
  %OptimizeFunctionOnNextCall(f);
  f();
  assertOptimized(f);
})();

Debug.setListener(null);
assertNull(exception);
assertEquals(3, step);
