// Copyright 2011 the V8 project authors. All rights reserved.
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


// Test debug evaluation for functions without local context, but with
// nested catch contexts.

"use strict";

var x;
var result;

function f() {
  {                   // Line 1.
    let i = 1;        // Line 2.
    try {             // Line 3.
      throw 'stuff';  // Line 4.
    } catch (e) {     // Line 5.
      x = 2;          // Line 6.
    }
  }
};

var Debug = debug.Debug

function listener(event, exec_state, event_data, data) {
  if (event == Debug.DebugEvent.Break) {
    result = exec_state.frame().evaluate("i").value();
  }
};

// Add the debug event listener.
Debug.setListener(listener);

//Set breakpoint on line 6.
var bp = Debug.setBreakPoint(f, 6);

result = -1;
f();
assertEquals(1, result);

// Clear breakpoint.
Debug.clearBreakPoint(bp);
// Get rid of the debug event listener.
Debug.setListener(null);


function f1() {
  {
    let i = 1;
    debugger;
    assertEquals(2, i);
  }
}

function f2() {
  {
    let i = 1;
    debugger;
    assertEquals(2, i);
    return function() { return i++; }
  }
}

var exception;
Debug.setListener(function (event, exec_state, event_data, data) {
  try {
    if (event == Debug.DebugEvent.Break) {
      var frame = exec_state.frame();
      assertEquals(1, frame.evaluate("i").value());
      var allScopes = frame.allScopes();
      assertEquals(1, allScopes[0].scopeObject().value().i);
      allScopes[0].setVariableValue("i", 2);
    }
  } catch (e) {
    exception = e;
  }
});

exception = null;
f1();
assertEquals(null, exception, exception);
exception = null;
f2();
assertEquals(null, exception, exception);

Debug.setListener(null);
