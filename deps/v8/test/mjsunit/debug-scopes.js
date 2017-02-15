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

// Flags: --expose-debug-as debug --allow-natives-syntax
// The functions used for testing backtraces. They are at the top to make the
// testing of source line/column easier.

// Get the Debug object exposed from the debug context global object.
var Debug = debug.Debug;

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
  assertNull(exception, test_name + " / " + exception);
  end_test_count++;
}


// Check that two scope are the same.
function assertScopeMirrorEquals(scope1, scope2) {
  assertEquals(scope1.scopeType(), scope2.scopeType());
  assertEquals(scope1.frameIndex(), scope2.frameIndex());
  assertEquals(scope1.scopeIndex(), scope2.scopeIndex());
  assertPropertiesEqual(scope1.scopeObject().value(), scope2.scopeObject().value());
}

function CheckFastAllScopes(scopes, exec_state)
{
  var fast_all_scopes = exec_state.frame().allScopes(true);
  var length = fast_all_scopes.length;
  assertTrue(scopes.length >= length);
  for (var i = 0; i < scopes.length && i < length; i++) {
    var scope = fast_all_scopes[length - i - 1];
    assertTrue(scope.isScope());
    assertEquals(scopes[scopes.length - i - 1], scope.scopeType());
  }
}


// Check that the scope chain contains the expected types of scopes.
function CheckScopeChain(scopes, exec_state) {
  var all_scopes = exec_state.frame().allScopes();
  assertEquals(scopes.length, exec_state.frame().scopeCount());
  assertEquals(scopes.length, all_scopes.length, "FrameMirror.allScopes length");
  for (var i = 0; i < scopes.length; i++) {
    var scope = exec_state.frame().scope(i);
    assertTrue(scope.isScope());
    assertEquals(scopes[i], scope.scopeType());
    assertScopeMirrorEquals(all_scopes[i], scope);

    // Check the global object when hitting the global scope.
    if (scopes[i] == debug.ScopeType.Global) {
      // Objects don't have same class (one is "global", other is "Object",
      // so just check the properties directly.
      assertPropertiesEqual(this, scope.scopeObject().value());
    }
  }
  CheckFastAllScopes(scopes, exec_state);

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
        scopes[i] == debug.ScopeType.Script ||
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


// Check that the scope chain contains the expected names of scopes.
function CheckScopeChainNames(names, exec_state) {
  var all_scopes = exec_state.frame().allScopes();
  assertEquals(names.length, all_scopes.length, "FrameMirror.allScopes length");
  for (var i = 0; i < names.length; i++) {
    var scope = exec_state.frame().scope(i);
    assertTrue(scope.isScope());
    assertEquals(names[i], scope.details().name())
  }
}


// Check that the scope contains at least minimum_content. For functions just
// check that there is a function.
function CheckScopeContent(minimum_content, number, exec_state) {
  var scope = exec_state.frame().scope(number);
  var minimum_count = 0;
  for (var p in minimum_content) {
    var property_mirror = scope.scopeObject().property(p);
    assertFalse(property_mirror.isUndefined(), 'property ' + p + ' not found in scope');
    if (typeof(minimum_content[p]) === 'function') {
      assertTrue(property_mirror.value().isFunction());
    } else {
      assertEquals(minimum_content[p], property_mirror.value().value(), 'property ' + p + ' has unexpected value');
    }
    minimum_count++;
  }

  // 'arguments' and might be exposed in the local and closure scope. Just
  // ignore this.
  var scope_size = scope.scopeObject().properties().length;
  if (!scope.scopeObject().property('arguments').isUndefined()) {
    scope_size--;
  }
  // Ditto for 'this'.
  if (!scope.scopeObject().property('this').isUndefined()) {
    scope_size--;
  }
  // Temporary variables introduced by the parser have not been materialized.
  assertTrue(scope.scopeObject().property('').isUndefined());

  if (scope_size < minimum_count) {
    print('Names found in scope:');
    var names = scope.scopeObject().propertyNames();
    for (var i = 0; i < names.length; i++) {
      print(names[i]);
    }
  }
  assertTrue(scope_size >= minimum_count);

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
      scope.scopeType() == debug.ScopeType.Script ||
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

// Check that the scopes have positions as expected.
function CheckScopeChainPositions(positions, exec_state) {
  var all_scopes = exec_state.frame().allScopes();
  assertEquals(positions.length, all_scopes.length, "FrameMirror.allScopes length");
  for (var i = 0; i < positions.length; i++) {
    var scope = exec_state.frame().scope(i);
    assertTrue(scope.isScope());
    var position = positions[i];
    if (!position)
      continue;

    assertEquals(position.start, scope.details().startPosition())
    assertEquals(position.end, scope.details().endPosition())
  }
}

// Simple empty local scope.
BeginTest("Local 1");

function local_1() {
  debugger;
}

listener_delegate = function(exec_state) {
  CheckScopeChain([debug.ScopeType.Local,
                   debug.ScopeType.Script,
                   debug.ScopeType.Global], exec_state);
  CheckScopeContent({}, 0, exec_state);
};
local_1();
EndTest();


// Local scope with a parameter.
BeginTest("Local 2");

function local_2(a) {
  debugger;
}

listener_delegate = function(exec_state) {
  CheckScopeChain([debug.ScopeType.Local,
                   debug.ScopeType.Script,
                   debug.ScopeType.Global], exec_state);
  CheckScopeContent({a:1}, 0, exec_state);
};
local_2(1);
EndTest();


// Local scope with a parameter and a local variable.
BeginTest("Local 3");

function local_3(a) {
  var x = 3;
  debugger;
}

listener_delegate = function(exec_state) {
  CheckScopeChain([debug.ScopeType.Local,
                   debug.ScopeType.Script,
                   debug.ScopeType.Global], exec_state);
  CheckScopeContent({a:1,x:3}, 0, exec_state);
};
local_3(1);
EndTest();


// Local scope with parameters and local variables.
BeginTest("Local 4");

function local_4(a, b) {
  var x = 3;
  var y = 4;
  debugger;
}

listener_delegate = function(exec_state) {
  CheckScopeChain([debug.ScopeType.Local,
                   debug.ScopeType.Script,
                   debug.ScopeType.Global], exec_state);
  CheckScopeContent({a:1,b:2,x:3,y:4}, 0, exec_state);
};
local_4(1, 2);
EndTest();


// Empty local scope with use of eval.
BeginTest("Local 5");

function local_5() {
  eval('');
  debugger;
}

listener_delegate = function(exec_state) {
  CheckScopeChain([debug.ScopeType.Local,
                   debug.ScopeType.Script,
                   debug.ScopeType.Global], exec_state);
  CheckScopeContent({}, 0, exec_state);
};
local_5();
EndTest();


// Local introducing local variable using eval.
BeginTest("Local 6");

function local_6() {
  eval('var i = 5');
  debugger;
}

listener_delegate = function(exec_state) {
  CheckScopeChain([debug.ScopeType.Local,
                   debug.ScopeType.Script,
                   debug.ScopeType.Global], exec_state);
  CheckScopeContent({i:5}, 0, exec_state);
};
local_6();
EndTest();


// Local scope with parameters, local variables and local variable introduced
// using eval.
BeginTest("Local 7");

function local_7(a, b) {
  var x = 3;
  var y = 4;
  eval('var i = 5');
  eval('var j = 6');
  debugger;
}

listener_delegate = function(exec_state) {
  CheckScopeChain([debug.ScopeType.Local,
                   debug.ScopeType.Script,
                   debug.ScopeType.Global], exec_state);
  CheckScopeContent({a:1,b:2,x:3,y:4,i:5,j:6}, 0, exec_state);
};
local_7(1, 2);
EndTest();


// Single empty with block.
BeginTest("With 1");

function with_1() {
  with({}) {
    debugger;
  }
}

listener_delegate = function(exec_state) {
  CheckScopeChain([debug.ScopeType.With,
                   debug.ScopeType.Local,
                   debug.ScopeType.Script,
                   debug.ScopeType.Global], exec_state);
  CheckScopeContent({}, 0, exec_state);
};
with_1();
EndTest();


// Nested empty with blocks.
BeginTest("With 2");

function with_2() {
  with({}) {
    with({}) {
      debugger;
    }
  }
}

listener_delegate = function(exec_state) {
  CheckScopeChain([debug.ScopeType.With,
                   debug.ScopeType.With,
                   debug.ScopeType.Local,
                   debug.ScopeType.Script,
                   debug.ScopeType.Global], exec_state);
  CheckScopeContent({}, 0, exec_state);
  CheckScopeContent({}, 1, exec_state);
};
with_2();
EndTest();


// With block using an in-place object literal.
BeginTest("With 3");

function with_3() {
  with({a:1,b:2}) {
    debugger;
  }
}

listener_delegate = function(exec_state) {
  CheckScopeChain([debug.ScopeType.With,
                   debug.ScopeType.Local,
                   debug.ScopeType.Script,
                   debug.ScopeType.Global], exec_state);
  CheckScopeContent({a:1,b:2}, 0, exec_state);
};
with_3();
EndTest();


// Nested with blocks using in-place object literals.
BeginTest("With 4");

function with_4() {
  with({a:1,b:2}) {
    with({a:2,b:1}) {
      debugger;
    }
  }
}

listener_delegate = function(exec_state) {
  CheckScopeChain([debug.ScopeType.With,
                   debug.ScopeType.With,
                   debug.ScopeType.Local,
                   debug.ScopeType.Script,
                   debug.ScopeType.Global], exec_state);
  CheckScopeContent({a:2,b:1}, 0, exec_state);
  CheckScopeContent({a:1,b:2}, 1, exec_state);
};
with_4();
EndTest();


// Nested with blocks using existing object.
BeginTest("With 5");

var with_object = {c:3,d:4};
function with_5() {
  with(with_object) {
    with(with_object) {
      debugger;
    }
  }
}

listener_delegate = function(exec_state) {
  CheckScopeChain([debug.ScopeType.With,
                   debug.ScopeType.With,
                   debug.ScopeType.Local,
                   debug.ScopeType.Script,
                   debug.ScopeType.Global], exec_state);
  CheckScopeContent(with_object, 0, exec_state);
  CheckScopeContent(with_object, 1, exec_state);
  assertEquals(exec_state.frame().scope(0).scopeObject(), exec_state.frame().scope(1).scopeObject());
  assertEquals(with_object, exec_state.frame().scope(1).scopeObject().value());
};
with_5();
EndTest();


// Nested with blocks using existing object in global code.
BeginTest("With 6");
listener_delegate = function(exec_state) {
  CheckScopeChain([debug.ScopeType.With,
                   debug.ScopeType.With,
                   debug.ScopeType.Script,
                   debug.ScopeType.Global], exec_state);
  CheckScopeContent(with_object, 0, exec_state);
  CheckScopeContent(with_object, 1, exec_state);
  assertEquals(exec_state.frame().scope(0).scopeObject(), exec_state.frame().scope(1).scopeObject());
  assertEquals(with_object, exec_state.frame().scope(1).scopeObject().value());
};

var with_object = {c:3,d:4};
with(with_object) {
  with(with_object) {
    debugger;
  }
}
EndTest();


// With block in function that is marked for optimization while being executed.
BeginTest("With 7");

function with_7() {
  with({}) {
    %OptimizeFunctionOnNextCall(with_7);
    debugger;
  }
}

listener_delegate = function(exec_state) {
  CheckScopeChain([debug.ScopeType.With,
                   debug.ScopeType.Local,
                   debug.ScopeType.Script,
                   debug.ScopeType.Global], exec_state);
  CheckScopeContent({}, 0, exec_state);
};
with_7();
EndTest();


// Simple closure formed by returning an inner function referering the outer
// functions arguments.
BeginTest("Closure 1");

function closure_1(a) {
  function f() {
    debugger;
    return a;
  };
  return f;
}

listener_delegate = function(exec_state) {
  CheckScopeChain([debug.ScopeType.Local,
                   debug.ScopeType.Closure,
                   debug.ScopeType.Script,
                   debug.ScopeType.Global], exec_state);
  CheckScopeContent({a:1}, 1, exec_state);
  CheckScopeChainNames(["f", "closure_1", undefined, undefined], exec_state)
};
closure_1(1)();
EndTest();


// Simple closure formed by returning an inner function referering the outer
// functions arguments. Due to VM optimizations parts of the actual closure is
// missing from the debugger information.
BeginTest("Closure 2");

function closure_2(a, b) {
  var x = a + 2;
  var y = b + 2;
  function f() {
    debugger;
    return a + x;
  };
  return f;
}

listener_delegate = function(exec_state) {
  CheckScopeChain([debug.ScopeType.Local,
                   debug.ScopeType.Closure,
                   debug.ScopeType.Script,
                   debug.ScopeType.Global], exec_state);
  CheckScopeContent({a:1,x:3}, 1, exec_state);
  CheckScopeChainNames(["f", "closure_2", undefined, undefined], exec_state)
};
closure_2(1, 2)();
EndTest();


// Simple closure formed by returning an inner function referering the outer
// functions arguments. Using all arguments and locals from the outer function
// in the inner function makes these part of the debugger information on the
// closure.
BeginTest("Closure 3");

function closure_3(a, b) {
  var x = a + 2;
  var y = b + 2;
  function f() {
    debugger;
    return a + b + x + y;
  };
  return f;
}

listener_delegate = function(exec_state) {
  CheckScopeChain([debug.ScopeType.Local,
                   debug.ScopeType.Closure,
                   debug.ScopeType.Script,
                   debug.ScopeType.Global], exec_state);
  CheckScopeContent({a:1,b:2,x:3,y:4}, 1, exec_state);
  CheckScopeChainNames(["f", "closure_3", undefined, undefined], exec_state)
};
closure_3(1, 2)();
EndTest();



// Simple closure formed by returning an inner function referering the outer
// functions arguments. Using all arguments and locals from the outer function
// in the inner function makes these part of the debugger information on the
// closure. Use the inner function as well...
BeginTest("Closure 4");

function closure_4(a, b) {
  var x = a + 2;
  var y = b + 2;
  function f() {
    debugger;
    if (f) {
      return a + b + x + y;
    }
  };
  return f;
}

listener_delegate = function(exec_state) {
  CheckScopeChain([debug.ScopeType.Local,
                   debug.ScopeType.Closure,
                   debug.ScopeType.Script,
                   debug.ScopeType.Global], exec_state);
  CheckScopeContent({a:1,b:2,x:3,y:4,f:function(){}}, 1, exec_state);
  CheckScopeChainNames(["f", "closure_4", undefined, undefined], exec_state)
};
closure_4(1, 2)();
EndTest();



// Simple closure formed by returning an inner function referering the outer
// functions arguments. In the presence of eval all arguments and locals
// (including the inner function itself) from the outer function becomes part of
// the debugger infformation on the closure.
BeginTest("Closure 5");

function closure_5(a, b) {
  var x = 3;
  var y = 4;
  function f() {
    eval('');
    debugger;
    return 1;
  };
  return f;
}

listener_delegate = function(exec_state) {
  CheckScopeChain([debug.ScopeType.Local,
                   debug.ScopeType.Closure,
                   debug.ScopeType.Script,
                   debug.ScopeType.Global], exec_state);
  CheckScopeContent({a:1,b:2,x:3,y:4,f:function(){}}, 1, exec_state);
  CheckScopeChainNames(["f", "closure_5", undefined, undefined], exec_state)
};
closure_5(1, 2)();
EndTest();


// Two closures. Due to optimizations only the parts actually used are provided
// through the debugger information.
BeginTest("Closure 6");
function closure_6(a, b) {
  function f(a, b) {
    var x = 3;
    var y = 4;
    return function() {
      var x = 3;
      var y = 4;
      debugger;
      some_global = a;
      return f;
    };
  }
  return f(a, b);
}

listener_delegate = function(exec_state) {
  CheckScopeChain([debug.ScopeType.Local,
                   debug.ScopeType.Closure,
                   debug.ScopeType.Closure,
                   debug.ScopeType.Script,
                   debug.ScopeType.Global], exec_state);
  CheckScopeContent({a:1}, 1, exec_state);
  CheckScopeContent({f:function(){}}, 2, exec_state);
  CheckScopeChainNames([undefined, "f", "closure_6", undefined, undefined], exec_state)
};
closure_6(1, 2)();
EndTest();


// Two closures. In the presence of eval all information is provided as the
// compiler cannot determine which parts are used.
BeginTest("Closure 7");
function closure_7(a, b) {
  var x = 3;
  var y = 4;
  eval('var i = 5');
  eval('var j = 6');
  function f(a, b) {
    var x = 3;
    var y = 4;
    eval('var i = 5');
    eval('var j = 6');
    return function() {
      debugger;
      some_global = a;
      return f;
    };
  }
  return f(a, b);
}

listener_delegate = function(exec_state) {
  CheckScopeChain([debug.ScopeType.Local,
                   debug.ScopeType.Closure,
                   debug.ScopeType.Closure,
                   debug.ScopeType.Script,
                   debug.ScopeType.Global], exec_state);
  CheckScopeContent({}, 0, exec_state);
  CheckScopeContent({a:1,b:2,x:3,y:4,i:5,j:6}, 1, exec_state);
  CheckScopeContent({a:1,b:2,x:3,y:4,i:5,j:6,f:function(){}}, 2, exec_state);
  CheckScopeChainNames([undefined, "f", "closure_7", undefined, undefined], exec_state)
};
closure_7(1, 2)();
EndTest();


// Closure that may be optimized out.
BeginTest("Closure 8");
function closure_8() {
  (function inner(x) {
    debugger;
  })(2);
}

listener_delegate = function(exec_state) {
  CheckScopeChain([debug.ScopeType.Local,
                   debug.ScopeType.Script,
                   debug.ScopeType.Global], exec_state);
  CheckScopeContent({x: 2}, 0, exec_state);
  CheckScopeChainNames(["inner", undefined, undefined], exec_state)
};
closure_8();
EndTest();


BeginTest("Closure 9");
function closure_9() {
  eval("var y = 1;");
  eval("var z = 1;");
  (function inner(x) {
    y++;
    z++;
    debugger;
  })(2);
}

listener_delegate = function(exec_state) {
  CheckScopeChain([debug.ScopeType.Local,
                   debug.ScopeType.Closure,
                   debug.ScopeType.Script,
                   debug.ScopeType.Global], exec_state);
  CheckScopeChainNames(["inner", "closure_9", undefined, undefined], exec_state)
};
closure_9();
EndTest();


// Test a mixture of scopes.
BeginTest("The full monty");
function the_full_monty(a, b) {
  var x = 3;
  var y = 4;
  eval('var i = 5');
  eval('var j = 6');
  function f(a, b) {
    var x = 9;
    var y = 10;
    eval('var i = 11');
    eval('var j = 12');
    with ({j:13}){
      return function() {
        var x = 14;
        with ({a:15}) {
          with ({b:16}) {
            debugger;
            some_global = a;
            return f;
          }
        }
      };
    }
  }
  return f(a, b);
}

listener_delegate = function(exec_state) {
  CheckScopeChain([debug.ScopeType.With,
                   debug.ScopeType.With,
                   debug.ScopeType.Local,
                   debug.ScopeType.With,
                   debug.ScopeType.Closure,
                   debug.ScopeType.Closure,
                   debug.ScopeType.Script,
                   debug.ScopeType.Global], exec_state);
  CheckScopeContent({b:16}, 0, exec_state);
  CheckScopeContent({a:15}, 1, exec_state);
  CheckScopeContent({x:14}, 2, exec_state);
  CheckScopeContent({j:13}, 3, exec_state);
  CheckScopeContent({a:1,b:2,x:9,y:10,i:11,j:12}, 4, exec_state);
  CheckScopeContent({a:1,b:2,x:3,y:4,i:5,j:6,f:function(){}}, 5, exec_state);
  CheckScopeChainNames([undefined, undefined, undefined, "f", "f", "the_full_monty", undefined, undefined], exec_state)
};
the_full_monty(1, 2)();
EndTest();


BeginTest("Closure inside With 1");
function closure_in_with_1() {
  with({x:1}) {
    (function inner(x) {
      debugger;
    })(2);
  }
}

listener_delegate = function(exec_state) {
  CheckScopeChain([debug.ScopeType.Local,
                   debug.ScopeType.With,
                   debug.ScopeType.Script,
                   debug.ScopeType.Global], exec_state);
  CheckScopeContent({x: 2}, 0, exec_state);
  CheckScopeContent({x: 1}, 1, exec_state);
};
closure_in_with_1();
EndTest();


BeginTest("Closure inside With 2");
function closure_in_with_2() {
  with({x:1}) {
    (function inner(x) {
      with({x:3}) {
        debugger;
      }
    })(2);
  }
}

listener_delegate = function(exec_state) {
  CheckScopeChain([debug.ScopeType.With,
                   debug.ScopeType.Local,
                   debug.ScopeType.With,
                   debug.ScopeType.Script,
                   debug.ScopeType.Global], exec_state);
  CheckScopeContent({x: 3}, 0, exec_state);
  CheckScopeContent({x: 2}, 1, exec_state);
  CheckScopeContent({x: 1}, 2, exec_state);
  CheckScopeChainNames(["inner", "inner", "closure_in_with_2", undefined, undefined], exec_state)
};
closure_in_with_2();
EndTest();


BeginTest("Closure inside With 3");
function createClosure(a) {
   var b = a + 1;
   return function closure() {
     var c = b;
     (function inner(x) {
       with({x:c}) {
         debugger;
       }
     })(2);
   };
}

function closure_in_with_3() {
  var f = createClosure(0);
  f();
}

listener_delegate = function(exec_state) {
  CheckScopeChain([debug.ScopeType.With,
                   debug.ScopeType.Local,
                   debug.ScopeType.Closure,
                   debug.ScopeType.Closure,
                   debug.ScopeType.Script,
                   debug.ScopeType.Global], exec_state);
  CheckScopeChainNames(["inner", "inner", "closure", "createClosure", undefined, undefined], exec_state)
}
closure_in_with_3();
EndTest();


BeginTest("Closure inside With 4");
listener_delegate = function(exec_state) {
  CheckScopeChain([debug.ScopeType.Local,
                   debug.ScopeType.With,
                   debug.ScopeType.Script,
                   debug.ScopeType.Global], exec_state);
  CheckScopeContent({x: 2}, 0, exec_state);
  CheckScopeContent({x: 1}, 1, exec_state);
  CheckScopeChainNames([undefined, undefined, undefined, undefined], exec_state)
};

with({x:1}) {
  (function(x) {
    debugger;
  })(2);
}
EndTest();


// Test global scope.
BeginTest("Global");
listener_delegate = function(exec_state) {
  CheckScopeChain([debug.ScopeType.Script, debug.ScopeType.Global], exec_state);
  CheckScopeChainNames([undefined, undefined], exec_state)
};
debugger;
EndTest();


BeginTest("Catch block 1");
function catch_block_1() {
  try {
    throw 'Exception';
  } catch (e) {
    debugger;
  }
};


listener_delegate = function(exec_state) {
  CheckScopeChain([debug.ScopeType.Catch,
                   debug.ScopeType.Local,
                   debug.ScopeType.Script,
                   debug.ScopeType.Global], exec_state);
  CheckScopeContent({e:'Exception'}, 0, exec_state);
  CheckScopeChainNames(["catch_block_1", "catch_block_1", undefined, undefined], exec_state)
};
catch_block_1();
EndTest();


BeginTest("Catch block 2");
function catch_block_2() {
  try {
    throw 'Exception';
  } catch (e) {
    with({n:10}) {
      debugger;
    }
  }
};


listener_delegate = function(exec_state) {
  CheckScopeChain([debug.ScopeType.With,
                   debug.ScopeType.Catch,
                   debug.ScopeType.Local,
                   debug.ScopeType.Script,
                   debug.ScopeType.Global], exec_state);
  CheckScopeContent({n:10}, 0, exec_state);
  CheckScopeContent({e:'Exception'}, 1, exec_state);
  CheckScopeChainNames(["catch_block_2", "catch_block_2", "catch_block_2", undefined, undefined], exec_state)
};
catch_block_2();
EndTest();


BeginTest("Catch block 3");
function catch_block_3() {
  // Do eval to dynamically declare a local variable so that the context's
  // extension slot is initialized with JSContextExtensionObject.
  eval("var y = 78;");
  try {
    throw 'Exception';
  } catch (e) {
    debugger;
  }
};


listener_delegate = function(exec_state) {
  CheckScopeChain([debug.ScopeType.Catch,
                   debug.ScopeType.Local,
                   debug.ScopeType.Script,
                   debug.ScopeType.Global], exec_state);
  CheckScopeContent({e:'Exception'}, 0, exec_state);
  CheckScopeContent({y:78}, 1, exec_state);
  CheckScopeChainNames(["catch_block_3", "catch_block_3", undefined, undefined], exec_state)
};
catch_block_3();
EndTest();


BeginTest("Catch block 4");
function catch_block_4() {
  // Do eval to dynamically declare a local variable so that the context's
  // extension slot is initialized with JSContextExtensionObject.
  eval("var y = 98;");
  try {
    throw 'Exception';
  } catch (e) {
    with({n:10}) {
      debugger;
    }
  }
};

listener_delegate = function(exec_state) {
  CheckScopeChain([debug.ScopeType.With,
                   debug.ScopeType.Catch,
                   debug.ScopeType.Local,
                   debug.ScopeType.Script,
                   debug.ScopeType.Global], exec_state);
  CheckScopeContent({n:10}, 0, exec_state);
  CheckScopeContent({e:'Exception'}, 1, exec_state);
  CheckScopeContent({y:98}, 2, exec_state);
  CheckScopeChainNames(["catch_block_4", "catch_block_4", "catch_block_4", undefined, undefined], exec_state)
};
catch_block_4();
EndTest();


// Test catch in global scope.
BeginTest("Catch block 5");
listener_delegate = function(exec_state) {
  CheckScopeChain([debug.ScopeType.Catch,
                   debug.ScopeType.Script,
                   debug.ScopeType.Global], exec_state);
  CheckScopeContent({e:'Exception'}, 0, exec_state);
  CheckScopeChainNames([undefined, undefined, undefined], exec_state)
};

try {
  throw 'Exception';
} catch (e) {
  debugger;
}

EndTest();


// Closure inside catch in global code.
BeginTest("Catch block 6");
listener_delegate = function(exec_state) {
  CheckScopeChain([debug.ScopeType.Local,
                   debug.ScopeType.Catch,
                   debug.ScopeType.Script,
                   debug.ScopeType.Global], exec_state);
  CheckScopeContent({x: 2}, 0, exec_state);
  CheckScopeContent({e:'Exception'}, 1, exec_state);
  CheckScopeChainNames([undefined, undefined, undefined, undefined], exec_state)
};

try {
  throw 'Exception';
} catch (e) {
  (function(x) {
    debugger;
  })(2);
}
EndTest();


// Catch block in function that is marked for optimization while being executed.
BeginTest("Catch block 7");
function catch_block_7() {
  %OptimizeFunctionOnNextCall(catch_block_7);
  try {
    throw 'Exception';
  } catch (e) {
    debugger;
  }
};


listener_delegate = function(exec_state) {
  CheckScopeChain([debug.ScopeType.Catch,
                   debug.ScopeType.Local,
                   debug.ScopeType.Script,
                   debug.ScopeType.Global], exec_state);
  CheckScopeContent({e:'Exception'}, 0, exec_state);
  CheckScopeChainNames(["catch_block_7", "catch_block_7", undefined, undefined], exec_state)
};
catch_block_7();
EndTest();


BeginTest("Classes and methods 1");

listener_delegate = function(exec_state) {
  "use strict"
  CheckScopeChain([debug.ScopeType.Local,
                   debug.ScopeType.Script,
                   debug.ScopeType.Global], exec_state);
  CheckScopeContent({}, 1, exec_state);
  CheckScopeChainNames(["m", undefined, undefined], exec_state)
};

(function() {
  "use strict";
  class C1 {
    m() {
      debugger;
    }
  }
  new C1().m();
})();

EndTest();

BeginTest("Scope positions");
var code1 = "function f() {        \n" +
            "  var a = 1;          \n" +
            "  function b() {      \n" +
            "    debugger;         \n" +
            "    return a + 1;     \n" +
            "  }                   \n" +
            "  b();                \n" +
            "}                     \n" +
            "f();                  \n";

listener_delegate = function(exec_state) {
  CheckScopeChainPositions([{start: 58, end: 118}, {start: 10, end: 162}, {}, {}], exec_state);
}
eval(code1);
EndTest();


function catch_block_2() {
  try {
    throw 'Exception';
  } catch (e) {
    with({n:10}) {
      debugger;
    }
  }
};

BeginTest("Scope positions in catch and 'with' statement");
var code2 = "function catch_block() {   \n" +
            "  try {                    \n" +
            "    throw 'Exception';     \n" +
            "  } catch (e) {            \n" +
            "    with({n : 10}) {       \n" +
            "      debugger;            \n" +
            "    }                      \n" +
            "  }                        \n" +
            "}                          \n" +
            "catch_block();             \n";

listener_delegate = function(exec_state) {
  CheckScopeChainPositions([{start: 131, end: 173}, {start: 94, end: 199}, {start: 20, end: 225}, {}, {}], exec_state);
}
eval(code2);
EndTest();

BeginTest("Scope positions in for statement");
var code3 = "function for_statement() {         \n" +
            "  for (let i = 0; i < 1; i++) {    \n" +
            "    debugger;                      \n" +
            "  }                                \n" +
            "}                                  \n" +
            "for_statement();                   \n";

listener_delegate = function(exec_state) {
  CheckScopeChain([debug.ScopeType.Block,
                   debug.ScopeType.Local,
                   debug.ScopeType.Script,
                   debug.ScopeType.Global], exec_state);
  CheckScopeChainPositions([{start: 52, end: 111}, {start: 22, end: 145}, {}, {}], exec_state);
}
eval(code3);
EndTest();

BeginTest("Scope positions in for statement with lexical block");
var code4 = "function for_statement() {         \n" +
            "  for (let i = 0; i < 1; i++) {    \n" +
            "    let j;                         \n" +
            "    debugger;                      \n" +
            "  }                                \n" +
            "}                                  \n" +
            "for_statement();                   \n";

listener_delegate = function(exec_state) {
  CheckScopeChain([debug.ScopeType.Block,
                   debug.ScopeType.Block,
                   debug.ScopeType.Local,
                   debug.ScopeType.Script,
                   debug.ScopeType.Global], exec_state);
  CheckScopeChainPositions([{start: 66, end: 147}, {start: 52, end: 147}, {start: 22, end: 181}, {}, {}], exec_state);
}
eval(code4);
EndTest();

BeginTest("Scope positions in lexical for each statement");
var code5 = "function for_each_statement() {    \n" +
            "  for (let i of [0]) {             \n" +
            "    debugger;                      \n" +
            "  }                                \n" +
            "}                                  \n" +
            "for_each_statement();              \n";

listener_delegate = function(exec_state) {
  CheckScopeChain([debug.ScopeType.Block,
                   debug.ScopeType.Local,
                   debug.ScopeType.Script,
                   debug.ScopeType.Global], exec_state);
  CheckScopeChainPositions([{start: 55, end: 111}, {start: 27, end: 145}, {}, {}], exec_state);
}
eval(code5);
EndTest();

BeginTest("Scope positions in lexical for each statement with lexical block");
var code6 = "function for_each_statement() {    \n" +
            "  for (let i of [0]) {             \n" +
            "    let j;                         \n" +
            "    debugger;                      \n" +
            "  }                                \n" +
            "}                                  \n" +
            "for_each_statement();              \n";

listener_delegate = function(exec_state) {
  CheckScopeChain([debug.ScopeType.Block,
                   debug.ScopeType.Block,
                   debug.ScopeType.Local,
                   debug.ScopeType.Script,
                   debug.ScopeType.Global], exec_state);
  CheckScopeChainPositions([{start: 57, end: 147}, {start: 55, end: 147}, {start: 27, end: 181}, {}, {}], exec_state);
}
eval(code6);
EndTest();

BeginTest("Scope positions in non-lexical for each statement");
var code7 = "function for_each_statement() {    \n" +
            "  var i;                           \n" +
            "  for (i of [0]) {                 \n" +
            "    debugger;                      \n" +
            "  }                                \n" +
            "}                                  \n" +
            "for_each_statement();              \n";

listener_delegate = function(exec_state) {
  CheckScopeChain([debug.ScopeType.Local,
                   debug.ScopeType.Script,
                   debug.ScopeType.Global], exec_state);
  CheckScopeChainPositions([{start: 27, end: 181}, {}, {}], exec_state);
}
eval(code7);
EndTest();

BeginTest("Scope positions in non-lexical for each statement with lexical block");
var code8 = "function for_each_statement() {    \n" +
            "  var i;                           \n" +
            "  for (i of [0]) {                 \n" +
            "    let j;                         \n" +
            "    debugger;                      \n" +
            "  }                                \n" +
            "}                                  \n" +
            "for_each_statement();              \n";

listener_delegate = function(exec_state) {
  CheckScopeChain([debug.ScopeType.Block,
                   debug.ScopeType.Local,
                   debug.ScopeType.Script,
                   debug.ScopeType.Global], exec_state);
  CheckScopeChainPositions([{start: 89, end: 183}, {start: 27, end: 217}, {}, {}], exec_state);
}
eval(code8);
EndTest();

assertEquals(begin_test_count, break_count,
             'one or more tests did not enter the debugger');
assertEquals(begin_test_count, end_test_count,
             'one or more tests did not have its result checked');
