// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Test that *local* debug-evaluate properly works for REPL 'let'
// re-declarations.

Debug = debug.Debug

let exception = null;
function listener(event, exec_state, event_data, data) {
  if (event != Debug.DebugEvent.Break) return;
  try {
    assertEquals(42, exec_state.frame(0).evaluate("x").value());
  } catch (e) {
    exception = e;
    print(e + e.stack);
  }
}

Debug.setListener(listener);

// First script introduces a let-binding 'x'. The value of
// 'x' lives in the ScriptContext of this REPL script.
Debug.evaluateGlobalREPL('let x = 21;');

// The second script re-declares 'x', but then breaks and
// evaluates x.
Debug.evaluateGlobalREPL(`
  let x = 42;
  (function foo() {
    debugger;
  })();
`);

Debug.setListener(null);
assertNull(exception);
