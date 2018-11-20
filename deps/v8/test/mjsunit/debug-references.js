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

// Flags: --expose-debug-as debug --turbo-deoptimization
// Get the Debug object exposed from the debug context global object.
Debug = debug.Debug

listenerComplete = false;
exception = false;

// The base part of all evaluate requests.
var base_request = '"seq":0,"type":"request","command":"references"'

function safeEval(code) {
  try {
    return eval('(' + code + ')');
  } catch (e) {
    assertEquals(void 0, e);
    return undefined;
  }
}

function testRequest(dcp, arguments, success, count) {
  // Generate request with the supplied arguments.
  var request;
  if (arguments) {
    request = '{' + base_request + ',"arguments":' + arguments + '}';
  } else {
    request = '{' + base_request + '}'
  }

  // Process the request and check expectation.
  var response = safeEval(dcp.processDebugJSONRequest(request));
  if (success) {
    assertTrue(response.success, request + ' -> ' + response.message);
    assertTrue(response.body instanceof Array);
    if (count) {
      assertEquals(count, response.body.length);
    } else {
      assertTrue(response.body.length > 0);
    }
  } else {
    assertFalse(response.success, request + ' -> ' + response.message);
  }
  assertEquals(response.running, dcp.isRunning(), request + ' -> expected not running');
}

function listener(event, exec_state, event_data, data) {
  try {
  if (event == Debug.DebugEvent.Break) {
    // Get the debug command processor.
    var dcp = exec_state.debugCommandProcessor("unspecified_running_state");

    // Test some illegal references requests.
    testRequest(dcp, void 0, false);
    testRequest(dcp, '{"handle":"a"}', false);
    testRequest(dcp, '{"handle":1}', false);
    testRequest(dcp, '{"type":"referencedBy"}', false);
    testRequest(dcp, '{"type":"constructedBy"}', false);

    // Evaluate Point.
    var evaluate_point = '{"seq":0,"type":"request","command":"evaluate",' +
                         '"arguments":{"expression":"Point"}}';
    var response = safeEval(dcp.processDebugJSONRequest(evaluate_point));
    assertTrue(response.success, "Evaluation of Point failed");
    var handle = response.body.handle;

    // Test some legal references requests.
    testRequest(dcp, '{"handle":' + handle + ',"type":"referencedBy"}', true);
    testRequest(dcp, '{"handle":' + handle + ',"type":"constructedBy"}',
                true, 2);

    // Indicate that all was processed.
    listenerComplete = true;
  }
  } catch (e) {
    exception = e
  };
};

// Add the debug event listener.
Debug.setListener(listener);

// Test constructor and objects.
function Point(x, y) { this.x_ = x; this.y_ = y;}
p = new Point(0,0);
q = new Point(1,2);

// Enter debugger causing the event listener to be called.
debugger;

// Make sure that the debug event listener was invoked.
assertFalse(exception, "exception in listener")
assertTrue(listenerComplete, "listener did not run to completion");
