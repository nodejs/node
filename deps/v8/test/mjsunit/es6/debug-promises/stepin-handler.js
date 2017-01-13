// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-debug-as debug --allow-natives-syntax --noalways-opt
// Tests stepping into through Promises.

Debug = debug.Debug
var exception = null;
var break_count = 0;
var expected_breaks = -1;

function listener(event, exec_state, event_data, data) {
  try {
    if (event == Debug.DebugEvent.Break) {
      assertTrue(exec_state.frameCount() != 0, "FAIL: Empty stack trace");
      if (!break_count) {
        // Count number of expected breakpoints in this source file.
        var source_text = exec_state.frame(0).func().script().source();
        expected_breaks = source_text.match(/\/\/\s*Break\s+\d+\./g).length;
        print("Expected breaks: " + expected_breaks);
      }
      var source = exec_state.frame(0).sourceLineText();
      print("paused at: " + source);
      assertTrue(source.indexOf("// Break " + break_count + ".") > 0,
                 "Unexpected pause at: " + source + "\n" +
                 "Expected: // Break " + break_count + ".");
      ++break_count;
      if (break_count !== expected_breaks) {
        exec_state.prepareStep(Debug.StepAction.StepIn);
      }
    }
  } catch(e) {
    exception = e;
    print(e, e.stack);
  }
};

Debug.setListener(listener);

Promise.resolve(42)
  .then(
    function f0() {
      debugger;  // Break 0.
    } // Break 1.
  )
  .then(callback)
  .then(callback.bind(null))
  .then(Object)
  .then(callback.bind(null).bind(null))
  .then(finalize)
  .catch(function(err) {
    %AbortJS("FAIL: " + err);
  });

function callback(x) {
  return x; // Break 2. // Break 4. // Break 6.
} // Break 3. // Break 5. // Break 7.

function finalize() {
  assertNull(exception); // Break 8.
  assertEquals(expected_breaks, break_count);

  Debug.setListener(null);
}
