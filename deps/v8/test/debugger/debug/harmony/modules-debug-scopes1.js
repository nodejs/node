// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// MODULE
// Flags: --allow-natives-syntax --noanalyze-environment-liveness

// These tests are copied from mjsunit/debug-scopes.js and adapted for modules.


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
  assertTrue(listener_called, "listener not called for " + test_name);
  assertNull(exception, test_name + " / " + exception);
  end_test_count++;
}


// Check that two scope are the same.
function assertScopeMirrorEquals(scope1, scope2) {
  assertEquals(scope1.scopeType(), scope2.scopeType());
  assertEquals(scope1.frameIndex(), scope2.frameIndex());
  assertEquals(scope1.scopeIndex(), scope2.scopeIndex());
  assertPropertiesEqual(scope1.scopeObject().value(),
                        scope2.scopeObject().value());
}

function CheckFastAllScopes(scopes, exec_state)
{
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
  assertEquals(scopes.length, exec_state.frame().scopeCount());
  assertEquals(scopes.length, all_scopes.length,
               "FrameMirror.allScopes length");
  for (var i = 0; i < scopes.length; i++) {
    var scope = exec_state.frame().scope(i);
    assertEquals(scopes[i], scope.scopeType());
    assertScopeMirrorEquals(all_scopes[i], scope);
  }
  CheckFastAllScopes(scopes, exec_state);
}


// Check that the scope chain contains the expected names of scopes.
function CheckScopeChainNames(names, exec_state) {
  var all_scopes = exec_state.frame().allScopes();
  assertEquals(names.length, all_scopes.length, "FrameMirror.allScopes length");
  for (var i = 0; i < names.length; i++) {
    var scope = exec_state.frame().scope(i);
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
    assertFalse(property_mirror.isUndefined(),
                'property ' + p + ' not found in scope');
    assertEquals(minimum_content[p], property_mirror.value().value(),
                 'property ' + p + ' has unexpected value');
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
}

// Check that the scopes have positions as expected.
function CheckScopeChainPositions(positions, exec_state) {
  var all_scopes = exec_state.frame().allScopes();
  assertTrue(positions.length <= all_scopes.length,
             "FrameMirror.allScopes length");
  for (var i = 0; i < positions.length; i++) {
    var scope = exec_state.frame().scope(i);
    var position = positions[i];
    if (!position)
      continue;

    print(
        `Checking position.start = ${position.start}, .end = ${position.end}`);
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
                   debug.ScopeType.Module,
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
                   debug.ScopeType.Module,
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
                   debug.ScopeType.Module,
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
                   debug.ScopeType.Module,
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
                   debug.ScopeType.Module,
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
                   debug.ScopeType.Module,
                   debug.ScopeType.Script,
                   debug.ScopeType.Global], exec_state);
  CheckScopeContent({}, 0, exec_state);
};
local_6();
EndTest();


// Local scope with parameters and local variables.
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
                   debug.ScopeType.Module,
                   debug.ScopeType.Script,
                   debug.ScopeType.Global], exec_state);
  CheckScopeContent({a:1,b:2,x:3,y:4}, 0, exec_state);
};
local_7(1, 2);
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
                   debug.ScopeType.Module,
                   debug.ScopeType.Script,
                   debug.ScopeType.Global], exec_state);
  CheckScopeContent({a:1}, 1, exec_state);
  CheckScopeChainNames(["f", "closure_1", undefined, undefined, undefined], exec_state)
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
                   debug.ScopeType.Module,
                   debug.ScopeType.Script,
                   debug.ScopeType.Global], exec_state);
  CheckScopeContent({a:1,x:3}, 1, exec_state);
  CheckScopeChainNames(["f", "closure_2", undefined, undefined, undefined],
                       exec_state)
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
                   debug.ScopeType.Module,
                   debug.ScopeType.Script,
                   debug.ScopeType.Global], exec_state);
  CheckScopeContent({a:1,b:2,x:3,y:4}, 1, exec_state);
  CheckScopeChainNames(["f", "closure_3", undefined, undefined, undefined],
                       exec_state)
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
                   debug.ScopeType.Module,
                   debug.ScopeType.Script,
                   debug.ScopeType.Global], exec_state);
  CheckScopeContent({a:1,b:2,x:3,y:4,f:undefined}, 1, exec_state);
  CheckScopeChainNames(["f", "closure_4", undefined, undefined, undefined],
                       exec_state)
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
                   debug.ScopeType.Module,
                   debug.ScopeType.Script,
                   debug.ScopeType.Global], exec_state);
  CheckScopeContent({a:1,b:2,x:3,y:4,f:undefined}, 1, exec_state);
  CheckScopeChainNames(["f", "closure_5", undefined, undefined, undefined],
                       exec_state)
};
closure_5(1, 2)();
EndTest();


// Two closures. Due to optimizations only the parts actually used are provided
// through the debugger information.
BeginTest("Closure 6");
let some_global;
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
                   debug.ScopeType.Module,
                   debug.ScopeType.Script,
                   debug.ScopeType.Global], exec_state);
  CheckScopeContent({a:1}, 1, exec_state);
  CheckScopeContent({f:undefined}, 2, exec_state);
  CheckScopeChainNames(
      [undefined, "f", "closure_6", undefined, undefined, undefined],
      exec_state);
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
                   debug.ScopeType.Module,
                   debug.ScopeType.Script,
                   debug.ScopeType.Global], exec_state);
  CheckScopeContent({}, 0, exec_state);
  CheckScopeContent({a:1,b:2,x:3,y:4}, 1, exec_state);
  CheckScopeContent({a:1,b:2,x:3,y:4,f:undefined}, 2, exec_state);
  CheckScopeChainNames(
      [undefined, "f", "closure_7", undefined, undefined, undefined],
      exec_state);
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
                   debug.ScopeType.Module,
                   debug.ScopeType.Script,
                   debug.ScopeType.Global], exec_state);
  CheckScopeContent({x: 2}, 0, exec_state);
  CheckScopeChainNames(["inner", undefined, undefined, undefined], exec_state);
};
closure_8();
EndTest();


