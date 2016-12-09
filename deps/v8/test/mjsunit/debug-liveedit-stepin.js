// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-debug-as debug

Debug = debug.Debug

function BestEditor() {
  return 'Emacs';
}

var exception = null;
var results = [];
var log = []

function listener(event, exec_state, event_data, data) {
  if (event != Debug.DebugEvent.Break) return;
  try {
    var source_line = event_data.sourceLineText();
    log.push(source_line);
    if (source_line.indexOf("return") >= 0) {
      switch (results.length) {
        case 0:
          break;
        case 1:
          Replace(BestEditor, "Emacs", "Eclipse");
          break;
        case 2:
          Replace(BestEditor, "Eclipse", "Vim");
          break;
        default:
          assertUnreachable();
      }
    }
    exec_state.prepareStep(Debug.StepAction.StepIn);
  } catch (e) {
    exception = e;
  }
};

function Replace(fun, original, patch) {
  var script = Debug.findScript(fun);
  if (fun.toString().indexOf(original) < 0) return;
  var patch_pos = script.source.indexOf(original);
  var change_log = [];
  Debug.LiveEdit.TestApi.ApplySingleChunkPatch(script, patch_pos, original.length, patch, change_log);
}

Debug.setListener(listener);

debugger;
results.push(BestEditor());
results.push(BestEditor());
results.push(BestEditor());
Debug.setListener(null);

assertNull(exception);
assertEquals(["Emacs", "Eclipse", "Vim"], results);
print(JSON.stringify(log, 1));
assertEquals([
  "debugger;",
  "results.push(BestEditor());",
  "  return 'Emacs';","}",
  "results.push(BestEditor());",
  "results.push(BestEditor());",
  "  return 'Emacs';",
  "  return 'Eclipse';","}",
  "results.push(BestEditor());",
  "results.push(BestEditor());",
  "  return 'Eclipse';",
  "  return 'Vim';",
  "}","results.push(BestEditor());",
  "Debug.setListener(null);"
], log);
