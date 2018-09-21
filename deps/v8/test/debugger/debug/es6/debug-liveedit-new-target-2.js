// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax
// Test that live-editing a frame to introduce new.target fails.

Debug = debug.Debug
var calls = 0;
var exceptions = 0;
var results = [];
var replace_again;

eval(`
  function LogNewTarget() {
    calls++;
    ReplaceOnce();
    results.push(true);
  }
`);

function ExecuteInDebugContext(f) {
  var result;
  var exception = null;
  Debug.setListener(function(event) {
    if (event == Debug.DebugEvent.Break) {
      try {
        result = f();
      } catch (e) {
        // Rethrow this exception later.
        exception = e;
      }
    }
  });
  debugger;
  Debug.setListener(null);
  if (exception !== null) throw exception;
  return result;
}

function Replace(fun, original, patch) {
  ExecuteInDebugContext(function() {
    try {
      %LiveEditPatchScript(fun, Debug.scriptSource(fun).replace(original, patch));
    } catch (e) {
      assertEquals(e, 'LiveEdit failed: BLOCKED_BY_NEW_TARGET_IN_RESTART_FRAME');
      exceptions++;
    }
  });
}

function ReplaceOnce(x) {
  if (replace_again) {
    replace_again = false;
    Replace(LogNewTarget, "true", "new.target");
  }
}

function Revert() {
  Replace(LogNewTarget, "new.target", "true");
}

replace_again = true;
ReplaceOnce();
new LogNewTarget();
Revert();
assertEquals(1, calls);
assertEquals(0, exceptions);
assertEquals([LogNewTarget], results);

replace_again = true;
new LogNewTarget();
assertEquals(2, calls);  // No restarts
assertEquals(1, exceptions);  // Replace failed.
assertEquals([LogNewTarget, true], results);
