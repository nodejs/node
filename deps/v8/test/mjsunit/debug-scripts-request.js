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

// State to check that the listener code was invoked and that no exceptions
// occoured.
listenerComplete = false;
exception = false;

var base_request = '"seq":0,"type":"request","command":"scripts"'

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

    // Test illegal scripts requests.
    testArguments(dcp, '{"types":"xx"}', false);

    // Test legal scripts requests.
    testArguments(dcp, '{}', true);
    testArguments(dcp, '{"types":1}', true);
    testArguments(dcp, '{"types":2}', true);
    testArguments(dcp, '{"types":4}', true);
    testArguments(dcp, '{"types":7}', true);
    testArguments(dcp, '{"types":255}', true);

    // Test request for all scripts.
    var request = '{' + base_request + '}'
    var response = safeEval(dcp.processDebugJSONRequest(request));
    assertTrue(response.success);

    // Test filtering by id.
    assertEquals(2, response.body.length);
    var script = response.body[0];
    var request = '{' + base_request + ',"arguments":{"ids":[' +
                  script.id + ']}}';
    var response = safeEval(dcp.processDebugJSONRequest(request));
    assertTrue(response.success);
    assertEquals(1, response.body.length);
    assertEquals(script.id, response.body[0].id);

    // Indicate that all was processed.
    listenerComplete = true;
  }
  } catch (e) {
    exception = e
  };
};

// Add the debug event listener.
Debug.setListener(listener);

// Call debugger to invoke the debug event listener.
debugger;

// Make sure that the debug event listener vas invoked with no exceptions.
assertTrue(listenerComplete,
           "listener did not run to completion, exception: " + exception);
assertFalse(exception, "exception in listener")
