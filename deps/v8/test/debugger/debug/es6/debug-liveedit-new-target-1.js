// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


// Test that live-editing a frame that uses new.target fails.

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
    results.push(new.target);
  }
`);

function Dummy() {}

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

function ReplaceOnce() {
  if (replace_again) {
    replace_again = false;
    Replace(LogNewTarget, "true", "false");
  }
}

function Revert() {
  Replace(LogNewTarget, "false", "true");
}

replace_again = true;
ReplaceOnce();
new LogNewTarget();
Revert();
assertEquals(1, calls);
assertEquals(0, exceptions);
assertEquals([false, LogNewTarget], results);

replace_again = true;
LogNewTarget();

replace_again = true;
new LogNewTarget();

replace_again = true;
Reflect.construct(LogNewTarget, [], Dummy);

assertEquals(
    [false, LogNewTarget, true, undefined, true, LogNewTarget, true, Dummy],
    results);
assertEquals(4, calls);  // No restarts
assertEquals(3, exceptions);  // Replace failed.
