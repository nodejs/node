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

// Flags: --expose-debug-as debug --allow-natives-syntax --turbo-deoptimization

// This test tests that deoptimization due to debug breaks works for
// inlined functions where the full-code is generated before the
// debugger is attached.
//
//See http://code.google.com/p/chromium/issues/detail?id=105375

// Get the Debug object exposed from the debug context global object.
Debug = debug.Debug;

var count = 0;
var break_count = 0;

// Debug event listener which sets a breakpoint first time it is hit
// and otherwise counts break points hit and checks that the expected
// state is reached.
function listener(event, exec_state, event_data, data) {
  if (event == Debug.DebugEvent.Break) {
    break_count++;
    if (break_count == 1) {
      Debug.setBreakPoint(g, 3);

      for (var i = 0; i < exec_state.frameCount(); i++) {
        var frame = exec_state.frame(i);
        // When function f is optimized (1 means YES, see runtime.cc) we
        // expect an optimized frame for f and g.
        if (%GetOptimizationStatus(f) == 1) {
          if (i == 1) {
            assertTrue(frame.isOptimizedFrame());
            assertTrue(frame.isInlinedFrame());
            assertEquals(4 - i, frame.inlinedFrameIndex());
          } else if (i == 2) {
            assertTrue(frame.isOptimizedFrame());
            assertFalse(frame.isInlinedFrame());
          } else {
            assertFalse(frame.isOptimizedFrame());
            assertFalse(frame.isInlinedFrame());
          }
        }
      }
    }
  }
}

function f() {
  g();
}

function g() {
  count++;
  h();
  var b = 1;  // Break point is set here.
}

function h() {
  debugger;
}

f();f();f();
%OptimizeFunctionOnNextCall(f);
f();

// Add the debug event listener.
Debug.setListener(listener);

f();

assertEquals(5, count);
assertEquals(2, break_count);

// Get rid of the debug event listener.
Debug.setListener(null);
