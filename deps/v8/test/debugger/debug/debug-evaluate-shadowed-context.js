// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --no-analyze-environment-liveness

// Test that debug-evaluate only resolves variables that are used by
// the function inside which we debug-evaluate. This is to avoid
// incorrect variable resolution when a context-allocated variable is
// shadowed by a stack-allocated variable.

Debug = debug.Debug

var exception = null;
function listener(event, exec_state, event_data, data) {
  if (event != Debug.DebugEvent.Break) return;
  try {
    for (var i = 0; i < exec_state.frameCount() - 1; i++) {
      var frame = exec_state.frame(i);
      var value;
      try {
        value = frame.evaluate("x").value();
      } catch (e) {
        value = e.name;
      }
      print(frame.sourceLineText());
      var expected = frame.sourceLineText().match(/\/\/ (.*$)/)[1];
      assertEquals(String(expected), String(value));
    }
    assertEquals("[object global]",
                 String(exec_state.frame(0).evaluate("this").value()));
    assertEquals("y", exec_state.frame(0).evaluate("y").value());
    assertEquals("a", exec_state.frame(0).evaluate("a").value());
    exec_state.frame(0).evaluate("a = 'A'");
    assertThrows(() => exec_state.frame(0).evaluate("z"), ReferenceError);
  } catch (e) {
    exception = e;
    print(e + e.stack);
  }
}

Debug.setListener(listener);

var a = "a";
(function() {
  var x = 1;     // context allocate x
  (() => x);
  var y = "y";
  var z = "z";
  (function() {
    var x = 2;   // stack allocate shadowing x
    (function() {
      y;         // access y
      debugger;  // ReferenceError
    })();        // 2
  })();          // 1
  return y;
})();

assertEquals("A", a);
a = "a";

(function() {
  var x = 1;     // context allocate x
  (() => x);
  var y = "y";
  var z = "z";
  (function() {
    var x = 2;   // stack allocate shadowing x
    (() => {
      y;
      a;
      this;      // context allocate receiver
      debugger;  // ReferenceError
    })();        // 2
  })();          // 1
  return y;
})();

assertEquals("A", a);

Debug.setListener(null);
assertNull(exception);
