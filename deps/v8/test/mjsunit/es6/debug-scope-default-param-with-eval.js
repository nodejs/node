// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-debug-as debug

// Test that the parameter initialization block scope set up for
// sloppy eval is visible to the debugger.

var Debug = debug.Debug;
var exception = null;
var break_count = 0;

function call_for_break() {
  return 5;
}

function test(x = eval("var y = 7; debugger; y") + call_for_break()) {
  return x;
}

function listener(event, exec_state, event_data, data) {
  if (event != Debug.DebugEvent.Break) return;
  try {
    var frame = exec_state.frame(0);
    var block_scope;
    if (break_count++ == 0) {
      // Inside eval.
      assertEquals([ debug.ScopeType.Eval,
                     debug.ScopeType.Block,
                     debug.ScopeType.Closure,
                     debug.ScopeType.Script,
                     debug.ScopeType.Global ],
                   frame.allScopes().map(s => s.scopeType()));
      exec_state.prepareStep(Debug.StepAction.StepOut);
      block_scope = frame.scope(1);
    } else {
      // Outside of eval.
      assertEquals([ debug.ScopeType.Block,
                     debug.ScopeType.Local,
                     debug.ScopeType.Script,
                     debug.ScopeType.Global ],
                   frame.allScopes().map(s => s.scopeType()));
      block_scope = frame.scope(0);
    }
    assertTrue(block_scope.scopeObject().propertyNames().includes('y'));
    assertEquals(7, block_scope.scopeObject().property('y').value().value());
  } catch (e) {
    print(e);
    exception = e;
  }
}

Debug.setListener(listener);

assertEquals(12, test());

Debug.setListener(null);

assertNull(exception);
assertEquals(2, break_count);
