// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var Debug = debug.Debug;
var global_marker = 7;

async function thrower() { throw 'Exception'; }

async function test(name, func, args, handler, continuation) {
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

  Debug.setListener(listener);

  var result;
  if (typeof func === "object")
    result = await func.method.apply(func, args);
  else
    result = await func.apply(null, args);

  if (typeof continuation === "function") {
    await continuation(result);
  }

  assertTrue(handler_called, `Expected ${name} handler to be called`);
  if (exception) {
    exception.message = `${name} / ${exception.message}`;
    print(exception.stack);
    quit(1);
  }

  Debug.setListener(null);
}

async function runTests() {

// Simple
await test(
    "(AsyncFunctionExpression) Local 1",
    async function() { debugger; }, [],
    exec_state => {
      CheckScopeChain([debug.ScopeType.Local,
                       debug.ScopeType.Closure,
                       debug.ScopeType.Script,
                       debug.ScopeType.Global], exec_state);
      CheckScopeContent({}, 0, exec_state);
    });

await test(
    "(AsyncFunctionExpression) Local 1 --- resume normal",
    async function() { let z = await 2; debugger; }, [],
    exec_state => {
      CheckScopeChain([debug.ScopeType.Local,
                       debug.ScopeType.Closure,
                       debug.ScopeType.Script,
                       debug.ScopeType.Global], exec_state);
      CheckScopeContent({z: 2}, 0, exec_state);
    });

await test(
    "(AsyncFunctionExpression) Local 1 --- resume throw",
    async function() { let q = await 1;
                       try { let z = await thrower(); }
                       catch (e) { debugger; } }, [],
    exec_state => {
      CheckScopeChain([debug.ScopeType.Catch,
                       debug.ScopeType.Local,
                       debug.ScopeType.Closure,
                       debug.ScopeType.Script,
                       debug.ScopeType.Global], exec_state);
      CheckScopeContent({e: 'Exception'}, 0, exec_state);
      CheckScopeContent({q: 1}, 1, exec_state);

    });

// Simple With Parameter
await test(
    "(AsyncFunctionExpression) Local 2",
    async function(a) { debugger; }, [1],
    exec_state => {
      CheckScopeChain([debug.ScopeType.Local,
                       debug.ScopeType.Closure,
                       debug.ScopeType.Script,
                       debug.ScopeType.Global], exec_state);
      CheckScopeContent({ a: 1 }, 0, exec_state);
    });

await test(
    "(AsyncFunctionExpression) Local 2 --- resume normal",
    async function(a) { let z = await 2; debugger; }, [1],
    exec_state => {
      CheckScopeChain([debug.ScopeType.Local,
                       debug.ScopeType.Closure,
                       debug.ScopeType.Script,
                       debug.ScopeType.Global], exec_state);
      CheckScopeContent({ a: 1, z: 2 }, 0, exec_state);
    });

await test(
    "(AsyncFunctionExpression) Local 2 --- resume throw",
    async function(a) { let z = await 2;
                        try { await thrower(); } catch (e) { debugger; } }, [1],
    exec_state => {
      CheckScopeChain([debug.ScopeType.Catch,
                       debug.ScopeType.Local,
                       debug.ScopeType.Closure,
                       debug.ScopeType.Script,
                       debug.ScopeType.Global], exec_state);
      CheckScopeContent({ e: 'Exception' }, 0, exec_state);
      CheckScopeContent({ a: 1, z: 2 }, 1, exec_state);
    });

// Simple With Parameter and Variable
await test(
    "(AsyncFunctionExpression) Local 3",
    async function(a) { var b = 2; debugger; }, [1],
    exec_state => {
      CheckScopeChain([debug.ScopeType.Local,
                       debug.ScopeType.Closure,
                       debug.ScopeType.Script,
                       debug.ScopeType.Global], exec_state);
      CheckScopeContent({ a: 1, b: 2 }, 0, exec_state);
    });

await test(
    "(AsyncFunctionExpression) Local 3 --- resume normal",
    async function(a) { let y = await 3; var b = 2; let z = await 4;
                        debugger; }, [1],
    exec_state => {
      CheckScopeChain([debug.ScopeType.Local,
                       debug.ScopeType.Closure,
                       debug.ScopeType.Script,
                       debug.ScopeType.Global], exec_state);
      CheckScopeContent({ a: 1, b: 2, y: 3, z: 4 }, 0, exec_state);
    });

await test(
    "(AsyncFunctionExpression) Local 3 --- resume throw",
    async function(a) { let y = await 3;
                        try { var b = 2; let z = await thrower(); }
                        catch (e) { debugger; } }, [1],
    exec_state => {
      CheckScopeChain([debug.ScopeType.Catch,
                       debug.ScopeType.Local,
                       debug.ScopeType.Closure,
                       debug.ScopeType.Script,
                       debug.ScopeType.Global], exec_state);
      CheckScopeContent({ e: 'Exception' }, 0, exec_state);
      CheckScopeContent({ a: 1, b: 2, y: 3 }, 1, exec_state);
    });

// Local scope with parameters and local variables.
await test(
    "(AsyncFunctionExpression) Local 4",
    async function(a, b) { var x = 3; var y = 4; debugger; }, [1, 2],
    exec_state => {
      CheckScopeChain([debug.ScopeType.Local,
                       debug.ScopeType.Closure,
                       debug.ScopeType.Script,
                       debug.ScopeType.Global], exec_state);
      CheckScopeContent({a:1,b:2,x:3,y:4}, 0, exec_state);
    });

await test(
    "(AsyncFunctionExpression) Local 4 --- resume normal",
    async function(a, b) { let q = await 5; var x = 3; var y = 4;
                           let r = await 6; debugger; }, [1, 2],
    exec_state => {
      CheckScopeChain([debug.ScopeType.Local,
                       debug.ScopeType.Closure,
                       debug.ScopeType.Script,
                       debug.ScopeType.Global], exec_state);
      CheckScopeContent({a:1,b:2,x:3,y:4, q: 5, r: 6}, 0, exec_state);
    });

await test(
    "(AsyncFunctionExpression) Local 4 --- resume throw",
    async function(a, b) { let q = await 5; var x = 3; var y = 4;
                           try { let r = await thrower(); }
                           catch (e) { debugger; } }, [1, 2],
    exec_state => {
      CheckScopeChain([debug.ScopeType.Catch,
                       debug.ScopeType.Local,
                       debug.ScopeType.Closure,
                       debug.ScopeType.Script,
                       debug.ScopeType.Global], exec_state);
      CheckScopeContent({e: 'Exception'}, 0, exec_state);
      CheckScopeContent({a:1,b:2,x:3,y:4, q: 5}, 1, exec_state);
    });

// Empty local scope with use of eval.
await test(
    "(AsyncFunctionExpression) Local 5",
    async function() { eval(""); debugger; }, [],
    exec_state => {
      CheckScopeChain([debug.ScopeType.Local,
                       debug.ScopeType.Closure,
                       debug.ScopeType.Script,
                       debug.ScopeType.Global], exec_state);
      CheckScopeContent({}, 0, exec_state);
    });

await test(
    "(AsyncFunctionExpression) Local 5 --- resume normal",
    async function() { let x = await 1; eval(""); let y = await 2;
                       debugger; }, [],
    exec_state => {
      CheckScopeChain([debug.ScopeType.Local,
                       debug.ScopeType.Closure,
                       debug.ScopeType.Script,
                       debug.ScopeType.Global], exec_state);
      CheckScopeContent({ x: 1, y: 2 }, 0, exec_state);
    });

await test(
    "(AsyncFunctionExpression) Local 5 --- resume throw",
    async function() { let x = await 1; eval("");
                       try { let y = await thrower(); }
                       catch (e) { debugger; } }, [],
    exec_state => {
      CheckScopeChain([debug.ScopeType.Catch,
                       debug.ScopeType.Local,
                       debug.ScopeType.Closure,
                       debug.ScopeType.Script,
                       debug.ScopeType.Global], exec_state);
      CheckScopeContent({ e: 'Exception' }, 0, exec_state);
      CheckScopeContent({ x: 1 }, 1, exec_state);
    });

// Local introducing local variable using eval.
await test(
    "(AsyncFunctionExpression) Local 6",
    async function() { eval("var i = 5"); debugger; }, [],
    exec_state => {
      CheckScopeChain([debug.ScopeType.Local,
                       debug.ScopeType.Closure,
                       debug.ScopeType.Script,
                       debug.ScopeType.Global], exec_state);
      CheckScopeContent({i:5}, 0, exec_state);
    });

await test(
    "(AsyncFunctionExpression) Local 6 --- resume normal",
    async function() { let x = await 1; eval("var i = 5"); let y = await 2;
                       debugger; }, [],
    exec_state => {
      CheckScopeChain([debug.ScopeType.Local,
                       debug.ScopeType.Closure,
                       debug.ScopeType.Script,
                       debug.ScopeType.Global], exec_state);
      CheckScopeContent({i:5, x: 1, y: 2}, 0, exec_state);
    });

await test(
    "(AsyncFunctionExpression) Local 6 --- resume throw",
    async function() { let x = await 1; eval("var i = 5");
                       try { let y = await thrower(); }
                       catch (e) { debugger; } }, [],
    exec_state => {
      CheckScopeChain([debug.ScopeType.Catch,
                       debug.ScopeType.Local,
                       debug.ScopeType.Closure,
                       debug.ScopeType.Script,
                       debug.ScopeType.Global], exec_state);
      CheckScopeContent({e: 'Exception' }, 0, exec_state);
      CheckScopeContent({i:5, x: 1}, 1, exec_state);
    });

// Local scope with parameters, local variables and local variable introduced
// using eval.
await test(
    "(AsyncFunctionExpression) Local 7",
    async function(a, b) { var x = 3; var y = 4;
                           eval("var i = 5;"); eval("var j = 6");
                           debugger; }, [1, 2],
    exec_state => {
      CheckScopeChain([debug.ScopeType.Local,
                       debug.ScopeType.Closure,
                       debug.ScopeType.Script,
                       debug.ScopeType.Global], exec_state);
      CheckScopeContent({a:1,b:2,x:3,y:4,i:5,j:6}, 0, exec_state);
    });

await test(
    "(AsyncFunctionExpression) Local 7 --- resume normal",
    async function(a, b) { let z = await 7; var x = 3; var y = 4;
                           eval("var i = 5;"); eval("var j = 6");
                           let q = await 8;
                           debugger; }, [1, 2],
    exec_state => {
      CheckScopeChain([debug.ScopeType.Local,
                       debug.ScopeType.Closure,
                       debug.ScopeType.Script,
                       debug.ScopeType.Global], exec_state);
      CheckScopeContent({a:1,b:2,x:3,y:4,i:5,j:6, z:7, q:8}, 0, exec_state);
    });

await test(
    "(AsyncFunctionExpression) Local 7 --- resume throw",
    async function(a, b) { let z = await 7; var x = 3; var y = 4;
                           eval("var i = 5;"); eval("var j = 6");
                           try { let q = await thrower(); }
                           catch (e) { debugger; } }, [1, 2],
    exec_state => {
      CheckScopeChain([debug.ScopeType.Catch,
                       debug.ScopeType.Local,
                       debug.ScopeType.Closure,
                       debug.ScopeType.Script,
                       debug.ScopeType.Global], exec_state);
      CheckScopeContent({e: 'Exception'}, 0, exec_state);
      //CheckScopeContent({a:1,b:2,x:3,y:4,i:5,j:6, z:7}, 1, exec_state);
    });

// Nested empty with blocks.
await test(
    "(AsyncFunctionExpression) With",
    async function() { with ({}) { with ({}) { debugger; } } }, [],
    exec_state => {
      CheckScopeChain([debug.ScopeType.With,
                       debug.ScopeType.With,
                       debug.ScopeType.Local,
                       debug.ScopeType.Closure,
                       debug.ScopeType.Script,
                       debug.ScopeType.Global], exec_state);
      CheckScopeContent({}, 0, exec_state);
      CheckScopeContent({}, 1, exec_state);
    });

await test(
    "(AsyncFunctionExpression) With --- resume normal",
    async function() { let x = await 1; with ({}) { with ({}) {
                       let y = await 2; debugger; } } }, [],
    exec_state => {
      CheckScopeChain([debug.ScopeType.Block,
                       debug.ScopeType.With,
                       debug.ScopeType.With,
                       debug.ScopeType.Local,
                       debug.ScopeType.Closure,
                       debug.ScopeType.Script,
                       debug.ScopeType.Global], exec_state);
      CheckScopeContent({y:2}, 0, exec_state);
      CheckScopeContent({}, 1, exec_state);
      CheckScopeContent({}, 2, exec_state);
      CheckScopeContent({x:1}, 3, exec_state);
    });

await test(
    "(AsyncFunctionExpression) With --- resume throw",
    async function() { let x = await 1; with ({}) { with ({}) {
                       try { let y = await thrower(); }
                       catch (e) { debugger; } } } }, [],
    exec_state => {
      CheckScopeChain([debug.ScopeType.Catch,
                       debug.ScopeType.With,
                       debug.ScopeType.With,
                       debug.ScopeType.Local,
                       debug.ScopeType.Closure,
                       debug.ScopeType.Script,
                       debug.ScopeType.Global], exec_state);
      CheckScopeContent({ e: 'Exception'}, 0, exec_state);
      CheckScopeContent({}, 1, exec_state);
      CheckScopeContent({}, 2, exec_state);
      CheckScopeContent({x:1}, 3, exec_state);
    });

// Simple closure formed by returning an inner function referering the outer
// functions arguments.
await test(
    "(AsyncFunctionExpression) Closure 1",
    async function(a) { return function() { debugger; return a; } }, [1],
    exec_state => {
      CheckScopeChain([debug.ScopeType.Local,
                       debug.ScopeType.Closure,
                       debug.ScopeType.Closure,
                       debug.ScopeType.Script,
                       debug.ScopeType.Global], exec_state);
      CheckScopeContent({a:1}, 1, exec_state);
    },
    result => result());

await test(
    "(AsyncFunctionExpression) Closure 1 --- resume normal",
    async function(a) { let x = await 2;
                        return function() { debugger; return a; } }, [1],
    exec_state => {
      CheckScopeChain([debug.ScopeType.Local,
                       debug.ScopeType.Closure,
                       debug.ScopeType.Closure,
                       debug.ScopeType.Script,
                       debug.ScopeType.Global], exec_state);
      CheckScopeContent({a:1, x: 2}, 1, exec_state);
    },
    result => result());

await test(
    "(AsyncFunctionExpression) Closure 1 --- resume throw",
    async function(a) { let x = await 2;
                        return async function() {
                            try { await thrower(); }
                            catch (e) { debugger; } return a; }; }, [1],
    exec_state => {
      CheckScopeChain([debug.ScopeType.Catch,
                       debug.ScopeType.Local,
                       debug.ScopeType.Closure,
                       debug.ScopeType.Closure,
                       debug.ScopeType.Script,
                       debug.ScopeType.Global], exec_state);
      CheckScopeContent({e: 'Exception'}, 0, exec_state);
      CheckScopeContent({a:1, x: 2}, 2, exec_state);
    },
    result => result());

await test(
    "(AsyncFunctionExpression) Catch block 1",
    async function() { try { throw 'Exception'; } catch (e) { debugger; } }, [],
    exec_state => {
      CheckScopeChain([debug.ScopeType.Catch,
                       debug.ScopeType.Local,
                       debug.ScopeType.Closure,
                       debug.ScopeType.Script,
                       debug.ScopeType.Global], exec_state);
      CheckScopeContent({e:'Exception'}, 0, exec_state);
    });

await test(
    "(AsyncFunctionExpression) Catch block 1 --- resume normal",
    async function() {
      let x = await 1;
      try { throw 'Exception'; } catch (e) { let y = await 2; debugger; } }, [],
    exec_state => {
      CheckScopeChain([debug.ScopeType.Block,
                       debug.ScopeType.Catch,
                       debug.ScopeType.Local,
                       debug.ScopeType.Closure,
                       debug.ScopeType.Script,
                       debug.ScopeType.Global], exec_state);
      CheckScopeContent({y: 2}, 0, exec_state);
      CheckScopeContent({e:'Exception'}, 1, exec_state);
      CheckScopeContent({x: 1}, 2, exec_state);
    });

await test(
    "(AsyncFunctionExpression) Catch block 1 --- resume throw",
    async function() {
      let x = await 1;
      try { throw 'Exception!'; } catch (e) {
        try { let y = await thrower(); } catch (e) { debugger; } } }, [],
    exec_state => {
      CheckScopeChain([debug.ScopeType.Catch,
                       debug.ScopeType.Catch,
                       debug.ScopeType.Local,
                       debug.ScopeType.Closure,
                       debug.ScopeType.Script,
                       debug.ScopeType.Global], exec_state);
      CheckScopeContent({e:'Exception'}, 0, exec_state);
      CheckScopeContent({e:'Exception!'}, 1, exec_state);
      CheckScopeContent({x: 1}, 2, exec_state);
    });
}

