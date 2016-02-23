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

// Simple function which stores the last debug event.
listenerComplete = false;
exception = false;

var base_request = '"seq":0,"type":"request","command":"continue"'

function safeEval(code) {
  try {
    return eval('(' + code + ')');
  } catch (e) {
    assertEquals(void 0, e);
    return undefined;
  }
}

function testArguments(exec_state, arguments, success) {
  // Get the debug command processor in paused state.
  var dcp = exec_state.debugCommandProcessor(false);

  // Generate request with the supplied arguments
  var request;
  if (arguments) {
    request = '{' + base_request + ',"arguments":' + arguments + '}';
  } else {
    request = '{' + base_request + '}'
  }
  var response = safeEval(dcp.processDebugJSONRequest(request));
  if (success) {
    assertTrue(response.success, request + ' -> ' + response.message);
    assertTrue(response.running, request + ' -> expected running');
  } else {
    assertFalse(response.success, request + ' -> ' + response.message);
    assertFalse(response.running, request + ' -> expected not running');
  }
}

function listener(event, exec_state, event_data, data) {
  try {
  if (event == Debug.DebugEvent.Break) {

    // Test simple continue request.
    testArguments(exec_state, void 0, true);

    // Test some illegal continue requests.
    testArguments(exec_state, '{"stepaction":"maybe"}', false);
    testArguments(exec_state, '{"stepcount":-1}', false);

    // Test some legal continue requests.
    testArguments(exec_state, '{"stepaction":"in"}', true);
    testArguments(exec_state, '{"stepaction":"min"}', true);
    testArguments(exec_state, '{"stepaction":"next"}', true);
    testArguments(exec_state, '{"stepaction":"out"}', true);
    testArguments(exec_state, '{"stepcount":1}', true);
    testArguments(exec_state, '{"stepcount":10}', true);
    testArguments(exec_state, '{"stepcount":"10"}', true);
    testArguments(exec_state, '{"stepaction":"next","stepcount":10}', true);

    // Indicate that all was processed.
    listenerComplete = true;
  }
  } catch (e) {
    exception = e
  };
};

// Add the debug event listener.
Debug.setListener(listener);

function f() {
  a=1
};

function g() {
  f();
};

// Set a break point and call to invoke the debug event listener.
Debug.setBreakPoint(g, 0, 0);
g();

assertFalse(exception, "exception in listener")
// Make sure that the debug event listener vas invoked.
assertTrue(listenerComplete, "listener did not run to completion");
