// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


Debug = debug.Debug
var exception = null;
var break_count = 0;
const expected_breaks = 8;

function listener(event, exec_state, event_data, data) {
  try {
    if (event == Debug.DebugEvent.Break) {
      assertTrue(exec_state.frameCount() != 0, "FAIL: Empty stack trace");
      var frameMirror = exec_state.frame(0);

      frameMirror.allScopes();
      var source = frameMirror.sourceLineText();
      print("paused at: " + source);
      assertTrue(source.indexOf("// Break " + break_count + ".") > 0,
                 "Unexpected pause at: " + source + "\n" +
                 "Expected: // Break " + break_count + ".");
      ++break_count;

      if (break_count !== expected_breaks) {
        exec_state.prepareStep(Debug.StepAction.StepIn);
        print("Next step prepared");
      }
    }
  } catch(e) {
    exception = e;
    print(e, e.stack);
  }
};

Debug.setListener(listener);

var sum = 0;
(function (){
  'use strict';

  debugger; // Break 0.
  var i = 0; // Break 1.
  i++; // Break 2.
  i++; // Break 3.
  debugger;  // Break 4.
  return i  // Break 5.
  ; // Break 6.
}());

assertNull(exception); // Break 7.
assertEquals(expected_breaks, break_count);

Debug.setListener(null);
