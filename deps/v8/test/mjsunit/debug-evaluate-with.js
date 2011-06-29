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

listenerComplete = false;
exception = false;
breakPointCount = 0;

function listener(event, exec_state, event_data, data) {
  try {
    if (event == Debug.DebugEvent.Break)
    {
      breakPointCount++;
      if (breakPointCount == 1) {
        // Break point in first with block.
        assertEquals(2, exec_state.frame(0).evaluate('a').value());
        assertEquals(2, exec_state.frame(0).evaluate('b').value());
      } else if (breakPointCount == 2) {
        // Break point in second with block.
        assertEquals(3, exec_state.frame(0).evaluate('a').value());
        assertEquals(1, exec_state.frame(0).evaluate('b').value());
      } else if (breakPointCount == 3) {
        // Break point in eval with block.
        assertEquals('local', exec_state.frame(0).evaluate('foo').value());
      }
    }
  } catch (e) {
    exception = e
  };
};

// Add the debug event listener.
Debug.setListener(listener);

function f() {
  var a = 1;
  var b = 2;
  with ({a:2}) {
    debugger;  // Breakpoint.
    x = {a:3,b:1};
    with (x) {
      debugger;  // Breakpoint.
    }
  }
};

f();

var foo = "global";
eval("with({bar:'with'}) { (function g() { var foo = 'local'; debugger; })(); }");

// Make sure that the debug event listener vas invoked.
assertEquals(3, breakPointCount);
assertFalse(exception, "exception in listener")
