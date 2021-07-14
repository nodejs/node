// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

Debug = debug.Debug;

function test(i) {
  if (i == 3) {
    return (function* gen() { yield test(i - 1); })().next().value;
  } else if (i > 1) {
    return test(i - 1);
  } else {
    debugger;
    return 5;
  }
}

let patch = null, exception = null;

Debug.setListener(listener);
patch = ['return 5', 'return 3'];
assertEquals(5, test(2)); // generator on stack
assertEquals(exception,
    'LiveEdit failed: BLOCKED_BY_ACTIVE_FUNCTION');
patch = ['return 3', 'return -1'];
assertEquals(5, test(5)); // there is running generator
assertEquals(exception,
    'LiveEdit failed: BLOCKED_BY_ACTIVE_FUNCTION');
Debug.setListener(null);

function listener(event) {
  if (event != Debug.DebugEvent.Break || !patch) return;
  try {
    %LiveEditPatchScript(test,
        Debug.scriptSource(test).replace(patch[0], patch[1]));
  } catch (e) {
    exception = e;
  }
  patch = null;
}
