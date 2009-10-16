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
function evaluateRequest(exec_state, arguments) {
  // Get the debug command processor.
  var dcp = exec_state.debugCommandProcessor("unspecified_running_state");

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
function lookupRequest(exec_state, arguments, success) {
  // Get the debug command processor.
  var dcp = exec_state.debugCommandProcessor("unspecified_running_state");

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
  assertEquals(response.running, dcp.isRunning(), request + ' -> expected not running');

  return response;
}


function listener(event, exec_state, event_data, data) {
  try {
  if (event == Debug.DebugEvent.Break) {
    // Test some illegal lookup requests.
    lookupRequest(exec_state, void 0, false);
    lookupRequest(exec_state, '{"handles":["a"]}', false);
    lookupRequest(exec_state, '{"handles":[-1]}', false);

    // Evaluate and get some handles.
    var handle_o = evaluateRequest(exec_state, '{"expression":"o"}');
    var handle_p = evaluateRequest(exec_state, '{"expression":"p"}');
    var handle_b = evaluateRequest(exec_state, '{"expression":"a"}');
    var handle_a = evaluateRequest(exec_state, '{"expression":"b","frame":1}');
    assertEquals(handle_o, handle_a);
    assertEquals(handle_a, handle_b);
    assertFalse(handle_o == handle_p, "o and p have he same handle");

    var response;
    var count;
    response = lookupRequest(exec_state, '{"handles":[' + handle_o + ']}', true);
    var obj = response.body[handle_o];
    assertTrue(!!obj, 'Object not found: ' + handle_o);
    assertEquals(handle_o, obj.handle);
    count = 0;
    for (i in obj.properties) {
      switch (obj.properties[i].name) {
        case 'o':
          obj.properties[i].ref = handle_o;
          count++;
          break;
        case 'p':
          obj.properties[i].ref = handle_p;
          count++;
          break;
      }
    }
    assertEquals(2, count, 'Either "o" or "p" not found');
    response = lookupRequest(exec_state, '{"handles":[' + handle_p + ']}', true);
    obj = response.body[handle_p];
    assertTrue(!!obj, 'Object not found: ' + handle_p);
    assertEquals(handle_p, obj.handle);

    // Check handles for functions on the stack.
    var handle_f = evaluateRequest(exec_state, '{"expression":"f"}');
    var handle_g = evaluateRequest(exec_state, '{"expression":"g"}');
    var handle_caller = evaluateRequest(exec_state, '{"expression":"f.caller"}');

    assertFalse(handle_f == handle_g, "f and g have he same handle");
    assertEquals(handle_g, handle_caller, "caller for f should be g");

    response = lookupRequest(exec_state, '{"handles":[' + handle_f + ']}', true);
    obj = response.body[handle_f];
    assertEquals(handle_f, obj.handle);

    count = 0;
    for (i in obj.properties) {
      var ref = obj.properties[i].ref;
      var arguments = '{"handles":[' + ref + ']}';
      switch (obj.properties[i].name) {
        case 'name':
          var response_name;
          response_name = lookupRequest(exec_state, arguments, true);
          assertEquals('string', response_name.body[ref].type);
          assertEquals("f", response_name.body[ref].value);
          count++;
          break;
        case 'length':
          var response_length;
          response_length = lookupRequest(exec_state, arguments, true);
          assertEquals('number', response_length.body[ref].type);
          assertEquals(1, response_length.body[ref].value);
          count++;
          break;
        case 'caller':
          assertEquals(handle_g, obj.properties[i].ref);
          count++;
          break;
      }
    }
    assertEquals(3, count, 'Either "name", "length" or "caller" not found');


    // Resolve all at once.
    var refs = [];
    for (i in obj.properties) {
      refs.push(obj.properties[i].ref);
    }

    var arguments = '{"handles":[' + refs.join(',') + ']}';
    response = lookupRequest(exec_state, arguments, true);
    count = 0;
    for (i in obj.properties) {
      var ref = obj.properties[i].ref;
      var val = response.body[ref];
      assertTrue(!!val, 'Failed to lookup "' + obj.properties[i].name + '"');
      switch (obj.properties[i].name) {
        case 'name':
          assertEquals('string', val.type);
          assertEquals("f", val.value);
          count++;
          break;
        case 'length':
          assertEquals('number', val.type);
          assertEquals(1, val.value);
          count++;
          break;
        case 'caller':
          assertEquals('function', val.type);
          assertEquals(handle_g, ref);
          count++;
          break;
      }
    }
    assertEquals(3, count, 'Either "name", "length" or "caller" not found');

    count = 0;
    for (var handle in response.body) {
      assertTrue(refs.indexOf(parseInt(handle)) != -1,
                 'Handle not in the request: ' + handle);
      count++;
    }
    assertEquals(count, obj.properties.length, 
                 'Unexpected number of resolved objects');


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

assertFalse(exception, "exception in listener")
// Make sure that the debug event listener vas invoked.
assertTrue(listenerComplete, "listener did not run to completion: " + exception);
