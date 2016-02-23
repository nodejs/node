// Copyright 2012 the V8 project authors. All rights reserved.
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

// Test stepping into callbacks passed to builtin functions.

Debug = debug.Debug

var exception = false;

function array_listener(event, exec_state, event_data, data) {
  try {
    if (event == Debug.DebugEvent.Break) {
      if (breaks == 0) {
        exec_state.prepareStep(Debug.StepAction.StepIn, 2);
        breaks = 1;
      } else if (breaks <= 3) {
        breaks++;
        // Check whether we break at the expected line.
        print(event_data.sourceLineText());
        assertTrue(event_data.sourceLineText().indexOf("Expected to step") > 0);
        exec_state.prepareStep(Debug.StepAction.StepIn, 3);
      }
    }
  } catch (e) {
    exception = true;
  }
};

function cb_false(num) {
  print("element " + num);  // Expected to step to this point.
  return false;
}

function cb_true(num) {
  print("element " + num);  // Expected to step to this point.
  return true;
}

function cb_reduce(a, b) {
  print("elements " + a + " and " + b);  // Expected to step to this point.
  return a + b;
}

var a = [1, 2, 3, 4];

Debug.setListener(array_listener);

var breaks = 0;
debugger;
a.forEach(cb_true);
assertFalse(exception);
assertEquals(4, breaks);

breaks = 0;
debugger;
a.some(cb_false);
assertFalse(exception);
assertEquals(4, breaks);

breaks = 0;
debugger;
a.every(cb_true);
assertEquals(4, breaks);
assertFalse(exception);

breaks = 0;
debugger;
a.map(cb_true);
assertFalse(exception);
assertEquals(4, breaks);

breaks = 0;
debugger;
a.filter(cb_true);
assertFalse(exception);
assertEquals(4, breaks);

breaks = 0;
debugger;
a.reduce(cb_reduce);
assertFalse(exception);
assertEquals(4, breaks);

breaks = 0;
debugger;
a.reduceRight(cb_reduce);
assertFalse(exception);
assertEquals(4, breaks);

Debug.setListener(null);


// Test two levels of builtin callbacks:
// Array.forEach calls a callback function, which by itself uses
// Array.forEach with another callback function.

function second_level_listener(event, exec_state, event_data, data) {
  try {
    if (event == Debug.DebugEvent.Break) {
      if (breaks == 0) {
        exec_state.prepareStep(Debug.StepAction.StepIn, 3);
        breaks = 1;
      } else if (breaks <= 16) {
        breaks++;
        // Check whether we break at the expected line.
        assertTrue(event_data.sourceLineText().indexOf("Expected to step") > 0);
        // Step two steps further every four breaks to skip the
        // forEach call in the first level of recurision.
        var step = (breaks % 4 == 1) ? 6 : 3;
        exec_state.prepareStep(Debug.StepAction.StepIn, step);
      }
    }
  } catch (e) {
    exception = true;
  }
};

function cb_foreach(num) {
  a.forEach(cb_true);
  print("back to the first level of recursion.");
}

Debug.setListener(second_level_listener);

breaks = 0;
debugger;
a.forEach(cb_foreach);
assertFalse(exception);
assertEquals(17, breaks);

Debug.setListener(null);
