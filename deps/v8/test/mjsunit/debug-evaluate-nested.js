// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-debug-as debug

Debug = debug.Debug;
ScopeType = debug.ScopeType;
var exception = null;
var nested = false;

function bar() {
  let a = 1;
  (function foo() {
    let b = a;
    with (new Proxy({}, {})) {
      debugger;
    }
  })();
}

function checkScopes(scopes, expectation) {
  assertEquals(scopes.map(s => s.scopeType()), expectation);
}

function listener(event, exec_state, event_data, data) {
  if (event != Debug.DebugEvent.Break) return;
  try {
    if (!nested) {
      nested = true;
      checkScopes(exec_state.frame(0).allScopes(),
                  [ ScopeType.With, ScopeType.Local, ScopeType.Closure,
                    ScopeType.Script, ScopeType.Global ]);
      exec_state.frame(0).evaluate("debugger;");
    } else {
      checkScopes(exec_state.frame(0).allScopes(),
                  [ ScopeType.Eval, ScopeType.With, ScopeType.Closure,
                    ScopeType.Script, ScopeType.Global ]);
    }
  } catch (e) {
    exception = e;
    print(e + e.stack);
  }
}

Debug.setListener(listener);
bar();
Debug.setListener(null);
assertNull(exception);
