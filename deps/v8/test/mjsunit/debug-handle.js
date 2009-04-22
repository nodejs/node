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

listenerComplete = false;
exception = false;

function safeEval(code) {
  try {
    return eval('(' + code + ')');
  } catch (e) {
    assertEquals(void 0, e);
    return undefined;
  }
}


// Send an evaluation request and return the handle of the result.
function evaluateRequest(dcp, arguments) {
  // The base part of all evaluate requests.
  var base_request = '"seq":0,"type":"request","command":"evaluate"'

  // Generate request with the supplied arguments.
  var request;
  if (arguments) {
    request = '{' + base_request + ',"arguments":' + arguments + '}';
  } else {
    request = '{' + base_request + '}'
  }

  var response = safeEval(dcp.processDebugJSONRequest(request));
  assertTrue(response.success, request + ' -> ' + response.message);

  return response.body.handle;
}


// Send a lookup request and return the evaluated JSON response.
function lookupRequest(dcp, arguments, success) {
  // The base part of all lookup requests.
  var base_request = '"seq":0,"type":"request","command":"lookup"'
  
  // Generate request with the supplied arguments.
  var request;
  if (arguments) {
    request = '{' + base_request + ',"arguments":' + arguments + '}';
  } else {
    request = '{' + base_request + '}'
  }

  var response = safeEval(dcp.processDebugJSONRequest(request));
  if (success) {
    assertTrue(response.success, request + ' -> ' + response.message);
  } else {
    assertFalse(response.success, request + ' -> ' + response.message);
  }
  assertFalse(response.running, request + ' -> expected not running');

  return response;
}


function listener(event, exec_state, event_data, data) {
  try {
  if (event == Debug.DebugEvent.Break) {
    // Get the debug command processor.
    var dcp = exec_state.debugCommandProcessor();

    // Test some illegal lookup requests.
    lookupRequest(dcp, void 0, false);
    lookupRequest(dcp, '{"handle":"a"}', false);
    lookupRequest(dcp, '{"handle":-1}', false);

    // Evaluate and get some handles.
    var handle_o = evaluateRequest(dcp, '{"expression":"o"}');
    var handle_p = evaluateRequest(dcp, '{"expression":"p"}');
    var handle_b = evaluateRequest(dcp, '{"expression":"a"}');
    var handle_a = evaluateRequest(dcp, '{"expression":"b","frame":1}');
    assertEquals(handle_o, handle_a);
    assertEquals(handle_a, handle_b);
    assertFalse(handle_o == handle_p, "o and p have he same handle");

    var response;
    var count;
    response = lookupRequest(dcp, '{"handle":' + handle_o + '}', true);
    assertEquals(handle_o, response.body.handle);
    count = 0;
    for (i in response.body.properties) {
      switch (response.body.properties[i].name) {
        case 'o':
          response.body.properties[i].ref = handle_o;
          count++;
          break;
        case 'p':
          response.body.properties[i].ref = handle_p;
          count++;
          break;
      }
    }
    assertEquals(2, count, 'Either "o" or "p" not found');
    response = lookupRequest(dcp, '{"handle":' + handle_p + '}', true);
    assertEquals(handle_p, response.body.handle);

    // Check handles for functions on the stack.
    var handle_f = evaluateRequest(dcp, '{"expression":"f"}');
    var handle_g = evaluateRequest(dcp, '{"expression":"g"}');
    var handle_caller = evaluateRequest(dcp, '{"expression":"f.caller"}');

    assertFalse(handle_f == handle_g, "f and g have he same handle");
    assertEquals(handle_g, handle_caller, "caller for f should be g");

    response = lookupRequest(dcp, '{"handle":' + handle_f + '}', true);
    assertEquals(handle_f, response.body.handle);
    count = 0;
    for (i in response.body.properties) {
      var arguments = '{"handle":' + response.body.properties[i].ref + '}'
      switch (response.body.properties[i].name) {
        case 'name':
          var response_name;
          response_name = lookupRequest(dcp, arguments, true);
          assertEquals('string', response_name.body.type);
          assertEquals("f", response_name.body.value);
          count++;
          break;
        case 'length':
          var response_length;
          response_length = lookupRequest(dcp, arguments, true);
          assertEquals('number', response_length.body.type);
          assertEquals(1, response_length.body.value);
          count++;
          break;
        case 'caller':
          assertEquals(handle_g, response.body.properties[i].ref);
          count++;
          break;
      }
    }
    assertEquals(3, count, 'Either "name", "length" or "caller" not found');


    // Indicate that all was processed.
    listenerComplete = true;
  }
  } catch (e) {
    exception = e
  };
};

// Add the debug event listener.
Debug.setListener(listener);

function f(a) {
  debugger;
};

function g(b) {
  f(b);
};

// Set a break point at return in f and invoke g to hit the breakpoint.
Debug.setBreakPoint(f, 2, 0);
o = {};
p = {}
o.o = o;
o.p = p;
p.o = o;
p.p = p;
g(o);

// Make sure that the debug event listener vas invoked.
assertTrue(listenerComplete, "listener did not run to completion");
assertFalse(exception, "exception in listener")