runTests().catch(error => {
  print(error.stack);
  quit(1);
})

// Check that two scope are the same.
function assertScopeMirrorEquals(scope1, scope2) {
  assertEquals(scope1.scopeType(), scope2.scopeType());
  assertEquals(scope1.frameIndex(), scope2.frameIndex());
  assertEquals(scope1.scopeIndex(), scope2.scopeIndex());
  assertPropertiesEqual(
      scope1.scopeObject().value(), scope2.scopeObject().value());
}

function CheckFastAllScopes(scopes, exec_state) {
  var fast_all_scopes = exec_state.frame().allScopes(true);
  var length = fast_all_scopes.length;
  assertTrue(scopes.length >= length);
  for (var i = 0; i < scopes.length && i < length; i++) {
    var scope = fast_all_scopes[length - i - 1];
    assertEquals(scopes[scopes.length - i - 1], scope.scopeType());
  }
}

// Check that the scope chain contains the expected types of scopes.
function CheckScopeChain(scopes, exec_state) {
  var all_scopes = exec_state.frame().allScopes();
  assertEquals(
      scopes.length, all_scopes.length, "FrameMirror.allScopes length");
  for (var i = 0; i < scopes.length; i++) {
    var scope = exec_state.frame().scope(i);
    assertEquals(scopes[i], scope.scopeType());
    assertScopeMirrorEquals(all_scopes[i], scope);

    // Check the global object when hitting the global scope.
    if (scopes[i] == debug.ScopeType.Global) {
      assertEquals(global_marker, scope.scopeObject().value().global_marker);
    }
  }
  CheckFastAllScopes(scopes, exec_state);
}

// Check that the content of the scope is as expected. For functions just check
// that there is a function.
function CheckScopeContent(content, number, exec_state) {
  var scope = exec_state.frame().scope(number);
  var count = 0;
  for (var p in content) {
    var property_mirror = scope.scopeObject().property(p);
    assertFalse(property_mirror.isUndefined(),
                `property ${p} not found in scope`);
    assertEquals(content[p], property_mirror.value().value(),
                 `property ${p} has unexpected value`);
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

  if (scope_size < count) {
    print('Names found in scope:');
    var names = scope.scopeObject().propertyNames();
    for (var i = 0; i < names.length; i++) {
      print(names[i]);
    }
  }
  assertTrue(scope_size >= count);
}
