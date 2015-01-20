// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --expose-debug-as debug

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
      if (source.indexOf("StepOver.") !== -1) {
        exec_state.prepareStep(Debug.StepAction.StepNext, 1);
      } else {
        exec_state.prepareStep(Debug.StepAction.StepIn, 1);
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
  .catch(function(e) {
    %AbortJS("FAIL: uncaught exception " + e);
  });

function promise1() {
  debugger; // Break 0.
  return exception || 1; // Break 1.
} // Break 2.

function promise2() {
  throw new Error; // Break 3.
}

function promise3() {
  installObservers(); // Break 4. StepOver.
  return break_count; // Break 5.
} // Break 6.

function installObservers() {
  var dummy = {};
  Object.observe(dummy, observer1);
  Object.observe(dummy, Object); // Should skip stepping into native.
  Object.observe(dummy, Boolean); // Should skip stepping into native.
  Object.observe(dummy, observer2);
  dummy.foo = 1;
}

function observer1() {
  return exception || 3; // Break 7.
} // Break 8.

function observer2() {
  Promise.resolve().then(promise4); // Break 9. StepOver.
  return break_count + 1; // Break 10.
} // Break 11.

function promise4() {
  finalize(); // Break 12. StepOver.
  return 0; // Break 13.
} // Break 14. StepOver.

function finalize() {
  var dummy = {};
  Object.observe(dummy, function() {
    if (expected_breaks !== break_count) {
      %AbortJS("FAIL: expected <" + expected_breaks + "> breaks instead of <" +
               break_count + ">");
    }
    if (exception !== null) {
      %AbortJS("FAIL: exception: " + exception);
    }
  });
  dummy.foo = 1;
}
