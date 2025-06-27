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

Debug = debug.Debug

// Simple debug event handler which counts the number of breaks hit and steps.
var break_break_point_hit_count = 0;
function listener(event, exec_state, event_data, data) {
  if (event == Debug.DebugEvent.Break) {
    break_break_point_hit_count++;
    // Continue stepping until returned to bottom frame.
    if (exec_state.frameCount() > 1) {
      exec_state.prepareStep(Debug.StepAction.StepInto);
    }
  }
};

// Add the debug event listener.
Debug.setListener(listener);

// Use 'eval' to ensure that the call to print is through CodeStub CallFunction.
// See Ia32CodeGenerator::VisitCall and Ia32CodeGenerator::CallWithArguments.
function f() {
  debugger;
  eval('');
  print('Hello, world!');
};

break_break_point_hit_count = 0;
f();
assertEquals(6, break_break_point_hit_count);

// Use an inner function to ensure that the function call is through CodeStub
// CallFunction see Ia32CodeGenerator::VisitCall and
// Ia32CodeGenerator::CallWithArguments.
function g() {
  function h() {}
  debugger;
  h();
};

break_break_point_hit_count = 0;
g();
assertEquals(5, break_break_point_hit_count);


// Use an inner function to ensure that the function call is through CodeStub
// CallFunction.
function testCallInExpreesion() {
  function h() {}
  debugger;
  var x = 's' + h(10, 20);
};

break_break_point_hit_count = 0;
testCallInExpreesion();
assertEquals(5, break_break_point_hit_count);


// Get rid of the debug event listener.
Debug.setListener(null);
