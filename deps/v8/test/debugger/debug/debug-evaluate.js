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

listenerComplete = false;
exception = false;

function listener(event, exec_state, event_data, data) {
  try {
    if (event == Debug.DebugEvent.Break) {
      assertEquals(3, exec_state.frame(0).evaluate("1+2").value());
      assertEquals(5, exec_state.frame(0).evaluate("a+2").value());
      assertEquals(4, exec_state.frame(0).evaluate("({a:1,b:2}).b+2").value());

      assertEquals(3, exec_state.frame(0).evaluate("a").value());
      assertEquals(2, exec_state.frame(1).evaluate("a").value());
      assertEquals(1, exec_state.frame(2).evaluate("a").value());

      assertEquals(1, exec_state.evaluateGlobal("a").value());
      assertEquals(1, exec_state.evaluateGlobal("this.a").value());

      assertEquals(longString,
                   exec_state.evaluateGlobal("this.longString").value());

      // Indicate that all was processed.
      listenerComplete = true;
    }
  } catch (e) {
   exception = e
  };
};

// Add the debug event listener.
Debug.setListener(listener);

function f() {
  var a = 3;
};

function g() {
  var a = 2;
  f();
  return a;  // Use the value to prevent it being removed by DCE.
};

a = 1;

// String which is longer than 80 chars.
var longString = "1234567890_";
for (var i = 0; i < 4; i++) {
  longString += longString;
}

// Set a break point at return in f and invoke g to hit the breakpoint.
Debug.setBreakPoint(f, 2, 0);
g();

assertFalse(exception, "exception in listener")
// Make sure that the debug event listener vas invoked.
assertTrue(listenerComplete, "listener did not run to completion");
