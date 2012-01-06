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

var listenerComplete = false;
var exception = false;

var testingConstructCall = false;


function listener(event, exec_state, event_data, data) {
  try {
    if (event == Debug.DebugEvent.Break)
    {
      assertEquals(6, exec_state.frameCount());

      for (var i = 0; i < exec_state.frameCount(); i++) {
        var frame = exec_state.frame(i);
        if (i < exec_state.frameCount() - 1) {
          var expected_a = i * 2 + 1 + (i * 2 + 1) / 100;
          var expected_b = i * 2 + 2 + (i * 2 + 2) / 100;
          var expected_x = (i + 1) * 2 + 1 + ((i + 1) * 2 + 1) / 100;
          var expected_y = (i + 1) * 2 + 2 + ((i + 1) * 2 + 2) / 100;

          // All frames except the bottom one has normal variables a and b.
          var a = ('a' === frame.localName(0)) ? 0 : 1;
          var b = 1 - a;
          assertEquals('a', frame.localName(a));
          assertEquals('b', frame.localName(b));
          assertEquals(expected_a, frame.localValue(a).value());
          assertEquals(expected_b, frame.localValue(b).value());

          // All frames except the bottom one has arguments variables x and y.
          assertEquals('x', frame.argumentName(0));
          assertEquals('y', frame.argumentName(1));
          assertEquals(expected_x, frame.argumentValue(0).value());
          assertEquals(expected_y, frame.argumentValue(1).value());

          // All frames except the bottom one have two scopes.
          assertEquals(2, frame.scopeCount());
          assertEquals(debug.ScopeType.Local, frame.scope(0).scopeType());
          assertEquals(debug.ScopeType.Global, frame.scope(1).scopeType());
          assertEquals(expected_a, frame.scope(0).scopeObject().value()['a']);
          assertEquals(expected_b, frame.scope(0).scopeObject().value()['b']);
          assertEquals(expected_x, frame.scope(0).scopeObject().value()['x']);
          assertEquals(expected_y, frame.scope(0).scopeObject().value()['y']);

          // Evaluate in the inlined frame.
          assertEquals(expected_a, frame.evaluate('a').value());
          assertEquals(expected_x, frame.evaluate('x').value());
          assertEquals(expected_x, frame.evaluate('arguments[0]').value());
          assertEquals(expected_a + expected_b + expected_x + expected_y,
                       frame.evaluate('a + b + x + y').value());
          assertEquals(expected_x + expected_y,
                       frame.evaluate('arguments[0] + arguments[1]').value());
        } else {
          // The bottom frame only have the global scope.
          assertEquals(1, frame.scopeCount());
          assertEquals(debug.ScopeType.Global, frame.scope(0).scopeType());
        }

        // Check the frame function.
        switch (i) {
          case 0: assertEquals(h, frame.func().value()); break;
          case 1: assertEquals(g3, frame.func().value()); break;
          case 2: assertEquals(g2, frame.func().value()); break;
          case 3: assertEquals(g1, frame.func().value()); break;
          case 4: assertEquals(f, frame.func().value()); break;
          case 5: break;
          default: assertUnreachable();
        }

        // Check for construct call.
        assertEquals(testingConstructCall && i == 4, frame.isConstructCall());

        // When function f is optimized (1 means YES, see runtime.cc) we
        // expect an optimized frame for f with g1, g2 and g3 inlined.
        if (%GetOptimizationStatus(f) == 1) {
          if (i == 1 || i == 2 || i == 3) {
            assertTrue(frame.isOptimizedFrame());
            assertTrue(frame.isInlinedFrame());
            assertEquals(4 - i, frame.inlinedFrameIndex());
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
  a = a + a / 100;
  b = b + b / 100;
  debugger;  // Breakpoint.
};

function g3(x, y) {
  var a = 3;
  var b = 4;
  a = a + a / 100;
  b = b + b / 100;
  h(a, b);
  return a+b;
};

function g2(x, y) {
  var a = 5;
  var b = 6;
  a = a + a / 100;
  b = b + b / 100;
  g3(a, b);
};

function g1(x, y) {
  var a = 7;
  var b = 8;
  a = a + a / 100;
  b = b + b / 100;
  g2(a, b);
};

function f(x, y) {
  var a = 9;
  var b = 10;
  a = a + a / 100;
  b = b + b / 100;
  g1(a, b);
};

// Test calling f normally and as a constructor.
f(11.11, 12.12);
testingConstructCall = true;
new f(11.11, 12.12);

// Make sure that the debug event listener vas invoked.
assertFalse(exception, "exception in listener " + exception)
assertTrue(listenerComplete);

Debug.setListener(null);
