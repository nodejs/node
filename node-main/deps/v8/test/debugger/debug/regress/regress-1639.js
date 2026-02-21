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

Debug = debug.Debug
var breaks = 0;
var exceptionThrown = false;

function listener(event, exec_state, event_data, data) {
  try {
    if (event == Debug.DebugEvent.Break) {
      var line = event_data.sourceLineText();
      print('break: ' + line);

      assertEquals(-1, line.indexOf('NOBREAK'),
                   "should not break on unexpected lines")
      assertEquals('BREAK ' + breaks, line.substr(-7));
      breaks++;
      if (breaks < 4) exec_state.prepareStep(Debug.StepAction.StepOver);
    }
  } catch (e) {
    print(e);
    exceptionThrown = true;
  }
}

// Add the debug event listener.
Debug.setListener(listener);

function a(f) {
  if (f) {  // NOBREAK: should not break here!
    try {
      f();
    } catch(e) {
    }
  }
}  // BREAK 2

function b() {
  c();  // BREAK 0
}  // BREAK 1

function c() {
  a();
}

// Set a break point and call to invoke the debug event listener.
Debug.setBreakPoint(b, 0, 0);
a(b);
a(); // BREAK 3

assertFalse(exceptionThrown);
