// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This test fails without defeating TDZ check elision because 'return x;'
// below, without a ThrowReferenceErrorIfHole bytecode, becomes a return
// position breakpoint. Return position breakpoints can only inspect the
// function scope because their source position is always at the end of the
// function.

function* serialize() {
  debugger;
  switch (0) {
    case 0:
      let x = 1;  // Defeat TDZ check elision
    case 1:
      return x;  // Check scopes
  }
}

let exception = null;
let step_count = 0;
let scopes_checked = false;

function listener(event, exec_state, event_data, data) {
  if (event != Debug.DebugEvent.Break) return;
  try {
    if (exec_state.frame().sourceLineText().includes("Check scopes")) {
      let expected = [ debug.ScopeType.Block,
                       debug.ScopeType.Local,
                       debug.ScopeType.Script,
                       debug.ScopeType.Global ];
      for (let i = 0; i < exec_state.frame().scopeCount(); i++) {
        assertEquals(expected[i], exec_state.frame().scope(i).scopeType());
      }
      scopes_checked = true;
    }
    if (step_count++ < 3) exec_state.prepareStep(Debug.StepAction.StepOver);
  } catch (e) {
    exception = e;
    print(e, e.stack);
  }
}



let Debug = debug.Debug;
Debug.setListener(listener);

let gen = serialize();
gen.next();

Debug.setListener(null);
assertNull(exception);
assertEquals(4, step_count);
assertTrue(scopes_checked);
