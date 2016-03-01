// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-debug-as debug --allow-natives-syntax

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

function Replace(fun, original, patch) {
  %ExecuteInDebugContext(function() {
    var change_log = [];
    try {
      var script = Debug.findScript(fun);
      var patch_pos = script.source.indexOf(original);
      Debug.LiveEdit.TestApi.ApplySingleChunkPatch(
          script, patch_pos, original.length, patch, change_log);
    } catch (e) {
      assertEquals("BLOCKED_NO_NEW_TARGET_ON_RESTART",
                   change_log[0].functions_on_stack[0].replace_problem);
      assertInstanceof(e, Debug.LiveEdit.Failure);
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
