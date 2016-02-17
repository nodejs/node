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

function checkArguments(frame, names, values) {
  var argc = Math.max(names.length, values.length);
  assertEquals(argc, frame.argumentCount());
  for (var i = 0; i < argc; i++) {
    if (i < names.length) {
      assertEquals(names[i], frame.argumentName(i));
    } else {
      assertEquals(void 0, frame.argumentName(i));
    }

    if (i < values.length) {
      assertEquals(values[i], frame.argumentValue(i).value());
    } else {
      assertEquals(void 0, frame.argumentValue(i).value());
    }
  }
}

function listener(event, exec_state, event_data, data) {
  try {
    if (event == Debug.DebugEvent.Break)
    {
      // Frame 0 - called with less parameters than arguments.
      checkArguments(exec_state.frame(0), ['x', 'y'], [1]);

      // Frame 1 - called with more parameters than arguments.
      checkArguments(exec_state.frame(1), ['x', 'y'], [1, 2, 3]);

      // Frame 2 - called with same number of parameters than arguments.
      checkArguments(exec_state.frame(2), ['x', 'y', 'z'], [1, 2, 3]);

      // Indicate that all was processed.
      listenerComplete = true;
    }
  } catch (e) {
    exception = e
  };
};

// Add the debug event listener.
Debug.setListener(listener);

function h(x, y) {
  debugger;  // Breakpoint.
};

function g(x, y) {
  h(x);
};

function f(x, y, z) {
  g.apply(null, [x, y, z]);
};

f(1, 2, 3);

// Make sure that the debug event listener vas invoked.
assertTrue(listenerComplete);
assertFalse(exception, "exception in listener")
