// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --noanalyze-environment-liveness


'use strict';
var Debug = debug.Debug;
var exception = null;

function listener(event, exec_state, event_data, data) {
  if (event != Debug.DebugEvent.Break) return;
  try {
    var scopes = exec_state.frame(0).allScopes();
    assertEquals(debug.ScopeType.Eval, scopes[0].scopeType());
    assertEquals(1, scopes[0].scopeObject().value().a);
    assertEquals(debug.ScopeType.Script, scopes[1].scopeType());
    assertEquals(undefined, scopes[1].scopeObject().value().a);
    assertEquals(debug.ScopeType.Global, scopes[2].scopeType());
    assertEquals(undefined, scopes[2].scopeObject().value().a);
  } catch (e) {
    exception = e;
  }
}

Debug.setListener(listener);
// Eval inherits strict mode.
eval("var a = 1; debugger;");
Debug.setListener(null);
assertNull(exception);
