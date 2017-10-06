// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


Debug = debug.Debug
var exception = null;
var break_count = 0;
const expected_breaks = 10;

function listener(event, exec_state, event_data, data) {
  try {
    if (event == Debug.DebugEvent.Break) {
      assertTrue(exec_state.frameCount() != 0, "FAIL: Empty stack trace");
      var source = exec_state.frame(0).sourceLineText();
      print("paused at: " + source);
      assertTrue(source.indexOf("// Break " + break_count + ".") > 0,
                 "Unexpected pause at: " + source + "\n" +
                 "Expected: // Break " + break_count + ".");
      if (source.indexOf("StepOver.") !== -1) {
        exec_state.prepareStep(Debug.StepAction.StepNext);
      } else if (source.indexOf("StepOut.") !== -1) {
        exec_state.prepareStep(Debug.StepAction.StepOut);
      } else {
        exec_state.prepareStep(Debug.StepAction.StepIn);
      }
      ++break_count;
    }
  } catch (e) {
    exception = e;
    print(e, e.stack);
  }
};

Debug.setListener(listener);

Promise.resolve(42)
  .then(promise1)
  .then(Object) // Should skip stepping into native.
  .then(Boolean) // Should skip stepping into native.
  .then(promise2)
  .catch(promise3)
  .then(promise4)
  .catch(function(e) {
    %AbortJS("FAIL: uncaught exception " + e);
  });

function promise1() {
  debugger; // Break 0.
  return exception || 1 // Break 1.
  ; // Break 2. StepOver.
}

function promise2() {
  throw new Error; // Break 3.
}

function promise3() {
  return break_count // Break 4.
  ; // Break 5.
}

function promise4() {
  finalize(); // Break 6. StepOver.
  return 0 // Break 7.
  ; // Break 8. StepOut.
}

function finalize() {
  Promise.resolve().then(function() {
    if (expected_breaks !== break_count) { // Break 9. StepOut.
      %AbortJS("FAIL: expected <" + expected_breaks + "> breaks instead of <" +
               break_count + ">");
    }
    if (exception !== null) {
      %AbortJS("FAIL: exception: " + exception);
    }
  });
}
