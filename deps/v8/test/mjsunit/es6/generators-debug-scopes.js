// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-debug-as debug

var Debug = debug.Debug;

function RunTest(name, formals_and_body, args, handler, continuation) {
  var handler_called = false;
  var exception = null;

  function listener(event, exec_state, event_data, data) {
    try {
      if (event == Debug.DebugEvent.Break) {
        handler_called = true;
        handler(exec_state);
      }
    } catch (e) {
      exception = e;
    }
  }

  function run(thunk) {
    handler_called = false;
    exception = null;

    var res = thunk();
    if (continuation)
      continuation(res);

    assertTrue(handler_called, "listener not called for " + name);
    assertNull(exception, name + " / " + exception);
  }

  var fun = Function.apply(null, formals_and_body);
  var gen = (function*(){}).constructor.apply(null, formals_and_body);

  Debug.setListener(listener);

  run(function () { return fun.apply(null, args) });
  run(function () { return gen.apply(null, args).next().value });

  // TODO(wingo): Uncomment after bug 2838 is fixed.
  // Debug.setListener(null);
}

// Check that two scope are the same.
function assertScopeMirrorEquals(scope1, scope2) {
  assertEquals(scope1.scopeType(), scope2.scopeType());
  assertEquals(scope1.frameIndex(), scope2.frameIndex());
  assertEquals(scope1.scopeIndex(), scope2.scopeIndex());
  assertPropertiesEqual(scope1.scopeObject().value(), scope2.scopeObject().value());
}

function CheckFastAllScopes(scopes, exec_state) {
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

// Check that the content of the scope is as expected. For functions just check
// that there is a function.
function CheckScopeContent(content, number, exec_state) {
  var scope = exec_state.frame().scope(number);
  var count = 0;
  for (var p in content) {
    var property_mirror = scope.scopeObject().property(p);
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
  // Skip property with empty name.
  if (!scope.scopeObject().property('').isUndefined()) {
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


// Simple empty local scope.
RunTest("Local 1",
        ['debugger;'],
        [],
        function (exec_state) {
          CheckScopeChain([debug.ScopeType.Local,
                           debug.ScopeType.Script,
                           debug.ScopeType.Global], exec_state);
          CheckScopeContent({}, 0, exec_state);
        });

// Local scope with a parameter.
RunTest("Local 2",
        ['a', 'debugger;'],
        [1],
        function (exec_state) {
          CheckScopeChain([debug.ScopeType.Local,
                           debug.ScopeType.Script,
                           debug.ScopeType.Global], exec_state);
          CheckScopeContent({a:1}, 0, exec_state);
        });

// Local scope with a parameter and a local variable.
RunTest("Local 3",
        ['a', 'var x = 3; debugger;'],
        [1],
        function (exec_state) {
          CheckScopeChain([debug.ScopeType.Local,
                           debug.ScopeType.Script,
                           debug.ScopeType.Global], exec_state);
          CheckScopeContent({a:1,x:3}, 0, exec_state);
        });

// Local scope with parameters and local variables.
RunTest("Local 4",
        ['a', 'b', 'var x = 3; var y = 4; debugger;'],
        [1, 2],
        function (exec_state) {
          CheckScopeChain([debug.ScopeType.Local,
                           debug.ScopeType.Script,
                           debug.ScopeType.Global], exec_state);
          CheckScopeContent({a:1,b:2,x:3,y:4}, 0, exec_state);
        });

// Empty local scope with use of eval.
RunTest("Local 5",
        ['eval(""); debugger;'],
        [],
        function (exec_state) {
          CheckScopeChain([debug.ScopeType.Local,
                           debug.ScopeType.Script,
                           debug.ScopeType.Global], exec_state);
          CheckScopeContent({}, 0, exec_state);
        });

// Local introducing local variable using eval.
RunTest("Local 6",
        ['eval("var i = 5"); debugger;'],
        [],
        function (exec_state) {
          CheckScopeChain([debug.ScopeType.Local,
                           debug.ScopeType.Script,
                           debug.ScopeType.Global], exec_state);
          CheckScopeContent({i:5}, 0, exec_state);
        });

// Local scope with parameters, local variables and local variable introduced
// using eval.
RunTest("Local 7",
        ['a', 'b',
         "var x = 3; var y = 4;\n"
         + "eval('var i = 5'); eval ('var j = 6');\n"
         + "debugger;"],
        [1, 2],
        function (exec_state) {
          CheckScopeChain([debug.ScopeType.Local,
                           debug.ScopeType.Script,
                           debug.ScopeType.Global], exec_state);
          CheckScopeContent({a:1,b:2,x:3,y:4,i:5,j:6}, 0, exec_state);
        });

// Nested empty with blocks.
RunTest("With",
        ["with ({}) { with ({}) { debugger; } }"],
        [],
        function (exec_state) {
          CheckScopeChain([debug.ScopeType.With,
                           debug.ScopeType.With,
                           debug.ScopeType.Local,
                           debug.ScopeType.Script,
                           debug.ScopeType.Global], exec_state);
          CheckScopeContent({}, 0, exec_state);
          CheckScopeContent({}, 1, exec_state);
        });

// Simple closure formed by returning an inner function referering the outer
// functions arguments.
RunTest("Closure 1",
        ['a', 'return function() { debugger; return a; }'],
        [1],
        function (exec_state) {
          CheckScopeChain([debug.ScopeType.Local,
                           debug.ScopeType.Closure,
                           debug.ScopeType.Script,
                           debug.ScopeType.Global], exec_state);
          CheckScopeContent({a:1}, 1, exec_state);
        },
       function (result) { result() });

RunTest("The full monty",
        ['a', 'b',
         "var x = 3;\n" +
         "var y = 4;\n" +
         "eval('var i = 5');\n" +
         "eval('var j = 6');\n" +
         "function f(a, b) {\n" +
         "  var x = 9;\n" +
         "  var y = 10;\n" +
         "  eval('var i = 11');\n" +
         "  eval('var j = 12');\n" +
         "  with ({j:13}){\n" +
         "    return function() {\n" +
         "      var x = 14;\n" +
         "      with ({a:15}) {\n" +
         "        with ({b:16}) {\n" +
         "          debugger;\n" +
         "          some_global = a;\n" +
         "          return f;\n" +
         "        }\n" +
         "      }\n" +
         "    };\n" +
         "  }\n" +
         "}\n" +
         "return f(a, b);"],
        [1, 2],
        function (exec_state) {
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
        },
        function (result) { result() });

RunTest("Catch block 1",
        ["try { throw 'Exception'; } catch (e) { debugger; }"],
        [],
        function (exec_state) {
          CheckScopeChain([debug.ScopeType.Catch,
                           debug.ScopeType.Local,
                           debug.ScopeType.Script,
                           debug.ScopeType.Global], exec_state);
          CheckScopeContent({e:'Exception'}, 0, exec_state);
        });
