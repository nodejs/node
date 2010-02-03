// Copyright 2008 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

// Flags: --expose-debug-as debug
// Get the Debug object exposed from the debug context global object.
Debug = debug.Debug

// Simple debug event handler which first time hit will perform 1000 steps and
// second time hit will evaluate and store the value of "i". If requires that
// the global property "state" is initially zero.

var bp1, bp2;

function listener(event, exec_state, event_data, data) {
  if (event == Debug.DebugEvent.Break) {
    if (state == 0) {
      exec_state.prepareStep(Debug.StepAction.StepIn, 1000);
      state = 1;
    } else if (state == 1) {
      result = exec_state.frame().evaluate("i").value();
      // Clear the break point on line 2 if set.
      if (bp2) {
        Debug.clearBreakPoint(bp2);
      }
    }
  }
};

// Add the debug event listener.
Debug.setListener(listener);

// Test debug event for break point.
function f() {
  for (i = 0; i < 1000; i++) {  //  Line 1.
    x = 1;                      //  Line 2.
  }
};

// Set a breakpoint on the for statement (line 1).
bp1 = Debug.setBreakPoint(f, 1);

// Check that performing 1000 steps will make i 499.
state = 0;
result = -1;
f();
assertEquals(499, result);

// Check that performing 1000 steps with a break point on the statement in the
// for loop (line 2) will only make i 0 as a real break point breaks even when
// multiple steps have been requested.
state = 0;
result = -1;
bp2 = Debug.setBreakPoint(f, 2);
f();
assertEquals(0, result);

// Get rid of the debug event listener.
Debug.setListener(null);
