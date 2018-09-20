// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

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
  if (fun.toString().indexOf(original) < 0) return;
  %LiveEditPatchScript(fun, Debug.scriptSource(fun).replace(original, patch));
}

Debug.setListener(listener);

debugger;
results.push(BestEditor());
results.push(BestEditor());
results.push(BestEditor());
Debug.setListener(null);

assertNull(exception);
assertEquals(["Emacs", "Eclipse", "Vim"], results);
assertEquals([
  "debugger;",
  "results.push(BestEditor());",
  "  return 'Emacs';",
  "  return 'Emacs';",
  "results.push(BestEditor());",
  "results.push(BestEditor());",
  "  return 'Emacs';",
  "  return 'Eclipse';",
  "  return 'Eclipse';",
  "results.push(BestEditor());",
  "results.push(BestEditor());",
  "  return 'Eclipse';",
  "  return 'Vim';",
  "  return 'Vim';",
  "results.push(BestEditor());",
  "Debug.setListener(null);"
], log);
