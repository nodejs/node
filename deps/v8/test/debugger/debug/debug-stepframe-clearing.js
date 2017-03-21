// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


// This test ensures that IC learning doesn't interfere with stepping into
// property accessor. f1()'s ICs are allowed to learn to a monomorphic state,
// and the breakpoints flooding get() are allowed to expire, then we ensure
// that we can step into get() again later (when k == 1).
function f1() {
  for (var k = 0; k < 2; k++) {     // Break 1
    var v10 = 0;                    // Line 2
    for (var i = 0; i < 10; i++) {  // Line 3
      var v12 = o.slappy;           // Line 4
      var v13 = 3                   // Line 5
    }                               // Line 6
    print("break here");            // Break 3
  }                                 // Line 8
  print("exiting f1");              // Line 9 (dummy break)
}

function get() {
  var g0 = 0;                       // Break 2
  var g1 = 1;
  return 3;
}


var o = {};
Object.defineProperty(o, "slappy", { get : get });

Debug = debug.Debug;
var break_count = 0
var exception = null;
var bp_f1_line7;
var bp_f1_line9;

function listener(event, exec_state, event_data, data) {
  if (event != Debug.DebugEvent.Break) return;
  try {
    var line = exec_state.frame(0).sourceLineText();
    print(line);
    var match = line.match(/\/\/ Break (\d+)$/);
    assertEquals(2, match.length);
    var match_value = parseInt(match[1]);

    if (break_count >= 0 && break_count < 2) {
      // 0, 1: Keep stepping through frames.
      assertEquals(break_count, match_value);
      exec_state.prepareStep(Debug.StepAction.StepFrame);
    } else if (break_count === 2) {
      // 2: let the code run to a breakpoint we set. The load should
      // go monomorphic.
      assertEquals(break_count, match_value);
    } else if (break_count === 3) {
      // 3: back to frame stepping. Does the monomorphic slappy accessor
      // call still have the ability to break like before?
      assertEquals(break_count, match_value);
      Debug.clearBreakPoint(bp_f1_line7);
      exec_state.prepareStep(Debug.StepAction.StepFrame);
    } else {
      assertEquals(4, break_count);
      assertEquals(2, match_value);
      // Apparently we can still stop in the accessor even though we cleared
      // breakpoints earlier and there was a monomorphic step.
      // Allow running to completion now.
      Debug.clearBreakPoint(bp_f1_line9);
    }

    break_count++;
  } catch (e) {
    print(e + e.stack);
    exception = e;
  }
}

for (var j = 1; j < 3; j++) {
  break_count = 0;
  Debug.setListener(listener);

  // Breakpoints are added here rather than in the listener because their
  // addition causes a full (clearing) gc that clears type feedback when we
  // want to let it build up. Also, bp_f1_line9 is set simply because if we
  // handled then deleted bp_f1_line7, then the debugger clears DebugInfo from
  // f1 while we are still using it, again, resetting type feedback which is
  // undesirable.
  bp_f1_line7 = Debug.setBreakPoint(f1, 7);
  bp_f1_line9 = Debug.setBreakPoint(f1, 9);

  debugger;                         // Break 0
  f1();
  Debug.setListener(null);
  assertTrue(break_count === 5);
}

assertNull(exception);
