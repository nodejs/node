// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-debug-as debug

var test_y = false;

function foo(a = 1) {
  var x = 2;
  debugger;
  eval("var y = 3");
  test_y = true;
  debugger;
}

var exception = null;
var break_count = 0;
var Debug = debug.Debug;
var ScopeType = debug.ScopeType;

function listener(event, exec_state) {
  if (event != Debug.DebugEvent.Break) return;
  try {
    var scopes = exec_state.frame(0).allScopes();
    var expectation = [ ScopeType.Block,
                        ScopeType.Local,
                        ScopeType.Script,
                        ScopeType.Global ];
    assertEquals(expectation, scopes.map(x => x.scopeType()));
    assertEquals(2, scopes[0].scopeObject().value().x);
    if (test_y) assertEquals(3, scopes[0].scopeObject().value().y);
    assertEquals(1, scopes[1].scopeObject().value().a);
    break_count++;
  } catch (e) {
    print(e);
    exception = e;
  }
}
Debug.setListener(listener);
foo();

assertNull(exception);
assertEquals(2, break_count);
