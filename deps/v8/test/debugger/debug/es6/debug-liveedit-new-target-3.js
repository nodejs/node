// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


// Test that live-editing a frame above one that uses new.target succeeds.

Debug = debug.Debug
var wrapper_calls = 0;
var construct_calls = 0;
var exceptions = 0;
var results = [];
var replace_again;

eval(`
  function LogNewTarget(arg) {
    construct_calls++;
    results.push(new.target);
  }
  function Wrapper() {
    wrapper_calls++;
    ReplaceOnce();
    new LogNewTarget(true);
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
    var change_log = [];
    try {
      var script = Debug.findScript(fun);
      var patch_pos = script.source.indexOf(original);
      Debug.LiveEdit.TestApi.ApplySingleChunkPatch(
          script, patch_pos, original.length, patch, change_log);
    } catch (e) {
      exceptions++;
    }
  });
}

function ReplaceOnce(x) {
  if (replace_again) {
    replace_again = false;
    Replace(Wrapper, "true", "false");
  }
}

function Revert() {
  Replace(Wrapper, "false", "true");
}

replace_again = true;
ReplaceOnce();
Wrapper();
Revert();
assertEquals(1, construct_calls);
assertEquals(1, wrapper_calls);
assertEquals(0, exceptions);  // Replace succeeds
assertEquals([LogNewTarget], results);

Wrapper();
assertEquals(2, construct_calls);
assertEquals(2, wrapper_calls);
assertEquals(0, exceptions);  // Replace succeeds
assertEquals([LogNewTarget, LogNewTarget], results);

replace_again = true;
Wrapper();
assertEquals(3, construct_calls);
assertEquals(4, wrapper_calls);  // Restarts
assertEquals(0, exceptions);  // Replace succeeds
assertEquals([LogNewTarget, LogNewTarget, LogNewTarget], results);
