// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-debug-as debug

function listener(event, exec_state, event_data, data) {
  if (event != Debug.DebugEvent.Break) return;
  try {
    var all_scopes = exec_state.frame().allScopes();
    assertEquals([ debug.ScopeType.Block,
                   debug.ScopeType.Local,
                   debug.ScopeType.Script,
                   debug.ScopeType.Global ],
                 all_scopes.map(scope => scope.scopeType()));
  } catch (e) {
    exception = e;
  }
}

debug.Debug.setListener(listener);

(function(arg, ...rest) {
  var one = 1;
  function inner() {
    one;
    arg;
  }
  debugger;
})();
