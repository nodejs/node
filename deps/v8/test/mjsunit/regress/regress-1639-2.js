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
// Get the Debug object exposed from the debug context global object.
Debug = debug.Debug
var exception = false;

function sendCommand(state, cmd) {
  // Get the debug command processor in paused state.
  var dcp = state.debugCommandProcessor(false);
  var request = JSON.stringify(cmd);
  var response = dcp.processDebugJSONRequest(request);
}

var state = 0;

function listener(event, exec_state, event_data, data) {
  try {
    if (event == Debug.DebugEvent.Break) {
      var line = event_data.sourceLineText();
      print('break: ' + line);
      print('event data: ' + event_data.toJSONProtocol());
      print();
      assertEquals('// BREAK', line.substr(-8),
                   "should not break outside evaluate");

      switch (state) {
      case 0:
        state = 1;
        // While in the debugger and stepping through a set of instructions
        // executed in the evaluate command, the stepping must stop at the end
        // of the said set of instructions and not step further into native
        // debugger code.
        sendCommand(exec_state, {
          seq : 0,
          type : "request",
          command : "evaluate",
          arguments : {
            'expression' : 'print("A"); debugger; print("B"); // BREAK',
            'global' : true
          }
        });
        break;
      case 1:
        sendCommand(exec_state, {
          seq : 0,
          type : "request",
          command : "continue",
          arguments : {
            stepaction : "next"
          }
        });
        break;
      }
    }
  } catch (e) {
    print(e);
    exception = true;
  }
}

// Add the debug event listener.
Debug.setListener(listener);

function a() {
} // BREAK

// Set a break point and call to invoke the debug event listener.
Debug.setBreakPoint(a, 0, 0);
a();
assertFalse(exception);
