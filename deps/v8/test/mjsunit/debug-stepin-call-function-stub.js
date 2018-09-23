// Copyright 2009 the V8 project authors. All rights reserved.
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

var exception = null;
var state = 0;
var expected_function_name = null;
var expected_source_line_text = null;
var expected_caller_source_line = null;
var step_in_count = 2;

// Simple debug event handler which first time will cause 'step in' action
// to get into g.call and than check that execution is pauesed inside
// function 'g'.
function listener(event, exec_state, event_data, data) {
  try {
    if (event == Debug.DebugEvent.Break) {
      if (state == 0) {
        // Step into f().
        exec_state.prepareStep(Debug.StepAction.StepIn, step_in_count);
        state = 2;
      } else if (state == 2) {
        assertEquals(expected_source_line_text,
                     event_data.sourceLineText());
        assertEquals(expected_function_name, event_data.func().name());
        state = 3;
      }
    }
  } catch(e) {
    exception = e;
  }
};

// Add the debug event listener.
Debug.setListener(listener);


function g() {
   return "s";  // expected line
}

function testFunction() {
  var f = g;
  var s = 1 +f(10);
}

function g2() {
   return "s2";  // expected line
}

function testFunction2() {
  var f = g2;
  var s = 1 +f(10, 20);
}

// Run three times. First time the function will be compiled lazily,
// second time cached version will be used.
for (var i = 0; i < 3; i++) {
  state = 0;
  expected_function_name = 'g';
  expected_source_line_text = '   return "s";  // expected line';
  step_in_count = 2;
  // Set a break point and call to invoke the debug event listener.
  Debug.setBreakPoint(testFunction, 1, 0);
  testFunction();
  assertNull(exception);
  assertEquals(3, state);
}

// Test stepping into function call when a breakpoint is set at the place
// of call. Use different pair of functions so that g2 is compiled lazily.
// Run twice: first time function will be compiled lazily, second time
// cached version will be used.
for (var i = 0; i < 3; i++) {
  state = 0;
  expected_function_name = 'g2';
  expected_source_line_text = '   return "s2";  // expected line';
  step_in_count = 1;
  // Set a break point and call to invoke the debug event listener.
  Debug.setBreakPoint(testFunction2, 2, 0);
  testFunction2();
  assertNull(exception);
  assertEquals(3, state);
}


// Get rid of the debug event listener.
Debug.setListener(null);
