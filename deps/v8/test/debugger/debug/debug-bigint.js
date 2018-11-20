// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --harmony-bigint

Debug = debug.Debug
let exceptionThrown = false;

Debug.setListener(function(event, exec_state, event_data, data) {
  if (event != Debug.DebugEvent.Break) return;
  try {
    const o = exec_state.frame(0).evaluate("42n", true);
    assertEquals("bigint", o.type());
    assertFalse(o.isUndefined());
    assertEquals("bigint", typeof(o.value()));
    assertEquals(42n, o.value());
  } catch (e) {
    exceptionThrown = true;
  };
});

!function() { debugger; }();
assertFalse(exceptionThrown, "exception in listener")
