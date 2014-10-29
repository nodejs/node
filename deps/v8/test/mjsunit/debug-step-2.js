// Copyright 2011 the V8 project authors. All rights reserved.
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
// Flags: --turbo-deoptimization

// This test tests that full code compiled without debug break slots
// is recompiled with debug break slots when debugging is started.

// Get the Debug object exposed from the debug context global object.
Debug = debug.Debug

var bp;
var done = false;
var step_count = 0;

// Debug event listener which steps until the global variable done is true.
function listener(event, exec_state, event_data, data) {
  if (event == Debug.DebugEvent.Break) {
    if (!done) exec_state.prepareStep(Debug.StepAction.StepNext);
    step_count++;
  }
};

// Set the global variables state to prpare the stepping test.
function prepare_step_test() {
  done = false;
  step_count = 0;
}

// Test function to step through.
function f() {
  var i = 1;
  var j = 2;
  done = true;
};

prepare_step_test();
f();

// Add the debug event listener.
Debug.setListener(listener);

bp = Debug.setBreakPoint(f, 1);

prepare_step_test();
f();
assertEquals(4, step_count);
Debug.clearBreakPoint(bp);

// Set a breakpoint on the first var statement (line 1).
bp = Debug.setBreakPoint(f, 1);

// Step through the function ensuring that the var statements are hit as well.
prepare_step_test();
f();
assertEquals(4, step_count);

// Clear the breakpoint and check that no stepping happens.
Debug.clearBreakPoint(bp);
prepare_step_test();
f();
assertEquals(0, step_count);

// Get rid of the debug event listener.
Debug.setListener(null);
