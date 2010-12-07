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
var Debug = debug.Debug

// Simple function which stores the last debug event.
var listenerComplete = false;
var exception = false;

var base_request = '"seq":0,"type":"request","command":"clearbreakpointgroup"';
var scriptId = null;

function safeEval(code) {
  try {
    return eval('(' + code + ')');
  } catch (e) {
    assertEquals(void 0, e);
    return undefined;
  }
}

function testArguments(dcp, arguments, success) {
  var request = '{' + base_request + ',"arguments":' + arguments + '}'
  var json_response = dcp.processDebugJSONRequest(request);
  var response = safeEval(json_response);
  if (success) {
    assertTrue(response.success, json_response);
  } else {
    assertFalse(response.success, json_response);
  }
}

function listener(event, exec_state, event_data, data) {
  try {
    if (event == Debug.DebugEvent.Break) {
      // Get the debug command processor.
      var dcp = exec_state.debugCommandProcessor("unspecified_running_state");

      // Clear breakpoint group 1.
      testArguments(dcp, '{"groupId":1}', true);

      // Indicate that all was processed.
      listenerComplete = true;
    } else if (event == Debug.DebugEvent.AfterCompile) {
      scriptId = event_data.script().id();
      assertEquals(source, event_data.script().source());
    }
  } catch (e) {
    exception = e
  };
};


// Add the debug event listener.
Debug.setListener(listener);

var source = 'function f(n) {\nreturn n+1;\n}\nfunction g() {return f(10);}' +
             '\nvar r = g(); g;';
eval(source);

assertNotNull(scriptId);

var groupId1 = 1;
var groupId2 = 2;
// Set a break point and call to invoke the debug event listener.
var bp1 = Debug.setScriptBreakPointById(scriptId, 1, null, null, groupId1);
var bp2 = Debug.setScriptBreakPointById(scriptId, 1, null, null, groupId2);
var bp3 = Debug.setScriptBreakPointById(scriptId, 1, null, null, null);
var bp4 = Debug.setScriptBreakPointById(scriptId, 3, null, null, groupId1);
var bp5 = Debug.setScriptBreakPointById(scriptId, 4, null, null, groupId2);

assertEquals(5, Debug.scriptBreakPoints().length);

// Call function 'g' from the compiled script to trigger breakpoint.
g();

// Make sure that the debug event listener vas invoked.
assertTrue(listenerComplete,
           "listener did not run to completion: " + exception);

var breakpoints = Debug.scriptBreakPoints();
assertEquals(3, breakpoints.length);
var breakpointNumbers = breakpoints.map(
    function(scriptBreakpoint) { return scriptBreakpoint.number(); },
    breakpointNumbers);

// Check that all breakpoints from group 1 were deleted and all the
// rest are preserved.
assertEquals([bp2, bp3, bp5].sort(), breakpointNumbers.sort());

assertFalse(exception, "exception in listener");

// Clear all breakpoints to allow the test to run again (--stress-opt).
Debug.clearBreakPoint(bp2);
Debug.clearBreakPoint(bp3);
Debug.clearBreakPoint(bp5);
