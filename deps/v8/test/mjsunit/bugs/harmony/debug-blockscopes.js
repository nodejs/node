// Copyright 2011 the V8 project authors. All rights reserved.
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

// Flags: --expose-debug-as debug --harmony-scoping
// The functions used for testing backtraces. They are at the top to make the
// testing of source line/column easier.


// Get the Debug object exposed from the debug context global object.
Debug = debug.Debug;

var test_name;
var listener_delegate;
var listener_called;
var exception;
var begin_test_count = 0;
var end_test_count = 0;
var break_count = 0;


// Debug event listener which delegates.
function listener(event, exec_state, event_data, data) {
  try {
    if (event == Debug.DebugEvent.Break) {
      break_count++;
      listener_called = true;
      listener_delegate(exec_state);
    }
  } catch (e) {
    exception = e;
  }
}

// Add the debug event listener.
Debug.setListener(listener);


// Initialize for a new test.
function BeginTest(name) {
  test_name = name;
  listener_delegate = null;
  listener_called = false;
  exception = null;
  begin_test_count++;
}


// Check result of a test.
function EndTest() {
  assertTrue(listener_called, "listerner not called for " + test_name);
  assertNull(exception, test_name);
  end_test_count++;
}


// Check that the scope chain contains the expected types of scopes.
function CheckScopeChain(scopes, exec_state) {
  assertEquals(scopes.length, exec_state.frame().scopeCount());
  for (var i = 0; i < scopes.length; i++) {
    var scope = exec_state.frame().scope(i);
    assertTrue(scope.isScope());
    assertEquals(scopes[i], scope.scopeType());

    // Check the global object when hitting the global scope.
    if (scopes[i] == debug.ScopeType.Global) {
      // Objects don't have same class (one is "global", other is "Object",
      // so just check the properties directly.
      assertPropertiesEqual(this, scope.scopeObject().value());
    }
  }

  // Get the debug command processor.
  var dcp = exec_state.debugCommandProcessor("unspecified_running_state");

  // Send a scopes request and check the result.
  var json;
  var request_json = '{"seq":0,"type":"request","command":"scopes"}';
  var response_json = dcp.processDebugJSONRequest(request_json);
  var response = JSON.parse(response_json);
  assertEquals(scopes.length, response.body.scopes.length);
  for (var i = 0; i < scopes.length; i++) {
    assertEquals(i, response.body.scopes[i].index);
    assertEquals(scopes[i], response.body.scopes[i].type);
    if (scopes[i] == debug.ScopeType.Local ||
        scopes[i] == debug.ScopeType.Closure) {
      assertTrue(response.body.scopes[i].object.ref < 0);
    } else {
      assertTrue(response.body.scopes[i].object.ref >= 0);
    }
    var found = false;
    for (var j = 0; j < response.refs.length && !found; j++) {
      found = response.refs[j].handle == response.body.scopes[i].object.ref;
    }
    assertTrue(found, "Scope object " + response.body.scopes[i].object.ref + " not found");
  }
}

// Check that the content of the scope is as expected. For functions just check
// that there is a function.
function CheckScopeContent(content, number, exec_state) {
  var scope = exec_state.frame().scope(number);
  var count = 0;
  for (var p in content) {
    var property_mirror = scope.scopeObject().property(p);
    if (property_mirror.isUndefined()) {
      print('property ' + p + ' not found in scope');
    }
    assertFalse(property_mirror.isUndefined(), 'property ' + p + ' not found in scope');
    if (typeof(content[p]) === 'function') {
      assertTrue(property_mirror.value().isFunction());
    } else {
      assertEquals(content[p], property_mirror.value().value(), 'property ' + p + ' has unexpected value');
    }
    count++;
  }

  // 'arguments' and might be exposed in the local and closure scope. Just
  // ignore this.
  var scope_size = scope.scopeObject().properties().length;
  if (!scope.scopeObject().property('arguments').isUndefined()) {
    scope_size--;
  }
  // Also ignore synthetic variable from catch block.
  if (!scope.scopeObject().property('.catch-var').isUndefined()) {
    scope_size--;
  }
  // Skip property with empty name.
  if (!scope.scopeObject().property('').isUndefined()) {
    scope_size--;
  }
  // Also ignore synthetic variable from block scopes.
  if (!scope.scopeObject().property('.block').isUndefined()) {
    scope_size--;
  }

  if (count != scope_size) {
    print('Names found in scope:');
    var names = scope.scopeObject().propertyNames();
    for (var i = 0; i < names.length; i++) {
      print(names[i]);
    }
  }
  assertEquals(count, scope_size);

  // Get the debug command processor.
  var dcp = exec_state.debugCommandProcessor("unspecified_running_state");

  // Send a scope request for information on a single scope and check the
  // result.
  var request_json = '{"seq":0,"type":"request","command":"scope","arguments":{"number":';
  request_json += scope.scopeIndex();
  request_json += '}}';
  var response_json = dcp.processDebugJSONRequest(request_json);
  var response = JSON.parse(response_json);
  assertEquals(scope.scopeType(), response.body.type);
  assertEquals(number, response.body.index);
  if (scope.scopeType() == debug.ScopeType.Local ||
      scope.scopeType() == debug.ScopeType.Closure) {
    assertTrue(response.body.object.ref < 0);
  } else {
    assertTrue(response.body.object.ref >= 0);
  }
  var found = false;
  for (var i = 0; i < response.refs.length && !found; i++) {
    found = response.refs[i].handle == response.body.object.ref;
  }
  assertTrue(found, "Scope object " + response.body.object.ref + " not found");
}


// Simple closure formed by returning an inner function referering to an outer
// block local variable and an outer function's parameter. Due to VM
// optimizations parts of the actual closure is missing from the debugger
// information.
BeginTest("Closure 1");

function closure_1(a) {
  var x = 2;
  let y = 3;
  if (true) {
    let z = 4;
    function f() {
      debugger;
      return a + x + y + z;
    };
    return f;
  }
}

listener_delegate = function(exec_state) {
  CheckScopeChain([debug.ScopeType.Local,
                   debug.ScopeType.Block,
                   debug.ScopeType.Closure,
                   debug.ScopeType.Global], exec_state);
  CheckScopeContent({}, 0, exec_state);
  CheckScopeContent({z:4}, 1, exec_state);
  CheckScopeContent({a:1,x:2,y:3}, 2, exec_state);
};
closure_1(1)();
EndTest();
