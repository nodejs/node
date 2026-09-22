// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --no-analyze-environment-liveness

Debug = debug.Debug;

let listener_delegate;
let error = null;

function listener(event, exec_state, event_data, data) {
  try {
    if (event == Debug.DebugEvent.Break) {
      listener_delegate(exec_state);
    }
  } catch (e) {
    error = e;
  }
}
Debug.setListener(listener);

var x = 'global_x';

function outer() {
  let y = 'y';
  () => y;  // force context allocation for outer

  // intermediate scope, no context needed
  function intermediate() {
    let x = 'intermediate_x';  // stack variable

    // innermost scope, paused here. No context needed.
    function inner() {
      debugger;
    }
    inner();
  }

  return intermediate;
}

let f = outer();

listener_delegate = function(exec_state) {
  // exec_state.frame(0).scope(1) is 'outer's closure scope.
  // We evaluate 'x' inside 'outer's scope.
  // It should return 'global_x' because 'outer' does not have an 'x'.
  // But due to the defect, 'intermediate's stack variable 'x' leaks
  // into 'outer's context blocklist, causing a ReferenceError.
  let val;
  try {
    val = exec_state.frame(0).scope(1).evaluate('x').value();
  } catch (e) {
    throw new Error('Spurious error: ' + e.message);
  }

  if (val !== 'global_x') {
    throw new Error('Expected global_x, got ' + val);
  }
};

f();

Debug.setListener(null);

if (error) {
  throw error;
}
