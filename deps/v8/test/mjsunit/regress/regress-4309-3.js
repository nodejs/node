// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-debug-as debug --allow-natives-syntax

var Debug = debug.Debug;

var exception = null;

function listener(event, exec_state, event_data, data) {
  if (event != Debug.DebugEvent.Break) return;
  try {
    var scopes = exec_state.frame().allScopes();
    assertEquals(4, scopes.length);
    assertEquals(debug.ScopeType.With, scopes[0].scopeType());
    assertEquals(debug.ScopeType.Local, scopes[1].scopeType());
    assertEquals(debug.ScopeType.Script, scopes[2].scopeType());
    assertEquals(debug.ScopeType.Global, scopes[3].scopeType());
  } catch (e) {
    exception = e;
  }
}

function f() {
  with({}) {
    debugger;
  }
}

f();
f();

%OptimizeFunctionOnNextCall(f);
Debug.setListener(listener);

f();

assertNull(exception);
