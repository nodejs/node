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

var exception = null;
var state = 0;

// Simple debug event handler which first time will cause 'step in' action
// to get into g.call and than check that execution is pauesed inside
// function 'g'.
function listener(event, exec_state, event_data, data) {
  try {
    if (event == Debug.DebugEvent.Break) {
      if (state == 0) {
        // Step into f2.call:
        exec_state.prepareStep(Debug.StepAction.StepIn, 2);
        state = 2;
      } else if (state == 2) {
        assertEquals('g', event_data.func().name());
        assertEquals('  return t + 1; // expected line',
                     event_data.sourceLineText());
        state = 3;
      }
    }
  } catch(e) {
    exception = e;
  }
};

// Add the debug event listener.
Debug.setListener(listener);


// Sample functions.
function g(t) {
  return t + 1; // expected line
}

// Test step into function call from a function without local variables.
function call1() {
  debugger;
  g.call(null, 3);
}


// Test step into function call from a function with some local variables.
function call2() {
  var aLocalVar = 'test';
  var anotherLocalVar  = g(aLocalVar) + 's';
  var yetAnotherLocal = 10;
  debugger;
  g.call(null, 3);
}

// Test step into function call which is a part of an expression.
function call3() {
  var alias = g;
  debugger;
  var r = 10 + alias.call(null, 3);
  var aLocalVar = 'test';
  var anotherLocalVar  = g(aLocalVar) + 's';
  var yetAnotherLocal = 10;
}

// Test step into function call from a function with some local variables.
function call4() {
  var alias = g;
  debugger;
  alias.call(null, 3);
  var aLocalVar = 'test';
  var anotherLocalVar  = g(aLocalVar) + 's';
  var yetAnotherLocal = 10;
}

// Test step into function apply from a function without local variables.
function apply1() {
  debugger;
  g.apply(null, [3]);
}


// Test step into function apply from a function with some local variables.
function apply2() {
  var aLocalVar = 'test';
  var anotherLocalVar  = g(aLocalVar) + 's';
  var yetAnotherLocal = 10;
  debugger;
  g.apply(null, [3, 4]);
}

// Test step into function apply which is a part of an expression.
function apply3() {
  var alias = g;
  debugger;
  var r = 10 + alias.apply(null, [3, 'unused arg']);
  var aLocalVar = 'test';
  var anotherLocalVar  = g(aLocalVar) + 's';
  var yetAnotherLocal = 10;
}

// Test step into function apply from a function with some local variables.
function apply4() {
  var alias = g;
  debugger;
  alias.apply(null, [3]);
  var aLocalVar = 'test';
  var anotherLocalVar  = g(aLocalVar) + 's';
  var yetAnotherLocal = 10;
}

// Test step into bound function.
function bind1() {
  var bound = g.bind(null, 3);
  debugger;
  bound();
}

// Test step into apply of bound function.
function applyAndBind1() {
  var bound = g.bind(null, 3);
  debugger;
  bound.apply(null, [3]);
  var aLocalVar = 'test';
  var anotherLocalVar  = g(aLocalVar) + 's';
  var yetAnotherLocal = 10;
}

var testFunctions =
    [call1, call2, call3, call4, apply1, apply2, apply3, apply4, bind1,
    applyAndBind1];

for (var i = 0; i < testFunctions.length; i++) {
  state = 0;
  testFunctions[i]();
  assertNull(exception);
  assertEquals(3, state);
}

// Test global bound function.
state = 0;
var globalBound = g.bind(null, 3);
debugger;
globalBound();
assertNull(exception);
assertEquals(3, state);

// Get rid of the debug event listener.
Debug.setListener(null);
