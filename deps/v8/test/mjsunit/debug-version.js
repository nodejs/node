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

var base_version_request = '"seq":0,"type":"request","command":"version"'

function safeEval(code) {
  try {
    return eval('(' + code + ')');
  } catch (e) {
    assertEquals(void 0, e);
    return undefined;
  }
}

function testArguments(exec_state) {
  // Get the debug command processor in running state.
  var dcp = exec_state.debugCommandProcessor(true);

  assertTrue(dcp.isRunning());

  var version_request = '{' + base_version_request + '}'
  var version_response = safeEval(dcp.processDebugJSONRequest(version_request));

  assertTrue(version_response.success);

  var version_string = version_response.body.V8Version;

  assertTrue(!!version_string, version_request + ' -> expected version string');

  var version_pattern = /^\d*\.\d*\.\d*/;

  assertTrue(!!(version_string.match(version_pattern)), "unexpected format of version: " + version_string);
}

function listener(event, exec_state, event_data, data) {
  try {
    if (event == Debug.DebugEvent.Break) {

      // Test simple suspend request.
      testArguments(exec_state);

      // Indicate that all was processed.
      listenerComplete = true;
    }
  } catch (e) {
    exception = e
  };
};

// Add the debug event listener.
Debug.setListener(listener);

// Stop debugger and check that suspend command changes running flag.
debugger;

assertFalse(exception, "exception in listener")
// Make sure that the debug event listener vas invoked.
assertTrue(listenerComplete, "listener did not run to completion");
