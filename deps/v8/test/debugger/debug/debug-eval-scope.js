// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --no-always-turbofan

// Test that the (strict) eval scope is visible to the debugger.

var Debug = debug.Debug;
var exception = null;
var delegate = null;

function listener(event, exec_state, event_data, data) {
  if (event != Debug.DebugEvent.Break) return;
  try {
    delegate(exec_state);
  } catch (e) {
    exception = e;
  }
}

Debug.setListener(listener);

// Current function is the top-level eval.
// We can access stack- and context-allocated values in the eval-scope.
delegate = function(exec_state) {
  assertEquals([ debug.ScopeType.Eval,
                 debug.ScopeType.Script,
                 debug.ScopeType.Global ],
               exec_state.frame(0).allScopes().map(s => s.scopeType()));
  var scope = exec_state.frame(0).scope(0);
  assertEquals(1, scope.scopeObject().property("a").value().value());
  assertEquals(1, exec_state.frame(0).evaluate("a").value());
  scope.setVariableValue("a", 2);
  assertEquals(2, exec_state.frame(0).evaluate("a++").value());
}

eval("'use strict';      \n" +
     "var a = 1;         \n" +
     "debugger;          \n" +
     "assertEquals(3, a);\n");

eval("'use strict';      \n" +
     "var a = 1;         \n" +
     "(x=>a);            \n" +  // Force context-allocation.
     "debugger;          \n" +
     "assertEquals(3, a);\n");

// Current function is an inner function.
// We cannot access stack-allocated values in the eval-scope.
delegate = function(exec_state) {
  assertEquals([ debug.ScopeType.Local,
                 debug.ScopeType.Script,
                 debug.ScopeType.Global ],
               exec_state.frame(0).allScopes().map(s => s.scopeType()));
  assertEquals([ debug.ScopeType.Eval,
                 debug.ScopeType.Script,
                 debug.ScopeType.Global ],
               exec_state.frame(1).allScopes().map(s => s.scopeType()));
  var scope = exec_state.frame(0).scope(0);
  assertThrows(() => exec_state.frame(0).evaluate("a"), ReferenceError);
  assertTrue(scope.scopeObject().property("a").isUndefined());
}

eval("'use strict';       \n" +
     "var a = 1;          \n" +
     "(() => {debugger})()\n");

// Current function is an escaped inner function.
delegate = function(exec_state) {
  assertEquals([ debug.ScopeType.Local,
                 debug.ScopeType.Script,
                 debug.ScopeType.Global ],
               exec_state.frame(0).allScopes().map(s => s.scopeType()));
  assertEquals([ debug.ScopeType.Script,
                 debug.ScopeType.Global ],
               exec_state.frame(1).allScopes().map(s => s.scopeType()));
  var scope = exec_state.frame(0).scope(0);
  assertThrows(() => exec_state.frame(0).evaluate("a"), ReferenceError);
  assertTrue(scope.scopeObject().property("a").isUndefined());
}

var f = eval("'use strict';   \n" +
             "var a = 1;      \n" +
             "() => {debugger}\n");
f();

// Current function is an inner function.
// We can access context-allocated values in the eval-scope.
delegate = function(exec_state) {
  assertEquals([ debug.ScopeType.Local,
                 debug.ScopeType.Closure,
                 debug.ScopeType.Script,
                 debug.ScopeType.Global ],
               exec_state.frame(0).allScopes().map(s => s.scopeType()));
  assertEquals([ debug.ScopeType.Eval,
                 debug.ScopeType.Script,
                 debug.ScopeType.Global ],
               exec_state.frame(1).allScopes().map(s => s.scopeType()));
  var scope = exec_state.frame(1).scope(0);
  assertEquals(1, scope.scopeObject().property("a").value().value());
  assertEquals(1, exec_state.frame(1).evaluate("a").value());
  assertEquals(1, exec_state.frame(0).evaluate("a").value());
  scope.setVariableValue("a", 2);
  assertEquals(2, exec_state.frame(0).evaluate("a++").value());
  assertEquals(3, exec_state.frame(1).evaluate("a++").value());
}

eval("'use strict';               \n" +
     "var a = 1;                  \n" +
     "(() => { a;                 \n" +  // Force context-allocation.
     "         debugger;          \n" +
     "         assertEquals(4, a);\n" +
     "       })();                \n"
     );

// Current function is an escaped inner function.
// We can access context-allocated values in the eval-scope.
delegate = function(exec_state) {
  assertEquals([ debug.ScopeType.Local,
                 debug.ScopeType.Closure,
                 debug.ScopeType.Script,
                 debug.ScopeType.Global ],
               exec_state.frame(0).allScopes().map(s => s.scopeType()));
  assertEquals([ debug.ScopeType.Script,
                 debug.ScopeType.Global ],
               exec_state.frame(1).allScopes().map(s => s.scopeType()));
  var scope = exec_state.frame(0).scope(1);
  assertEquals(1, scope.scopeObject().property("a").value().value());
  assertEquals(1, exec_state.frame(0).evaluate("a").value());
  scope.setVariableValue("a", 2);
  assertEquals(2, exec_state.frame(0).evaluate("a++").value());
}

var g = eval("'use strict';              \n" +
             "var a = 1;                 \n" +
             "() => { a;                 \n" +
             "        debugger;          \n" +
             "        assertEquals(3, a);\n" +
             "      }                    \n");
g();

Debug.setListener(null);
assertNull(exception);
