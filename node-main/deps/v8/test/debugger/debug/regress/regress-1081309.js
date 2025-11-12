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

// Make sure that the backtrace command can be processed when the receiver is
// undefined.
let listenerCalled = false;
let exceptionThrown = false;

function listener(event, exec_state, event_data, data) {
  try {
    if (event == Debug.DebugEvent.Exception) {
      // The expected backtrace is
      // 1: g
      // 0: [anonymous]

      assertEquals(2, exec_state.frameCount());
      assertEquals("g", exec_state.frame(0).func().name());
      assertEquals("", exec_state.frame(1).func().name());

      listenerCalled = true;
    }
  } catch (e) {
    print(e);
    exceptionThrown = true;
  };
};

// Add the debug event listener.
Debug.setListener(listener);

// Call method on undefined.
function g() {
  (void 0).f();
};

// Break on the exception to do a backtrace with undefined as receiver.
Debug.setBreakOnException(true);
try {
  g();
} catch(e) {
  // Ignore the exception "Cannot call method 'x' of undefined"
}

assertFalse(exceptionThrown, "exception in listener");
// Make sure that the debug event listener vas invoked.
assertTrue(listenerCalled, "listener not called");
