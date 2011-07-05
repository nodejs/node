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

// Flags: --expose-debug-as debug --allow-natives-syntax
// Get the Debug object exposed from the debug context global object.
Debug = debug.Debug

listenerComplete = false;
exception = false;


function listener(event, exec_state, event_data, data) {
  try {
    if (event == Debug.DebugEvent.Break)
    {
      assertEquals(6, exec_state.frameCount());

      for (var i = 0; i < exec_state.frameCount(); i++) {
        var frame = exec_state.frame(i);
        // All frames except the bottom one has normal variables a and b.
        if (i < exec_state.frameCount() - 1) {
          assertEquals('a', frame.localName(0));
          assertEquals('b', frame.localName(1));
          assertEquals(i * 2 + 1, frame.localValue(0).value());
          assertEquals(i * 2 + 2, frame.localValue(1).value());
        }

        // When function f is optimized (2 means YES, see runtime.cc) we
        // expect an optimized frame for f with g1, g2 and g3 inlined.
        if (%GetOptimizationStatus(f) == 2) {
          if (i == 1 || i == 2 || i == 3) {
            assertTrue(frame.isOptimizedFrame());
            assertTrue(frame.isInlinedFrame());
          } else if (i == 4) {
            assertTrue(frame.isOptimizedFrame());
            assertFalse(frame.isInlinedFrame());
          } else {
            assertFalse(frame.isOptimizedFrame());
            assertFalse(frame.isInlinedFrame());
          }
        }
      }

      // Indicate that all was processed.
      listenerComplete = true;
    }
  } catch (e) {
    exception = e
  };
};

f();f();f();
%OptimizeFunctionOnNextCall(f);
f();

// Add the debug event listener.
Debug.setListener(listener);

function h(x, y) {
  var a = 1;
  var b = 2;
  debugger;  // Breakpoint.
};

function g3(x, y) {
  var a = 3;
  var b = 4;
  h(a, b);
};

function g2(x, y) {
  var a = 5;
  var b = 6;
  g3(a, b);
};

function g1(x, y) {
  var a = 7;
  var b = 8;
  g2(a, b);
};

function f(x, y) {
  var a = 9;
  var b = 10;
  g1(a, b);
};

f(11, 12);

// Make sure that the debug event listener vas invoked.
assertFalse(exception, "exception in listener " + exception)
assertTrue(listenerComplete);

Debug.setListener(null);