BeginTest("Closure 9");
let closure_9 = Function(' \
  eval("var y = 1;"); \
  eval("var z = 1;"); \
  (function inner(x) { \
    y++; \
    z++; \
    debugger; \
  })(2); \
')

listener_delegate = function(exec_state) {
  CheckScopeChain([debug.ScopeType.Local,
                   debug.ScopeType.Closure,
                   debug.ScopeType.Script,
                   debug.ScopeType.Global], exec_state);
  CheckScopeChainNames(["inner", undefined, undefined, undefined], exec_state);
};
closure_9();
EndTest();


// Test global scope.
BeginTest("Global");
listener_delegate = function(exec_state) {
  CheckScopeChain(
      [debug.ScopeType.Module, debug.ScopeType.Script, debug.ScopeType.Global],
      exec_state);
  CheckScopeChainNames([undefined, undefined, undefined], exec_state);
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
                   debug.ScopeType.Module,
                   debug.ScopeType.Script,
                   debug.ScopeType.Global], exec_state);
  CheckScopeContent({e:'Exception'}, 0, exec_state);
  CheckScopeChainNames(
      ["catch_block_1", "catch_block_1", undefined, undefined, undefined],
      exec_state);
};
catch_block_1();
EndTest();


BeginTest("Catch block 3");
function catch_block_3() {
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
                   debug.ScopeType.Module,
                   debug.ScopeType.Script,
                   debug.ScopeType.Global], exec_state);
  CheckScopeContent({e:'Exception'}, 0, exec_state);
  CheckScopeContent({}, 1, exec_state);
  CheckScopeChainNames(
      ["catch_block_3", "catch_block_3", undefined, undefined, undefined],
      exec_state);
};
catch_block_3();
EndTest();


// Test catch in global scope.
BeginTest("Catch block 5");
listener_delegate = function(exec_state) {
  CheckScopeChain([debug.ScopeType.Catch,
                   debug.ScopeType.Module,
                   debug.ScopeType.Script,
                   debug.ScopeType.Global], exec_state);
  CheckScopeContent({e:'Exception'}, 0, exec_state);
  CheckScopeChainNames([undefined, undefined, undefined, undefined],
                       exec_state);
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
                   debug.ScopeType.Module,
                   debug.ScopeType.Script,
                   debug.ScopeType.Global], exec_state);
  CheckScopeContent({x: 2}, 0, exec_state);
  CheckScopeContent({e:'Exception'}, 1, exec_state);
  CheckScopeChainNames([undefined, undefined, undefined, undefined, undefined],
                       exec_state);
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
                   debug.ScopeType.Module,
                   debug.ScopeType.Script,
                   debug.ScopeType.Global], exec_state);
  CheckScopeContent({e:'Exception'}, 0, exec_state);
  CheckScopeChainNames(
      ["catch_block_7", "catch_block_7", undefined, undefined, undefined],
      exec_state);
};
catch_block_7();
EndTest();


BeginTest("Classes and methods 1");

listener_delegate = function(exec_state) {
  "use strict"
  CheckScopeChain([debug.ScopeType.Local,
                   debug.ScopeType.Module,
                   debug.ScopeType.Script,
                   debug.ScopeType.Global], exec_state);
  CheckScopeContent({}, 1, exec_state);
  CheckScopeChainNames(["m", undefined, undefined, undefined], exec_state);
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
  CheckScopeChainPositions([{start: 58, end: 118}, {start: 10, end: 162}],
                           exec_state);
}
eval(code1);
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
                   debug.ScopeType.Module,
                   debug.ScopeType.Script,
                   debug.ScopeType.Global], exec_state);
  CheckScopeChainPositions([{start: 52, end: 111}, {start: 22, end: 145}],
                           exec_state);
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
                   debug.ScopeType.Module,
                   debug.ScopeType.Script,
                   debug.ScopeType.Global], exec_state);
  CheckScopeChainPositions([{start: 66, end: 147},
                            {start: 52, end: 147},
                            {start: 22, end: 181}], exec_state);
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
                   debug.ScopeType.Module,
                   debug.ScopeType.Script,
                   debug.ScopeType.Global], exec_state);
  CheckScopeChainPositions([{start: 55, end: 111}, {start: 27, end: 145}],
                           exec_state);
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
                   debug.ScopeType.Module,
                   debug.ScopeType.Script,
                   debug.ScopeType.Global], exec_state);
  CheckScopeChainPositions([{start: 57, end: 147},
                            {start: 55, end: 147},
                            {start: 27, end: 181}], exec_state);
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
                   debug.ScopeType.Module,
                   debug.ScopeType.Script,
                   debug.ScopeType.Global], exec_state);
  CheckScopeChainPositions([{start: 27, end: 181}], exec_state);
}
eval(code7);
EndTest();

BeginTest(
    "Scope positions in non-lexical for each statement with lexical block");
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
                   debug.ScopeType.Module,
                   debug.ScopeType.Script,
                   debug.ScopeType.Global], exec_state);
  CheckScopeChainPositions([{start: 89, end: 183}, {start: 27, end: 217}],
                           exec_state);
}
eval(code8);
EndTest();

assertEquals(begin_test_count, break_count,
             'one or more tests did not enter the debugger');
assertEquals(begin_test_count, end_test_count,
             'one or more tests did not have its result checked');
