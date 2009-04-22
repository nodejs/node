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


function checkFrame0(name, value) {
  assertTrue(name == 'a' || name == 'b');
  if (name == 'a') {
    assertEquals(1, value);
  }
  if (name == 'b') {
    assertEquals(2, value);
  }
}


function checkFrame1(name, value) {
  assertTrue(name == '.arguments' || name == 'a');
  if (name == 'a') {
    assertEquals(3, value);
  }
}


function checkFrame2(name, value) {
  assertTrue(name == '.arguments' || name == 'a' ||
             name == 'arguments' || name == 'b');
  if (name == 'a') {
    assertEquals(5, value);
  }
  if (name == 'b') {
    assertEquals(0, value);
  }
}


function listener(event, exec_state, event_data, data) {
  try {
    if (event == Debug.DebugEvent.Break)
    {
      // Frame 0 has normal variables a and b.
      var frame0 = exec_state.frame(0);
      checkFrame0(frame0.localName(0), frame0.localValue(0).value());
      checkFrame0(frame0.localName(1), frame0.localValue(1).value());

      // Frame 1 has normal variable a (and the .arguments variable).
      var frame1 = exec_state.frame(1);
      checkFrame1(frame1.localName(0), frame1.localValue(0).value());
      checkFrame1(frame1.localName(1), frame1.localValue(1).value());

      // Frame 2 has normal variables a and b (and both the .arguments and
      // arguments variable).
      var frame2 = exec_state.frame(2);
      checkFrame2(frame2.localName(0), frame2.localValue(0).value());
      checkFrame2(frame2.localName(1), frame2.localValue(1).value());
      checkFrame2(frame2.localName(2), frame2.localValue(2).value());
      checkFrame2(frame2.localName(3), frame2.localValue(3).value());

      // Evaluating a and b on frames 0, 1 and 2 produces 1, 2, 3, 4, 5 and 6.
      assertEquals(1, exec_state.frame(0).evaluate('a').value());
      assertEquals(2, exec_state.frame(0).evaluate('b').value());
      assertEquals(3, exec_state.frame(1).evaluate('a').value());
      assertEquals(4, exec_state.frame(1).evaluate('b').value());
      assertEquals(5, exec_state.frame(2).evaluate('a').value());
      assertEquals(6, exec_state.frame(2).evaluate('b').value());

      // Indicate that all was processed.
      listenerComplete = true;
    }
  } catch (e) {
    exception = e
  };
};

// Add the debug event listener.
Debug.setListener(listener);

function h() {
  var a = 1;
  var b = 2;
  debugger;  // Breakpoint.
};

function g() {
  var a = 3;
  eval("var b = 4;");
  h();
};

function f() {
  var a = 5;
  var b = 0;
  with ({b:6}) {
    g();
  }
};

f();

// Make sure that the debug event listener vas invoked.
assertTrue(listenerComplete);
assertFalse(exception, "exception in listener")
