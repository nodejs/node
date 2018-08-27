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


// Make sure that the retreival of local variables are performed correctly even
// when an adapter frame is present.

Debug = debug.Debug

let listenerCalled = false;
let exceptionThrown = false;


function checkName(name) {
  const validNames = new Set([ 'a', 'b', 'c', 'x', 'y' ]);
  assertTrue(validNames.has(name));
}


function checkValue(value) {
  assertEquals(void 0, value);
}


function listener(event, exec_state, event_data, data) {
  try {
    if (event == Debug.DebugEvent.Break) {
      var local0Name = exec_state.frame(0).localName(0);
      var local1Name = exec_state.frame(0).localName(1);
      var local2Name = exec_state.frame(0).localName(2);
      checkName(local0Name);
      checkName(local1Name);
      checkName(local2Name);
      var local0Value = exec_state.frame(0).localValue(0).value();
      var local1Value = exec_state.frame(0).localValue(1).value();
      var local2Value = exec_state.frame(0).localValue(2).value();
      checkValue(local0Value);
      checkValue(local1Value);
      checkValue(local2Value);
      listenerCalled = true;
    }
  } catch (e) {
    exceptionThrown = true;
  };
};

// Add the debug event listener.
Debug.setListener(listener);

// Call a function with local variables passing a different number parameters
// that the number of arguments.
(function(x,y){var a,b,c; debugger; return 3})()

// Make sure that the debug event listener vas invoked (again).
assertTrue(listenerCalled);
assertFalse(exceptionThrown, "exception in listener")
