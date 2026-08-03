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

// Simple function which stores the last debug event.
lastDebugEvent = new Object();
function listener(event, exec_state, event_data, data) {
  if (event == Debug.DebugEvent.Break ||
      event == Debug.DebugEvent.Exception)
  {
    lastDebugEvent.event = event;
    lastDebugEvent.frameFuncName = exec_state.frame().func().name();
    lastDebugEvent.event_data = event_data;
  }
};

// Add the debug event listener.
Debug.setListener(listener);
// Get events from handled exceptions.
Debug.setBreakOnException();

// Test debug event for handled exception.
(function f(){
  try {
    x();
  } catch(e) {
    // Do nothing. Ignore exception.
  }
})();
assertTrue(lastDebugEvent.event == Debug.DebugEvent.Exception);
assertEquals(lastDebugEvent.frameFuncName, "f");
assertFalse(lastDebugEvent.event_data.uncaught());
Debug.clearBreakOnException();

// Test debug event for break point.
function a() {
  x = 1;
  y = 2;
  z = 3;
};
Debug.setBreakPoint(a, 1);
a();
assertTrue(lastDebugEvent.event == Debug.DebugEvent.Break);
assertEquals(lastDebugEvent.frameFuncName, "a");

Debug.setListener(null);
