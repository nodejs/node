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

// Flags: --expose-debug-as debug
// Flags: --turbo-deoptimization

// Test that a variable in the local scope that shadows a context-allocated
// variable is correctly resolved when being evaluated in the debugger.

Debug = debug.Debug;

var exception = false;

function listener(event, exec_state, event_data, data) {
  if (event != Debug.DebugEvent.Break) return;
  var breakpoint = exec_state.frame(0);
  try {
    // Assert correct break point.
    assertTrue(breakpoint.sourceLineText().indexOf("// Break") > -1);
    // Assert correct value.
    assertEquals(3, breakpoint.evaluate('x').value());
  } catch (e) {
    exception = e;
  }
}

Debug.setListener(listener);

function h() {
  var x;  // Context-allocated due to g().

  var g = function g() {
    x = -7;
  };

  var f = function f() {
    var x = 3;  // Allocated in the local scope.
    debugger;  // Break.
  };

  f();
}

h();

assertFalse(exception);
