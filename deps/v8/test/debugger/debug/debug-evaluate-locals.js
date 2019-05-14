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

let listenerComplete = false;
let exceptionThrown = false;


function h() {
  var a = 1;
  var b = 2;
  var eval = 5;  // Overriding eval should not break anything.
  debugger;  // Breakpoint.
  return a;
}

function checkFrame0(frame) {
  // Frame 0 (h) has normal variables a and b.
  var count = frame.localCount();
  assertEquals(3, count);
  for (var i = 0; i < count; ++i) {
    var name = frame.localName(i);
    var value = frame.localValue(i).value();
    if (name == 'a') {
      assertEquals(1, value);
    } else if (name !='eval') {
      assertEquals('b', name);
      assertEquals(2, value);
    }
  }
}


function g() {
  var a = 3;
  eval("var b = 4;");
  return h() + a;
}

function checkFrame1(frame) {
  // Frame 1 (g) has normal variable a, b (and arguments).
  var count = frame.localCount();
  assertEquals(3, count);
  for (var i = 0; i < count; ++i) {
    var name = frame.localName(i);
    var value = frame.localValue(i).value();
    if (name == 'a') {
      assertEquals(3, value);
    } else if (name == 'b') {
      assertEquals(4, value);
    } else {
      assertEquals('arguments', name);
    }
  }
}


function f() {
  var a = 5;
  var b = 0;
  with ({b:6}) {
    return g();
  }
}

function checkFrame2(frame) {
  // Frame 2 (f) has normal variables a and b.
  var count = frame.localCount();
  assertEquals(2, count);
  for (var i = 0; i < count; ++i) {
    var name = frame.localName(i);
    var value = frame.localValue(i).value();
    if (name == 'a') {
      assertEquals(5, value);
    } else {
      assertEquals('b', name);
      assertEquals(0, value);
    }
  }
}


function listener(event, exec_state, event_data, data) {
  try {
    if (event == Debug.DebugEvent.Break)
    {
      checkFrame0(exec_state.frame(0));
      checkFrame1(exec_state.frame(1));
      checkFrame2(exec_state.frame(2));

      assertEquals(1, exec_state.frame(0).evaluate('a').value());
      assertEquals(2, exec_state.frame(0).evaluate('b').value());
      assertEquals(5, exec_state.frame(0).evaluate('eval').value());
      assertEquals(3, exec_state.frame(1).evaluate('a').value());
      assertEquals(4, exec_state.frame(1).evaluate('b').value());
      assertEquals("function",
                   exec_state.frame(1).evaluate('typeof eval').value());
      assertEquals(5, exec_state.frame(2).evaluate('a').value());
      assertEquals(6, exec_state.frame(2).evaluate('b').value());
      assertEquals("function",
                   exec_state.frame(2).evaluate('typeof eval').value());
      assertEquals("foo",
                   exec_state.frame(0).evaluate('a = "foo"').value());
      assertEquals("bar",
                   exec_state.frame(1).evaluate('a = "bar"').value());
      // Indicate that all was processed.
      listenerComplete = true;
    }
  } catch (e) {
    exceptionThrown = true;
    print("Caught something. " + e + " " + e.stack);
  };
};

// Add the debug event listener.
Debug.setListener(listener);

var f_result = f();

assertEquals("foobar", f_result);

// Make sure that the debug event listener was invoked.
assertFalse(exceptionThrown, "exception in listener");
assertTrue(listenerComplete);
