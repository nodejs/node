// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


// Test that setting break point works correctly when the debugger is
// activated late, which leads to duplicate shared function infos.

(function() {
  var Debug = %GetDebugContext().Debug;

  function listener(event, exec_state, event_data, data) {
    if (event != Debug.DebugEvent.Break) return;
    try {
      assertTrue(/foo/.test(exec_state.frame(0).sourceLineText()));
      break_count++;
    } catch (e) {
      exception = e;
    }
  }

  for (var i = 0; i < 3; i++) {
    var foo = function() { a = 1; }
    var exception = null;
    var break_count = 0;
    Debug.setListener(listener);
    if (i < 2) Debug.setBreakPoint(foo, 0, 0);
    assertTrue(/\[B\d\]a = 1/.test(Debug.showBreakPoints(foo)));
    foo();
    assertEquals(1, break_count);
    assertNull(exception);
  }

  Debug.setListener(null);
})();
