// Copyright 2009 the V8 project authors. All rights reserved.
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

var listenerComplete = false;
var exception = false;

function listener(event, exec_state, event_data, data) {
  try {
    if (event == Debug.DebugEvent.Break) {
      // Get the debug command processor.
      var dcp = exec_state.debugCommandProcessor();

      var request = {
         seq: 0,
         type: 'request',
         command: 'evaluate',
         arguments: {
           expression: 'a',
           frame: 0
         }
      };
      request = JSON.stringify(request);

      var resp = dcp.processDebugJSONRequest(request);
      var response = JSON.parse(resp);
      assertTrue(response.success, 'Command failed: ' + resp);
      assertEquals('object', response.body.type);
      assertEquals('Object', response.body.className);

      // Indicate that all was processed.
      listenerComplete = true;
    }
  } catch (e) {
   exception = e
  };
};

// Add the debug event listener.
Debug.setListener(listener);

function callDebugger() {
  // Add set constructor field to a non-function value.
  var a = {constructor:true};
  debugger;
}

callDebugger();


// Make sure that the debug event listener vas invoked.
assertFalse(exception, "exception in listener")
assertTrue(listenerComplete, "listener did not run to completion");
