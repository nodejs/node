// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Tests stepping into through Array.prototype.forEach callbacks.

Debug = debug.Debug
var exception = null;
var break_count = 0;
var expected_breaks = 11;

function listener(event, exec_state, event_data, data) {
  try {
    if (event == Debug.DebugEvent.Break) {
      assertTrue(exec_state.frameCount() != 0, "FAIL: Empty stack trace");
      var source = exec_state.frame(0).sourceLineText();
      print("paused at: " + source);
      assertTrue(source.indexOf("// Break " + break_count + ".") > 0,
                 "Unexpected pause at: " + source + "\n" +
                 "Expected: // Break " + break_count + ".");
      ++break_count;
      if (break_count !== expected_breaks) {
        exec_state.prepareStep(Debug.StepAction.StepInto);
      }
    }
  } catch(e) {
    exception = e;
    print(e, e.stack);
  }
};

Debug.setListener(listener);
var bound_callback = callback.bind(null);

debugger; // Break 0.
[1,2].forEach(callback); // Break 1.
[3,4].forEach(bound_callback); // Break 6.

function callback(x) {
  return x // Break 2. // Break 4. // Break 7. // Break 9.
  ; // Break 3. // Break 5. // Break 8. // Break 10.
}

assertNull(exception); // Break 11.
assertEquals(expected_breaks, break_count);

Debug.setListener(null);
