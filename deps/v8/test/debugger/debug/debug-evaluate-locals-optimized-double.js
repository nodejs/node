// Copyright 2012 the V8 project authors. All rights reserved.
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

// Flags: --expose-gc

Debug = debug.Debug

var listenerComplete = false;
var exceptionThrown = false;

var testingConstructCall = false;

var input = [
  {a: 1, b: 2},
  {a: 3, b: 4},
  {a: 5, b: 6},
  {a: 7, b: 8},
  {a: 9, b: 10}
];

var expected = [
  { locals: {i: 0, x0: 3.03, y0: 4.04, a0: 1.01, b0: 2.02},
    args: { names: ["i", "x0", "y0"], values: [0, 3.03, 4.04] } },
  { locals: {i: 1, x1: 5.05, y1: 6.06, a1: 3.03, b1: 4.04},
    args: { names: ["i", "x1", "y1"], values: [1, 5.05, 6.06] } },
  { locals: {i: 2, a2: 5.05, b2: 6.06},
    args: { names: ["i"], values: [2] } },
  { locals: {i: 3, x3: 9.09, y3: 10.10, z3: undefined, a3: 7.07, b3: 8.08},
    args: { names: ["i", "x3", "y3", "z3"],
            values: [3, 9.09, 10.10, undefined] } },
  { locals: {i: 4, x4: 11.11, y4: 12.12, a4: 9.09, b4: 10.10},
    args: { names: ["i", "x4", "y4"], values: [4, 11.11, 12.12] } }
];

function listener(event, exec_state, event_data, data) {
  try {
    if (event == Debug.DebugEvent.Break)
    {
      assertEquals(6, exec_state.frameCount());

      for (var i = 0; i < exec_state.frameCount(); i++) {
        var frame = exec_state.frame(i);
        if (i < exec_state.frameCount() - 1) {
          var expected_args = expected[i].args;
          var expected_locals = expected[i].locals;

          // All frames except the bottom one have expected locals.
          var locals = {};
          for (var j = 0; j < frame.localCount(); j++) {
            locals[frame.localName(j)] = frame.localValue(j).value();
          }
          assertPropertiesEqual(expected_locals, locals);

          // All frames except the bottom one have two scopes.
          assertEquals(3, frame.scopeCount());
          assertEquals(debug.ScopeType.Local, frame.scope(0).scopeType());
          assertEquals(debug.ScopeType.Script, frame.scope(1).scopeType());
          assertEquals(debug.ScopeType.Global, frame.scope(2).scopeType());

          Object.keys(expected_locals).forEach(function (name) {
            assertEquals(expected_locals[name],
                         frame.scope(0).scopeObject().value()[name]);
          });

          for (var j = 0; j < expected_args.names.length; j++) {
            var arg_name = expected_args.names[j];
            var arg_value = expected_args.values[j];
            assertEquals(arg_value,
                         frame.scope(0).scopeObject().value()[arg_name]);
          }

          // Evaluate in the inlined frame.
          Object.keys(expected_locals).forEach(function (name) {
            assertEquals(expected_locals[name], frame.evaluate(name).value());
          });

          for (var j = 0; j < expected_args.names.length; j++) {
            var arg_name = expected_args.names[j];
            var arg_value = expected_args.values[j];
            assertEquals(arg_value, frame.evaluate(arg_name).value());
            assertEquals(arg_value, frame.evaluate('arguments['+j+']').value());
          }
        } else {
          // The bottom frame only have the global scope.
          assertEquals(2, frame.scopeCount());
          assertEquals(debug.ScopeType.Script, frame.scope(0).scopeType());
          assertEquals(debug.ScopeType.Global, frame.scope(1).scopeType());
        }

        // Check the frame function.
        switch (i) {
          case 0: assertEquals("h", frame.func().name()); break;
          case 1: assertEquals("g3", frame.func().name()); break;
          case 2: assertEquals("g2", frame.func().name()); break;
          case 3: assertEquals("g1", frame.func().name()); break;
          case 4: assertEquals("f", frame.func().name()); break;
          case 5: break;
          default: assertUnreachable();
        }
      }

      // Indicate that all was processed.
      listenerComplete = true;
    }
  } catch (e) {
    exceptionThrown = true;
  };
};

%PrepareFunctionForOptimization(f);
for (var i = 0; i < 4; i++) f(input.length - 1, 11.11, 12.12);
%OptimizeFunctionOnNextCall(f);
f(input.length - 1, 11.11, 12.12);

// Add the debug event listener.
Debug.setListener(listener);

function h(i, x0, y0) {
  var a0 = input[i].a;
  var b0 = input[i].b;
  a0 = a0 + a0 / 100;
  b0 = b0 + b0 / 100;
  debugger;  // Breakpoint.
  return a0 + b0;
};

function g3(i, x1, y1) {
  var a1 = input[i].a;
  var b1 = input[i].b;
  a1 = a1 + a1 / 100;
  b1 = b1 + b1 / 100;
  h(i - 1, a1, b1);
  return a1 + b1;
};

function g2(i) {
  var a2 = input[i].a;
  var b2 = input[i].b;
  a2 = a2 + a2 / 100;
  b2 = b2 + b2 / 100;
  g3(i - 1, a2, b2);
  return a2 + b2;
};

function g1(i, x3, y3, z3) {
  var a3 = input[i].a;
  var b3 = input[i].b;
  a3 = a3 + a3 / 100;
  b3 = b3 + b3 / 100;
  new g2(i - 1, a3, b3);
  return a3 + b3;
};

function f(i, x4, y4) {
  var a4 = input[i].a;
  var b4 = input[i].b;
  a4 = a4 + a4 / 100;
  b4 = b4 + b4 / 100;
  g1(i - 1, a4, b4);
  return a4 + b4;
};

// Test calling f normally and as a constructor.
f(input.length - 1, 11.11, 12.12);
f(input.length - 1, 11.11, 12.12, "");
testingConstructCall = true;
new f(input.length - 1, 11.11, 12.12);
new f(input.length - 1, 11.11, 12.12, "");

// Make sure that the debug event listener was invoked.
assertFalse(exceptionThrown, "exception in listener");
assertTrue(listenerComplete);

//Throw away type information for next run.
gc();

Debug.setListener(null);
