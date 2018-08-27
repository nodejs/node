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

// The functions used for testing backtraces. They are at the top to make the
// testing of source line/column easier.
function f(x, y) {
  a=1;
};

var m = (0, function() {
  new f(1);
});

function g() {
  m();
};


Debug = debug.Debug

let listenerCalled = false;
let exceptionThrown = false;

function listener(event, exec_state, event_data, data) {
  try {
    if (event == Debug.DebugEvent.Break) {
      // The expected backtrace is
      // 0: f
      // 1: m
      // 2: g
      // 3: [anonymous]
      assertEquals(4, exec_state.frameCount());

      var frame0 = exec_state.frame(0);
      assertEquals("f", frame0.func().name());
      assertEquals(0, frame0.index());
      assertEquals(31, frame0.sourceLine());
      assertEquals(2, frame0.sourceColumn());
      assertEquals(2, frame0.localCount());
      assertEquals("x", frame0.localName(0));
      assertEquals(1, frame0.localValue(0).value());
      assertEquals("y", frame0.localName(1));
      assertEquals(undefined, frame0.localValue(1).value());

      var frame1 = exec_state.frame(1);
      assertEquals("m", frame1.func().name());
      assertEquals(1, frame1.index());
      assertEquals(35, frame1.sourceLine());
      assertEquals(2, frame1.sourceColumn());
      assertEquals(0, frame1.localCount());

      var frame2 = exec_state.frame(2);
      assertEquals("g", frame2.func().name());

      var frame3 = exec_state.frame(3);
      assertEquals("", frame3.func().name());

      listenerCalled = true;
    }
  } catch (e) {
    exceptionThrown = true;
  };
};

// Add the debug event listener.
Debug.setListener(listener);

// Set a break point and call to invoke the debug event listener.
Debug.setBreakPoint(f, 0, 0);
g();

// Make sure that the debug event listener vas invoked.
assertFalse(exceptionThrown, "exception in listener");
assertTrue(listenerCalled);
